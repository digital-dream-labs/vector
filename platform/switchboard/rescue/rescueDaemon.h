/**
* File: rescueDaemon.h
*
* Author: Paul Aluri
* Date:   02/27/2019
*
* Description: Entry point for vic-rescue. This program establishes
*              connection with ankibluetoothd and enters pairing
*              mode while waiting for a client to connect, in case
*              a user desires to gather logs or perform other
*              diagnostics after a crash.
*
* Copyright: Anki, Inc. 2019
**/

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <signals/simpleSignal.hpp>
#include "switchboardd/taskExecutor.h"

#define LOG_PROCNAME "vic-rescue"

// Forward declarations

namespace Anki {
namespace Switchboard {
  class BleClient;
  class RescueClient;
  class RtsComms;
  class INetworkStream;
} // Switchboard
namespace Vector {
namespace ExternalInterface {
  class MessageEngineToGame;
} // ExternalInterface
} // Vector
} // Anki

// libev
struct ev_loop;
struct ev_timer;

// Class Implementation

namespace Anki {
namespace Switchboard {
enum OtaStatusCode {
  UNKNOWN     = 1,
  IN_PROGRESS = 2,
  COMPLETED   = 3,
  REBOOTING   = 4,
  ERROR       = 5,
};

class RescueDaemon
{
public:
  RescueDaemon(struct ev_loop* loop, int faultCode, int timeout_s); 

  void Start();
  void Stop();

private:
  using EvTimerSignal = Signal::Signal<void ()>;

  const std::string kUpdateEngineEnvPath = "/run/vic-switchboard/update-engine.env";
  const std::string kUpdateEngineDisablePath = "/run/vic-switchboard/disable-update-engine";
  const std::string kUpdateEngineDataPath = "/run/update-engine";
  const std::string kUpdateEngineDonePath = "/run/update-engine/done";
  const std::string kUpdateEngineErrorPath = "/run/update-engine/error";
  const std::string kUpdateEngineExitCodePath = "/run/update-engine/exit_code";
  const std::string kUpdateEngineExecPath = "/anki/bin/update-engine";
  const std::string kUpdateEngineServicePath = "/lib/systemd/system/update-engine.service";

  static void sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents);

  struct ev_loop* _loop;
  std::shared_ptr<TaskExecutor> _taskExecutor;
  std::unique_ptr<BleClient> _bleClient;
  std::shared_ptr<RescueClient> _rescueEngineClient;
  std::unique_ptr<RtsComms> _securePairing;

  Signal::SmartHandle _bleOnConnectedHandle;
  Signal::SmartHandle _bleOnDisconnectedHandle;
  Signal::SmartHandle _bleOnIpcPeerDisconnectedHandle;
  Signal::SmartHandle _receivedPinHandle;
  Signal::SmartHandle _startOtaHandle;
  Signal::SmartHandle _stopPairingHandle;

  ev_timer _rescueTimer;

  int _faultCode = 0;
  uint32_t _rescueTimeout_s = 30;
  bool _centralRequestedDisconnect = false;
  bool _isOtaUpdating = false;
  bool _isUpdateEngineServiceRunning = false;
  const uint8_t kOtaUpdateInterval_s = 3;

  struct ev_TimerStruct {
    ev_timer timer;
    EvTimerSignal* signal;
  } _handleOtaTimer;
  
  EvTimerSignal _otaUpdateTimerSignal;

  void SetAdvertisement();
  void InitializeRescueEngineClient();
  void InitializeBleComms();
  void OnBleConnected(int connId, INetworkStream* stream);
  void OnBleDisconnected(int connId, INetworkStream* stream);
  void OnBleIpcDisconnected(); 
  void RestartRescueTimer();

  void OnPairingStatus(Anki::Vector::ExternalInterface::MessageEngineToGame message);
  void OnReceivedPin(std::string pin);
  void OnStopPairing();
  void OnOtaUpdatedRequest(std::string url);
  void HandleOtaUpdateExit(int rc);
  void HandleOtaUpdateProgress();
  void HandleReboot();
  int GetOtaProgress(uint64_t* progress, uint64_t* expected);

  static void OnRescueTimeout(struct ev_loop* loop, struct ev_timer* w, int revents);

};

} // Switchboard
} // Anki