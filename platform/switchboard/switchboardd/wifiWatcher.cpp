/**
 * File: wifiWatcher.cpp
 *
 * Author: paluri
 * Created: 8/28/2018
 *
 * Description: A watchdog to ensure robot is connected to robot (if possible) and to
 * try to connect if it is not.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "log.h"
#include "util/fileUtils/fileUtils.h"
#include "wifiWatcher.h"

namespace Anki {
namespace Switchboard {

WifiWatcher::WifiWatcher(struct ev_loop* loop)
: _loop(loop),
_enabled(true) {
  _timer.self = this;
  ev_timer_init(&_timer.timer, WatcherTick, 0, kWifiTick_s);
  ev_timer_start(_loop, &_timer.timer);
}

WifiWatcher::~WifiWatcher() {
  ev_timer_stop(_loop, &_timer.timer);
}

void WifiWatcher::Enable() {
  _enabled = true;
}

void WifiWatcher::Disable() {
  _enabled = false;
}

void WifiWatcher::ConnectIfNoWifi() {
  if(!_enabled) {
    Log::Write("WifiWatcher: disabled, doing nothing this tick.");
    return;
  }

  if(Wifi::IsAccessPointMode()) {
    Log::Write("WifiWatcher: access point mode enabled, doing nothing this tick.");
    return;
  }

  if(!HasKnownWifiConfigurations()) {
    Log::Write("WifiWatcher: no known wifi networks, doing nothing this tick.");
    return;
  }

  Anki::Wifi::WiFiState wifiState = Anki::Wifi::GetWiFiState();

  if((wifiState.connState == Anki::Wifi::WiFiConnState::CONNECTED) ||
    (wifiState.connState == Anki::Wifi::WiFiConnState::ONLINE)) {
    // robot is on wifi, so carry on!
    return;
  }

  Log::Write("WifiWatcher: detected no wifi. Scanning for networks...");

  // Scan async ... this won't benefit us this tick, but it will populate the list for next time!
  Anki::Wifi::ScanForWiFiAccessPointsAsync();

  std::vector<Anki::Wifi::WiFiScanResult> results;
  bool shouldScan = false;
  Anki::Wifi::WifiScanErrorCode code = Anki::Wifi::GetWiFiServices(results, shouldScan);

  if(code != Anki::Wifi::WifiScanErrorCode::SUCCESS) {
    // can't scan for wifi
    return;
  }

  for(const Anki::Wifi::WiFiScanResult& result : results) {
    if(result.provisioned) {
      Anki::Wifi::ConnectWifiResult connectResult = 
        Anki::Wifi::ConnectWiFiBySsid(result.ssid, 
                                      "", // don't need pw since already provisioned
                                      (uint8_t)result.auth,
                                      result.hidden,
                                      nullptr,
                                      nullptr);

      if(connectResult == Anki::Wifi::ConnectWifiResult::CONNECT_SUCCESS) {
        // reset error counter
        _connectErrorCount = 0;

        Log::Write("WifiWatcher: Switchboard autoconnected to wifi successfully.");
      } else {
        if(++_connectErrorCount >= kMaxErrorBeforeRestart) {
          // restart wpa_supplicant/connman
          Anki::Wifi::RecoverNetworkServices();

          // reset error counter
          _connectErrorCount = 0;
        }
        
        Log::Write("WifiWatcher: Switchboard failed to autoconnect.");
      }
      break;
    }
  }
}

bool WifiWatcher::HasKnownWifiConfigurations() {
  // Check if any networks are configured
  std::vector<std::string> configs;
  Anki::Util::FileUtils::ListAllDirectories("/data/lib/connman", configs);

  return configs.size() > 0;
}

void WifiWatcher::WatcherTick(struct ev_loop* loop, struct ev_timer* w, int revents) {
  struct ev_WifiTimerStruct* timerStruct = (struct ev_WifiTimerStruct*)w;
  timerStruct->self->ConnectIfNoWifi();
}

} // Switchboard
} // Anki