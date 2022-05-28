/**
* File: rescueDaemon.cpp
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

#include <linux/reboot.h>
#include <sys/reboot.h>

#include "rescue/rescueDaemon.h"

#include "anki-ble/common/anki_ble_uuids.h"
#include "anki-ble/common/ble_advertise_settings.h"
#include "anki-wifi/exec_command.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "core/lcd.h"
#include "platform/victorCrashReports/victorCrashReporter.h"
#include "rescue/miniFaceDisplay.h"

#include "bleClient/bleClient.h"
#include "switchboardd/rtsComms.h"
#include "switchboardd/taskExecutor.h"
#include "rescue/rescueClient.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"

using namespace Anki::Switchboard;

// ============================================================================================

RescueDaemon::RescueDaemon(struct ev_loop* loop, int faultCode, int timeout_s)
: _loop(loop)
, _taskExecutor(std::make_shared<Anki::TaskExecutor>(_loop))
, _bleClient(nullptr)
, _rescueEngineClient(std::make_shared<RescueClient>())
, _faultCode(faultCode)
, _rescueTimeout_s(timeout_s)
{}

// ============================================================================================

void RescueDaemon::Start()
{
  _rescueTimer.data = this;
  ev_timer_init(&_rescueTimer, OnRescueTimeout, _rescueTimeout_s, 0);
  ev_timer_start(_loop, &_rescueTimer);

  _handleOtaTimer.signal = &_otaUpdateTimerSignal;
  _otaUpdateTimerSignal.SubscribeForever(std::bind(&RescueDaemon::HandleOtaUpdateProgress, this));
  ev_timer_init(&_handleOtaTimer.timer, &RescueDaemon::sEvTimerHandler, kOtaUpdateInterval_s, kOtaUpdateInterval_s);

  InitializeBleComms();
  InitializeRescueEngineClient();

  _rescueEngineClient->StartPairing();
}

// ============================================================================================

void RescueDaemon::Stop()
{
  // kill things and gracefully exit
  Log::Write("Exiting vic-rescue...");
  ev_timer_stop(_loop, &_rescueTimer);
  ev_timer_stop(_loop, &_handleOtaTimer.timer);
  ev_unloop(_loop, EVUNLOOP_ALL);

  Anki::Util::gLoggerProvider = nullptr;
  Anki::Util::gEventProvider = nullptr;

  Anki::Vector::UninstallCrashReporter();
}

// ============================================================================================

void RescueDaemon::SetAdvertisement()
{
  if(_bleClient == nullptr || !_bleClient->IsConnected()) 
  {
    Log::Write("Tried to update BLE advertisement when not connected to ankibluetoothd.");
    return;
  }

  Anki::BLEAdvertiseSettings settings;
  settings.GetAdvertisement().SetServiceUUID(Anki::kAnkiSingleMessageService_128_BIT_UUID);
  settings.GetAdvertisement().SetIncludeDeviceName(true);
  std::vector<uint8_t> mdata = Anki::kAnkiBluetoothSIGCompanyIdentifier;
  mdata.push_back(Anki::kVictorProductIdentifier); // distinguish from future Anki products
  mdata.push_back('p'); // to indicate that we are pairing
  settings.GetAdvertisement().SetManufacturerData(mdata);

  std::string robotName = SavedSessionManager::GetRobotName();
  _bleClient->SetAdapterName(robotName);
  _bleClient->StartAdvertising(settings);
}

// ============================================================================================

void RescueDaemon::InitializeRescueEngineClient() 
{
  _rescueEngineClient->Init();
  _rescueEngineClient->SetFaultCode(_faultCode);
  _rescueEngineClient->OnReceivePairingStatus().SubscribeForever(
    std::bind(&RescueDaemon::OnPairingStatus, this, std::placeholders::_1)
  );
}

// ============================================================================================

void RescueDaemon::InitializeBleComms()
{
  if(_bleClient.get() == nullptr)
  {
    _bleClient = std::make_unique<Anki::Switchboard::BleClient>(_loop);

    _bleOnConnectedHandle = _bleClient->OnConnectedEvent().ScopedSubscribe(
      std::bind(&RescueDaemon::OnBleConnected, this, std::placeholders::_1, std::placeholders::_2));

    _bleOnIpcPeerDisconnectedHandle = _bleClient->OnIpcDisconnection().ScopedSubscribe(
      std::bind(&RescueDaemon::OnBleIpcDisconnected, this));
  }

  while(!_bleClient->IsConnected())
  {
    (void)_bleClient->Connect();

    if(_bleClient->IsConnected())
    {
      Log::Write("Ble IPC client connected.");
      SetAdvertisement();
    }
    else
    {
      Log::Write("Failed to connect to ankibluetoothd ... trying again.");
    }

    ev_loop(_loop, EVRUN_ONCE);
  }
}

// ============================================================================================

void RescueDaemon::RestartRescueTimer()
{
  // Restart timeout -- if no BLE connection in 30 seconds, Stop()
  ev_timer_stop(_loop, &_rescueTimer);
  ev_timer_set(&_rescueTimer, _rescueTimeout_s, 0);
  ev_timer_start(_loop, &_rescueTimer);
}

// ============================================================================================

void RescueDaemon::OnBleConnected(int connId, INetworkStream* stream)
{
  Log::Write("A BLE central connected to us.");

  // Listen to disconnection event
  // fixme: this could introduce a race condition where disconnection is missed
  _bleOnDisconnectedHandle = _bleClient->OnDisconnectedEvent().ScopedSubscribe(
  std::bind(&RescueDaemon::OnBleDisconnected, this, std::placeholders::_1, std::placeholders::_2));

  // Stop Rescue timeout
  ev_timer_stop(_loop, &_rescueTimer);

  // If we receive second connection, ignore
  if(_securePairing != nullptr) {
    Log::Write("Ignoring second BLE connection.");
    return;
  }
  
  // Instantiate RtsComms
  _securePairing = std::make_unique<RtsComms>(
    stream,               // network stream
    _loop,                // ev loop
    _rescueEngineClient,  // engineClient
    nullptr,              // gatewayServer
    nullptr,              // tokenClient
    nullptr,              // connectionIdManager
    nullptr,              // wifiWatcher
    _taskExecutor,
    true,                 // is pairing
    _isOtaUpdating,
    true);               // has cloud owner

  // Subscribe to events
  _startOtaHandle = _securePairing->OnOtaUpdateRequestEvent().ScopedSubscribe(
    std::bind(&RescueDaemon::OnOtaUpdatedRequest, this, std::placeholders::_1));
  _stopPairingHandle = _securePairing->OnStopPairingEvent().ScopedSubscribe(
    std::bind(&RescueDaemon::OnStopPairing, this));
  _receivedPinHandle = _securePairing->OnUpdatedPinEvent().ScopedSubscribe(
    std::bind(&RescueDaemon::OnReceivedPin, this, std::placeholders::_1)
  );

  // Begin pairing process
  _securePairing->BeginPairing();
}

// ============================================================================================ 

void RescueDaemon::OnOtaUpdatedRequest(std::string url) {
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
                            std::bind(&RescueDaemon::HandleOtaUpdateExit, this, std::placeholders::_1));
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

// ============================================================================================

void RescueDaemon::HandleOtaUpdateExit(int rc) {
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
        _rescueEngineClient->ShowPairingStatus(Anki::Vector::SwitchboardInterface::ConnectionStatus::END_PAIRING);
      }
    }
  });
}

// ============================================================================================

void RescueDaemon::HandleOtaUpdateProgress() {
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

// ============================================================================================

int RescueDaemon::GetOtaProgress(uint64_t* progressVal, uint64_t* expectedVal) {
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

// ============================================================================================

void RescueDaemon::HandleReboot() {
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

// ============================================================================================

void RescueDaemon::OnBleDisconnected(int connId, INetworkStream* stream)
{
  Log::Write("A BLE central disconnected from us.");

  if(_securePairing != nullptr)
  {
    _securePairing->StopPairing();
    _receivedPinHandle = nullptr;
    _startOtaHandle = nullptr;
    _stopPairingHandle = nullptr;
    _securePairing = nullptr;
  }

  if(!_centralRequestedDisconnect)
  {
    RestartRescueTimer();
  }
}

// ============================================================================================

void RescueDaemon::OnBleIpcDisconnected()
{
  Log::Write("Disconnected from ankibluetoothd.");
}

// ============================================================================================

void RescueDaemon::OnPairingStatus(Anki::Vector::ExternalInterface::MessageEngineToGame message)
{
  Anki::Vector::ExternalInterface::MessageEngineToGameTag tag = message.GetTag();

  switch(tag)
  {
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::EnterPairing:
    {
      _rescueEngineClient->ShowPairingStatus(
        Anki::Vector::SwitchboardInterface::ConnectionStatus::SHOW_PRE_PIN
      );
      break;
    }
    case Anki::Vector::ExternalInterface::MessageEngineToGameTag::ExitPairing:
    {
      break;
    }
    default: {
      Log::Write("Unknown message from RescueEngineClient: %hhu\n", tag);
      break;
    }
  }
}

// ============================================================================================

void RescueDaemon::OnReceivedPin(std::string pin)
{
  _rescueEngineClient->SetPairingPin(pin);
  _rescueEngineClient->ShowPairingStatus(
    Anki::Vector::SwitchboardInterface::ConnectionStatus::SHOW_PIN
  );
  Log::Blue((" " + pin + " ").c_str());
}

// ============================================================================================

void RescueDaemon::OnStopPairing()
{
  _centralRequestedDisconnect = true;

  Stop();
}

// ============================================================================================

void RescueDaemon::OnRescueTimeout(struct ev_loop* loop, struct ev_timer* w, int revents)
{
  RescueDaemon* rescue = (RescueDaemon*)w->data;
  rescue->Stop();
}

// ============================================================================================

void RescueDaemon::sEvTimerHandler(struct ev_loop* loop, struct ev_timer* w, int revents)
{
  struct ev_TimerStruct *wData = (struct ev_TimerStruct*)w;
  wData->signal->emit();
}

// ============================================================================================

namespace
{ 
  // private members
  static struct ev_loop* sLoop;
  static struct ev_signal sSigInt;
  static struct ev_signal sSigTerm;
  static std::unique_ptr<Anki::Switchboard::RescueDaemon> sDaemon;
  static const int kDefaultFaultCode = 1000;
  static const int kDefaultTimeout_s = 30;
}

// ============================================================================================

static void OnSignalCallback(struct ev_loop* loop, struct ev_signal* w, int revents)
{
  Log::Write("Exiting for signal: %d", w->signum);

  if(sDaemon != nullptr) {
    sDaemon->Stop();
  }
}

// ============================================================================================

bool parse_args(int argc, char** argv, int* faultCode, int* timeout)
{
  int opt, value;
  
  while((opt = getopt(argc, argv, "ht:c:")) != -1)
  {
    switch(opt)
    {
      case 'h':
        printf("Options:\n");
        printf("  -t N   [N is positive integer] Set the timeout (sec) for vic-rescue to wait for BLE connection before exiting.\n\n");
        printf("  -c N   [N is positive integer] Set the fault code to draw on screen while vic-rescue is connected over BLE.\n\n");

        exit(0);
        break;
      case 't':
        // timeout 
        errno = 0;
        value = (int)std::strtol(optarg, nullptr, 10);

        if( (value > 0) && (errno == 0) )
        {
          *timeout = value;
        }
        else 
        {
          return false;
        }
        break;
      case 'c':
        // fault code
        errno = 0;
        value = (int)std::strtol(optarg, nullptr, 10); 

        if( (value > 0) && (errno == 0) )
        { 
          *faultCode = value;
        }
        else
        {
          return false;
        }
        break;
      default:
        // unknown option, ignore
        break;
    }
  }

  return true;
}

int main(int argc, char** argv)
{
  // logging initialization
  setAndroidLoggingTag("vic-rescue");
  Log::Write("Loading up vic-rescue");

  Anki::Vector::InstallCrashReporter(LOG_PROCNAME);

  Anki::Util::VictorLogger logger(LOG_PROCNAME);
  Anki::Util::gLoggerProvider = &logger;
  Anki::Util::gEventProvider = &logger;

  DASMSG(rescue_hello, "vic-rescue.hello", "vic-rescue started");
  DASMSG_SEND();

  // get main loop
  sLoop = ev_default_loop(0);

  // init lcd
  int rc = lcd_init();
  if (rc != 0)
  {
    Log::Write("Failed to init LCD.");
    return rc;
  }

  // listen to sigint and sigterm
  ev_signal_init(&sSigInt, OnSignalCallback, SIGINT);
  ev_signal_start(sLoop, &sSigInt);
  ev_signal_init(&sSigTerm, OnSignalCallback, SIGTERM);
  ev_signal_start(sLoop, &sSigTerm);

  // set default options
  int fault = kDefaultFaultCode;
  int timeout_s = kDefaultTimeout_s;

  // parse options and override defaults if exist
  if(!parse_args(argc, argv, &fault, &timeout_s)) {
    // error occured while parsing
    Log::Write("Args '-t' (timeout seconds) and '-c' (fault code) must be positive integer values.");
    return -1;
  }

  // initialize daemon
  sDaemon = std::make_unique<Anki::Switchboard::RescueDaemon>(sLoop, fault, timeout_s);
  sDaemon->Start();

  // enter loop
  ev_loop(sLoop, 0);

  // exit
  return 0;
}