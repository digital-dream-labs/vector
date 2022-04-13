/**
 * File: gameComms.cpp
 *
 * Author: Kevin Yoon
 * Created: 12/16/2014
 *
 * Description: Interface class to allow UI to communicate with game
 *
 * Copyright: Anki, Inc. 2014
 *
 **/
#include "engine/cozmoAPI/comms/gameComms.h"
#include "anki/cozmo/shared/cozmoConfig.h"

#include "util/logging/logging.h"
#include "util/helpers/printByteArray.h"
#include "clad/externalInterface/messageGameToEngine.h"

// For strcpy
#include <stdio.h>
#include <string.h>

// For getting local host's IP address
#ifdef __APPLE__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#endif
#include <ifaddrs.h>
#endif

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include "util/transport/udpTransport.h"

namespace Anki {
namespace Vector {
  
  //const size_t HEADER_SIZE = sizeof(RADIO_PACKET_HEADER);
  
  GameComms::GameComms(int deviceID, int serverListenPort, const char* advertisementRegIP, int advertisementRegPort)
  : server_("gameComms")
  , isInitialized_(false)
  , deviceID_(deviceID)
  , serverListenPort_(serverListenPort)
  , advertisementRegIP_(advertisementRegIP)
  , advertisementRegPort_(advertisementRegPort)
  {
    if (false == server_.StartListening(serverListenPort_)) {
      PRINT_NAMED_ERROR("GameComms.Constructor", "Failed to start listening on port %d", serverListenPort_);
    }
  }
 
  
  GameComms::~GameComms()
  {
    DisconnectClient();
  }
 
  
  // Returns true if we are ready to use TCP
  bool GameComms::IsInitialized()
  {
    return isInitialized_;
  }
  
  ssize_t GameComms::Send(const Comms::MsgPacket &p)
  {

    if (HasClient()) {
      
      // Wrap message in header/footer
      // TODO: Include timestamp too?
      char sendBuf[Comms::MsgPacket::MAX_SIZE];
      size_t sendBufLen = 0;

      assert(p.dataLen < sizeof(sendBuf));
      memcpy(sendBuf, p.data, p.dataLen);
      sendBufLen = p.dataLen;

      /*
      printf("SENDBUF (hex): ");
      PrintBytesHex(sendBuf, sendBufLen);
      printf("\nSENDBUF (uint): ");
      PrintBytesUInt(sendBuf, sendBufLen);
      printf("\n");
      */
      
      return server_.Send(sendBuf, sendBufLen);
    }
    return -1;
    
  }
  
  u32 GameComms::GetNumMsgPacketsInSendQueue(int devID)
  {
    // TODO: This function isn't used on the game side, sent messages aren't queued anyway, so just returning 0.
    return 0;
  }
  
  void GameComms::Update(bool send_queued_msgs)
  {
    if(!IsInitialized()) {
      // Register with advertisement service
      if (regClient_.Connect(advertisementRegIP_, advertisementRegPort_)) {
        regMsg_.id = deviceID_;
        
        {
          const uint32_t localIpAddress = Util::UDPTransport::GetLocalIpAddress();
          const uint8_t* ipBytes = (const uint8_t*)&localIpAddress;
          
          char tempBuff[32];
          snprintf(tempBuff, sizeof(tempBuff), "%d.%d.%d.%d", (int)ipBytes[0], (int)ipBytes[1], (int)ipBytes[2], (int)ipBytes[3]);
          regMsg_.ip = tempBuff;
        }

        regMsg_.toEnginePort = serverListenPort_;
        regMsg_.fromEnginePort = regMsg_.toEnginePort;
        
        isInitialized_ = true;
      } else {
        PRINT_NAMED_INFO("GameComms.Update", "Waiting to connect to advertisement service");
        return;
      }
    }

    if (!server_.HasClient()) {
      AdvertiseToService();
    }
    
    // Read all messages from all connected robots
    ReadAllMsgPackets();
  
  }
  
  
  void GameComms::PrintRecvBuf()
  {
      for (int i=0; i<recvDataSize;i++){
        u8 t = _recvBuf[i];
        printf("0x%x ", t);
      }
      printf("\n");
  }
  
  void GameComms::ReadAllMsgPackets()
  { 
    // Read from all connected clients.
    // Enqueue complete messages.
    
    // Process all datagrams
    while( (recvDataSize = server_.Recv((char*)(_recvBuf), MAX_RECV_BUF_SIZE)) > 0)
    {
      recvdMsgPackets_.emplace_back( (s32)(0),  // Source device ID. Not used for anything now so just 0.
                                    (s32)-1,
                                    recvDataSize,
                                    (u8*)(_recvBuf));
    }
    
    if (recvDataSize < 0) {
      // Disconnect client
      printf("GameComms: Recv failed. Disconnecting client\n");
      server_.DisconnectClient();
    }
  }
  
  // Returns true if a MsgPacket was successfully gotten
  bool GameComms::GetNextMsgPacket(std::vector<uint8_t>& buf)
  {
    if (!recvdMsgPackets_.empty()) {
      const auto& packet = recvdMsgPackets_.front();
      buf = {packet.data, packet.data+packet.dataLen};
      recvdMsgPackets_.pop_front();
      return true;
    }
    
    return false;
  }
  
  
  u32 GameComms::GetNumPendingMsgPackets()
  {
    return (u32)recvdMsgPackets_.size();
  };
  
  void GameComms::ClearMsgPackets()
  {
    recvdMsgPackets_.clear();
    
  };
  
  
  bool GameComms::HasClient()
  {
    return server_.HasClient();
  }
  
  void GameComms::DisconnectClient()
  {
    server_.DisconnectClient();
  }
  
  
  // Register this UI device with advertisement service
  void GameComms::AdvertiseToService()
  {
    regMsg_.enableAdvertisement = 1;
    regMsg_.oneShot = 1;
    
    PRINT_NAMED_INFO("GameComms.AdvertiseToService", "Sending registration for UI device %d at address %s on port %d/%d", regMsg_.id, regMsg_.ip.c_str(),
           (int)regMsg_.toEnginePort, (int)regMsg_.fromEnginePort);
  
    Vector::ExternalInterface::MessageGameToEngine outMessage;
    outMessage.Set_AdvertisementRegistrationMsg(regMsg_);
    
    uint8_t messageBuffer[64];
    const size_t bytesPacked = outMessage.Pack(messageBuffer, sizeof(messageBuffer));
    
    regClient_.Send((const char*)messageBuffer, bytesPacked);
  }
  
  
}  // namespace Vector
}  // namespace Anki


