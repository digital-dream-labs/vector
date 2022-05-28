/**
 * File: ipcBleStream.cpp
 *
 * Author: paluri
 * Created: 2/21/2018
 *
 * Description: Interface for BLE network streams on Victor.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "bleClient/ipcBleStream.h"

namespace Anki {
namespace Switchboard {

const uint8_t Anki::Switchboard::IpcBleStream::kMaxPacketSize;

void IpcBleStream::HandleSendRawPlainText(uint8_t* buffer, size_t size){
  _sendSignal.emit(buffer, (int)size, false);
}

void IpcBleStream::HandleSendRawEncrypted(uint8_t* buffer, size_t size){
  _sendSignal.emit(buffer, (int)size, true);
}

void IpcBleStream::HandleReceivePlainText(uint8_t* buffer, size_t size) {
  if(_encryptedChannelEstablished) {
    INetworkStream::ReceiveEncrypted(buffer, (int)size);
  } else {
    INetworkStream::ReceivePlainText(buffer, (int)size);
  }
}

void IpcBleStream::HandleReceiveEncrypted(uint8_t* buffer, size_t size) {
  INetworkStream::ReceiveEncrypted(buffer, (int)size);
}

int IpcBleStream::SendPlainText(uint8_t* bytes, int length) {
  if(_encryptedChannelEstablished) {
    return SendEncrypted(bytes, length);
  } else {
    _bleMessageProtocolPlainText->SendMessage(bytes, (size_t)length);
  }
  return 0;
}

int IpcBleStream::SendEncrypted(uint8_t* bytes, int length) {
  uint8_t* bytesWithExtension = (uint8_t*)malloc(length + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  
  uint64_t encryptedLength = 0;
  
  int encryptResult = Encrypt(bytes, length, bytesWithExtension, &encryptedLength);
  
  if(encryptResult != ENCRYPTION_SUCCESS) {
    free(bytesWithExtension);
    return NetworkResult::MsgFailure;
  }
  
  _bleMessageProtocolEncrypted->SendMessage(bytesWithExtension, (size_t)encryptedLength);
  
  free(bytesWithExtension);
  return 0;
}

void IpcBleStream::ReceivePlainText(uint8_t* bytes, int length) {
  if(_encryptedChannelEstablished) {
    _bleMessageProtocolEncrypted->ReceiveRawBuffer(bytes, (size_t)length);
  } else {
    _bleMessageProtocolPlainText->ReceiveRawBuffer(bytes, (size_t)length);
  }
}

void IpcBleStream::ReceiveEncrypted(uint8_t* bytes, int length) {
  _bleMessageProtocolEncrypted->ReceiveRawBuffer(bytes, (size_t)length);
}

} // Switchboard
} // Anki
