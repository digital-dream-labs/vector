/**
 * File: gameComms.h
 *
 * Author: Kevin Yoon
 * Created: 12/16/2014
 *
 * Description: Interface class to allow UI to communicate with game
 *
 * Copyright: Anki, Inc. 2014
 *
 **/
#ifndef BASESTATION_COMMS_GAME_COMMS_H_
#define BASESTATION_COMMS_GAME_COMMS_H_

#include <deque>
#include "coretech/messaging/engine/IComms.h"
#include "coretech/messaging/shared/TcpServer.h"
#include "coretech/messaging/shared/UdpClient.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "clad/externalInterface/messageShared.h"
#include "engine/messaging/advertisementService.h"


namespace Anki {
namespace Vector {

  
  class GameComms : public Comms::IComms {
  public:
    
    // Default constructor
    GameComms(int deviceID, int serverListenPort, const char* advertisementRegIP_, int advertisementRegPort_);
    
    // The destructor will automatically cleans up
    virtual ~GameComms();
    
    // Returns true if we are ready to use TCP
    virtual bool IsInitialized();
    
    // Returns 0 if no messages are available.
    virtual u32 GetNumPendingMsgPackets();
  
    virtual ssize_t Send(const Comms::MsgPacket &p);

    virtual bool GetNextMsgPacket(std::vector<uint8_t> &buf);
    
    
    // when game is unpaused we need to dump old messages
    virtual void ClearMsgPackets();
    
    virtual u32 GetNumMsgPacketsInSendQueue(int devID);
    
    // Updates the list of advertising robots
    virtual void Update(bool send_queued_msgs = true);
    
    bool HasClient();
    void DisconnectClient();
    
  private:
    
    // For connection from game
    UdpServer server_;
    
    // For connecting to advertisement service
    UdpClient regClient_;
    AdvertisementRegistrationMsg regMsg_;
    void AdvertiseToService();
    
    void ReadAllMsgPackets();
    
    void PrintRecvBuf();
    
    // 'Queue' of received messages from all connected user devices with their received times.
    std::deque<Comms::MsgPacket> recvdMsgPackets_;

    bool           isInitialized_;

    // Device ID to use for registering with advertisement service
    int            deviceID_;
    
    int            serverListenPort_;
    const char*    advertisementRegIP_;
    int            advertisementRegPort_;
    
    static const size_t MAX_RECV_BUF_SIZE = 1920000; // [TODO] 1.9MB seems excessive?
    u8  _recvBuf[MAX_RECV_BUF_SIZE];
    ssize_t recvDataSize = 0;
    
  };

}  // namespace Vector
}  // namespace Anki

#endif  // #ifndef BASESTATION_COMMS_TCPCOMMS_H_

