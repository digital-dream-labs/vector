/*
 * File:          advertisementService.cpp
 * Date:
 * Description:   A service with which devices can register at the registration port if they want to 
 *                advertise to presence to others.
 *                Listener devices can connect to the advertisementPort if they want to see 
 *                advertising devices.
 *
 * Author:        Kevin Yoon
 */

#include <iostream>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "engine/messaging/advertisementService.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/externalInterface/messageGameToEngineTag.h"
#include "util/debug/messageDebugging.h"
#include "util/logging/logging.h"


// Enable define to see warnings from malformed adverts etc.
// We ignore these by default as they're likely spurios network traffic etc.
#define DEBUG_AD_SERVICE  0


namespace Anki {
  namespace Comms {

    AdvertisementService::AdvertisementService(const char* serviceName, RegMsgTag regMsgTag)
    : regServer_("regServer")
    , advertisingServer_("advertisingServer")
    {
      if (regMsgTag == kInvalidRegMsgTag)
      {
        using EMessageTag = Vector::ExternalInterface::MessageGameToEngineTag;
        static_assert(sizeof(EMessageTag) == sizeof(RegMsgTag), "Robot and Game tag size must match");
        _regMsgTag = Util::numeric_cast<RegMsgTag>(EMessageTag::AdvertisementRegistrationMsg);
      }
      else
      {
        _regMsgTag = regMsgTag;
      }
      
      strcpy(serviceName_, serviceName);
    }

    void AdvertisementService::StartService(int registrationPort, int advertisementPort)
    {
      // Start listening for clients that want to advertise
      regServer_.StartListening(registrationPort);
      
      // Start listening for clients that want to receive advertisements
      advertisingServer_.StartListening(advertisementPort);
    }
    
    void AdvertisementService::StopService()
    {
      regServer_.StopListening();
      advertisingServer_.StopListening();
      
      connectionInfoMap_.clear();
    }
    
    void AdvertisementService::Update()
    {
      // Message from device that wants to (de)register for advertising.
      Vector::AdvertisementRegistrationMsg regMsg;
      const size_t kMinAdRegMsgSize = sizeof(RegMsgTag) + regMsg.Size(); // Size of message with an empty ip string
  
      connectionInfoMapIt it;
      
      // Update registered devices
      ssize_t bytes_recvd = 0;
      do {
        uint8_t messageData[64];
        
        bytes_recvd = regServer_.Recv((char*)messageData, sizeof(messageData));
        
        if (bytes_recvd >= kMinAdRegMsgSize)
        {
          const RegMsgTag messageTag = *(RegMsgTag*)messageData;
          
          if (messageTag == _regMsgTag)
          {
            const uint8_t* innerMessageBytes = &messageData[sizeof(RegMsgTag)];
            const size_t   innerMessageSize  = (size_t) bytes_recvd - sizeof(RegMsgTag);
          
            const size_t bytesUnpacked = regMsg.Unpack(innerMessageBytes, innerMessageSize);
            if (bytesUnpacked == innerMessageSize)
            {
              ProcessRegistrationMsg(regMsg);
            }
            else
            {
              PRINT_NAMED_WARNING("AdvertisementService.Recv.ErrorUnpacking", "Unpacked %zu bytes, expected %zu", bytesUnpacked, innerMessageSize);
            }
          }
          else
          {
          #if DEBUG_AD_SERVICE
            namespace CoEx = Vector::ExternalInterface;
            using GToETag = CoEx::MessageGameToEngineTag;
            PRINT_NAMED_WARNING("AdvertisementService.Recv.BadTag",
                                "%s: Received %d byte message with tag %u('%s') when expected %u('%s')\n%s",
                                serviceName_, bytes_recvd, (int)messageTag, CoEx::MessageGameToEngineTagToString((GToETag)messageTag),
                                (int)_regMsgTag, CoEx::MessageGameToEngineTagToString((GToETag)_regMsgTag),
                                Util::ConvertMessageBufferToString(messageData, bytes_recvd, Util::eBTTT_Ascii).c_str());
          #endif // DEBUG_AD_SERVICE
          }
        }
        else if (bytes_recvd > 0)
        {
        #if DEBUG_AD_SERVICE
          PRINT_NAMED_WARNING("AdvertisementService.Recv.AdRegTooSmall",
                              "%s: Received datagram with %d bytes. < %zu bytes min\n%s",
                              serviceName_, bytes_recvd, kMinAdRegMsgSize,
                              Util::ConvertMessageBufferToString(messageData, bytes_recvd, Util::eBTTT_Ascii).c_str());
        #endif // DEBUG_AD_SERVICE
        }
        
      } while (bytes_recvd > 0);

      
      // Get clients that are interested in knowing about advertising devices
      do {
        // The type of data is irrelevant and we don't do anything with it
        // Server automatically adds client to internal list when recv is called.
        uint8_t messageData[64];
        bytes_recvd = advertisingServer_.Recv((char*)messageData, sizeof(messageData));
        
        //if (bytes_recvd > 0) {
        //  std::cout << serviceName_ << ": " << "Received ping from advertisement listener\n";
        //}
      } while(bytes_recvd > 0);
      
      
      // Notify all clients of advertising devices
      if (advertisingServer_.GetNumClients() > 0 && (!connectionInfoMap_.empty() || !oneShotAdvertiseConnectionInfoMap_.empty() )) {
        
        PRINT_NAMED_INFO("AdvertisementService.NotifyClients",
                         "%s: Notifying %d clients of advertising devices",
                         serviceName_, advertisingServer_.GetNumClients());
        
        // Send registered devices' advertisement
        for (int mapType = 0; mapType < 2; ++mapType)
        {
          const ConnectionInfoMap& connectionMap = (mapType == 0) ? connectionInfoMap_ : oneShotAdvertiseConnectionInfoMap_;
          
          for (const auto& kv : connectionMap)
          {
            const Vector::AdvertisementMsg& adMsg = kv.second;
            Anki::Vector::ExternalInterface::MessageGameToEngine message; // We pretend that this came directly from Game
            message.Set_AdvertisementMsg(adMsg);

            PRINT_NAMED_INFO("AdvertisementService.NotifyClients",
                             "%s: Sending %s Advertisement: Device %d on host %s at ports ToEngine: %d FromEngine: %d",
                             serviceName_, (mapType == 0) ? "Connected" : "One-shot",
                             (int)adMsg.id, adMsg.ip.c_str(), (int)adMsg.toEnginePort, (int)adMsg.fromEnginePort);
            
            uint8_t messageData[64];
            size_t bytesPacked = message.Pack(messageData, sizeof(messageData));
            
            advertisingServer_.Send((char*)messageData, bytesPacked);
          }
        }
        
        // Clear all one-shots now that adverts have been sent for them
        oneShotAdvertiseConnectionInfoMap_.clear();
      }
      
    }
    
  
    void AdvertisementService::ProcessRegistrationMsg(const Vector::AdvertisementRegistrationMsg& regMsg)
    {
      if (regMsg.enableAdvertisement)
      {
        PRINT_NAMED_INFO(regMsg.oneShot ? "ProcessRegistrationMsg.ReceivedOneShot" : "ProcessRegistrationMsg.ReceivedRegReq",
                         "%s: Received from device %d on host %s at ports ToEngine: %d FromEngine: %d with advertisement service",
                         serviceName_, (int)regMsg.id, regMsg.ip.c_str(), (int)regMsg.toEnginePort, (int)regMsg.fromEnginePort);

        Vector::AdvertisementMsg& destMsg = regMsg.oneShot ? oneShotAdvertiseConnectionInfoMap_[regMsg.id] : connectionInfoMap_[regMsg.id];
        
        destMsg.id = regMsg.id;
        destMsg.toEnginePort   = regMsg.toEnginePort;
        destMsg.fromEnginePort = regMsg.fromEnginePort;
        destMsg.ip = regMsg.ip;
      }
      else
      {
        PRINT_NAMED_INFO("ProcessRegistrationMsg.ReceivedDereg",
                         "%s: Received from device %d on host %s at ports ToEngine: %d FromEngine: %d with advertisement service",
                         serviceName_, (int)regMsg.id, regMsg.ip.c_str(), (int)regMsg.toEnginePort, (int)regMsg.fromEnginePort);

        connectionInfoMap_.erase(regMsg.id);
      }
    }
    
  
    void AdvertisementService::DeregisterAllAdvertisers()
    {
      connectionInfoMap_.clear();
    }
    
    
  }  // namespace Comms
}  // namespace Anki
