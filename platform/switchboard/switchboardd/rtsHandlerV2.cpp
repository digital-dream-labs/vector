/**
 * File: rtsHandlerV2.cpp
 *
 * Author: paluri
 * Created: 6/27/2018
 *
 * Description: V2 of BLE Protocol
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "exec_command.h"
#include "switchboardd/rtsHandlerV2.h"

#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "util/fileUtils/fileUtils.h"

#include <sstream>
#include <cutils/properties.h>

namespace Anki {
namespace Switchboard {

using namespace Anki::Vector::ExternalComms;
long long RtsHandlerV2::sTimeStarted;

RtsHandlerV2::RtsHandlerV2(INetworkStream* stream, 
    struct ev_loop* evloop,
    std::shared_ptr<ISwitchboardCommandClient> engineClient,
    std::shared_ptr<TokenClient> tokenClient,
    std::shared_ptr<TaskExecutor> taskExecutor,
    std::shared_ptr<WifiWatcher> wifiWatcher,
    bool isPairing,
    bool isOtaUpdating,
    bool hasOwner) :
IRtsHandler(isPairing, isOtaUpdating, hasOwner, tokenClient),
_stream(stream),
_loop(evloop),
_engineClient(engineClient),
_taskExecutor(taskExecutor),
_wifiWatcher(wifiWatcher),
_pin(""),
_challengeAttempts(0),
_numPinDigits(0),
_pingChallenge(0),
_abnormalityCount(0),
_inetTimerCount(0),
_wifiConnectTimeout_s(15)
{
  Log::Write("Instantiate with isPairing:%s", isPairing?"true":"false");
  sTimeStarted = std::time(0);

  // Initialize the key exchange object
  _keyExchange = std::make_unique<KeyExchange>(kNumPinDigits);

  // Register with stream events
  _onReceivePlainTextHandle = _stream->OnReceivedPlainTextEvent().ScopedSubscribe(
    std::bind(&RtsHandlerV2::HandleMessageReceived,
              this, std::placeholders::_1, std::placeholders::_2));

  _onReceiveEncryptedHandle = _stream->OnReceivedEncryptedEvent().ScopedSubscribe(
    std::bind(&RtsHandlerV2::HandleMessageReceived,
              this, std::placeholders::_1, std::placeholders::_2));

  _onFailedDecryptionHandle = _stream->OnFailedDecryptionEvent().ScopedSubscribe(
    std::bind(&RtsHandlerV2::HandleDecryptionFailed, this));

  // Register with private events
  _internetTimerSignal.SubscribeForever(std::bind(&RtsHandlerV2::HandleInternetTimerTick, this));

  // Initialize the message handler
  _cladHandler = std::make_unique<ExternalCommsCladHandlerV2>();
  SubscribeToCladMessages();

  // Initialize ev timer
  _handleInternet.signal = &_internetTimerSignal;
  ev_timer_init(&_handleInternet.timer, &RtsHandlerV2::sEvTimerHandler, kWifiConnectInterval_s, kWifiConnectInterval_s);
  
  Log::Write("RtsComms V2 starting up.");
}

RtsHandlerV2::~RtsHandlerV2() {
  _onReceivePlainTextHandle = nullptr;
  _onReceiveEncryptedHandle = nullptr;
  _onFailedDecryptionHandle = nullptr;

  ev_timer_stop(_loop, &_handleInternet.timer);
}

bool RtsHandlerV2::StartRts() {
  SendPublicKey();
  _state = RtsPairingPhase::AwaitingPublicKey;
  
  return true;
}

//
//
//

void RtsHandlerV2::Reset(bool forced) {
  // Tell the stream that we can no longer send over encrypted channel
  _stream->SetEncryptedChannelEstablished(false);

  // Send cancel message -- must do this before state is RAW
  SendCancelPairing();

  // Tell RtsComms to reset
  _resetSignal.emit(forced);
}

//
//
//

void RtsHandlerV2::StopPairing() {
  Reset(true);
}

void RtsHandlerV2::ForceDisconnect() {
  SendRtsMessage<RtsForceDisconnect>();
}

void RtsHandlerV2::SubscribeToCladMessages() {
  _rtsConnResponseHandle = _cladHandler->OnReceiveRtsConnResponse().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsConnResponse, this, std::placeholders::_1));
  _rtsChallengeMessageHandle = _cladHandler->OnReceiveRtsChallengeMessage().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsChallengeMessage, this, std::placeholders::_1));
  _rtsWifiConnectRequestHandle = _cladHandler->OnReceiveRtsWifiConnectRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsWifiConnectRequest, this, std::placeholders::_1));
  _rtsWifiIpRequestHandle = _cladHandler->OnReceiveRtsWifiIpRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsWifiIpRequest, this, std::placeholders::_1));
  _rtsRtsStatusRequestHandle = _cladHandler->OnReceiveRtsStatusRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsStatusRequest, this, std::placeholders::_1));
  _rtsWifiScanRequestHandle = _cladHandler->OnReceiveRtsWifiScanRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsWifiScanRequest, this, std::placeholders::_1));
  _rtsOtaUpdateRequestHandle = _cladHandler->OnReceiveRtsOtaUpdateRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsOtaUpdateRequest, this, std::placeholders::_1));
  _rtsOtaCancelRequestHandle = _cladHandler->OnReceiveRtsOtaCancelRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsOtaCancelRequest, this, std::placeholders::_1));
  _rtsWifiAccessPointRequestHandle = _cladHandler->OnReceiveRtsWifiAccessPointRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsWifiAccessPointRequest, this, std::placeholders::_1));
  _rtsCancelPairingHandle = _cladHandler->OnReceiveCancelPairingRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsCancelPairing, this, std::placeholders::_1));
  _rtsLogRequestHandle = _cladHandler->OnReceiveRtsLogRequest().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsLogRequest, this, std::placeholders::_1));
  _rtsForceDisconnectHandle = _cladHandler->OnReceiveRtsForceDisconnect().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsForceDisconnect, this, std::placeholders::_1)); 
  _rtsAckHandle = _cladHandler->OnReceiveRtsAck().ScopedSubscribe(std::bind(&RtsHandlerV2::HandleRtsAck, this, std::placeholders::_1));
}

//
//
//

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Event handling methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void RtsHandlerV2::HandleRtsConnResponse(const Anki::Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Unencrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::AwaitingPublicKey) {
    Anki::Vector::ExternalComms::RtsConnResponse connResponse = msg.Get_RtsConnResponse();

    if(connResponse.connectionType == Anki::Vector::ExternalComms::RtsConnType::FirstTimePair) {
      bool cloudAuth = false;

      #if ANKI_SWITCHBOARD_CLOUD_AUTH
      cloudAuth = true;
      #endif

      if(_hasOwner && cloudAuth) {
        //
        // Because RTSv3 requires cloud authorization if there is already an owner,
        // and because RTSv2 does not support that functionality, we must require all
        // initial pairs to be done with RTSv3+ if the robot already has a cloud account
        // owner.
        //
        Log::Write("Client tried to initial pair with V2 of protocol. Disconnecting.");
        Reset(true);
      }
      else if(_isPairing && !_isOtaUpdating) {
        HandleInitialPair((uint8_t*)connResponse.publicKey.data(), crypto_kx_PUBLICKEYBYTES);
        _state = RtsPairingPhase::AwaitingNonceAck;
      } else {
        Log::Write("Client tried to initial pair while not in pairing mode.");
      }
    } else {
      bool hasClient = false;;

      for(int i = 0; i < _rtsKeys.clients.size(); i++) {
        if(memcmp((uint8_t*)connResponse.publicKey.data(), (uint8_t*)&(_rtsKeys.clients[i].publicKey), crypto_kx_PUBLICKEYBYTES) == 0) {
          hasClient = true;
          _stream->SetCryptoKeys(
            _rtsKeys.clients[i].sessionTx,
            _rtsKeys.clients[i].sessionRx);

          SendNonce();
          _state = RtsPairingPhase::AwaitingNonceAck;
          Log::Write("Received renew connection request.");
          break;
        }
      }

      if(!hasClient) {
        Reset();
        Log::Write("No stored session for public key.");
      }
    }
  } else {
    // ignore msg
    IncrementAbnormalityCount();
    Log::Write("Received initial pair request in wrong state.");
  }
}

void RtsHandlerV2::HandleRtsChallengeMessage(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::AwaitingChallengeResponse) {
    Anki::Vector::ExternalComms::RtsChallengeMessage challengeMessage = msg.Get_RtsChallengeMessage();

    HandleChallengeResponse((uint8_t*)&challengeMessage.number, sizeof(challengeMessage.number));
  } else {
    // ignore msg
    IncrementAbnormalityCount();
    Log::Write("Received challenge response in wrong state.");
  }
}

void RtsHandlerV2::HandleRtsWifiConnectRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret) {
    Anki::Vector::ExternalComms::RtsWifiConnectRequest wifiConnectMessage = msg.Get_RtsWifiConnectRequest();

    Log::Write("Trying to connect to wifi network.");

    _wifiConnectTimeout_s = std::max(kWifiConnectMinTimeout_s, wifiConnectMessage.timeout);

    UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::SETTING_WIFI);

    // Disable autoconnect before connecting manually
    if(_wifiWatcher != nullptr) {
      _wifiWatcher->Disable();
    }

    Wifi::ConnectWifiResult connected = Wifi::ConnectWiFiBySsid(wifiConnectMessage.wifiSsidHex,
      wifiConnectMessage.password,
      wifiConnectMessage.authType,
      (bool)wifiConnectMessage.hidden,
      nullptr,
      nullptr);

    Wifi::WiFiState state = Wifi::GetWiFiState();
    bool online = state.connState == Wifi::WiFiConnState::ONLINE;

    if(online || (connected == Wifi::ConnectWifiResult::CONNECT_INVALIDKEY)) {
      ev_timer_stop(_loop, &_handleInternet.timer);
      _inetTimerCount = 0;
      SendWifiConnectResult(connected);
    } else {
      ev_timer_again(_loop, &_handleInternet.timer);
    }

    if(connected == Wifi::ConnectWifiResult::CONNECT_SUCCESS) {
      Log::Write("Connected to wifi.");
    } else if(connected == Wifi::ConnectWifiResult::CONNECT_INVALIDKEY) {
      Log::Write("Failure to connect: invalid wifi password.");
    } else {
      Log::Write("Failure to connect.");
    }
  } else {
    Log::Write("Received wifi credentials in wrong state.");
  }
}

void RtsHandlerV2::HandleRtsWifiIpRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret) {
    std::array<uint8_t, 4> ipV4;
    std::array<uint8_t, 16> ipV6;

    Wifi::WiFiIpFlags flags = Wifi::GetIpAddress(ipV4.data(), ipV6.data());
    bool hasIpV4 = (flags & Wifi::WiFiIpFlags::HAS_IPV4) != 0;
    bool hasIpV6 = (flags & Wifi::WiFiIpFlags::HAS_IPV6) != 0;

    SendRtsMessage<RtsWifiIpResponse>(hasIpV4, hasIpV6, ipV4, ipV6);
  }

  Log::Write("Received wifi ip request.");
}

void RtsHandlerV2::HandleRtsStatusRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret) {
    SendStatusResponse();
  } else {
    Log::Write("Received status request in the wrong state.");
  }
}

void RtsHandlerV2::HandleRtsWifiScanRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret) {
    UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::SETTING_WIFI);
    SendWifiScanResult();
  } else {
    Log::Write("Received wifi scan request in wrong state.");
  }
}

void RtsHandlerV2::HandleRtsOtaUpdateRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret && !_isOtaUpdating) {
    Anki::Vector::ExternalComms::RtsOtaUpdateRequest otaMessage = msg.Get_RtsOtaUpdateRequest();
    _otaUpdateRequestSignal.emit(otaMessage.url);
    _isOtaUpdating = true;
  }

  Log::Write("Starting OTA update.");
}

void RtsHandlerV2::HandleRtsOtaCancelRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret && _isOtaUpdating) {
    (void) ExecCommand({"sudo", "/bin/systemctl", "stop", "update-engine.service"});
    _isOtaUpdating = false;
    Log::Write("Terminating OTA Update Engine");
  } else {
    Log::Write("Tried to cancel OTA when OTA not running.");
  }

  // Send status response
  SendStatusResponse();
}

void RtsHandlerV2::HandleRtsWifiAccessPointRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  if(_state == RtsPairingPhase::ConfirmedSharedSecret) {
    Anki::Vector::ExternalComms::RtsWifiAccessPointRequest accessPointMessage = msg.Get_RtsWifiAccessPointRequest();
    if(accessPointMessage.enable) {
      // enable access point mode on Victor
      char vicName[PROPERTY_VALUE_MAX] = {0};
      (void)property_get("anki.robot.name", vicName, "");

      std::string ssid(vicName);
      std::string password = _keyExchange->GeneratePin(kWifiApPasswordSize);

      UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::SETTING_WIFI);

      bool success = Wifi::EnableAccessPointMode(ssid, password);

      SendWifiAccessPointResponse(success, ssid, password);

      Log::Write("Received request to enter wifi access point mode.");
    } else {
      // disable access point mode on Victor
      bool success = Wifi::DisableAccessPointMode();

      SendWifiAccessPointResponse(success, "", "");

      Log::Write("Received request to disable access point mode.");
    }
  }
}

void RtsHandlerV2::HandleRtsForceDisconnect(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!(HasState(RtsCommsType::Encrypted) || 
    HasState(RtsCommsType::Unencrypted))) {
    return;
  }

  _stopPairingSignal.emit();
}

void RtsHandlerV2::HandleRtsLogRequest(const Vector::ExternalComms::RtsConnection_2& msg) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  int exitCode = ExecCommand({"sudo", "/anki/bin/diagnostics-logger"});

  std::vector<uint8_t> logBytes
    = Anki::Util::FileUtils::ReadFileAsBinary("/data/diagnostics/logs.tar.bz2");

  if (logBytes.empty()) {
    exitCode = -1;
  }

  // Send RtsLogResponse
  uint32_t fileId = 0;
  randombytes_buf(&fileId, sizeof(fileId));
  SendRtsMessage<RtsLogResponse>(exitCode, fileId);

  // Send file
  SendFile(fileId, logBytes);
}

void RtsHandlerV2::HandleRtsCancelPairing(const Vector::ExternalComms::RtsConnection_2& msg) {
  Log::Write("Stopping pairing due to client request.");
  StopPairing();
}

void RtsHandlerV2::HandleRtsAck(const Vector::ExternalComms::RtsConnection_2& msg) {
  Anki::Vector::ExternalComms::RtsAck ack = msg.Get_RtsAck();
  if(_state == RtsPairingPhase::AwaitingNonceAck &&
    ack.rtsConnectionTag == (uint8_t)Anki::Vector::ExternalComms::RtsConnection_2Tag::RtsNonceMessage) {
    HandleNonceAck();
  } else {
    // ignore msg
    IncrementAbnormalityCount();

    std::ostringstream ss;
    ss << "Received nonce ack in wrong state '" << _state << "'.";
    Log::Write(ss.str().c_str());
  }
}

void RtsHandlerV2::HandleInitialPair(uint8_t* publicKey, uint32_t publicKeyLength) {
  // Handle initial pair request from client

  // Generate a random number with kNumPinDigits digits
  _pin = _keyExchange->GeneratePin();
  _updatedPinSignal.emit(_pin);

  // Input client's public key and calculate shared keys
  _keyExchange->SetRemotePublicKey(publicKey);
  _keyExchange->CalculateSharedKeysServer((unsigned char*)_pin.c_str());
  
  // Give our shared keys to the network stream
  _stream->SetCryptoKeys(
    _keyExchange->GetEncryptKey(),
    _keyExchange->GetDecryptKey());

  // Save keys to file
  // For now only save one client

  RtsClientData client;

  memcpy(&client.publicKey, publicKey, sizeof(client.publicKey));
  memcpy(&client.sessionRx, _keyExchange->GetDecryptKey(), sizeof(client.sessionRx));
  memcpy(&client.sessionTx, _keyExchange->GetEncryptKey(), sizeof(client.sessionTx));

  _rtsKeys.clients.clear();
  _rtsKeys.clients.push_back(client);

  SaveKeys();

  // Send nonce
  SendNonce();

  Log::Write("Received initial pair request, sending nonce.");
}

void RtsHandlerV2::HandleDecryptionFailed() {
  Log::Write("Decryption failed...");
  Reset();
}

void RtsHandlerV2::HandleNonceAck() {
  // Send challenge to user
  _type = RtsCommsType::Encrypted;
  SendChallenge();

  Log::Write("Client acked nonce, sending challenge [%d].", _pingChallenge);
}

inline bool isChallengeSuccess(uint32_t challenge, uint32_t answer) {
  const bool isSuccess = answer == challenge + 1;
  return isSuccess;
}

void RtsHandlerV2::HandleChallengeResponse(uint8_t* pingChallengeAnswer, uint32_t length) {
  bool success = false;

  if(length < sizeof(uint32_t)) {
    success = false;
  } else {
    uint32_t answer = *((uint32_t*)pingChallengeAnswer);
    success = isChallengeSuccess(_pingChallenge, answer);
  }

  if(success) {
    // Inform client that we are good to go and
    // update our state
    SendChallengeSuccess();
    _state = RtsPairingPhase::ConfirmedSharedSecret;
    Log::Green("Challenge answer was accepted. Encrypted channel established.");

    if(_isPairing) {
      _completedPairingSignal.emit();
    }
  } else {
    // Increment our abnormality and attack counter, and
    // if at or above max attempts reset.
    IncrementAbnormalityCount();
    IncrementChallengeCount();
    Log::Write("Received faulty challenge response.");
  }
}

//
// Sending messages
//

void RtsHandlerV2::SendPublicKey() {
  if(!HasState(RtsCommsType::Unencrypted)) {
    return;
  }

  // Generate public, private key
  (void)LoadKeys();

  std::array<uint8_t, crypto_kx_PUBLICKEYBYTES> publicKeyArray;
  std::copy((uint8_t*)&_rtsKeys.keys.id.publicKey,
            (uint8_t*)&_rtsKeys.keys.id.publicKey + crypto_kx_PUBLICKEYBYTES,
            publicKeyArray.begin());

  SendRtsMessage<RtsConnRequest>(publicKeyArray);

  // Save public key to file
  Log::Write("Sending public key to client.");
}

void RtsHandlerV2::SendNonce() {
  if(!HasState(RtsCommsType::Unencrypted)) {
    return;
  }

  // Send nonce
  const uint8_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

  // Generate a nonce
  uint8_t* toRobotNonce = _keyExchange->GetToRobotNonce();
  randombytes_buf(toRobotNonce, NONCE_BYTES);

  uint8_t* toDeviceNonce = _keyExchange->GetToDeviceNonce();
  randombytes_buf(toDeviceNonce, NONCE_BYTES);

  // Give our nonce to the network stream
  _stream->SetNonce(toRobotNonce, toDeviceNonce);

  std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> toRobotNonceArray;
  memcpy(std::begin(toRobotNonceArray), toRobotNonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);

  std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> toDeviceNonceArray;
  memcpy(std::begin(toDeviceNonceArray), toDeviceNonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);

  SendRtsMessage<RtsNonceMessage>(toRobotNonceArray, toDeviceNonceArray);
}

void RtsHandlerV2::SendChallenge() {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  // Tell the stream that we can now send over encrypted channel
  _stream->SetEncryptedChannelEstablished(true);
  // Update state to secureClad
  _state = RtsPairingPhase::AwaitingChallengeResponse;

  // Create random challenge value
  randombytes_buf(&_pingChallenge, sizeof(_pingChallenge));

  SendRtsMessage<RtsChallengeMessage>(_pingChallenge);
}

void RtsHandlerV2::SendChallengeSuccess() {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);

  // Send challenge and update state
  SendRtsMessage<RtsChallengeSuccessMessage>();
}

void RtsHandlerV2::SendStatusResponse() {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  Wifi::WiFiState state = Wifi::GetWiFiState();
  uint8_t bleState = 1; // for now, if we are sending this message, we are connected
  uint8_t batteryState = 0; // for now, ignore this field until we have a way to get that info
  bool isApMode = Wifi::IsAccessPointMode();

  // Send challenge and update state
  std::string buildNoString = GetBuildIdString();

  SendRtsMessage<RtsStatusResponse_2>(state.ssid, state.connState, isApMode, bleState, batteryState, buildNoString, _isOtaUpdating);

  Log::Write("Send status response.");
}

void RtsHandlerV2::SendWifiAccessPointResponse(bool success, std::string ssid, std::string pw) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  // Send challenge and update state
  SendRtsMessage<RtsWifiAccessPointResponse>(success, ssid, pw);
}

void RtsHandlerV2::SendWifiScanResult() {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  std::vector<Wifi::WiFiScanResult> wifiResults;
  Wifi::WifiScanErrorCode code = Wifi::ScanForWiFiAccessPoints(wifiResults);

  const uint8_t statusCode = (uint8_t)code;

  std::vector<Anki::Vector::ExternalComms::RtsWifiScanResult_2> wifiScanResults;

  for(int i = 0; i < wifiResults.size(); i++) {
    Anki::Vector::ExternalComms::RtsWifiScanResult_2 result = Anki::Vector::ExternalComms::RtsWifiScanResult_2(wifiResults[i].auth,
      wifiResults[i].signal_level,
      wifiResults[i].ssid,
      wifiResults[i].hidden);

      wifiScanResults.push_back(result);
  }

  Log::Write("Sending wifi scan results.");
  SendRtsMessage<RtsWifiScanResponse_2>(statusCode, wifiScanResults);
}

void RtsHandlerV2::SendWifiConnectResult(Wifi::ConnectWifiResult result) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  // Re-enable autoconnect
  if(_wifiWatcher != nullptr) {
    _wifiWatcher->Enable();
  }

  // Send challenge and update state
  Wifi::WiFiState wifiState = Wifi::GetWiFiState();
  SendRtsMessage<RtsWifiConnectResponse>(wifiState.ssid, wifiState.connState);
}

void RtsHandlerV2::SendFile(uint32_t fileId, std::vector<uint8_t> fileBytes) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }

  // Send File
  const size_t chunkSize = 256; // can't be more than 2^16
  size_t fileSizeBytes = fileBytes.size();
  uint8_t status = 0; // not sure if we need status byte, but reserving so I don't regret ~PRA

  size_t remaining = fileSizeBytes;

  while((ssize_t)remaining > 0) {
    size_t msgSize = remaining >= chunkSize? chunkSize : remaining;
    size_t bytesWritten = fileSizeBytes - remaining;

    std::vector<uint8_t> dataChunk;
    auto fileIter = fileBytes.begin() + bytesWritten;
    std::copy(fileIter, fileIter + msgSize, back_inserter(dataChunk));

    SendRtsMessage<RtsFileDownload>(status, fileId, bytesWritten + msgSize, fileSizeBytes, dataChunk);

    remaining -= msgSize;
  }
}

void RtsHandlerV2::SendCancelPairing() {
  // Send challenge and update state
  SendRtsMessage<RtsCancelPairing>();
  Log::Write("Canceling pairing.");
}

void RtsHandlerV2::SendOtaProgress(int status, uint64_t progress, uint64_t expectedTotal) {
  if(!HasState(RtsCommsType::Encrypted)) {
    return;
  }
  // Send Ota Progress
  SendRtsMessage<RtsOtaUpdateResponse>(status, progress, expectedTotal);
  Log::Write("Sending OTA Progress Update");
}

void RtsHandlerV2::HandleMessageReceived(uint8_t* bytes, uint32_t length) {
  _taskExecutor->WakeSync([this, bytes, length]() {
    if(length < kMinMessageSize) {
      Log::Write("Length is less than kMinMessageSize.");
      return;
    }

    _cladHandler->ReceiveExternalCommsMsg(bytes, length);
  });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Helper methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void RtsHandlerV2::HandleTimeout() {
  if(_state != RtsPairingPhase::ConfirmedSharedSecret) {
    Log::Write("Pairing timeout. Client took too long.");
    Reset();
  }
}

void RtsHandlerV2::IncrementChallengeCount() {
  // Increment challenge count
  _challengeAttempts++;

  if(_challengeAttempts >= kMaxMatchAttempts) {
    Reset();
  }

  Log::Write("Client answered challenge.");
}

void RtsHandlerV2::IncrementAbnormalityCount() {
  // Increment abnormality count
  _abnormalityCount++;

  if(_abnormalityCount >= kMaxAbnormalityCount) {
    Reset();
  }

  Log::Write("Abnormality recorded.");
}

void RtsHandlerV2::HandleInternetTimerTick() {
  _inetTimerCount++;

  Wifi::WiFiState state = Wifi::GetWiFiState();
  bool online = state.connState == Wifi::WiFiConnState::ONLINE;

  if(online || _inetTimerCount > _wifiConnectTimeout_s) {
    ev_timer_stop(_loop, &_handleInternet.timer);
    _inetTimerCount = 0;
    SendWifiConnectResult(Wifi::ConnectWifiResult::CONNECT_NONE);
  }
}

void RtsHandlerV2::UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus state) {
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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Static methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void RtsHandlerV2::sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents)
{
  std::ostringstream ss;
  ss << "[timer] " << (time(0) - sTimeStarted) << "s since beginning.";
  Log::Write(ss.str().c_str());

  struct ev_TimerStruct *wData = (struct ev_TimerStruct*)w;
  wData->signal->emit();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// EOF
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

} // Switchboard
} // Anki
