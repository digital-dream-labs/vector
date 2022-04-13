/**
 * File: UdpSocketComms
 *
 * Author: Mark Wesley
 * Created: 05/14/16
 *
 * Description: UDP implementation for socket-based communications. Used by webots for Vector.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Cozmo_Game_Comms_UdpSocketComms_H__
#define __Cozmo_Game_Comms_UdpSocketComms_H__


#include "engine/cozmoAPI/comms/iSocketComms.h"


// Forward declarations
namespace Anki {
namespace Comms{
    class AdvertisementService;
}}

  
namespace Anki {
namespace Vector {
  
  
class MultiClientComms;


class UdpSocketComms : public ISocketComms
{
public:

  UdpSocketComms(UiConnectionType connectionType);
  virtual ~UdpSocketComms();

  virtual bool Init(UiConnectionType connectionType, const Json::Value& config) override;

  virtual bool AreMessagesGrouped() const override { return false; }

  virtual bool ConnectToDeviceByID(DeviceId deviceId) override;
  virtual bool DisconnectDeviceByID(DeviceId deviceId) override;
  virtual bool DisconnectAllDevices() override;

  virtual void GetAdvertisingDeviceIDs(std::vector<ISocketComms::DeviceId>& outDeviceIds) override;
  
  virtual uint32_t GetNumConnectedDevices() const override;
  
protected:
  virtual void UpdateInternal() override;

private:
  
  // ============================== Private Member Functions ==============================
  
  virtual bool SendMessageInternal(const Comms::MsgPacket& msgPacket) override;
  virtual bool RecvMessageInternal(std::vector<uint8_t>& outBuffer) override;

  void  StartAdvertising(UiConnectionType type);
  
  // ============================== Private Member Vars ==============================

  MultiClientComms*                  _comms;
  Comms::AdvertisementService*       _advertisementService;
};



} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Game_Comms_UdpSocketComms_H__

