/**
 * File: rtsComms.cpp
 *
 * Author: paluri
 * Created: 6/27/2018
 *
 * Description: Class to facilitate versioned comms with client over BLE
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "switchboardd/rtsHandlerV2.h"
#include "switchboardd/rtsHandlerV3.h"
#include "switchboardd/rtsHandlerV4.h"
#include "switchboardd/rtsHandlerV5.h"
#include "switchboardd/rtsComms.h"

#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"

namespace Anki {
namespace Switchboard {

RtsComms::RtsComms(INetworkStream* stream, 
    struct ev_loop* evloop,
    std::shared_ptr<ISwitchboardCommandClient> engineClient,
    std::shared_ptr<GatewayMessagingServer> gatewayServer,
    std::shared_ptr<TokenClient> tokenClient,
    std::shared_ptr<ConnectionIdManager> connectionIdManager,
    std::shared_ptr<WifiWatcher> wifiWatcher,
    std::shared_ptr<TaskExecutor> taskExecutor,
    bool isPairing,
    bool isOtaUpdating,
    bool hasCloudOwner) :
_pin(""),
_stream(stream),
_loop(evloop),
_engineClient(engineClient),
_gatewayServer(gatewayServer),
_tokenClient(tokenClient),
_connectionIdManager(connectionIdManager),
_wifiWatcher(wifiWatcher),
_taskExecutor(taskExecutor),
_isPairing(isPairing),
_isOtaUpdating(isOtaUpdating),
_hasCloudOwner(hasCloudOwner),
_totalPairingAttempts(0),
_rtsHandler(nullptr),
_rtsVersion(0)
{
  // Initialize safe handle
  _safeHandle = SafeHandle::Create();

  // pairing timeout
  _onPairingTimeoutReceived = _pairingTimeoutSignal.ScopedSubscribe(std::bind(&RtsComms::HandleTimeout, this));
  _handleTimeoutTimer.signal = &_pairingTimeoutSignal;
  ev_timer_init(&_handleTimeoutTimer.timer, &RtsComms::sEvTimerHandler, kPairingTimeout_s, kPairingTimeout_s);
}

RtsComms::~RtsComms() {
  if(_rtsHandler != nullptr) {
    delete _rtsHandler;
    _rtsHandler = nullptr;
  }

  ev_timer_stop(_loop, &_handleTimeoutTimer.timer);

  Log::Write("Destroying RTS Comms");
}

void RtsComms::BeginPairing() {  
  // Clear field values
  _totalPairingAttempts = 0;

  Init();
}

void RtsComms::Init() {  
  // Update our state
  _state = RtsPairingPhase::Initial;

  if(_rtsHandler != nullptr) {
    delete _rtsHandler;
    _rtsHandler = nullptr;

    ev_timer_stop(_loop, &_handleTimeoutTimer.timer);
  }

  // Register with stream events
  _onReceivePlainTextHandle = _stream->OnReceivedPlainTextEvent().ScopedSubscribe(
    std::bind(&RtsComms::HandleMessageReceived,
    this, std::placeholders::_1, std::placeholders::_2));

  // Send Handshake
  ev_timer_again(_loop, &_handleTimeoutTimer.timer);
  Log::Write("Sending Handshake to Client.");
  SendHandshake();
  _state = RtsPairingPhase::AwaitingHandshake;
}

void RtsComms::StopPairing() {
  if(_rtsHandler) {
    _rtsHandler->StopPairing();
  }
}

void RtsComms::ForceDisconnect() {
  if(_rtsHandler) {
    _rtsHandler->ForceDisconnect();
  }
}

void RtsComms::SetIsPairing(bool pairing) { 
  _isPairing = pairing;

  if(_rtsHandler) {
    _rtsHandler->SetIsPairing(_isPairing);
  }
}

void RtsComms::SetOtaUpdating(bool updating) { 
  _isOtaUpdating = updating; 

  if(_rtsHandler) {
    _rtsHandler->SetOtaUpdating(_isOtaUpdating);
  }
}

void RtsComms::SetHasOwner(bool hasOwner) { 
  _hasCloudOwner = hasOwner; 

  if(_rtsHandler) {
    _rtsHandler->SetHasOwner(_hasCloudOwner);
  }
}

void RtsComms::SendHandshake() {
  if(_state != RtsPairingPhase::Initial) {
    return;
  }
  // Send versioning handshake
  // ************************************************************
  // Handshake Message (first message)
  // This message is fixed. Cannot change. Ever.
  // If you are thinking about changing the code in this message,
  // DON'T. All Victor's for all time must send this message.
  // ANY victor needs to be able to communicate with
  // ANY version of the client, at least enough to
  // know if they can speak the same language.
  // ************************************************************
  const uint8_t kHandshakeMessageLength = 5;
  uint8_t handshakeMessage[kHandshakeMessageLength];
  handshakeMessage[0] = SetupMessage::MSG_HANDSHAKE;
  *(uint32_t*)(&handshakeMessage[1]) = PairingProtocolVersion::CURRENT;
  int result = _stream->SendPlainText(handshakeMessage, sizeof(handshakeMessage));

  if(result != 0) {
    Log::Write("Unable to send message.");
  }
}

void RtsComms::SendOtaProgress(int status, uint64_t progress, uint64_t expectedTotal) {
  // Send Ota Progress
  if(_rtsHandler) {
    _rtsHandler->SendOtaProgress(status, progress, expectedTotal);
  }
}

void RtsComms::UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus state) {
  if(_engineClient == nullptr) {
    // no engine client -- probably testing
    return;
  }

  if((state == Anki::Vector::SwitchboardInterface::ConnectionStatus::UPDATING_OS) ||
    (state == Anki::Vector::SwitchboardInterface::ConnectionStatus::SETTING_WIFI)) {
    return;
  }

  _engineClient->ShowPairingStatus(state);
}

void RtsComms::HandleReset(bool forced) {
  //
  // 'Wake' is necessary to prevent RtsComms from being destroyed in its own scope
  // the SafeHandle weak_ptr allows us to know if RtsComms was destroyed before 
  // the callback is executed.
  //
  std::weak_ptr<SafeHandle> weakSafeHandle(_safeHandle);
  _taskExecutor->Wake([this, weakSafeHandle, forced]() {    
    auto handle = weakSafeHandle.lock();
    if(!handle) {
      return;
    }

    _state = RtsPairingPhase::Initial;

    // Put us back in initial state
    if(forced) {
      Log::Write("Client disconnected. Stopping pairing.");
      ev_timer_stop(_loop, &_handleTimeoutTimer.timer);
      UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
    } else if(++_totalPairingAttempts < kMaxPairingAttempts) {
      Init();
      Log::Write("SecurePairing restarting.");
      if(_isPairing) {
        UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::SHOW_PRE_PIN);
      } else {
        UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
      }
    } else {
      Log::Write("SecurePairing ending due to multiple failures. Requires external restart.");
      ev_timer_stop(_loop, &_handleTimeoutTimer.timer);
      _stopPairingSignal.emit();
      UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
    }
  });
}

void RtsComms::HandleTimeout() {
  if(_rtsHandler) {
    _rtsHandler->HandleTimeout();
  } else {
    // if we aren't beyond handshake, mark as strike
    HandleReset(false);
  }
}

void RtsComms::HandleMessageReceived(uint8_t* bytes, uint32_t length) {
  _taskExecutor->WakeSync([this, bytes, length]() {
    if(length < kMinMessageSize) {
      Log::Write("Length is less than kMinMessageSize.");
      return;
    }

    if(_state == RtsPairingPhase::AwaitingHandshake) {
      // ************************************************************
      // Handshake Message (first message)
      // This message is fixed. Cannot change. Ever.
      // ANY victor needs to be able to communicate with
      // ANY version of the client, at least enough to
      // know if they can speak the same language.
      // ************************************************************
      if((SetupMessage)bytes[0] == SetupMessage::MSG_HANDSHAKE) {
        bool handleHandshake = false;

        if(length < sizeof(uint32_t) + 1) {
          Log::Write("Handshake message too short.");
        } else {
          uint32_t clientVersion = *(uint32_t*)(bytes + 1);
          handleHandshake = HandleHandshake(clientVersion);

          Log::Write("Searching for compatible comms version...");
          _rtsVersion = clientVersion;
        }

        if(handleHandshake) {
          Log::Write("Starting RtsHandler");
          switch(_rtsVersion) {
            case PairingProtocolVersion::CURRENT: {
              _rtsHandler = (IRtsHandler*)new RtsHandlerV5(_stream, 
                              _loop,
                              _engineClient,
                              _tokenClient,
                              _gatewayServer,
                              _connectionIdManager,
                              _taskExecutor,
                              _wifiWatcher,
                              _isPairing,
                              _isOtaUpdating,
                              _hasCloudOwner);

              RtsHandlerV5* _v5 = static_cast<RtsHandlerV5*>(_rtsHandler);
              _pinHandle = _v5->OnUpdatedPinEvent().ScopedSubscribe([this](std::string s){
                _pin = s;
                this->OnUpdatedPinEvent().emit(s);
              });
              _otaHandle = _v5->OnOtaUpdateRequestEvent().ScopedSubscribe([this](std::string s){
                this->OnOtaUpdateRequestEvent().emit(s);
              });
              _endHandle = _v5->OnStopPairingEvent().ScopedSubscribe([this](){
                this->OnStopPairingEvent().emit();
              });
              _completedPairingHandle = _v5->OnCompletedPairingEvent().ScopedSubscribe([this](){
                this->OnCompletedPairingEvent().emit();
              });
              _resetHandle = _v5->OnResetEvent().ScopedSubscribe(
                std::bind(&RtsComms::HandleReset, this, std::placeholders::_1));

              break;
            }
            case PairingProtocolVersion::V4: {
              _rtsHandler = (IRtsHandler*)new RtsHandlerV4(_stream, 
                              _loop,
                              _engineClient,
                              _tokenClient,
                              _gatewayServer,
                              _connectionIdManager,
                              _taskExecutor,
                              _wifiWatcher,
                              _isPairing,
                              _isOtaUpdating,
                              _hasCloudOwner);

              RtsHandlerV4* _v4 = static_cast<RtsHandlerV4*>(_rtsHandler);
              _pinHandle = _v4->OnUpdatedPinEvent().ScopedSubscribe([this](std::string s){
                _pin = s;
                this->OnUpdatedPinEvent().emit(s);
              });
              _otaHandle = _v4->OnOtaUpdateRequestEvent().ScopedSubscribe([this](std::string s){
                this->OnOtaUpdateRequestEvent().emit(s);
              });
              _endHandle = _v4->OnStopPairingEvent().ScopedSubscribe([this](){
                this->OnStopPairingEvent().emit();
              });
              _completedPairingHandle = _v4->OnCompletedPairingEvent().ScopedSubscribe([this](){
                this->OnCompletedPairingEvent().emit();
              });
              _resetHandle = _v4->OnResetEvent().ScopedSubscribe(
                std::bind(&RtsComms::HandleReset, this, std::placeholders::_1));

              break;
            }
            case PairingProtocolVersion::FACTORY: {
              _rtsHandler = (IRtsHandler*)new RtsHandlerV2(_stream, 
                              _loop,
                              _engineClient,
                              _tokenClient,
                              _taskExecutor,
                              _wifiWatcher,
                              _isPairing,
                              _isOtaUpdating,
                              _hasCloudOwner);

              RtsHandlerV2* _v2 = static_cast<RtsHandlerV2*>(_rtsHandler);
              _pinHandle = _v2->OnUpdatedPinEvent().ScopedSubscribe([this](std::string s){
                _pin = s;
                this->OnUpdatedPinEvent().emit(s);
              });
              _otaHandle = _v2->OnOtaUpdateRequestEvent().ScopedSubscribe([this](std::string s){
                this->OnOtaUpdateRequestEvent().emit(s);
              });
              _endHandle = _v2->OnStopPairingEvent().ScopedSubscribe([this](){
                this->OnStopPairingEvent().emit();
              });
              _completedPairingHandle = _v2->OnCompletedPairingEvent().ScopedSubscribe([this](){
                this->OnCompletedPairingEvent().emit();
              });
              _resetHandle = _v2->OnResetEvent().ScopedSubscribe(
                std::bind(&RtsComms::HandleReset, this, std::placeholders::_1));
              break;
            }
            case PairingProtocolVersion::V3:
            default: {
              // this case should never happen,
              // because handleHandshake is true
              Log::Write("Error: handleHandshake is true, but version is not handled.");
              StopPairing();
              return;
            }
          }

          _rtsHandler->StartRts();
          _onReceivePlainTextHandle = nullptr;
          _state = RtsPairingPhase::AwaitingPublicKey;
        } else {
          // If we can't handle handshake, must cancel
          // THIS SHOULD NEVER HAPPEN
          Log::Write("Unable to process handshake. Something very bad happened.");
          StopPairing();
        }
      } else {
        // ignore msg
        StopPairing();
        Log::Write("Received raw message that is not handshake.");
      }
    } else{
      // ignore msg
      StopPairing();
      Log::Write("Internal state machine error. Assuming raw message, but state is not initial [%d].", (int)_state);
    }
  });
}

bool RtsComms::HandleHandshake(uint16_t version) {
  // our supported versions
  if((version == PairingProtocolVersion::CURRENT) ||
     (version == PairingProtocolVersion::V4) ||
     (version == PairingProtocolVersion::FACTORY)) {
    return true;
  }
  else if(version == PairingProtocolVersion::INVALID) {
    // Client should never send us this message.
    Log::Write("Client reported incompatible version [%d]. Our version is [%d]", version, PairingProtocolVersion::CURRENT);
    return false;
  }

  return false;
}

} // Switchboard
} // Anki