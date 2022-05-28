/**
 * File: bleMessageProtocol.cpp
 *
 * Author: paluri
 * Created: 2/13/2018
 *
 * Description: Multipart BLE message protocol for ankiswitchboardd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "switchboardd/bleMessageProtocol.h"

namespace Anki {
namespace Switchboard {

BleMessageProtocol::BleMessageProtocol(size_t size)
: _maxSize(size),
  _receiveState(kMsgStart)
{
  if(_maxSize < 1) {
    // todo: handle
  }
}

void BleMessageProtocol::ReceiveRawBuffer(uint8_t* buffer, size_t size) {
  if(size < 1) {
    return;
  }
  
  uint8_t headerByte = buffer[0];
  
  uint8_t sizeByte = BleMessageProtocol::GetSize(headerByte);
  uint8_t mulipartState = BleMessageProtocol::GetMultipartBits(headerByte);
  
  if(sizeByte != size - 1) {
    printf("Size failure: %d %d\n", sizeByte, size);
    return;
  }
  
  switch(mulipartState) {
    case kMsgStart: {
      if(_receiveState != kMsgStart) {
        // State error
        // todo: handle
      }
      
      // Clear vector
      _buffer.clear();
      
      // Place in vector
      Append(buffer, size);
      
      _receiveState = kMsgContinue;
      break;
    }
    case kMsgContinue: {
      if(_receiveState != kMsgContinue) {
        // State error
        // todo: handle
      }
      
      // Place in vector
      Append(buffer, size);
      
      _receiveState = kMsgContinue;
      break;
    }
    case kMsgEnd: {
      if(_receiveState != kMsgContinue) {
        // State error
        // todo: handle
      }
      
      // Place in vector
      Append(buffer, size);
      
      // Notify
      _receiveMessageSignal.emit(reinterpret_cast<uint8_t*>(_buffer.data()), _buffer.size());
      
      _receiveState = kMsgStart;
      break;
    }
    default:
    case kMsgSolo: {
      if(_receiveState != kMsgStart) {
        // State error
        // todo: handle
      }
      
      // Notify
      _receiveMessageSignal.emit(buffer + 1, size - 1);
      
      _receiveState = kMsgStart;
      break;
    }
  }
}

void BleMessageProtocol::SendMessage(uint8_t* buffer, size_t size) {
  size_t sizeRemaining = size;
  
  if(size < _maxSize) {
    // Solo message
    SendRawMessage(kMsgSolo, buffer, size);
  } else {
    while(sizeRemaining > 0) {
      size_t offset = size - sizeRemaining;
      
      if(sizeRemaining == size) {
        // First message
        uint8_t msgSize = _maxSize - 1;
        SendRawMessage(kMsgStart, buffer + offset, msgSize);
        sizeRemaining -= msgSize;
      } else if(sizeRemaining < _maxSize) {
        // Final message
        SendRawMessage(kMsgEnd, buffer + offset, sizeRemaining);
        sizeRemaining = 0;
      } else {
        // Middle message
        uint8_t msgSize = _maxSize - 1;
        SendRawMessage(kMsgContinue, buffer + offset, msgSize);
        sizeRemaining -= msgSize;
      }
    }
  }
}

} // Switchboard
} // Anki