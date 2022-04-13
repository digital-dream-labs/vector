/**
 * File: multiClientComms.cpp
 *
 * Author: Kevin Yoon
 * Created: 1/22/2014
 *
 * Description: Interface class that creates multiple TCP or UDP clients to connect
 *              and communicate with advertising devices.
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "util/logging/logging.h"
#include "util/helpers/printByteArray.h"
#include "coretech/common/engine/utils/timer.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "engine/multiClientComms.h"

#include "clad/externalInterface/messageGameToEngineTag.h"
#include "util/debug/messageDebugging.h"
#include "util/time/universalTime.h"

#define DEBUG_COMMS 0

namespace Anki {
namespace Vector {
  
  
  static double GetCurrentTimeInSeconds()
  {
    // Note: BaseStationTimer returns 0.0 when not started, so we have to use universal time here
    return Util::Time::UniversalTime::GetCurrentTimeInSeconds();
  }
  
  
  ConnectedDeviceInfo::ConnectedDeviceInfo()
    : _inClient(nullptr)
    , _outClient(nullptr)
    , _initialConnectionTime_s(0.0)
    , _lastRecvTime_s(0.0)
  { 
  }
  
  
  ConnectedDeviceInfo::~ConnectedDeviceInfo()
  {
    DestroyClients();
  }
  
  
  void ConnectedDeviceInfo::ConnectToClients(UdpClient* inClient, UdpClient* outClient)
  {
    assert(_inClient == nullptr);
    assert(_outClient == nullptr);
    
    _inClient = inClient;
    _outClient = outClient;
    
    const double currentTime_s = GetCurrentTimeInSeconds();
    _initialConnectionTime_s = currentTime_s;
    // Pretend we just received something, so the timeout countdown starts from now
    _lastRecvTime_s = currentTime_s;
  }
  
  
  void ConnectedDeviceInfo::DestroyClients()
  {
    if (_inClient && (_inClient != _outClient))
    {
      _inClient->Disconnect();
      delete _inClient;
    }
    _inClient = nullptr;
    
    if (_outClient)
    {
      _outClient->Disconnect();
      delete _outClient;
      _outClient = nullptr;
    }
  }


  MultiClientComms::MultiClientComms()
  : isInitialized_(false)
  {
    
  }
  
  Result MultiClientComms::Init(const char* advertisingHostIP, int advertisingPort, unsigned int maxSentBytesPerTic)
  {
    if(isInitialized_) {
      PRINT_NAMED_WARNING("MultiClientComms.Init.AlreadyInitialized",
                          "Already initialized, disconnecting all devices and from "
                          "advertisement server, then will reinitialize");
      
      DisconnectAllDevices();
      advertisingChannelClient_.Disconnect();
      isInitialized_ = false;
    }
    
    maxSentBytesPerTic_ = maxSentBytesPerTic;
    advertisingHostIP_ = advertisingHostIP;
    
    if(false == advertisingChannelClient_.Connect(advertisingHostIP_, advertisingPort)) {
      PRINT_NAMED_ERROR("MultiClientComms.Init.FailedToConnect", "Failed to connect to advertising host at %s "
                        "on port %d", advertisingHostIP_, advertisingPort);
      return RESULT_FAIL;
    }
    
    #if(DO_SIM_COMMS_LATENCY)
    numRecvRdyMsgs_ = 0;
    #endif
    
    isInitialized_ = true;
    
    return RESULT_OK;
  }
  
  MultiClientComms::~MultiClientComms()
  {
    DisconnectAllDevices();
  }
 
  
  // Returns true if we are ready to use TCP
  bool MultiClientComms::IsInitialized()
  {
    return true;
  }
  
  ssize_t MultiClientComms::Send(const Comms::MsgPacket &p)
  {
    // TODO: Instead of sending immediately, maybe we should queue them and send them all at
    // once to more closely emulate BTLE.

    #if(DO_SIM_COMMS_LATENCY)
    /*
    // If no send latency, just send now
    if (SIM_SEND_LATENCY_SEC == 0.0) {
      if ((maxSentBytesPerTic_ > 0) && (bytesSentThisUpdateCycle_ + p.dataLen > maxSentBytesPerTic_)) {
        #if(DEBUG_COMMS)
        PRINT_NAMED_INFO("MultiClientComms.MaxSendLimitReached", "queueing message");
        #endif
      } else {
        bytesSentThisUpdateCycle_ += p.dataLen;
        return RealSend(p);
      }
    }
    */
    // Otherwise add to send queue
    sendMsgPackets_[p.destId].emplace_back(std::piecewise_construct,
                                           std::forward_as_tuple((GetCurrentTimeInSeconds() + SIM_SEND_LATENCY_SEC)),
                                           std::forward_as_tuple(p));
    
    // Fake the number of bytes sent
    size_t numBytesSent = sizeof(RADIO_PACKET_HEADER) + sizeof(u32) + p.dataLen;
    return numBytesSent;
  }
  
  ssize_t MultiClientComms::RealSend(const Comms::MsgPacket &p)
  {
    #endif // #if(DO_SIM_COMMS_LATENCY)
    
    connectedDevicesIt_t it = connectedDevices_.find(p.destId);
    if (it != connectedDevices_.end())
    {
      UdpClient* udpClient = it->second.GetOutClient();
      const ssize_t sent = udpClient->Send((const char*)p.data, p.dataLen);
    
      if (sent < 0)
      {
        PRINT_NAMED_WARNING("MultiClientComms.RealSend.SendFailed", "destId: %d, socket %d, sent = %zd (errno = %d '%s')",
                            p.destId, udpClient->GetSocketFd(), sent, errno, strerror(errno));
      }
      
      return sent;
    }
    else
    {
      PRINT_NAMED_WARNING("MultiClientComms.RealSend.NotConnected", "destId: %d", p.destId);
      return -1;
    }
  }
  
  
  void MultiClientComms::Update(bool send_queued_msgs)
  {
    const double currentTime_s = GetCurrentTimeInSeconds();
    
    // Read datagrams and update advertising device list.
    using EMessageTag = Vector::ExternalInterface::MessageGameToEngineTag;
    const EMessageTag kAdvertisementMsgTag = EMessageTag::AdvertisementMsg;
    AdvertisementMsg advMsg;
    const size_t kMinAdMsgSize = sizeof(EMessageTag) + advMsg.Size(); // Size of message with an empty ip string
    
    ssize_t bytes_recvd = 0;
    do {
      uint8_t messageData[64];
      bytes_recvd = advertisingChannelClient_.Recv((char*)messageData, sizeof(messageData));
      if (bytes_recvd >= kMinAdMsgSize)
      {
        const EMessageTag messageTag = *(EMessageTag*)messageData;
        
        if (messageTag == kAdvertisementMsgTag)
        {
          const uint8_t* innerMessageBytes = &messageData[sizeof(EMessageTag)];
          const size_t   innerMessageSize  = (size_t) bytes_recvd - sizeof(EMessageTag);
          
          const size_t bytesUnpacked = advMsg.Unpack(innerMessageBytes, innerMessageSize);
          
          if (bytesUnpacked == innerMessageSize)
          {
            // Check if already connected to this device.
            // Advertisement may have arrived right after connection.
            // If not already connected, add it to advertisement list.
            
            connectedDevicesIt_t it = connectedDevices_.find(advMsg.id);
            if (it != connectedDevices_.end())
            {
              // if connection is old assume this is a new connection attempt
              // disconnect the old connection and allow it to full connect on the next re-send
              ConnectedDeviceInfo& deviceInfo = it->second;
              const double initialConnectionTime = deviceInfo.GetInitialConnectionTime();
              const double timeConnected_s = currentTime_s - initialConnectionTime;
              const double kMinConnectedTimeBeforeNewConnect_s = 5.0;
              if (timeConnected_s > kMinConnectedTimeBeforeNewConnect_s)
              {
                PRINT_NAMED_INFO("MultiClientComms.Update.DisconnectOldConnection",
                                 "Advert for device %d connected for %.1f seconds, assume new connection attempt",
                                 advMsg.id, timeConnected_s);
                DisconnectDeviceByID(advMsg.id);
              }
            }
            else
            {
              if (DEBUG_COMMS && (advertisingDevices_.find(advMsg.id) == advertisingDevices_.end()))
              {
                PRINT_NAMED_INFO("MultiClientComms.Update.NewDevice", "Detected advertising device %d on host %s at ports ToEng=%d, FromEng=%d",
                                 advMsg.id, advMsg.ip.c_str(), (int)advMsg.toEnginePort, (int)advMsg.fromEnginePort);
              }
              
              advertisingDevices_[advMsg.id].devInfo = advMsg;
              advertisingDevices_[advMsg.id].lastSeenTime_s = currentTime_s;
            }
          }
          else
          {
            PRINT_NAMED_WARNING("MultiClientComms.Update.ErrorUnpackingAdMsg", "Unpacked %zu bytes, expected %zu", bytesUnpacked, innerMessageSize);
          }
        }
      }
    } while(bytes_recvd > 0);
    
    
    
    // Remove devices from advertising list if they're already connected.
    advertisingDevicesIt_t it = advertisingDevices_.begin();
    while(it != advertisingDevices_.end()) {
      if (currentTime_s - it->second.lastSeenTime_s > ROBOT_ADVERTISING_TIMEOUT_S) {
        #if(DEBUG_COMMS)
        PRINT_NAMED_INFO("MultiClientComms.Update.TimeoutDevice", "Removing device %d from advertising list. (Last seen: %f, curr time: %f)", it->second.devInfo.id, it->second.lastSeenTime, currTime);
        #endif
        it = advertisingDevices_.erase(it);
      } else {
        ++it;
      }
    }
    
    // Read all messages from all connected devices
    ReadAllMsgPackets();
    
    #if(DO_SIM_COMMS_LATENCY)
    // Update number of ready to receive messages
    numRecvRdyMsgs_ = 0;
    PacketQueue_t::iterator iter;
    for (iter = recvdMsgPackets_.begin(); iter != recvdMsgPackets_.end(); ++iter) {
      if (iter->first <= currentTime_s) {
        ++numRecvRdyMsgs_;
      } else {
        break;
      }
    }
    
    //printf("TIME %f: Total: %d, rel: %d\n", currTime, recvdMsgPackets_.size(), numRecvRdyMsgs_);
    
    // Send messages that are scheduled to be sent, up to the outgoing bytes limit.
    if (send_queued_msgs) {
      bytesSentThisUpdateCycle_ = 0;
      std::map<int, PacketQueue_t>::iterator sendQueueIt = sendMsgPackets_.begin();
      while (sendQueueIt != sendMsgPackets_.end()) {
        PacketQueue_t* pQueue = &(sendQueueIt->second);
        while (!pQueue->empty()) {
          if (pQueue->front().first <= currentTime_s) {
            
            if ((maxSentBytesPerTic_ > 0) && (bytesSentThisUpdateCycle_ + pQueue->front().second.dataLen > maxSentBytesPerTic_)) {
              #if(DEBUG_COMMS)
              PRINT_NAMED_INFO("MultiClientComms.MaxSendLimitReached", "%d messages left in queue to send later", (int)pQueue->size() - 1);
              #endif
              break;
            }
            bytesSentThisUpdateCycle_ += pQueue->front().second.dataLen;
            if (RealSend(pQueue->front().second) < 0) {
              // Failed (RealSend prints a warning internally)
            }
            pQueue->pop_front();
          } else {
            break;
          }
        }
        ++sendQueueIt;
      }
    }
    #endif  // #if(DO_SIM_COMMS_LATENCY)
    
    // Ping the advertisement channel in case it wasn't present at Init()
    static u8 pingTimer = 0;
    if (pingTimer++ == 10) {
      const char zero = 0;
      advertisingChannelClient_.Send(&zero,1);
      pingTimer = 0;
    }
  }
  
  
  void MultiClientComms::ReadAllMsgPackets()
  {
    static const size_t kMaxRecvBufSize = 2048;
    u8 recvBuf[kMaxRecvBufSize];
    
    // Read from all connected clients.
    // Enqueue complete messages.
    connectedDevicesIt_t it = connectedDevices_.begin();
    while ( it != connectedDevices_.end() )
    {
      ConnectedDeviceInfo& c = it->second;
      
      bool receivedAnything = false;
      double latestRecvTime = 0.0;

      while(1) // Keep reading socket until no bytes available
      {
        UdpClient* udpClient = c.GetInClient();
        
        const ssize_t bytes_recvd = udpClient->Recv((char*)recvBuf, kMaxRecvBufSize);
        
        if (bytes_recvd == 0) {
          it++;
          break;
        }
        if (bytes_recvd < 0) {
          // Disconnect client
          PRINT_NAMED_INFO("MultiClientComms.ReadAllMsgPackets", "Recv failed. Disconnecting client");

          c.DestroyClients();
          
          it = connectedDevices_.erase(it);
          break;
        }

        {
          if (bytes_recvd >= kMaxRecvBufSize) // == indicated truncation
          {
            PRINT_NAMED_ERROR("MultiClientComms.ReadTruncated", "Read %zd, buffer size only %zu", bytes_recvd, kMaxRecvBufSize);
          }
          
          receivedAnything = true;
          
          const double currentTime_s = GetCurrentTimeInSeconds();
          latestRecvTime = currentTime_s;
          double recvTime = currentTime_s;
          
          #if(DO_SIM_COMMS_LATENCY)
          recvTime += SIM_RECV_LATENCY_SEC;
          #endif
          
          recvdMsgPackets_.emplace_back(std::piecewise_construct,
                                        std::forward_as_tuple(recvTime),
                                        std::forward_as_tuple((s32)(it->first),
                                                              (s32)-1,
                                                              Util::numeric_cast<u16>(bytes_recvd),
                                                              recvBuf,
                                                              currentTime_s)
                                        );
          
        }
        
      } // end while(1) // keep reading socket until no bytes
      
      if (receivedAnything)
      {
        c.UpdateLastRecvTime(latestRecvTime);
      }
      
    } // end for (each robot)
  }
  
  
  bool MultiClientComms::ConnectToDeviceByID(int devID)
  {
    // Check if already connected
    if (connectedDevices_.find(devID) != connectedDevices_.end()) {
      return true;
    }
    
    // Check if the device is available to connect to
    advertisingDevicesIt_t it = advertisingDevices_.find(devID);
    if (it != advertisingDevices_.end())
    {
      const AdvertisementMsg& adMsg = it->second.devInfo;
      
      UdpClient* outClient = new UdpClient();
      
      if (outClient->Connect(adMsg.ip.c_str(), adMsg.fromEnginePort))
      {        
        UdpClient* inClient = nullptr;
        
        if (adMsg.fromEnginePort != adMsg.toEnginePort)
        {
          inClient = new UdpClient();
          
          if (!inClient->Connect(adMsg.ip.c_str(), adMsg.toEnginePort))
          {
            delete inClient;
            inClient = nullptr;
            
            PRINT_NAMED_WARNING("MultiClientComms.ConnectToDeviceByID.InFailed", "Connection attempt to device %d at %s:%d (ToEngine) failed",
                   adMsg.id, adMsg.ip.c_str(), adMsg.toEnginePort);
          }
        }
        else
        {
          inClient = outClient;
        }
        
        if (inClient)
        {
        #if(DEBUG_COMMS)
          PRINT_NAMED_INFO("MultiClientComms.ConnectToDeviceByID", "Connected to device %d at %s:%d/%d",
                           adMsg.id, adMsg.ip.c_str(), adMsg.toEnginePort, adMsg.fromEnginePort);
        #endif
          
          connectedDevices_[devID].ConnectToClients(inClient, outClient);
          
          // Remove from advertising list
          advertisingDevices_.erase(it);
          
          return true;
        }
      }
      
      delete outClient;
      
      PRINT_NAMED_WARNING("MultiClientComms.ConnectToDeviceByID.OutFailed", "Connection attempt to device %d at %s:%d (FromEngine) failed",
                          adMsg.id, adMsg.ip.c_str(), adMsg.fromEnginePort);
    }
    
    return false;
  }
  
  bool MultiClientComms::DisconnectDeviceByID(int devID)
  {
    connectedDevicesIt_t it = connectedDevices_.find(devID);
    if (it != connectedDevices_.end())
    {
      it->second.DestroyClients();
      
      connectedDevices_.erase(it);
      return true;
    }
    
    return false;
  }
  
  
  u32 MultiClientComms::ConnectToAllDevices()
  {
    for (advertisingDevicesIt_t it = advertisingDevices_.begin(); it != advertisingDevices_.end(); it++)
    {
      ConnectToDeviceByID(it->first);
    }
    
    return (u32)connectedDevices_.size();
  }
  
  u32 MultiClientComms::GetAdvertisingDeviceIDs(std::vector<int> &devIDs)
  {
    devIDs.clear();
    for (advertisingDevicesIt_t it = advertisingDevices_.begin(); it != advertisingDevices_.end(); it++)
    {
      devIDs.push_back(it->first);
    }
    
    return (u32)devIDs.size();
  }
  
  
  void MultiClientComms::ClearAdvertisingDevices()
  {
    advertisingDevices_.clear();
  }
  
  void MultiClientComms::DisconnectAllDevices()
  {
    for(connectedDevicesIt_t it = connectedDevices_.begin(); it != connectedDevices_.end(); )
    {
      it->second.DestroyClients();
      
      it = connectedDevices_.erase(it);
    }
    
    connectedDevices_.clear();
  }
  
  
  // Returns true if a MsgPacket was successfully gotten
  bool MultiClientComms::GetNextMsgPacket(std::vector<uint8_t>& buf)
  {
    #if(DO_SIM_COMMS_LATENCY)
    if (numRecvRdyMsgs_ > 0) {
      --numRecvRdyMsgs_;
    #else
    if (!recvdMsgPackets_.empty()) {
    #endif
      const auto& packet = recvdMsgPackets_.begin()->second;
      buf = {packet.data, packet.data+packet.dataLen};
      recvdMsgPackets_.pop_front();
      return true;
    }
    
    return false;
  }
  
  
  u32 MultiClientComms::GetNumPendingMsgPackets()
  {
    #if(DO_SIM_COMMS_LATENCY)
    return numRecvRdyMsgs_;
    #else
    return (u32)recvdMsgPackets_.size();
    #endif
  };
  
  void MultiClientComms::ClearMsgPackets()
  {
    recvdMsgPackets_.clear();
    
    #if(DO_SIM_COMMS_LATENCY)
    numRecvRdyMsgs_ = 0;
    #endif
  };
  
  u32 MultiClientComms::GetNumMsgPacketsInSendQueue(int devID)
  {
  #if(DO_SIM_COMMS_LATENCY)
    std::map<int, PacketQueue_t>::iterator it = sendMsgPackets_.find(devID);
    if (it != sendMsgPackets_.end()) {
      return static_cast<u32>(it->second.size());
    }
  #endif // #if(DO_SIM_COMMS_LATENCY)
    return 0;
  }
    
  u32 MultiClientComms::GetNumActiveConnectedDevices(double maxIdleTime_s) const
  {
    const double currentTime_s = GetCurrentTimeInSeconds();
    
    u32 numDevices = 0;
    for (auto& kv :connectedDevices_)
    {
      const ConnectedDeviceInfo& deviceInfo = kv.second;

      const double secondsSinceLastRecv = currentTime_s - deviceInfo.GetLastRecvTime();
      if (secondsSinceLastRecv < maxIdleTime_s)
      {
        ++numDevices;
      }
    }
    
    return numDevices;
  }
  
}  // namespace Vector
}  // namespace Anki


