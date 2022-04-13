/**
 * File: multiClientComms.h
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

#ifndef BASESTATION_COMMS_MULTI_CLIENT_COMMS_H_
#define BASESTATION_COMMS_MULTI_CLIENT_COMMS_H_

#include <map>
#include <vector>
#include <deque>
#include "coretech/messaging/engine/IComms.h"
#include "coretech/messaging/shared/TcpClient.h"
#include "coretech/messaging/shared/UdpClient.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "engine/messaging/advertisementService.h"

// Set to 1 to simulate a send/receive latencies
// beyond the actual latency of TCP.
// Note that the resolution of these latencies is currently equal to
// the Basestation frequency since that's what defines how often Update() is called.

#define DO_SIM_COMMS_LATENCY 0
#define SIM_RECV_LATENCY_SEC 0.0 // 0.03
#define SIM_SEND_LATENCY_SEC 0.0 // 0.03



namespace Anki {
namespace Vector {

  typedef struct {
    AdvertisementMsg devInfo;
    double lastSeenTime_s;
  } DeviceConnectionInfo_t;
  
  
  class ConnectedDeviceInfo
  {
  public:
    
    ConnectedDeviceInfo();
    ~ConnectedDeviceInfo();
    
    // Note: ConnectToClients takes ownership of the clients
    void ConnectToClients(UdpClient* inClient, UdpClient* outClient);
    
    void DestroyClients();
    
    void UpdateLastRecvTime(double newTime_s) { _lastRecvTime_s = newTime_s; }
    double GetLastRecvTime() const { return _lastRecvTime_s; }

    double GetInitialConnectionTime() const { return _initialConnectionTime_s; }
    
    UdpClient* GetInClient()  { return _inClient; }
    UdpClient* GetOutClient() { return _outClient; }
    
  private:
    
    // Note: in/out Client ptrs can be identical (if in and out are on the same port)
    UdpClient* _inClient;
    UdpClient* _outClient;
    
    double _initialConnectionTime_s;
    double _lastRecvTime_s;
  };
  
  
  
  class MultiClientComms : public Comms::IComms {
  public:
    
    MultiClientComms();
    
    // Init with the IP address to use as the advertising host
    // and the maximum number of bytes that can be sent out per call to Update().
    // If maxSentBytesPerTic == 0, then there is no limit.
    Result Init(const char* advertisingHostIP, int advertisingPort, unsigned int maxSentBytesPerTic = 0);
    
    // The destructor will automatically cleans up
    virtual ~MultiClientComms();
    
    // Returns true if we are ready to use TCP
    virtual bool IsInitialized();
    
    // Returns the number of messages ready for processing in the BLEVehicleMgr.
    // Returns 0 if no messages are available.
    virtual u32 GetNumPendingMsgPackets();
  
    virtual ssize_t Send(const Comms::MsgPacket &p);

    virtual bool GetNextMsgPacket(std::vector<uint8_t> &p);
    
    
    // when game is unpaused we need to dump old messages
    virtual void ClearMsgPackets();
    
    //virtual void SetCurrentTimestamp(BaseStationTime_t timestamp);
  
    // Return the number of MsgPackets in the send queue that are bound for devID
    virtual u32 GetNumMsgPacketsInSendQueue(int devID);
    
    // Updates the list of advertising devices
    virtual void Update(bool send_queued_msgs = true);
    
    // Connect to a device.
    // Returns true if successfully connected
    bool ConnectToDeviceByID(int devID);
    
    // Disconnect from a device
    bool DisconnectDeviceByID(int devID);
    
    // Connect to all advertising devices.
    // Returns the total number of devices that are connected.
    u32 ConnectToAllDevices();
    
    // Disconnects from all devices.
    void DisconnectAllDevices();
    
    u32 GetNumConnectedDevices() const { return (u32)connectedDevices_.size(); }
    u32 GetNumActiveConnectedDevices(double maxIdleTime_s) const;
    
    u32 GetNumAdvertisingDevices() const { return (u32)advertisingDevices_.size(); }
    
    u32 GetAdvertisingDeviceIDs(std::vector<int> &devIDs);
    
    const char* GetAdvertisingHostIP() const { return advertisingHostIP_; }
    
    // Clears the list of advertising devices.
    void ClearAdvertisingDevices();

    
  private:
    
    bool isInitialized_;
    
    const char* advertisingHostIP_;
    // Connects to "advertising" server to view available unconnected devices.
    UdpClient advertisingChannelClient_;
    
    // The number of bytes that can be sent out per call to Update(),
    // the assumption being Update() is called once per basestation tic.
    unsigned int maxSentBytesPerTic_;
    
    void ReadAllMsgPackets();
    
    // Map of advertising robots (key: dev id)
    using advertisingDevicesIt_t = std::map<int, DeviceConnectionInfo_t>::iterator;
    std::map<int, DeviceConnectionInfo_t> advertisingDevices_;
    
    // Map of connected robots (key: dev id)
    using connectedDevicesIt_t = std::map<int, ConnectedDeviceInfo>::iterator;
    std::map<int, ConnectedDeviceInfo> connectedDevices_;
    
    // 'Queue' of received messages from all connected devices with their received times.
    //std::multimap<TimeStamp_t, Comms::MsgPacket> recvdMsgPackets_;
    //std::deque<Comms::MsgPacket> recvdMsgPackets_;
    using PacketQueue_t = std::deque< std::pair<double, Comms::MsgPacket> >;
    PacketQueue_t recvdMsgPackets_;
    
#if(DO_SIM_COMMS_LATENCY)
    // The number of messages that have been in recvdMsgPackets for at least
    // SIM_RECV_LATENCY_SEC and are now available for reading.
    u32 numRecvRdyMsgs_;
    
    // Queue of messages to be sent with the times they should be sent at
    // (key: dev id)
    std::map<int, PacketQueue_t> sendMsgPackets_;

    // The actual function that does the sending when we're simulating latency
    ssize_t RealSend(const Comms::MsgPacket &p);
    
    // Outgoing bytes sent since last call to Update()
    int bytesSentThisUpdateCycle_;
#endif
    
  };

}  // namespace Vector
}  // namespace Anki

#endif  // #ifndef BASESTATION_COMMS_MULTI_CLIENT_COMMS_H_

