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

#include "engine/cozmoAPI/comms/localUdpSocketComms.h"
#include "coretech/messaging/shared/LocalUdpServer.h"
#include "engine/utils/parsingConstants/parsingConstants.h"
#include "coretech/messaging/engine/IComms.h"
#include "json/json.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {

LocalUdpSocketComms::LocalUdpSocketComms(bool isEnabled, std::string socket)
  : ISocketComms(isEnabled)
  , _udpServer( new LocalUdpServer() )
  , _connectedId( kDeviceIdInvalid )
  , _hasClient(false)
  , _socket(socket) {
  SetPingTimeoutForDisconnect(0, nullptr);
}

LocalUdpSocketComms::~LocalUdpSocketComms()
{}

bool LocalUdpSocketComms::Init(UiConnectionType connectionType, const Json::Value& config) {
  if(_udpServer->HasClient()) {
    _udpServer->Disconnect();
  }

  _udpServer->StopListening();
  _udpServer->StartListening(_socket);

  return true;
}

void LocalUdpSocketComms::OnEnableConnection(bool wasEnabled, bool isEnabled) {
  if (isEnabled) {
    _udpServer->StartListening(_socket);
  }
  else {
    _udpServer->Disconnect();
    _udpServer->StopListening();
  }
}

void LocalUdpSocketComms::UpdateInternal() {
  ANKI_CPU_PROFILE("LocalUdpSocketComms::Update");

  // See if we lost the client since last update
  if (_hasClient && !_udpServer->HasClient()) {
    PRINT_CH_INFO("UiComms", "LocalUdpSocketComms.Update.ClientLost", "Client Connection to Device %d lost", _connectedId);
    _udpServer->Disconnect();
    _hasClient = false;
  }
}

bool LocalUdpSocketComms::ConnectToDeviceByID(DeviceId deviceId) {
  assert(deviceId != kDeviceIdInvalid);

  if (_connectedId == kDeviceIdInvalid) {
    _connectedId = deviceId;
    return true;
  }
  else {
    PRINT_NAMED_WARNING("LocalUdpSocketComms.ConnectToDeviceByID.Failed",
                        "Cannot connect to device %d, already connected to %d", deviceId, _connectedId);
    return false;
  }
}

bool LocalUdpSocketComms::DisconnectDeviceByID(DeviceId deviceId) {
  assert(deviceId != kDeviceIdInvalid);

  if ((_connectedId != kDeviceIdInvalid) && (_connectedId == deviceId)) {
    _udpServer->Disconnect();
    return true;
  }
  else {
    return false;
  }
}

bool LocalUdpSocketComms::DisconnectAllDevices() {
  return DisconnectDeviceByID(_connectedId);
}

void LocalUdpSocketComms::GetAdvertisingDeviceIDs(std::vector<ISocketComms::DeviceId>& outDeviceIds) {
  if (_udpServer->HasClient()) {
    if (!IsConnected()) {
      // Advertising doesn't really make sense for domain socket, just pretend we have Id 1 whenever a client connection is made
      outDeviceIds.push_back(1);
    }
  }
}

uint32_t LocalUdpSocketComms::GetNumConnectedDevices() const {
  return IsConnected()? 1 : 0;
}

bool LocalUdpSocketComms::SendMessageInternal(const Comms::MsgPacket& msgPacket) {
  ANKI_CPU_PROFILE("LocalUdpSocketComms::SendMessage");

  if (IsConnected()) {
    const ssize_t res = _udpServer->Send((const char*)&msgPacket.dataLen, sizeof(msgPacket.dataLen) + msgPacket.dataLen);
    if (res < 0) {
      LOG_WARNING("LocalUdpSocketComms.SendMessageInternal.FailedSend", "Failed to send message from %d to %d",
        msgPacket.sourceId, msgPacket.destId);
      _udpServer->Disconnect();
      return false;
    }
    return true;
  }
  return false;
}

bool LocalUdpSocketComms::RecvMessageInternal(std::vector<uint8_t>& outBuffer) {
  // Reserve memory
  outBuffer.clear();
  outBuffer.reserve(MAX_PACKET_BUFFER_SIZE);

  // Read available datagram
  const ssize_t dataLen = _udpServer->Recv((char*)outBuffer.data(), MAX_PACKET_BUFFER_SIZE);

  if (dataLen == 0) {
    // No data to receive
    return false;
  }

  if (dataLen < 0) {
    // Something went wrong
    PRINT_NAMED_WARNING("LocalUdpSocketComms", "Shutting down server. Received dataLen < 0");
    _udpServer->Disconnect();
    _udpServer->StopListening();
    return false;
  }

  // Remove header from data
  uint16_t msgSize;
  memcpy(&msgSize, outBuffer.data(), kMessageHeaderSize);

  const uint8_t* sourceBuffer = outBuffer.data() + sizeof(kMessageHeaderSize);
  outBuffer = { sourceBuffer, sourceBuffer + msgSize };

  return true;
}

bool LocalUdpSocketComms::IsConnected() const {
  if ((kDeviceIdInvalid != _connectedId) && _udpServer->HasClient()) {
    return true;
  }

  return false;
}

} // Cozmo
} // Anki
