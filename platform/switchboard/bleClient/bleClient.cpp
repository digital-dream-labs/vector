/**
 * File: bleClient.cpp
 *
 * Author: paluri
 * Created: 2/20/2018
 *
 * Description: ble Client for ankibluetoothd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "anki-ble/common/anki_ble_uuids.h"
#include "bleClient/bleClient.h"

namespace Anki {
namespace Switchboard {

bool BleClient::SendPlainText(uint8_t* msg, size_t length) {
  return Send(msg, length, kAppReadCharacteristicUUID.c_str());
}

bool BleClient::SendEncrypted(uint8_t* msg, size_t length) {
  return Send(msg, length, kAppReadCharacteristicUUID.c_str());
}

bool BleClient::Send(uint8_t* msg, size_t length, std::string charUuid) {
  if(_connectionId == -1) {
    return false;
  }

  std::vector<uint8_t> msgVector(msg, msg+length);

  SendMessage(_connectionId,
              charUuid,
              true,
              msgVector);

  return true;
}

void BleClient::OnReceiveMessage(const int connection_id,
                                 const std::string& characteristic_uuid,
                                 const std::vector<uint8_t>& value) {
  if(characteristic_uuid == kAppWriteCharacteristicUUID) {
    // We are receiving input to read stream
    ((Anki::Switchboard::INetworkStream*)_stream)->ReceivePlainText((uint8_t*)&value[0], value.size());
  } else if(characteristic_uuid == kAppWriteCharacteristicUUID) {
    // We are receiving input to read stream
    ((Anki::Switchboard::INetworkStream*)_stream)->ReceiveEncrypted((uint8_t*)&value[0], value.size());
  }
}

void BleClient::OnInboundConnectionChange(int connection_id, int connected) {
  bool isConnectedToCentral = (bool)connected && (connection_id != -1);

  printf("connection_id [%d] connected [%d]\n", connection_id, connected);

  if (isConnectedToCentral) {
    _connectionId = connection_id;

    if(nullptr == _stream) {
      _stream = new IpcBleStream();
      _stream->OnSendEvent().SubscribeForever([this](uint8_t* bytes, int length, bool encrypted) {
        if(encrypted) {
          this->SendEncrypted(bytes, length);
        } else {
          this->SendPlainText(bytes, length);
        }
      });
    }

    _connectedSignal.emit(_connectionId, _stream);
  } else {
    _disconnectedSignal.emit(_connectionId, _stream);
    _connectionId = -1;
  }
}

void BleClient::OnPeripheralStateUpdate(const bool advertising,
                                        const int connection_id,
                                        const int connected,
                                        const bool congested) {
  (void)congested; // unused
  OnInboundConnectionChange(connection_id, connected);

  _advertisingUpdateSignal.emit(advertising);
}

void BleClient::OnPeerClose(const int sockfd) {
  IPCEndpoint::OnPeerClose(sockfd);
  _ipcDisconnectedSignal.emit();
}


} // Switchboard
} // Anki
