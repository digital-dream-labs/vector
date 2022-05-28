/**
 * File: RtsHandlerV5.h
 *
 * Author: paluri
 * Created: 11/07/2018
 *
 * Description: V5 of BLE Protocol
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include "switchboardd/IRtsHandler.h"
#include "switchboardd/tokenClient.h"
#include "switchboardd/connectionIdManager.h"
#include "switchboardd/INetworkStream.h"
#include "switchboardd/ISwitchboardCommandClient.h"
#include "switchboardd/wifiWatcher.h"
#include "switchboardd/taskExecutor.h"
#include "switchboardd/gatewayMessagingServer.h"
#include "switchboardd/externalCommsCladHandlerV5.h"
#include "anki-wifi/wifi.h"

#include <unordered_map>

namespace Anki {
namespace Switchboard {

class RtsHandlerV5 : public IRtsHandler {
public:
  RtsHandlerV5(INetworkStream* stream, 
    struct ev_loop* evloop,
    std::shared_ptr<ISwitchboardCommandClient> engineClient,
    std::shared_ptr<TokenClient> tokenClient,
    std::shared_ptr<GatewayMessagingServer> gatewayServer,
    std::shared_ptr<ConnectionIdManager> connectionIdManager,
    std::shared_ptr<TaskExecutor> taskExecutor,
    std::shared_ptr<WifiWatcher> wifiWatcher,
    bool isPairing,
    bool isOtaUpdating,
    bool hasOwner);

  ~RtsHandlerV5();

  bool StartRts() override;
  void StopPairing() override;
  void SendOtaProgress(int status, uint64_t progress, uint64_t expectedTotal) override;
  void HandleTimeout() override;
  void ForceDisconnect() override;

  // Types
  using StringSignal = Signal::Signal<void (std::string)>;
  using BoolSignal = Signal::Signal<void (bool)>;
  using VoidSignal = Signal::Signal<void ()>;

  // Events
  StringSignal& OnUpdatedPinEvent() { return _updatedPinSignal; }
  StringSignal& OnOtaUpdateRequestEvent() { return _otaUpdateRequestSignal; }
  VoidSignal& OnStopPairingEvent() { return _stopPairingSignal; }
  VoidSignal& OnCompletedPairingEvent() { return _completedPairingSignal; }
  BoolSignal& OnResetEvent() { return _resetSignal; }

private:
  // Statics
  static long long sTimeStarted;
  static void sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents);

  void Reset(bool forced=false);
  void SaveSessionKeys();
  bool IsAuthenticated();

  void SendPublicKey();
  void SendNonce();
  void SendChallenge();
  void SendCancelPairing();
  void SendChallengeSuccess();
  void SendWifiScanResult();
  void SendWifiConnectResult(Wifi::ConnectWifiResult result);
  void SendWifiAccessPointResponse(bool success, std::string ssid, std::string pw);
  void SendStatusResponse();
  void SendFile(uint32_t fileId, std::vector<uint8_t> fileBytes);

  void HandleMessageReceived(uint8_t* bytes, uint32_t length);
  void HandleDecryptionFailed();
  void HandleInitialPair(uint8_t* bytes, uint32_t length);
  void HandleCancelSetup();
  void HandleNonceAck();
  void HandleInternetTimerTick();
  void HandleOtaRequest();
  void HandleChallengeResponse(uint8_t* bytes, uint32_t length);
  void HandleCloudSessionAuthResponse(Anki::Vector::TokenError error, std::string appToken, std::string jwtToken);

  void ProcessCloudAuthResponse(bool isPrimary, Anki::Vector::TokenError authError, std::string appToken, std::string authJwtToken);

  void IncrementAbnormalityCount();
  void IncrementChallengeCount();

  void SubscribeToCladMessages();
  void UpdateFace(Anki::Vector::SwitchboardInterface::ConnectionStatus state);

  INetworkStream* _stream;
  struct ev_loop* _loop;
  std::shared_ptr<ISwitchboardCommandClient> _engineClient;
  std::shared_ptr<GatewayMessagingServer> _gatewayServer;
  std::shared_ptr<ConnectionIdManager> _connectionIdManager;
  std::shared_ptr<TaskExecutor> _taskExecutor;
  std::shared_ptr<WifiWatcher> _wifiWatcher;
  std::unique_ptr<ExternalCommsCladHandlerV5> _cladHandler;
  std::vector<std::weak_ptr<TokenResponseHandle>> _tokenClientHandles;

  const uint8_t kMaxMatchAttempts = 5;
  const uint8_t kMaxPairingAttempts = 3;
  const uint32_t kMaxAbnormalityCount = 5;
  const uint8_t kWifiApPasswordSize = 8;
  const uint8_t kNumPinDigits = 6;
  const uint8_t kWifiConnectMinTimeout_s = 1;
  const uint8_t kWifiConnectInterval_s = 1;
  const uint8_t kMinMessageSize = 2;
  const uint8_t kSdkRequestIdSize = 32;
  
  std::string _pin;
  uint8_t _challengeAttempts;
  uint8_t _numPinDigits;
  uint32_t _pingChallenge;
  uint32_t _abnormalityCount;
  uint8_t _inetTimerCount;
  uint8_t _wifiConnectTimeout_s;

  bool _isFirstTimePair = false;
  bool _hasCloudAuthed = false;
  bool _sessionReadyToSave = false;
  RtsClientData _clientSession;

  Signal::SmartHandle _onReceivePlainTextHandle;
  Signal::SmartHandle _onReceiveEncryptedHandle;
  Signal::SmartHandle _onFailedDecryptionHandle;

  struct ev_TimerStruct {
    ev_timer timer;
    VoidSignal* signal;
  } _handleInternet;

  StringSignal _updatedPinSignal;
  StringSignal _otaUpdateRequestSignal;
  VoidSignal _stopPairingSignal;
  VoidSignal _completedPairingSignal;
  BoolSignal _resetSignal;

  VoidSignal _internetTimerSignal;

  std::vector<std::shared_ptr<SafeHandle>> _handles;
  std::unordered_map<std::string, std::string> _sdkRequestIds;

  //
  // V3 Request to Listen for
  //
  Signal::SmartHandle _rtsConnResponseHandle;
  void HandleRtsConnResponse(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsChallengeMessageHandle;
  void HandleRtsChallengeMessage(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsWifiConnectRequestHandle;
  void HandleRtsWifiConnectRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsWifiIpRequestHandle;
  void HandleRtsWifiIpRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsRtsStatusRequestHandle;
  void HandleRtsStatusRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsWifiScanRequestHandle;
  void HandleRtsWifiScanRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsWifiForgetRequestHandle;
  void HandleRtsWifiForgetRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsOtaUpdateRequestHandle;
  void HandleRtsOtaUpdateRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsOtaCancelRequestHandle;
  void HandleRtsOtaCancelRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsWifiAccessPointRequestHandle;
  void HandleRtsWifiAccessPointRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsCancelPairingHandle;
  void HandleRtsCancelPairing(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsAckHandle;
  void HandleRtsAck(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsLogRequestHandle;
  void HandleRtsLogRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsCloudSessionHandle;
  void HandleRtsCloudSessionRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsAppConnectionIdHandle;
  void HandleRtsAppConnectionIdRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsForceDisconnectHandle;
  void HandleRtsForceDisconnect(const Vector::ExternalComms::RtsConnection_5& msg);

  Signal::SmartHandle _rtsSdkProxyHandle;
  void HandleRtsSdkProxyRequest(const Vector::ExternalComms::RtsConnection_5& msg);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Send messages method
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  template<typename T, typename... Args>
  int SendRtsMessage(Args&&... args) {
    Anki::Vector::ExternalComms::ExternalComms msg = Anki::Vector::ExternalComms::ExternalComms(
      Anki::Vector::ExternalComms::RtsConnection(Anki::Vector::ExternalComms::RtsConnection_5(T(std::forward<Args>(args)...))));
    std::vector<uint8_t> messageData(msg.Size());
    const size_t packedSize = msg.Pack(messageData.data(), msg.Size());

    if(_type == RtsCommsType::Unencrypted) {
      return _stream->SendPlainText(messageData.data(), packedSize);
    } else if(_type == RtsCommsType::Encrypted) {
      return _stream->SendEncrypted(messageData.data(), packedSize);
    } else {
      Log::Write("Tried to send clad message when state was already set back to RAW.");
    }

    return -1;
  }
};

} // Switchboard
} // Anki