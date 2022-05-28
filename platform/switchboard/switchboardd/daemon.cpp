/**
 * File: _switchboardMain.cpp
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sodium.h>
#include <signals/simpleSignal.hpp>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <fstream>
#include <iomanip>

#include "anki-ble/common/log.h"
#include "anki-ble/common/anki_ble_uuids.h"
#include "anki-ble/common/ble_advertise_settings.h"
#include "anki-wifi/wifi.h"
#include "anki-wifi/exec_command.h"
#include "auto-test/autoTest.h"
#include "cutils/properties.h"
#include "switchboardd/christen.h"
#include "platform/victorCrashReports/victorCrashReporter.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"
#include "switchboardd/daemon.h"

#define LOG_PROCNAME "vic-switchboard"

// --------------------------------------------------------------------------------------------------------------------
// Switchboard Daemon
// --------------------------------------------------------------------------------------------------------------------
// @paluri
// --------------------------------------------------------------------------------------------------------------------

namespace Anki {
namespace Switchboard {

void Daemon::Start() {
  setAndroidLoggingTag("vic-switchboard");
  Log::Write("Loading up Switchboard Daemon");

  _loop = ev_default_loop(0);

  _taskExecutor = std::make_shared<Anki::TaskExecutor>(_loop);
  _connectionIdManager = std::make_shared<ConnectionIdManager>();

  // Saved session manager
  int rc = SavedSessionManager::MigrateKeys();
  if (rc) {
    Log::Error("Failed to Migrate Keys. Exiting. rc = %d", rc);
    exit(EXIT_FAILURE);
  }

  // Initialize Ble Ipc Timer
  ev_timer_init(&_ankibtdTimer, HandleAnkibtdTimer, kRetryInterval_s, kRetryInterval_s);

  // Initialize Ota Timer
  _handleOtaTimer.signal = &_otaUpdateTimerSignal;
  _otaUpdateTimerSignal.SubscribeForever(std::bind(&Daemon::HandleOtaUpdateProgress, this));
  ev_timer_init(&_handleOtaTimer.timer, &Daemon::sEvTimerHandler, kOtaUpdateInterval_s, kOtaUpdateInterval_s);

  // Initialize Pairing Timer
  _pairingTimer.signal = &_pairingPreConnectionSignal;
  _pairingPreConnectionSignal.SubscribeForever(std::bind(&Daemon::HandlePairingTimeout, this));
  ev_timer_init(&_pairingTimer.timer, &Daemon::sEvTimerHandler, kPairingPreConnectionTimeout_s, 0);

  // Initialize wifi listeners
  Anki::Wifi::Initialize(_taskExecutor);
  _wifiWatcher = std::make_shared<WifiWatcher>(_loop);
  _wifiChangedHandle = Anki::Wifi::GetWifiChangedSignal().ScopedSubscribe(std::bind(&Daemon::OnWifiChanged, this, std::placeholders::_1, std::placeholders::_2));

  // Initialize IPC connections
  InitializeCloudComms();   // must come before gateway comms
  InitializeGatewayComms();
  InitializeEngineComms();

  // Log the initial wifi state
  LogWifiState();
  Log::Write("Finished Starting");
}

void Daemon::Stop() {
  if(_bleClient != nullptr) {
    _bleClient->Disconnect(_connectionId);
    _bleClient->StopAdvertising();
  }

  if(_engineMessagingClient != nullptr) {
    Log::Write("End pairing state.");
    _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
  }

  ev_timer_stop(_loop, &_engineTimer);
  ev_timer_stop(_loop, &_handleOtaTimer.timer);
}

void Daemon::OnWifiChanged(bool connected, std::string manufacturerMac) {
  if(!connected) {
    Log::Write("Daemon: OnWifiChanged -- trying to connect to wifi");
    _wifiWatcher->ConnectIfNoWifi();
  }
}

void Daemon::LogWifiState() {
  Anki::Wifi::WiFiState wifiState = Anki::Wifi::GetWiFiState();

  bool connected = (wifiState.connState == Anki::Wifi::WiFiConnState::CONNECTED) ||
                   (wifiState.connState == Anki::Wifi::WiFiConnState::ONLINE);

  std::string event = "wifi.initial_state";

  DASMSG(wifi_initial_connection_status, event,
          "WiFi connection state on Switchboard load up.");

  uint8_t apMac[6];
  bool hasMac = Anki::Wifi::GetApMacAddress(apMac);

  std::string apMacManufacturerBytes = "";

  if(hasMac) {
    // Strip ap MAC of last three bytes
    for(int i = 0; i < 3; i++) {
      std::stringstream ss;
      ss << std::setfill('0') << std::setw(2) << std::hex << (int)apMac[i];
      apMacManufacturerBytes += ss.str();
    }
  }

  DASMSG_SET(s1, connected?"connected":"disconnected", "Connection state.");
  DASMSG_SET(s2, apMacManufacturerBytes, "Mac address prefix.");
  DASMSG_SEND();
}

void Daemon::InitializeEngineComms() {
  _engineMessagingClient = std::make_shared<EngineMessagingClient>(_loop);
  _engineMessagingClient->Init();
  _engineMessagingClient->OnReceivePairingStatus().SubscribeForever(std::bind(&Daemon::OnPairingStatus, this, std::placeholders::_1));
  _engineTimer.data = this;
  ev_timer_init(&_engineTimer, HandleEngineTimer, kRetryInterval_s, kRetryInterval_s);
  ev_timer_start(_loop, &_engineTimer);
}

void Daemon::InitializeGatewayComms() {
  _gatewayMessagingServer = std::make_shared<GatewayMessagingServer>(_loop, _taskExecutor, _tokenClient, _connectionIdManager);
  _gatewayMessagingServer->Init();
}

void Daemon::InitializeCloudComms() {
  _tokenClient = std::make_shared<TokenClient>(_loop, _taskExecutor);
  _tokenClient->Init();

  _tokenTimer.data = this;
  ev_timer_init(&_tokenTimer, HandleTokenTimer, kRetryInterval_s, kRetryInterval_s);
  ev_timer_start(_loop, &_tokenTimer);
}

bool Daemon::TryConnectToEngineServer() {
  bool connected = _engineMessagingClient->Connect();

  if (connected) {
    Log::Write("Initialize EngineMessagingClient");
    _connectionFailureCounter = kFailureCountToLog;
  } else {
    if(++_connectionFailureCounter >= kFailureCountToLog) {
      Log::Write("Failed to Initialize EngineMessagingClient ... trying again.");
      _connectionFailureCounter = 0;
    }
  }

  return connected;
}

bool Daemon::TryConnectToAnkiBluetoothDaemon() {
  if(!_bleClient->IsConnected()) {
    (void)_bleClient->Connect();
  }

  if(_bleClient->IsConnected()) {
    Log::Write("Ble IPC client connected.");
    UpdateAdvertisement(false);
    _connectionFailureCounter = kFailureCountToLog;
  } else {
    if(++_connectionFailureCounter >= kFailureCountToLog) {
      Log::Write("Failed to connect to ankibluetoothd ... trying again.");
      _connectionFailureCounter = 0;
    }
  }

  return _bleClient->IsConnected();
}

bool Daemon::TryConnectToTokenServer() {
  bool connected = _tokenClient->Connect();

  if (connected) {
    Log::Write("Initialize TokenClient");
    _tokenConnectionFailureCounter = kFailureCountToLog;

    (void)_tokenClient->SendJwtRequest([this](Anki::Vector::TokenError error, std::string jwt){
      Log::Write("Received response from TokenClient.");
      _hasCloudOwner = (error != Anki::Vector::TokenError::NullToken);
      _isTokenClientFullyInitialized = true;
    });
  } else {
    if(++_tokenConnectionFailureCounter >= kFailureCountToLog) {
      Log::Write("Failed to Initialize EngineMessagingClient ... trying again.");
      _tokenConnectionFailureCounter = 0;
    }
  }

  return connected;
}

void Daemon::InitializeBleComms() {
  Log::Write("Initialize BLE");

  _engineMessagingClient->HandleHasBleKeysRequest();

  if(_bleClient.get() == nullptr) {
    _bleClient = std::make_unique<Anki::Switchboard::BleClient>(_loop);

    _bleOnConnectedHandle = _bleClient->OnConnectedEvent().ScopedSubscribe(std::bind(&Daemon::OnConnected, this, std::placeholders::_1, std::placeholders::_2));
    _bleOnDisconnectedHandle = _bleClient->OnDisconnectedEvent().ScopedSubscribe(std::bind(&Daemon::OnDisconnected, this, std::placeholders::_1, std::placeholders::_2));
    _bleOnIpcPeerDisconnectedHandle = _bleClient->OnIpcDisconnection().ScopedSubscribe(std::bind(&Daemon::OnBleIpcDisconnected, this));

    _ankibtdTimer.data = this;
  }

  ev_timer_again(_loop, &_ankibtdTimer);
}

void Daemon::UpdateAdvertisement(bool pairing) {
  if(_bleClient == nullptr || !_bleClient->IsConnected()) {
    Log::Write("Tried to update BLE advertisement when not connected to ankibluetoothd.");
    return;
  }

  if(AutoTest::IsAutoTestBot()) {
    if(!pairing) {
      Log::Write("automation: UpdatingAdvertisement - overriding pairing state. Forcing into pairing mode.");
    }
    pairing = true;
  }

  // update state
  _isPairing = pairing;

  if(_securePairing != nullptr) {
    _securePairing->SetIsPairing(pairing);
  }

  Anki::BLEAdvertiseSettings settings;
  settings.GetAdvertisement().SetServiceUUID(Anki::kAnkiSingleMessageService_128_BIT_UUID);
  settings.GetAdvertisement().SetIncludeDeviceName(true);
  std::vector<uint8_t> mdata = Anki::kAnkiBluetoothSIGCompanyIdentifier;
  mdata.push_back(Anki::kVictorProductIdentifier); // distinguish from future Anki products
  mdata.push_back(pairing?'p':0x00); // to indicate whether we are pairing
  settings.GetAdvertisement().SetManufacturerData(mdata);

  std::string robotName = SavedSessionManager::GetRobotName();
  _bleClient->SetAdapterName(robotName);
  _bleClient->StartAdvertising(settings);
}

void Daemon::OnConnected(int connId, INetworkStream* stream) {
  Log::Write("OnConnected");
  _taskExecutor->Wake([connId, stream, this](){
    // Stop pairing timer
    ev_timer_stop(_loop, &_pairingTimer.timer);

    Log::Write("Connected to a BLE central.");
    _connectionId = connId;

    if(_securePairing == nullptr) {
      _securePairing = std::make_unique<Anki::Switchboard::RtsComms>(stream, _loop, _engineMessagingClient, _gatewayMessagingServer, _tokenClient, _connectionIdManager, _wifiWatcher, _taskExecutor, _isPairing, _isOtaUpdating, _hasCloudOwner);
      _pinHandle = _securePairing->OnUpdatedPinEvent().ScopedSubscribe(std::bind(&Daemon::OnPinUpdated, this, std::placeholders::_1));
      _otaHandle = _securePairing->OnOtaUpdateRequestEvent().ScopedSubscribe(std::bind(&Daemon::OnOtaUpdatedRequest, this, std::placeholders::_1));
      _endHandle = _securePairing->OnStopPairingEvent().ScopedSubscribe(std::bind(&Daemon::OnEndPairing, this));
      _completedPairingHandle = _securePairing->OnCompletedPairingEvent().ScopedSubscribe(std::bind(&Daemon::OnCompletedPairing, this));
    }

    (void)_tokenClient->SendJwtRequest([this](Anki::Vector::TokenError error, std::string jwt){
      if(_securePairing == nullptr) {
        return;
      }

      // there is owner if JWT is not null
      // (this might need to be modified for re-associate case
      // to include invalid token)
      _hasCloudOwner = error != Anki::Vector::TokenError::NullToken;

      // Initiate pairing process
      _securePairing->SetHasOwner(_hasCloudOwner);
      _securePairing->BeginPairing();
    });

    // tell engine that we have BLE connection
    _engineMessagingClient->SendBLEConnectionStatus(true);
  });
  Log::Write("Done OnConnected");

  DASMSG(ble_connection_status, "ble.connection",
          "BLE connection status has changed.");
  DASMSG_SEND();
}

void Daemon::OnDisconnected(int connId, INetworkStream* stream) {
  _taskExecutor->Wake([this](){
    _connectionIdManager->Clear();

    // do any clean up needed
    if(_securePairing != nullptr) {
      _securePairing->StopPairing();
      Log::Write("BLE Central disconnected.");
      if(!_isOtaUpdating) {
        _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
      }
      Log::Write("Destroying secure pairing object.");
      _pinHandle = nullptr;
      _otaHandle = nullptr;
      _endHandle = nullptr;
      _completedPairingHandle = nullptr;
      _securePairing = nullptr;
    }

    UpdateAdvertisement(false);

    // Re-enable autoconnect in case BLE disconnected before 
    // RtsHandler could re-enable WifiWatcher
    _wifiWatcher->Enable();

    // tell engine that we lost BLE connection
    _engineMessagingClient->SendBLEConnectionStatus(false);

    DASMSG(ble_connection_status, "ble.disconnection",
            "BLE connection status has changed.");
    DASMSG_SEND();

    DASMSG(ble_conn_id_stop, DASMSG_BLE_CONN_ID_STOP, "BLE connection id");
    DASMSG_SEND();

    if(_shouldRestartPairing) {
      // if pairing should be restarted, restart it
      _shouldRestartPairing = false;
      StartPairing();
    }
  });
}

void Daemon::OnBleIpcDisconnected() {
  // Reinitialize Ble Comms
  InitializeBleComms();
}

void Daemon::OnPinUpdated(std::string pin) {
  _engineMessagingClient->SetPairingPin(pin);
  _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::SHOW_PIN);
  Log::Blue((" " + pin + " ").c_str());
}

void Daemon::OnEndPairing() {
  Log::Write("Turning off Pairing Mode and Disconnecting from Client");

  UpdateAdvertisement(false);

  if(_bleClient != nullptr) {
    Log::Write("Pairing ended: Disconnecting from BLE Central [%d]", _connectionId);
    _bleClient->Disconnect(_connectionId);
  }

  if(_engineMessagingClient != nullptr) {
    _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
  }
}

void Daemon::OnCompletedPairing() {
  // Handle Successful Pairing Event
  // (for now, the handling may be no different than failed pairing)
  UpdateAdvertisement(false);

  if(_bleClient != nullptr) {
    _bleClient->StopAdvertising();
  }
}

void Daemon::HandlePairingTimeout() {
  Log::Write("[PT] Pairing timed-out before connection made.");
  UpdateAdvertisement(false);
  if(_engineMessagingClient != nullptr) {
    _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
  }
}

void Daemon::HandleOtaUpdateProgress() {
  if(_securePairing != nullptr) {
    // Update connected client of status
    uint64_t progressVal = 0;
    uint64_t expectedVal = 0;

    int status = GetOtaProgress(&progressVal, &expectedVal);

    if(status == -1) {
      _securePairing->SendOtaProgress(OtaStatusCode::UNKNOWN, progressVal, expectedVal);
    } else {
      Log::Write("Downloaded %llu/%llu bytes.", progressVal, expectedVal);
      _securePairing->SendOtaProgress(OtaStatusCode::IN_PROGRESS, progressVal, expectedVal);
    }
  }

  if (_isUpdateEngineServiceRunning) {
    if (access(kUpdateEngineEnvPath.c_str(), F_OK) == -1) {
      // The update-engine env file has been deleted by systemd
      _isUpdateEngineServiceRunning = false;
      int rc = -1;
      if (access(kUpdateEngineDonePath.c_str(), F_OK) != -1) {
        rc = 0;
      }
      if (access(kUpdateEngineErrorPath.c_str(), F_OK) != -1) {
        rc = -1;
        std::string exitCodeString = Anki::Util::FileUtils::ReadFile(kUpdateEngineExitCodePath);
        if (!exitCodeString.empty()) {
          int exitCode = std::atoi(exitCodeString.c_str());
          if (exitCode) {
            rc = exitCode;
          }
        }
      }
      HandleOtaUpdateExit(rc);
    }
  }
}

int Daemon::GetOtaProgress(uint64_t* progressVal, uint64_t* expectedVal) {
  // read values from files
  std::string progress;
  std::string expected;

  std::ifstream progressFile;
  std::ifstream expectedFile;

  *progressVal = 0;
  *expectedVal = 0;

  progressFile.open(kUpdateEngineDataPath + "/progress");
  expectedFile.open(kUpdateEngineDataPath + "/expected-size");

  if(!progressFile.is_open() || !expectedFile.is_open()) {
    return -1;
  }

  getline(progressFile, progress);
  getline(expectedFile, expected);

  long int strtol (const char* str, char** endptr, int base);
  char* progressEndptr;
  char* expectedEndptr;

  long long int progressLong = std::strtoll(progress.c_str(), &progressEndptr, 10);
  long long int expectedLong = std::strtoll(expected.c_str(), &expectedEndptr, 10);

  if(progressEndptr == progress.c_str()) {
    progressLong = 0;
  }

  if(expectedEndptr == expected.c_str()) {
    return -1;
  }

  if(progressLong == LONG_MAX || progressLong == LONG_MIN) {
    // 0, LONG_MAX, LONG_MIN are error cases from strtol
    progressLong = 0;
  }

  if(expectedLong == LONG_MAX || expectedLong == LONG_MIN || expectedLong == 0) {
    // 0, LONG_MAX, LONG_MIN are error cases from strtol
    // if our expected size (denominator) is screwed, we shouldn't send progress
    return -1;
  }

  *progressVal = (unsigned)progressLong;
  *expectedVal = (unsigned)expectedLong;

  return 0;
}

void Daemon::HandleOtaUpdateExit(int rc) {
  (void) unlink(kUpdateEngineEnvPath.c_str());
  (void) unlink(kUpdateEngineDisablePath.c_str());
  _taskExecutor->Wake([rc, this] {
    if(rc == 0) {
      uint64_t progressVal = 0;
      uint64_t expectedVal = 0;

      int status = GetOtaProgress(&progressVal, &expectedVal);

      if(status == 0) {
        if(_securePairing != nullptr) {
          // inform client of status before rebooting
          _securePairing->SendOtaProgress(OtaStatusCode::COMPLETED, progressVal, expectedVal);
        }

        if(progressVal != 0 && progressVal == expectedVal) {
          Log::Write("Update download finished successfully. Rebooting in 3 seconds.");
          auto when = std::chrono::steady_clock::now() + std::chrono::seconds(3);
          _taskExecutor->WakeAfter([this]() {
            this->HandleReboot();
          }, when);
        } else {
          Log::Write("Update engine exited with status 0 but progress and expected-size did not match or were 0.");
        }
      } else {
        Log::Write("Trouble reading status files for update engine. Won't reboot.");
        if(_securePairing != nullptr) {
          _securePairing->SendOtaProgress(OtaStatusCode::ERROR, 0, 0);
        }
      }
    } else {
      // error happened while downloading OTA update
      if(_securePairing != nullptr) {
        _securePairing->SendOtaProgress(rc, 0, 0);
      }
      Log::Write("Update failed with error code: %d", rc);
    }

    if(_securePairing != nullptr) {
      _securePairing->SetOtaUpdating(false);
    }

    ev_timer_stop(_loop, &_handleOtaTimer.timer);
    _isOtaUpdating = false;

    if(rc != 0) {
      if(_securePairing == nullptr) {
        // Change the face back to end pairing state *only* if
        // we didn't update successfully and there is no BLE connection
        _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
      }
    }
  });
}

void Daemon::OnOtaUpdatedRequest(std::string url) {
  if(_isOtaUpdating) {
    // handle
    return;
  }

  _isOtaUpdating = true;
  ev_timer_again(_loop, &_handleOtaTimer.timer);

  Log::Write("Ota Update Initialized...");
  // If the update-engine.service file is not present then we are running on an older version of
  // the Victor OS that does not have automatic updates.  Instead, we can just directly launch
  // /anki/bin/update-engine in the background.
  if (access(kUpdateEngineServicePath.c_str(), F_OK) == -1) {
    ExecCommandInBackground({kUpdateEngineExecPath, url},
                            std::bind(&Daemon::HandleOtaUpdateExit, this, std::placeholders::_1));
    return;
  }

  // Disable update-engine from running automatically
  if (!Anki::Util::FileUtils::WriteFileAtomic(kUpdateEngineDisablePath, "1")) {
    HandleOtaUpdateExit(-1);
    return;
  }

  // Stop any running instance of update-engine
  int rc = ExecCommand({"sudo", "/bin/systemctl", "stop", "update-engine.service"});
  if (rc) {
    HandleOtaUpdateExit(rc);
    return;
  }

  // Write out the environment file for update engine to use
  std::ostringstream updateEngineEnv;
  updateEngineEnv << "UPDATE_ENGINE_ENABLED=True" << std::endl;
  updateEngineEnv << "UPDATE_ENGINE_MAX_SLEEP=1" << std::endl; // No sleep, execute right away
  updateEngineEnv << "UPDATE_ENGINE_URL=\"" << url << "\"" << std::endl;
  if (!Anki::Util::FileUtils::WriteFileAtomic(kUpdateEngineEnvPath, updateEngineEnv.str())) {
    HandleOtaUpdateExit(-1);
    return;
  }

  // Remove any previous "done" file so that we can run update-engine again
  (void) unlink(kUpdateEngineDonePath.c_str());

  // Remove the disable file so that update-engine can start
  (void) unlink(kUpdateEngineDisablePath.c_str());

  // Restart the update-engine service so that our new config will be loaded
  rc = ExecCommand({"sudo", "/bin/systemctl", "start", "update-engine.service"});

  if (rc != 0) {
    HandleOtaUpdateExit(rc);
    return;
  }
  _isUpdateEngineServiceRunning = true;
}

void Daemon::StartPairing() {
  Log::Write("Entering pairing mode.");

  if(_securePairing != nullptr) {
    if(_bleClient != nullptr) {
      _shouldRestartPairing = true;
      _securePairing->ForceDisconnect();
      _bleClient->Disconnect(_connectionId);
    } else {
      Log::Error("RtsComms was alive while BleClient was null.");
    }
    return;
  } 

  UpdateAdvertisement(true);
  _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::SHOW_PRE_PIN);
  
  ev_timer_stop(_loop, &_pairingTimer.timer);
  ev_timer_set(&_pairingTimer.timer, kPairingPreConnectionTimeout_s, 0.);
  ev_timer_start(_loop, &_pairingTimer.timer);
  
  Log::Write("[PT] Starting pairing timer... pairing will timeout in %d seconds.", kPairingPreConnectionTimeout_s);
}

void Daemon::OnPairingStatus(Anki::Vector::ExternalInterface::MessageEngineToGame message) {
  Anki::Vector::ExternalInterface::MessageEngineToGameTag tag = message.GetTag();

  switch(tag){
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::EnterPairing: {
      StartPairing();
      break;
    }
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::ExitPairing: {
      printf("Exit pairing: %hhu\n", tag);
      ev_timer_stop(_loop, &_pairingTimer.timer);
      UpdateAdvertisement(false);
      if(_securePairing != nullptr && _isPairing) {
        _securePairing->StopPairing();
      }
      _engineMessagingClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
      break;
    }
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::WifiScanRequest: {
      _engineMessagingClient->HandleWifiScanRequest();
      break;
    }
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::WifiConnectRequest: {
      Log::Write("Got WifiConnectRequest\n");
      const auto& payload = message.Get_WifiConnectRequest();
      _engineMessagingClient->HandleWifiConnectRequest(std::string((char*)&payload.ssid),
                                                       std::string((char*)&payload.pwd),
                                                       payload.disconnectAfterConnection);
      break;
    }
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::HasBleKeysRequest: {
      _engineMessagingClient->HandleHasBleKeysRequest();
      break;
    }
    default: {
      printf("Unknown Tag: %hhu\n", tag);
      break;
    }
  }
}

void Daemon::HandleEngineTimer(struct ev_loop* loop, struct ev_timer* w, int revents) {
  Daemon* daemon = (Daemon*)w->data;

  if(!daemon->IsTokenClientFullyInitialized()) {
    return;
  }

  bool connected = daemon->TryConnectToEngineServer();

  if(connected) {
    ev_timer_stop(loop, w);
    daemon->InitializeBleComms();
  }
}

void Daemon::HandleAnkibtdTimer(struct ev_loop* loop, struct ev_timer* w, int revents) {
  Daemon* daemon = (Daemon*)w->data;
  bool connected = daemon->TryConnectToAnkiBluetoothDaemon();

  if(connected) {
    ev_timer_stop(loop, w);
    Log::Write("Initialization complete.");
  }
}

void Daemon::HandleTokenTimer(struct ev_loop* loop, struct ev_timer* w, int revents) {
  Daemon* daemon = (Daemon*)w->data;
  bool connected = daemon->TryConnectToTokenServer();

  if(connected) {
    ev_timer_stop(loop, w);
  }
}

void Daemon::HandleReboot() {
  Log::Write("Rebooting...");

  // shut down timers
  Stop();

  // trigger reboot
  sync(); sync(); sync();
  int status = ExecCommand({"sudo", "/sbin/reboot"});


  if (!status) {
    Log::Write("Error while restarting: [%d]", status);
    (void) reboot(LINUX_REBOOT_CMD_RESTART);
  }
}

void Daemon::sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents)
{
  struct ev_TimerStruct *wData = (struct ev_TimerStruct*)w;
  wData->signal->emit();
}

} // Switchboard
} // Anki

// ####################################################################################################################
// Entry Point
// ####################################################################################################################
static struct ev_signal sIntSig;
static struct ev_signal sTermSig;
static ev_timer sTimer;
static struct ev_loop* sLoop;
const static uint32_t kTick_s = 30;
std::unique_ptr<Anki::Switchboard::Daemon> _daemon;

static void ExitHandler(int status = 0) {
  // todo: smoothly handle termination

  Anki::Util::gLoggerProvider = nullptr;
  Anki::Util::gEventProvider = nullptr;

  Anki::Vector::UninstallCrashReporter();

  _exit(status);
}

static void SignalCallback(struct ev_loop* loop, struct ev_signal* w, int revents)
{
  logi("Exiting for signal %d", w->signum);

  // Deinitialize Wifi
  Anki::Wifi::Deinitialize();

  if(_daemon != nullptr) {
    _daemon->Stop();
  }

  // Stop timers and end our ev loop.
  ev_timer_stop(sLoop, &sTimer);
  ev_unloop(sLoop, EVUNLOOP_ALL);
  ExitHandler();
}

static void Tick(struct ev_loop* loop, struct ev_timer* w, int revents) {
  // noop
}

int SwitchboardMain() {

  Anki::Vector::InstallCrashReporter(LOG_PROCNAME);

  Anki::Util::VictorLogger logger(LOG_PROCNAME);
  Anki::Util::gLoggerProvider = &logger;
  Anki::Util::gEventProvider = &logger;

  DASMSG(switchboard_hello, "switchboard.hello", "Switchboard service start");
  DASMSG_SET(s1, "hello", "Test string");
  DASMSG_SET(i1, getpid(), "Test value");
  DASMSG_SEND();

  sLoop = ev_default_loop(0);

  ev_signal_init(&sIntSig, SignalCallback, SIGINT);
  ev_signal_start(sLoop, &sIntSig);
  ev_signal_init(&sTermSig, SignalCallback, SIGTERM);
  ev_signal_start(sLoop, &sTermSig);

  // initialize daemon
  _daemon = std::make_unique<Anki::Switchboard::Daemon>(sLoop);
  _daemon->Start();

  // exit
  ev_timer_init(&sTimer, Tick, kTick_s, kTick_s);
  ev_timer_start(sLoop, &sTimer);
  ev_loop(sLoop, 0);
  ExitHandler();
  return 0;
}
// ####################################################################################################################
