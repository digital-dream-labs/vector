/**
 * File: localUdpSocketComms
 *
 * Author: Paul Aluri
 * Created: 04/09/18
 *
 * Description: ISocketComms Wrapper for local domain socket server
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include "engine/cozmoAPI/comms/iSocketComms.h"
#include <memory>

class LocalUdpServer;

namespace Anki {
namespace Vector {

class LocalUdpSocketComms : public ISocketComms
{
public:

  explicit LocalUdpSocketComms(bool isEnabled, std::string socket);
  virtual ~LocalUdpSocketComms();

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

  virtual void OnEnableConnection(bool wasEnabled, bool isEnabled) override;

  bool IsConnected() const;

  // ============================== Private Member Vars ==============================

  const uint32_t        MAX_PACKET_BUFFER_SIZE = 2048;
  const uint16_t        kMessageHeaderSize = 2;
  std::unique_ptr<LocalUdpServer>       _udpServer;
  DeviceId              _connectedId;
  bool                  _hasClient;
  std::string           _socket;
};

} // Victor
} // Anki
