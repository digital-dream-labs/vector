/**
 * File: externalCommsCladHandlerV3.h
 *
 * Author: Paul Aluri (@paluri)
 * Created: 3/15/2018
 *
 * Description: File for handling clad messages from
 * external devices and processing them into listenable
 * events.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include "signals/simpleSignal.hpp"
#include "clad/externalInterface/messageExternalComms.h"

namespace Anki {
namespace Switchboard {
  class ExternalCommsCladHandlerV3 {
    public:
    using RtsConnectionSignal = Signal::Signal<void (const Anki::Vector::ExternalComms::RtsConnection_3& msg)>;
    
    RtsConnectionSignal& OnReceiveRtsConnResponse() {
      return _receiveRtsConnResponse;
    }

    RtsConnectionSignal& OnReceiveRtsChallengeMessage() {
      return _receiveRtsChallengeMessage;
    }

    RtsConnectionSignal& OnReceiveRtsWifiConnectRequest() {
      return _receiveRtsWifiConnectRequest;
    }

    RtsConnectionSignal& OnReceiveRtsWifiIpRequest() {
      return _receiveRtsWifiIpRequest;
    }

    RtsConnectionSignal& OnReceiveRtsStatusRequest() {
      return _receiveRtsStatusRequest;
    }

    RtsConnectionSignal& OnReceiveRtsWifiScanRequest() {
      return _receiveRtsWifiScanRequest;
    }

    RtsConnectionSignal& OnReceiveRtsWifiForgetRequest() {
      return _receiveRtsWifiForgetRequest;
    }

    RtsConnectionSignal& OnReceiveRtsOtaUpdateRequest() {
      return _receiveRtsOtaUpdateRequest;
    }

    RtsConnectionSignal& OnReceiveRtsWifiAccessPointRequest() {
      return _receiveRtsWifiAccessPointRequest;
    }
    
    RtsConnectionSignal& OnReceiveCancelPairingRequest() {
      return _receiveRtsCancelPairing;
    }

    RtsConnectionSignal& OnReceiveRtsAck() {
      return _receiveRtsAck;
    }

    RtsConnectionSignal& OnReceiveRtsLogRequest() {
      return _receiveRtsLogRequest;
    }

    RtsConnectionSignal& OnReceiveRtsForceDisconnect() {
      return _receiveRtsForceDisconnect;
    }

    RtsConnectionSignal& OnReceiveRtsCloudSessionRequest() {
      return _receiveRtsCloudSessionRequest;
    }

    // RtsSsh
    RtsConnectionSignal& OnReceiveRtsSsh() {
      return _DEV_ReceiveSshKey;
    }

    // RtsSsh
    RtsConnectionSignal& OnReceiveRtsOtaCancelRequest() {
      return _receiveRtsOtaCancelRequest;
    }

    Anki::Vector::ExternalComms::ExternalComms ReceiveExternalCommsMsg(uint8_t* buffer, size_t length) {
      Anki::Vector::ExternalComms::ExternalComms extComms;

      if(length == 5 && buffer[0] == 1) {
        // for now, just ignore handshake
        return extComms;
      }

      const size_t unpackSize = extComms.Unpack(buffer, length);
      if(unpackSize != length) {
        // bugs
        Log::Write("externalCommsCladHandler - Somehow our bytes didn't unpack to the proper size.");
      }
      
      if(extComms.GetTag() == Anki::Vector::ExternalComms::ExternalCommsTag::RtsConnection) {
        Anki::Vector::ExternalComms::RtsConnection rstContainer = extComms.Get_RtsConnection();
        Anki::Vector::ExternalComms::RtsConnection_3 rtsMsg = rstContainer.Get_RtsConnection_3();
        
        switch(rtsMsg.GetTag()) {
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::Error:
            //
            break;
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsConnResponse: {
            _receiveRtsConnResponse.emit(rtsMsg);          
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsChallengeMessage: {
            _receiveRtsChallengeMessage.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsWifiConnectRequest: {
            _receiveRtsWifiConnectRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsWifiIpRequest: {
            _receiveRtsWifiIpRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsStatusRequest: {
            _receiveRtsStatusRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsWifiScanRequest: {
            _receiveRtsWifiScanRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsWifiForgetRequest: {
            _receiveRtsWifiForgetRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsOtaUpdateRequest: {
            _receiveRtsOtaUpdateRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsOtaCancelRequest: {
            _receiveRtsOtaCancelRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsWifiAccessPointRequest: {
            _receiveRtsWifiAccessPointRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsCancelPairing: {
            _receiveRtsCancelPairing.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsAck: {
            _receiveRtsAck.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsLogRequest: {
            _receiveRtsLogRequest.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsForceDisconnect: {
            _receiveRtsForceDisconnect.emit(rtsMsg);
            break;
          }
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsCloudSessionRequest: {
            _receiveRtsCloudSessionRequest.emit(rtsMsg);
            break;
          }
          // RtsSsh
          case Anki::Vector::ExternalComms::RtsConnection_3Tag::RtsSshRequest: {
            // only handle ssh message in debug build
            _DEV_ReceiveSshKey.emit(rtsMsg);
            break;
          }
          default:
            // unhandled messages
            break;
        }
      }

      return extComms;
    }

    static std::vector<uint8_t> SendExternalCommsMsg(Anki::Vector::ExternalComms::ExternalComms msg) {
      std::vector<uint8_t> messageData(msg.Size());

      const size_t packedSize = msg.Pack(messageData.data(), msg.Size());

      if(packedSize != msg.Size()) {
        // bugs
        Log::Write("externalCommsCladHandler - Somehow our bytes didn't pack to the proper size.");
      }

      return messageData;
    }

    private:
      RtsConnectionSignal _receiveRtsConnResponse;
      RtsConnectionSignal _receiveRtsChallengeMessage;
      RtsConnectionSignal _receiveRtsWifiConnectRequest;
      RtsConnectionSignal _receiveRtsWifiIpRequest;
      RtsConnectionSignal _receiveRtsStatusRequest;
      RtsConnectionSignal _receiveRtsWifiScanRequest;
      RtsConnectionSignal _receiveRtsWifiForgetRequest;
      RtsConnectionSignal _receiveRtsOtaUpdateRequest;
      RtsConnectionSignal _receiveRtsWifiAccessPointRequest;
      RtsConnectionSignal _receiveRtsCancelPairing;
      RtsConnectionSignal _receiveRtsAck;
      RtsConnectionSignal _receiveRtsOtaCancelRequest;
      RtsConnectionSignal _receiveRtsLogRequest;
      RtsConnectionSignal _receiveRtsForceDisconnect;
      RtsConnectionSignal _receiveRtsCloudSessionRequest;

      // RtsSsh 
      RtsConnectionSignal _DEV_ReceiveSshKey;
  };
} // Switchboard
} // Anki
