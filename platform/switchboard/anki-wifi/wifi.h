/**
 * File: wifi.h
 *
 * Author: seichert
 * Created: 1/22/2018
 *
 * Description: Routines for scanning and configuring WiFi
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#pragma once

#include "connmanbus.h"
#include "dbus_wpas.h"
#include "signals/simpleSignal.hpp"

#include <map>
#include <string>
#include <vector>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include "switchboardd/taskExecutor.h"

namespace Anki {
namespace Wifi {

enum WiFiIpFlags : uint8_t {
  NONE     = 0,
  HAS_IPV4 = 1 << 0,
  HAS_IPV6 = 1 << 1,
};

inline WiFiIpFlags operator|(WiFiIpFlags a, WiFiIpFlags b) {
  return static_cast<WiFiIpFlags>(static_cast<int>(a) | static_cast<int>(b));
}

enum WiFiAuth : uint8_t {
      AUTH_NONE_OPEN       = 0,
      AUTH_NONE_WEP        = 1,
      AUTH_NONE_WEP_SHARED = 2,
      AUTH_IEEE8021X       = 3,
      AUTH_WPA_PSK         = 4,
      AUTH_WPA_EAP         = 5,
      AUTH_WPA2_PSK        = 6,
      AUTH_WPA2_EAP        = 7
};

enum WiFiConnState : uint8_t {
  UNKNOWN       = 0,
  ONLINE        = 1,
  CONNECTED     = 2,
  DISCONNECTED  = 3,
};

enum WifiScanErrorCode : uint8_t {
    SUCCESS                   = 0,
    ERROR_GETTING_PROXY       = 100,
    ERROR_SCANNING            = 101,
    FAILED_SCANNING           = 102,
    ERROR_GETTING_MANAGER     = 103,
    ERROR_GETTING_SERVICES    = 104,
    FAILED_GETTING_SERVICES   = 105,
};

enum ConnectWifiResult : uint8_t {
  CONNECT_NONE = 255,
  CONNECT_SUCCESS = 0,
  CONNECT_FAILURE = 1,
  CONNECT_INVALIDKEY = 2,
};

class WiFiScanResult {
 public:
  WiFiAuth    auth;
  bool        encrypted;
  bool        wps;
  uint8_t     signal_level;
  std::string ssid;
  bool        hidden;
  bool        provisioned;
};

class WiFiConfig {
 public:
  WiFiAuth auth;
  bool     hidden;
  std::string ssid; /* hexadecimal representation of ssid name */
  std::string passphrase;
};

struct WiFiState {
  std::string ssid;
  WiFiConnState connState;
};

static const unsigned MAX_NUM_ATTEMPTS = 5;

struct ConnectAsyncData {
  bool completed;
  GCond *cond;
  GError *error;
  GCancellable *cancellable;
  ConnManBusService *service;
};

struct ConnectInfo {
  ConnManBusService* service;
  GCond* cond;
  GError* error;
};

struct WPAConnectInfo {
  const char *name;
  const char *ssid;
  const char *passphrase;

  guint agentId;
  GDBusConnection *connection;
  ConnManBusManager *manager;
  bool errRetry;
  uint8_t retryCount;
  ConnectWifiResult status;
};

Signal::Signal<void(bool, std::string)>& GetWifiChangedSignal();
Signal::Signal<void()>& GetWifiScanCompleteSignal(); 

std::string GetObjectPathForService(GVariant* service);
ConnectWifiResult ConnectToWifiService(ConnManBusService* service);
bool RemoveWifiService(std::string ssid);
bool DisconnectFromWifiService(ConnManBusService* service);
ConnManBusService* GetServiceForPath(std::string objectPath);
std::string GetHexSsidFromServicePath(const std::string& servicePath);

ConnectWifiResult ConnectWiFiBySsid(std::string ssid, std::string pw, uint8_t auth, bool hidden, GAsyncReadyCallback cb, gpointer userData);
WifiScanErrorCode ScanForWiFiAccessPoints(std::vector<WiFiScanResult>& results);
WifiScanErrorCode GetWiFiServices(std::vector<WiFiScanResult>& results, bool scan);
void ScanForWiFiAccessPointsAsync();
std::vector<uint8_t> PackWiFiScanResults(const std::vector<WiFiScanResult>& results);
void HandleOutputCallback(int rc, const std::string& output);
bool GetIpFromHostName(char* hostname, char* ip);
bool IsAccessPointMode();
bool EnableAccessPointMode(std::string ssid, std::string pw);
bool DisableAccessPointMode();
WiFiIpFlags GetIpAddress(uint8_t* ipv4_32bits, uint8_t* ipv6_128bits);
bool GetApMacAddress(uint8_t* mac_48bits);
WiFiState GetWiFiState();
void RecoverNetworkServices();
void WpaSupplicantScan();
void Initialize(std::shared_ptr<TaskExecutor> taskExecutor);
void Deinitialize();

} // namespace Wifi
} // namespace Anki
