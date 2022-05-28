/**
 * File: _switchboardMain.h
 *
 * Author: paluri
 * Created: 2/21/2018
 *
 * Description: Entry point for switchboardd. This program handles
 *              the incoming and outgoing external pairing and
 *              communication between Victor and BLE/WiFi clients.
 *              Switchboard accepts CLAD messages from engine/anim
 *              processes and routes them correctly to attached 
 *              clients, and vice versa. Switchboard also handles
 *              the initial authentication/secure pairing process
 *              which establishes confidential and authenticated
 *              channel of communication between Victor and an
 *              external client.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/
#include <stdlib.h>
#include "ev++.h"
#include "bleClient/bleClient.h"
#include "switchboardd/rtsComms.h"
#include "switchboardd/savedSessionManager.h"
#include "switchboardd/taskExecutor.h"
#include "switchboardd/tokenClient.h"
#include "switchboardd/connectionIdManager.h"
#include "switchboardd/engineMessagingClient.h"
#include "switchboardd/wifiWatcher.h"
#include "switchboardd/gatewayMessagingServer.h"

namespace Anki {
namespace Switchboard {
  enum OtaStatusCode {
    UNKNOWN     = 1,
    IN_PROGRESS = 2,
    COMPLETED   = 3,
    REBOOTING   = 4,
    ERROR       = 5,
  };

  class Daemon {
    public:
      Daemon(struct ev_loop* loop) :
        _loop(loop),
        _isPairing(false),
        _isOtaUpdating(false),
        _connectionFailureCounter(kFailureCountToLog),
        _tokenConnectionFailureCounter(kFailureCountToLog),
        _taskExecutor(nullptr),
        _bleClient(nullptr),
        _securePairing(nullptr),
        _engineMessagingClient(nullptr),
        _gatewayMessagingServer(nullptr),
        _isUpdateEngineServiceRunning(false),
        _shouldRestartPairing(false),
        _isTokenClientFullyInitialized(false),
        _hasCloudOwner(false)
      {}

      void Start();
      void Stop();

      bool IsTokenClientFullyInitialized() {
        return _isTokenClientFullyInitialized;
      }
    
    private:
      const std::string kSwitchboardRunPath = "/run/vic-switchboard";
      const std::string kUpdateEngineEnvPath = "/run/vic-switchboard/update-engine.env";
      const std::string kUpdateEngineDisablePath = "/run/vic-switchboard/disable-update-engine";
      const std::string kUpdateEngineDataPath = "/run/update-engine";
      const std::string kUpdateEngineDonePath = "/run/update-engine/done";
      const std::string kUpdateEngineErrorPath = "/run/update-engine/error";
      const std::string kUpdateEngineExitCodePath = "/run/update-engine/exit_code";
      const std::string kUpdateEngineExecPath = "/anki/bin/update-engine";
      const std::string kUpdateEngineServicePath = "/lib/systemd/system/update-engine.service";

      static void HandleEngineTimer(struct ev_loop* loop, struct ev_timer* w, int revents);
      static void HandleTokenTimer(struct ev_loop* loop, struct ev_timer* w, int revents);
      static void HandleAnkibtdTimer(struct ev_loop* loop, struct ev_timer* w, int revents);
      static void sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents);
      void HandleReboot();

      using EvTimerSignal = Signal::Signal<void ()>;

      void InitializeEngineComms();
      void InitializeGatewayComms();
      void InitializeCloudComms();
      void InitializeBleComms();
      void StartPairing();
      void OnConnected(int connId, INetworkStream* stream);
      void OnWifiChanged(bool connected, std::string manufacturerMac);
      void OnDisconnected(int connId, INetworkStream* stream);
      void OnBleIpcDisconnected();
      void OnPinUpdated(std::string pin);
      void OnOtaUpdatedRequest(std::string url);
      void OnEndPairing();
      void OnCompletedPairing();
      void OnPairingStatus(Anki::Vector::ExternalInterface::MessageEngineToGame message);
      bool TryConnectToEngineServer();
      bool TryConnectToTokenServer();
      bool TryConnectToAnkiBluetoothDaemon();
      void HandleOtaUpdateExit(int rc);
      void HandleOtaUpdateProgress();
      void HandlePairingTimeout();
      void LogWifiState();
      int GetOtaProgress(uint64_t* progress, uint64_t* expected);

      Signal::SmartHandle _pinHandle;
      Signal::SmartHandle _otaHandle;
      Signal::SmartHandle _endHandle;
      Signal::SmartHandle _completedPairingHandle;

      Signal::SmartHandle _bleOnConnectedHandle;
      Signal::SmartHandle _bleOnDisconnectedHandle;
      Signal::SmartHandle _bleOnIpcPeerDisconnectedHandle;

      Signal::SmartHandle _wifiChangedHandle;

      void UpdateAdvertisement(bool pairing);

      const uint8_t kOtaUpdateInterval_s = 1;
      const float kRetryInterval_s = 0.2f;
      const uint32_t kFailureCountToLog = 20;
      const uint32_t kPairingPreConnectionTimeout_s = 300;

      int _connectionId = -1;

      inline bool IsConnected() { return _connectionId != -1; }

      EvTimerSignal _otaUpdateTimerSignal;
      EvTimerSignal _pairingPreConnectionSignal;

      struct ev_loop* _loop;
      bool _isPairing;
      bool _isOtaUpdating;
      uint32_t _connectionFailureCounter;
      uint32_t _tokenConnectionFailureCounter;

      ev_timer _engineTimer;
      ev_timer _ankibtdTimer;
      ev_timer _tokenTimer;

      struct ev_TimerStruct {
        ev_timer timer;
        EvTimerSignal* signal;
      } _handleOtaTimer;

      struct ev_TimerStruct _pairingTimer;

      std::shared_ptr<TaskExecutor> _taskExecutor;
      std::unique_ptr<BleClient> _bleClient;
      std::unique_ptr<RtsComms> _securePairing;
      std::shared_ptr<EngineMessagingClient> _engineMessagingClient;
      std::shared_ptr<GatewayMessagingServer> _gatewayMessagingServer;
      std::shared_ptr<TokenClient> _tokenClient;
      std::shared_ptr<ConnectionIdManager> _connectionIdManager;
      bool _isUpdateEngineServiceRunning;
      bool _shouldRestartPairing;
      bool _isTokenClientFullyInitialized;
      bool _hasCloudOwner = false;

      std::shared_ptr<WifiWatcher> _wifiWatcher;
  };
}
}

// Entry point
int SwitchboardMain();
