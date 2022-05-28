/**
 * File: bleMessageProtocol.h
 *
 * Author: paluri
 * Created: 2/13/2018
 *
 * Description: Multipart BLE message protocol for ankiswitchboardd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include <cstdio>
#include <stdlib.h>
#include "signals/simpleSignal.hpp"

namespace Anki {
namespace Switchboard {
  class BleMessageProtocol {
  public:
    // Constructor
    BleMessageProtocol(size_t maxSize);
    
    // Signals
    using BleMessageSignal = Signal::Signal<void (uint8_t*, size_t)>;
    
    void ReceiveRawBuffer(uint8_t* buffer, size_t size);
    void SendMessage(uint8_t* buffer, size_t size);
    
    BleMessageSignal& OnReceiveMessageEvent() {
      return _receiveMessageSignal;
    }
    
    BleMessageSignal& OnSendRawBufferEvent() {
      return _sendRawBufferSignal;
    }
    
  private:
    static const uint8_t kMultipartBits = 0b11 << 6;
    static const uint8_t kSizeBits = 0b00111111;
    static const uint8_t kEncryptedBit = 0b1 << 5;
    static const uint8_t kMsgStart = 0b10;
    static const uint8_t kMsgContinue = 0b00;
    static const uint8_t kMsgEnd = 0b01;
    static const uint8_t kMsgSolo = kMsgStart | kMsgEnd;
    
    size_t _maxSize = 0;
    uint8_t _receiveState = kMsgStart;
    std::vector<uint8_t> _buffer;
    
    void Append(uint8_t* buffer, size_t size) {
      std::copy((buffer + 1), (buffer + 1) + size - 1, std::back_inserter(_buffer));
    }
    
    void SendRawMessage(uint8_t multipart, uint8_t* buffer, size_t msgSize) {
      uint8_t msgBuffer[msgSize + 1];
      
      msgBuffer[0] = GetHeaderByte(multipart, msgSize);
      memcpy(msgBuffer + 1, buffer, msgSize);
      _sendRawBufferSignal.emit(msgBuffer, msgSize + 1);
    }
    
    static inline uint8_t GetMultipartBits(uint8_t msgSize) {
      // Unsigned, so safe to shift right
      return msgSize >> 6;
    }
    
    static inline uint8_t GetHeaderByte(uint8_t multipart, uint8_t size) {
      return (multipart << 6) | (size & ~kMultipartBits);
    }
    
    static inline uint8_t GetSize(uint8_t msgSize) {
      return msgSize & (kSizeBits);
    }
    
    BleMessageSignal _sendRawBufferSignal;
    BleMessageSignal _receiveMessageSignal;
  };
} // Switchboard
} // Anki
