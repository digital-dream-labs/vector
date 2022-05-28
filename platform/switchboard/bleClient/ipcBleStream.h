/**
 * File: ipcBleStream.h
 *
 * Author: paluri
 * Created: 2/21/2018
 *
 * Description: Interface for BLE network streams on Victor.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef ipcBleStream_h
#define ipcBleStream_h

#include "switchboardd/INetworkStream.h"
#include "switchboardd/bleMessageProtocol.h"

namespace Anki {
namespace Switchboard {
  class IpcBleStream : public INetworkStream {
  public:
    IpcBleStream() {
      _bleMessageProtocolEncrypted = std::make_unique<BleMessageProtocol>(kMaxPacketSize);
      _bleMessageProtocolPlainText = std::make_unique<BleMessageProtocol>(kMaxPacketSize);
      
      _onSendRawHandle = _bleMessageProtocolEncrypted->OnSendRawBufferEvent()
        .ScopedSubscribe(std::bind(&IpcBleStream::HandleSendRawEncrypted, this, std::placeholders::_1, std::placeholders::_2));
      _onReceiveHandle = _bleMessageProtocolEncrypted->OnReceiveMessageEvent()
        .ScopedSubscribe(std::bind(&IpcBleStream::HandleReceiveEncrypted, this, std::placeholders::_1, std::placeholders::_2));

      _onSendRawPlainHandle = _bleMessageProtocolPlainText->OnSendRawBufferEvent()
        .ScopedSubscribe(std::bind(&IpcBleStream::HandleSendRawPlainText, this, std::placeholders::_1, std::placeholders::_2));
      _onReceivePlainHandle = _bleMessageProtocolPlainText->OnReceiveMessageEvent()
        .ScopedSubscribe(std::bind(&IpcBleStream::HandleReceivePlainText, this, std::placeholders::_1, std::placeholders::_2));
    }

    using SendSignal = Signal::Signal<void (uint8_t*, int, bool)>;
    
    SendSignal& OnSendEvent() {
      return _sendSignal;
    }
    
    int SendPlainText(uint8_t* bytes, int length) __attribute__((warn_unused_result));
    int SendEncrypted(uint8_t* bytes, int length) __attribute__((warn_unused_result));
    
    void ReceivePlainText(uint8_t* bytes, int length);
    void ReceiveEncrypted(uint8_t* bytes, int length);
  
  private:
    static const uint8_t kMaxPacketSize = 20;

    void HandleSendRawPlainText(uint8_t* buffer, size_t size);
    void HandleSendRawEncrypted(uint8_t* buffer, size_t size);
    void HandleReceivePlainText(uint8_t* buffer, size_t size);
    void HandleReceiveEncrypted(uint8_t* buffer, size_t size);
    
    std::unique_ptr<BleMessageProtocol> _bleMessageProtocolEncrypted;
    std::unique_ptr<BleMessageProtocol> _bleMessageProtocolPlainText;
    
    Signal::SmartHandle _onSendRawHandle;
    Signal::SmartHandle _onReceiveHandle;
    Signal::SmartHandle _onSendRawPlainHandle;
    Signal::SmartHandle _onReceivePlainHandle;
    
    SendSignal _sendSignal;
  };
} // Switchboard
} // Anki

#endif // ipcBleStream_h