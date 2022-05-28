/**
 * File: wifi.cpp
 *
 * Author: seichert
 * Created: 1/22/2018
 *
 * Description: Routines for scanning and configuring WiFi
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <netdb.h> //hostent
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <ifaddrs.h>
#include "anki-ble/common/stringutils.h"
#include "log.h"
#include "wifi.h"
#include "exec_command.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "util/logging/logging.h"
#include "util/logging/DAS.h"

namespace Anki {
namespace Wifi {
static GMutex connectMutex;
static const char* const agentPath = "/tmp/vic_switchboard/connman_agent";
static const char* WIFI_DEVICE = "wlan0";
static GMainLoop* gLoop = nullptr;

static gpointer ConnectionThread(gpointer data);
static std::shared_ptr<TaskExecutor> sTaskExecutor;
static Signal::Signal<void(bool, std::string)> sWifiChangedSignal;
static Signal::Signal<void()> sWifiScanCompleteSignal;

static std::time_t sLastWifiConnect_tm;
static std::time_t sLastWifiDisconnect_tm;

static void AgentCallback(GDBusConnection *connection,
                      const gchar *sender,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *method_name,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer user_data);

GDBusInterfaceVTable agentVtable = {
  .method_call = AgentCallback,
};

// Introspection data for the service we are exporting
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='net.connman.Agent'>"
  "    <method name='RequestInput'>"
  "      <arg type='o' name='service' direction='in'/>"
  "      <arg type='a{sv}' name='fields' direction='in'/>"
  "      <arg type='a{sv}' name='input' direction='out'/>"
  "    </method>"
  "    <method name='ReportError'>"
  "      <arg type='o' name='service' direction='in'/>"
  "      <arg type='s' name='error' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

Signal::Signal<void(bool, std::string)>& GetWifiChangedSignal() {
  return sWifiChangedSignal;
}

Signal::Signal<void()>& GetWifiScanCompleteSignal() {
  return sWifiScanCompleteSignal;
}

void OnTechnologyChanged (GDBusConnection *connection,
                        const gchar *sender_name,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data) {
  GVariant* nameChild = g_variant_get_child_value(parameters, 0);
  const char* propertyName = g_variant_get_string(nameChild, nullptr);
  const char MAC_BYTES = 6;
  const char MAC_MANUFAC_BYTES = 3;

  g_variant_unref(nameChild);

  if(!g_str_equal(propertyName, "Connected")) {
    // Not the property we care about.
    return;
  }

  GVariant* valueChild = g_variant_get_child_value(parameters, 1);
  bool connected = g_variant_get_boolean(g_variant_get_variant(valueChild));
  double duration_s = 0;

  std::time_t t = std::time(0);

  if(connected) {
    sLastWifiConnect_tm = t;
  } else {
    sLastWifiDisconnect_tm = t;
  }

  std::string connectionStatus = connected?"Connected.":"Disconnected.";

  uint8_t apMac[MAC_BYTES];
  bool hasMac = GetApMacAddress(apMac);

  std::string apMacManufacturerBytes = "";

  if(hasMac) {
    // Strip ap MAC of last three bytes
    for(int i = 0; i < MAC_MANUFAC_BYTES; i++) {
      std::stringstream ss;
      ss << std::setfill('0') << std::setw(2) << std::hex << (int)apMac[i];
      apMacManufacturerBytes += ss.str();
    }
  }

  sTaskExecutor->Wake([connected, apMacManufacturerBytes](){
    sWifiChangedSignal.emit(connected, apMacManufacturerBytes);
  });

  Log::Write("WiFi connection status changed: [connected=%s / mac=%s]",
    connected?"true":"false", apMacManufacturerBytes.c_str());

  std::string event = connected?"wifi.connection":"wifi.disconnection";

  duration_s = std::difftime(t, (connected? sLastWifiDisconnect_tm : sLastWifiConnect_tm));

  DASMSG(wifi_connection_status, event,
          "WiFi connection status changed.");
  DASMSG_SET(i1, (int)duration_s, "Seconds from last connect/disconnect");
  DASMSG_SET(s4, apMacManufacturerBytes, "AP MAC manufacturer bytes");
  DASMSG_SEND();

  if(connected) {
    sLastWifiConnect_tm = t;
  } else {
    sLastWifiDisconnect_tm = t;
  }

  g_variant_unref(valueChild);
}

void Initialize(std::shared_ptr<TaskExecutor> taskExecutor) {
  GError* error = nullptr;

  static GThread *thread2 = g_thread_new("init_thread", ConnectionThread, nullptr);

  if (thread2 == nullptr) {
    Log::Write("couldn't spawn init thread");
    return;
  }

  GDBusConnection* gdbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM,
                              nullptr,
                              &error);

  // Initialize wifi time trackers
  std::time_t t = std::time(0);
  sLastWifiConnect_tm = t;
  sLastWifiDisconnect_tm = t;

  guint handle = g_dbus_connection_signal_subscribe (gdbusConn,
    "net.connman",
    "net.connman.Technology",
    "PropertyChanged",          //member
    nullptr,                    //obj path
    nullptr,                    //arg0
    G_DBUS_SIGNAL_FLAGS_NONE,   //flags
    OnTechnologyChanged,        //callback
    nullptr,                    //user_data
    nullptr);                   //DestroyNotify

  (void)handle;

  sTaskExecutor = taskExecutor;
}

void Deinitialize() {
  // Exit our glib event listener thread so we don't
  // have our AgentCallback and OnTechnologyChanged 
  // methods called when we are dying.
  if(gLoop) {
    g_main_loop_quit(gLoop);
  }

  sTaskExecutor = nullptr;
}

void ScanCallback(GObject *source_object, GAsyncResult *result, gpointer user_data)
{
  sTaskExecutor->Wake([](){
    sWifiScanCompleteSignal.emit();
  });
}

WifiScanErrorCode ScanForWiFiAccessPoints(std::vector<WiFiScanResult>& results) {
  bool doScan = true;
  return GetWiFiServices(results, doScan);
}

WifiScanErrorCode GetWiFiServices(std::vector<WiFiScanResult>& results, bool scan) {
  results.clear();

  bool disabledApMode = DisableAccessPointMode();
  if(disabledApMode) {
    Log::Write("Disabled AccessPoint mode.");
  }

  GError* error = nullptr;
  gboolean success;

  if(scan) {
    ConnManBusTechnology* tech_proxy;
    tech_proxy = conn_man_bus_technology_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            "net.connman",
                                                            "/net/connman/technology/wifi",
                                                            nullptr,
                                                            &error);
    if (error) {
      loge("error getting proxy for net.connman /net/connman/technology/wifi");
      DASMSG(connman_error, "connman.error.technology_proxy", "Connman error.");
      DASMSG_SET(s1, DASMSG_ESCAPE(error->message), "Error message");
      DASMSG_SEND();

      g_error_free(error);
      return WifiScanErrorCode::ERROR_GETTING_PROXY;
    }

    success = conn_man_bus_technology_call_scan_sync(tech_proxy,
                                                              nullptr,
                                                              &error);
    g_object_unref(tech_proxy);
    if (error) {
      loge("error asking connman to scan for wifi access points [%s]", error->message);
      DASMSG(connman_error, "connman.error.call_scan", "Connman error.");
      DASMSG_SET(s1, DASMSG_ESCAPE(error->message), "Error message");
      DASMSG_SEND();

      RecoverNetworkServices();

      g_error_free(error);
      return WifiScanErrorCode::ERROR_SCANNING;
    }

    if (!success) {
      loge("connman failed to scan for wifi access points");
      return WifiScanErrorCode::FAILED_SCANNING;
    }
  }

  ConnManBusManager* manager_proxy;
  manager_proxy = conn_man_bus_manager_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/",
                                                              nullptr,
                                                              &error);
  if (error) {
    loge("error getting proxy for net.connman /");
    g_error_free(error);
    return WifiScanErrorCode::ERROR_GETTING_MANAGER;
  }

  GVariant* services = nullptr;
  success = conn_man_bus_manager_call_get_services_sync(manager_proxy,
                                                        &services,
                                                        nullptr,
                                                        &error);
  g_object_unref(manager_proxy);
  if (error) {
    loge("Error getting services from connman");
    return WifiScanErrorCode::ERROR_GETTING_SERVICES;
  }

  if (!success) {
    loge("connman failed to get list of services");
    return WifiScanErrorCode::FAILED_GETTING_SERVICES;
  }

  // Get hidden flag
  std::string configSsid = "";
  std::string fieldString = "Hidden";
  bool configIsHidden = false;

  for (gsize i = 0 ; i < g_variant_n_children(services); i++) {
    WiFiScanResult result{WiFiAuth::AUTH_NONE_OPEN, false, false, 0, "", false, false};
    GVariant* child = g_variant_get_child_value(services, i);
    GVariant* attrs = g_variant_get_child_value(child, 1);
    bool type_is_wifi = false;
    bool iface_is_wlan0 = false;

    for (gsize j = 0 ; j < g_variant_n_children(attrs); j++) {
      GVariant* attr = g_variant_get_child_value(attrs, j);
      GVariant* key_v = g_variant_get_child_value(attr, 0);
      GVariant* val_v = g_variant_get_child_value(attr, 1);
      GVariant* val = g_variant_get_variant(val_v);
      const char* key = g_variant_get_string(key_v, nullptr);

      // Make sure this is a wifi service and not something else
      if (g_str_equal(key, "Type")) {
        if (g_str_equal(g_variant_get_string(val, nullptr), "wifi")) {
          type_is_wifi = true;
        } else {
          type_is_wifi = false;
          g_variant_unref(attr);
          g_variant_unref(key_v);
          g_variant_unref(val_v);
          g_variant_unref(val);
          break;
        }
      }

      // Make sure this is for the wlan0 interface and not p2p0
      if (g_str_equal(key, "Ethernet")) {
        for (gsize k = 0 ; k < g_variant_n_children(val); k++) {
          GVariant* ethernet_attr = g_variant_get_child_value(val, k);
          GVariant* ethernet_key_v = g_variant_get_child_value(ethernet_attr, 0);
          GVariant* ethernet_val_v = g_variant_get_child_value(ethernet_attr, 1);
          GVariant* ethernet_val = g_variant_get_variant(ethernet_val_v);
          const char* ethernet_key = g_variant_get_string(ethernet_key_v, nullptr);
          if (g_str_equal(ethernet_key, "Interface")) {
            if (g_str_equal(g_variant_get_string(ethernet_val, nullptr), WIFI_DEVICE)) {
              iface_is_wlan0 = true;
            } else {
              iface_is_wlan0 = false;
              g_variant_unref(ethernet_attr);
              g_variant_unref(ethernet_key_v);
              g_variant_unref(ethernet_val_v);
              g_variant_unref(ethernet_val);
              break;
            }
          }

          g_variant_unref(ethernet_attr);
          g_variant_unref(ethernet_key_v);
          g_variant_unref(ethernet_val_v);
          g_variant_unref(ethernet_val);
        }
      }

      if (g_str_equal(key, "Strength")) {
        result.signal_level = (uint8_t)g_variant_get_byte(val);
      }

      if (g_str_equal(key, "Security")) {
        for (gsize k = 0 ; k < g_variant_n_children(val); k++) {
          GVariant* security_val = g_variant_get_child_value(val, k);
          const char* security_val_str = g_variant_get_string(security_val, nullptr);
          if (g_str_equal(security_val_str, "wps")) {
            result.wps = true;
          }
          if (g_str_equal(security_val_str, "none")) {
            result.auth = WiFiAuth::AUTH_NONE_OPEN;
            result.encrypted = false;
          }
          if (g_str_equal(security_val_str, "wep")) {
            result.auth = WiFiAuth::AUTH_NONE_WEP;
            result.encrypted = true;
          }
          if (g_str_equal(security_val_str, "ieee8021x")) {
            result.auth = WiFiAuth::AUTH_IEEE8021X;
            result.encrypted = true;
          }
          if (g_str_equal(security_val_str, "psk")) {
            result.auth = WiFiAuth::AUTH_WPA2_PSK;
            result.encrypted = true;
          }

          g_variant_unref(security_val);
        }
      }

      if (g_str_equal(key, "Favorite")) {
        result.provisioned = g_variant_get_boolean(val);
      }

      // free
      g_variant_unref(attr);
      g_variant_unref(key_v);
      g_variant_unref(val_v);
      g_variant_unref(val);
    }

    if (type_is_wifi && iface_is_wlan0) {
      result.ssid = GetHexSsidFromServicePath(GetObjectPathForService(child));

      if(result.ssid == configSsid) {
        result.hidden = configIsHidden;
      }

      results.push_back(result);
    }

    // free child and attrs
    g_variant_unref(attrs);
    g_variant_unref(child);
  }

  // free services
  g_variant_unref(services);

  return WifiScanErrorCode::SUCCESS;
}

void ScanForWiFiAccessPointsAsync() {
  bool disabledApMode = DisableAccessPointMode();
  if(disabledApMode) {
    Log::Write("Disabled AccessPoint mode.");
  }

  ConnManBusTechnology* tech_proxy;
  GError* error;

  error = nullptr;
  tech_proxy = conn_man_bus_technology_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/net/connman/technology/wifi",
                                                              nullptr,
                                                              &error);
  if (error) {
    loge("error getting proxy for net.connman /net/connman/technology/wifi");
    g_error_free(error);
    return;
  }

  conn_man_bus_technology_call_scan (tech_proxy,
    nullptr,
    ScanCallback,
    nullptr);
}

void HandleOutputCallback(int rc) {
  // noop
}

static gpointer ConnectionThread(gpointer data)
{
  gLoop = g_main_loop_new(NULL, true);

  if (!gLoop) {
      loge("error getting main loop");
      return nullptr;
  }

  g_main_loop_run(gLoop);
  g_main_loop_unref(gLoop);
  return nullptr;
}

void ConnectCallback(GObject *source_object, GAsyncResult *result, gpointer user_data)
{
  g_mutex_lock(&connectMutex);
  struct ConnectInfo* data = (ConnectInfo*)user_data;

  conn_man_bus_service_call_connect_finish(data->service,
                                          result,
                                          &data->error);

  g_cond_signal(data->cond);
  g_mutex_unlock(&connectMutex);
}

static void AgentCallback(GDBusConnection *connection,
                      const gchar *sender,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *method_name,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer user_data) {
  struct WPAConnectInfo *wpaConnectInfo = (struct WPAConnectInfo *)user_data;

  if (strcmp(object_path, agentPath)) {
    return; // not us
  }

  if (strcmp(interface_name, "net.connman.Agent")) {
    return; // not our Interface
  }

  if (!strcmp(method_name, "RequestInput")) {
    // Ok, let's provide the input
    gchar *obj;
    GVariant *dict;

    g_variant_get(parameters, "(oa{sv})", &obj, &dict);
    logi("%s: object %s", __func__, obj);

    GVariantBuilder *dict_builder = g_variant_builder_new(G_VARIANT_TYPE("(a{sv})"));
    g_variant_builder_open(dict_builder, G_VARIANT_TYPE("a{sv}"));
    if (wpaConnectInfo->name) {
      logi("%s: found 'Name'", __func__);
      g_variant_builder_add(dict_builder, "{sv}", "Name", g_variant_new_string(wpaConnectInfo->name));
    }
    if (wpaConnectInfo->ssid) {
      logi("%s: found 'SSID'", __func__);
      g_variant_builder_add(dict_builder, "{sv}", "SSID", g_variant_new_bytestring(wpaConnectInfo->ssid));
    }
    if (wpaConnectInfo->passphrase) {
      logi("%s: found 'Passphrase'", __func__);
      g_variant_builder_add(dict_builder, "{sv}", "Passphrase", g_variant_new_string(wpaConnectInfo->passphrase));
    }
    
    g_variant_builder_close(dict_builder);

    GVariant *response = g_variant_builder_end(dict_builder);
    g_variant_builder_unref(dict_builder);

    g_dbus_method_invocation_return_value(invocation, response);
  }

  if (!strcmp(method_name, "ReportError")) {
    gchar *obj;
    gchar *err;

    g_variant_get(parameters, "(os)", &obj, &err);

    if (!strcmp(err, "invalid-key")) {
      wpaConnectInfo->status = ConnectWifiResult::CONNECT_INVALIDKEY;
      g_dbus_method_invocation_return_value(invocation, NULL);
      return;
    }

    if(++wpaConnectInfo->retryCount < MAX_NUM_ATTEMPTS) {
      Log::Write("Connection Error: Retrying");
      g_dbus_method_invocation_return_dbus_error(invocation, "net.connman.Agent.Error.Retry", "");
    }
  }
}

bool RegisterAgent(struct WPAConnectInfo *wpaConnectInfo) {
  GError *error = nullptr;

  GDBusConnection *gdbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM,
                                              nullptr,
                                              &error);

  static GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);

  if (!introspection_data) {
    loge("error getting introspection data: %s", error->message);
    return false;
  }

  ConnManBusManager *manager;
  manager = conn_man_bus_manager_proxy_new_sync(gdbusConn,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "net.connman",
                                                "/",
                                                nullptr,
                                                &error);
  if (error) {
    loge("error getting manager");
    return false;
  }

  guint agentId = 0;
  agentId = g_dbus_connection_register_object(gdbusConn,
                                              agentPath,
                                              introspection_data->interfaces[0],
                                              &agentVtable,
                                              wpaConnectInfo,
                                              nullptr,
                                              &error);
  if (agentId == 0 || error != NULL) {
    loge("Error registering agent object");
    return false;
  }

  if (!conn_man_bus_manager_call_register_agent_sync(manager,
                                                     agentPath,
                                                     nullptr,
                                                     &error)) {
    g_dbus_connection_unregister_object(gdbusConn, agentId);
    loge("error registering agent");
    return false;
  }

  wpaConnectInfo->agentId = agentId;
  wpaConnectInfo->connection = gdbusConn;
  wpaConnectInfo->manager = manager;
  wpaConnectInfo->retryCount = 0;

  return true;
}

bool UnregisterAgent(struct WPAConnectInfo *wpaConnectInfo) {
  GError *error = nullptr;

  if (!wpaConnectInfo) {
    return false;
  }

  conn_man_bus_manager_call_unregister_agent_sync (wpaConnectInfo->manager,
                                                   agentPath,
                                                   nullptr,
                                                   &error);

  if (error) {
    g_error_free(error);
    return false;
  }

  bool unreg = g_dbus_connection_unregister_object(wpaConnectInfo->connection,
                                      wpaConnectInfo->agentId);

  if(!unreg) {
    loge("!!!! could not unregister object");
  }

  g_object_unref(wpaConnectInfo->manager);
  g_object_unref(wpaConnectInfo->connection);
  return true;
}

bool RemoveWifiService(std::string ssid) {
  GError* error;
  bool success;

  std::string nameFromHex = hexStringToAsciiString(ssid);

  error = nullptr;

  ConnManBusManager* manager_proxy;
  manager_proxy = conn_man_bus_manager_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/",
                                                              nullptr,
                                                              &error);
  if (error) {
    loge("error getting proxy for net.connman /");
    g_error_free(error);
    return false;
  }

  GVariant* services = nullptr;
  success = conn_man_bus_manager_call_get_services_sync(manager_proxy,
                                                        &services,
                                                        nullptr,
                                                        &error);

  g_object_unref(manager_proxy);
  if (error) {
    loge("Error getting services from connman");
    g_error_free(error);
    return false;
  }

  if (!success) {
    loge("connman failed to get list of services");
    return false;
  }

  GVariant* serviceVariant = nullptr;
  bool foundService = false;

  for (gsize i = 0 ; i < g_variant_n_children(services); i++) {
    if(foundService) {
      break;
    }

    GVariant* child = g_variant_get_child_value(services, i);
    GVariant* attrs = g_variant_get_child_value(child, 1);

    bool hasName = false;
    bool matchedName = false;
    bool matchedInterface = false;
    bool matchedType = false;

    for (gsize j = 0 ; j < g_variant_n_children(attrs); j++) {
      GVariant* attr = g_variant_get_child_value(attrs, j);
      GVariant* key_v = g_variant_get_child_value(attr, 0);
      GVariant* val_v = g_variant_get_child_value(attr, 1);
      GVariant* val = g_variant_get_variant(val_v);
      const char* key = g_variant_get_string(key_v, nullptr);

      if(g_str_equal(key, "Name")) {
        if(std::string(g_variant_get_string(val, nullptr)) == nameFromHex) {
          matchedName = true;
        } else {
          matchedName = false;
        }
        hasName = true;
      }

      if(g_str_equal(key, "Type")) {
        if (g_str_equal(g_variant_get_string(val, nullptr), "wifi")) {
          matchedType = true;
        } else {
          matchedType = false;
        }
      }

      // Make sure this is for the wlan0 interface and not p2p0
      if (g_str_equal(key, "Ethernet")) {
        for (gsize k = 0 ; k < g_variant_n_children(val); k++) {
          GVariant* ethernet_attr = g_variant_get_child_value(val, k);
          GVariant* ethernet_key_v = g_variant_get_child_value(ethernet_attr, 0);
          GVariant* ethernet_val_v = g_variant_get_child_value(ethernet_attr, 1);
          GVariant* ethernet_val = g_variant_get_variant(ethernet_val_v);
          const char* ethernet_key = g_variant_get_string(ethernet_key_v, nullptr);
          if (g_str_equal(ethernet_key, "Interface")) {
            if (g_str_equal(g_variant_get_string(ethernet_val, nullptr), WIFI_DEVICE)) {
              matchedInterface = true;
            } else {
              matchedInterface = false;
              g_variant_unref(ethernet_attr);
              g_variant_unref(ethernet_key_v);
              g_variant_unref(ethernet_val_v);
              g_variant_unref(ethernet_val);
              break;
            }
          }

          g_variant_unref(ethernet_attr);
          g_variant_unref(ethernet_key_v);
          g_variant_unref(ethernet_val_v);
          g_variant_unref(ethernet_val);
        }
      }

      g_variant_unref(attr);
      g_variant_unref(key_v);
      g_variant_unref(val_v);
      g_variant_unref(val);
    }

    if(matchedName && matchedInterface && matchedType) {
      // this is our service
      serviceVariant = child;
      foundService = true;
    }

    g_variant_unref(attrs);
    if(child != serviceVariant) {
      g_variant_unref(child);
    }
  }

  if(!foundService) {
    loge("Could not find service...");
    g_variant_unref(services);
    return false;
  }

  std::string servicePath = GetObjectPathForService(serviceVariant);
  Log::Write("Removing %s.", servicePath.c_str());

  // Get the ConnManBusService for our object path
  Log::Write("Service path: %s", servicePath.c_str());
  ConnManBusService* service = GetServiceForPath(servicePath);
  if(service == nullptr) {
    g_variant_unref(services);
    g_variant_unref(serviceVariant);
    return false;
  }

  success = conn_man_bus_service_call_remove_sync(
    service,
    nullptr,
    &error);

  g_object_unref(service);
  g_variant_unref(services);
  g_variant_unref(serviceVariant);

  return success && !error;
}

ConnectWifiResult ConnectWiFiBySsid(std::string ssid, std::string pw, uint8_t auth, bool hidden, GAsyncReadyCallback cb, gpointer userData) {
  GError* error;
  bool success;

  std::string nameFromHex = hexStringToAsciiString(ssid);

  error = nullptr;

  if (error) {
    loge("error getting proxy for net.connman /net/connman/technology/wifi");
    g_error_free(error);
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  ConnManBusManager* manager_proxy;
  manager_proxy = conn_man_bus_manager_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/",
                                                              nullptr,
                                                              &error);
  if (error) {
    loge("error getting proxy for net.connman /");
    g_error_free(error);
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  GVariant* services = nullptr;
  success = conn_man_bus_manager_call_get_services_sync(manager_proxy,
                                                        &services,
                                                        nullptr,
                                                        &error);

  g_object_unref(manager_proxy);
  if (error) {
    loge("Error getting services from connman");
    g_error_free(error);
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  if (!success) {
    loge("connman failed to get list of services");
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  GVariant* serviceVariant = nullptr;
  GVariant* currentServiceVariant = nullptr;
  bool foundService = false;

  for (gsize i = 0 ; i < g_variant_n_children(services); i++) {
    if(foundService) {
      break;
    }

    GVariant* child = g_variant_get_child_value(services, i);
    GVariant* attrs = g_variant_get_child_value(child, 1);

    bool hasName = false;
    bool matchedName = false;
    bool matchedInterface = false;
    bool matchedType = false;
    bool serviceOnline = false;

    for (gsize j = 0 ; j < g_variant_n_children(attrs); j++) {
      GVariant* attr = g_variant_get_child_value(attrs, j);
      GVariant* key_v = g_variant_get_child_value(attr, 0);
      GVariant* val_v = g_variant_get_child_value(attr, 1);
      GVariant* val = g_variant_get_variant(val_v);
      const char* key = g_variant_get_string(key_v, nullptr);

      if(g_str_equal(key, "Name")) {
        if(std::string(g_variant_get_string(val, nullptr)) == nameFromHex) {
          matchedName = true;
        } else {
          matchedName = false;
        }
        hasName = true;
      }

      if(g_str_equal(key, "Type")) {
        if (g_str_equal(g_variant_get_string(val, nullptr), "wifi")) {
          matchedType = true;
        } else {
          matchedType = false;
        }
      }

      // Make sure this is for the wlan0 interface and not p2p0
      if (g_str_equal(key, "Ethernet")) {
        for (gsize k = 0 ; k < g_variant_n_children(val); k++) {
          GVariant* ethernet_attr = g_variant_get_child_value(val, k);
          GVariant* ethernet_key_v = g_variant_get_child_value(ethernet_attr, 0);
          GVariant* ethernet_val_v = g_variant_get_child_value(ethernet_attr, 1);
          GVariant* ethernet_val = g_variant_get_variant(ethernet_val_v);
          const char* ethernet_key = g_variant_get_string(ethernet_key_v, nullptr);
          if (g_str_equal(ethernet_key, "Interface")) {
            if (g_str_equal(g_variant_get_string(ethernet_val, nullptr), "wlan0")) {
              matchedInterface = true;
            } else {
              matchedInterface = false;
              g_variant_unref(ethernet_attr);
              g_variant_unref(ethernet_key_v);
              g_variant_unref(ethernet_val_v);
              g_variant_unref(ethernet_val);
              break;
            }
          }

          g_variant_unref(ethernet_attr);
          g_variant_unref(ethernet_key_v);
          g_variant_unref(ethernet_val_v);
          g_variant_unref(ethernet_val);
        }
      }

      if (g_str_equal(key, "State")) {
        if (g_str_equal(g_variant_get_string(val, nullptr), "online") ||
            g_str_equal(g_variant_get_string(val, nullptr), "ready") ) {
          currentServiceVariant = child;
          serviceOnline = true;
        }
      }

      g_variant_unref(attr);
      g_variant_unref(key_v);
      g_variant_unref(val_v);
      g_variant_unref(val);
    }

    if(matchedName && matchedInterface && matchedType) {
      // this is our service
      serviceVariant = child;
      foundService = true;

      if(serviceOnline) {
        // early out--we are already connected!
        return ConnectWifiResult::CONNECT_SUCCESS;
      }
    } else if(hidden && !hasName) {
      serviceVariant = child;
      foundService = true;
    }

    g_variant_unref(attrs);
    if(child != serviceVariant && child != currentServiceVariant) {
      g_variant_unref(child);
    }
  }

  if(!foundService) {
    loge("Could not find service...");
    g_variant_unref(services);
    if(currentServiceVariant != nullptr) {
      g_variant_unref(currentServiceVariant);
    }
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  std::string servicePath = GetObjectPathForService(serviceVariant);
  Log::Write("Initiating connection to %s.", servicePath.c_str());

  // before connecting, lets disconnect from our current different network
  if(currentServiceVariant != nullptr) {
    // we have a connected service and it *isn't* the one we are currently connected to
    std::string currentOPath = GetObjectPathForService(currentServiceVariant);
    ConnManBusService* currentService = GetServiceForPath(currentOPath);
    bool disconnected = DisconnectFromWifiService(currentService);

    if(disconnected) {
      Log::Write("Disconnected from %s.", currentOPath.c_str());
    }

    g_object_unref(currentService);
    g_variant_unref(currentServiceVariant);
  }

  // Get the ConnManBusService for our object path
  Log::Write("Service path: %s", servicePath.c_str());
  ConnManBusService* service = GetServiceForPath(servicePath);
  if(service == nullptr) {
    g_variant_unref(services);
    g_variant_unref(serviceVariant);
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  WPAConnectInfo connectInfo = {};
  bool agent_registered = false;

  // Register agent
  connectInfo.name = nameFromHex.c_str();
  connectInfo.passphrase = pw.c_str();
  connectInfo.status = ConnectWifiResult::CONNECT_NONE;

  agent_registered = RegisterAgent(&connectInfo);
  if (!agent_registered) {
    loge("could not register agent, bailing out");
    g_variant_unref(services);
    g_variant_unref(serviceVariant);
    g_object_unref(service);
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  ConnectWifiResult connectStatus = ConnectToWifiService(service);

  if(connectInfo.status != ConnectWifiResult::CONNECT_NONE) {
    // If we set the status in the agent callback, use it
    // (it is probably the invalid key error)
    connectStatus = connectInfo.status;
  }

  std::string statusString = "failure";
  std::string errorString = "None";

  switch(connectStatus) {
    case ConnectWifiResult::CONNECT_SUCCESS:
      statusString = "success";
      break;
    case ConnectWifiResult::CONNECT_INVALIDKEY:
      errorString = "invalid password";
      break;
    default:
      statusString = "failure";
      errorString = "unknown";
      break;
  }

  DASMSG(wifi_connection_status, "wifi.manual_connect_attempt",
          "WiFi connection attempt.");
  DASMSG_SET(s1, statusString, "Connection attempt result");
  DASMSG_SET(s2, errorString, "Error reason");
  DASMSG_SET(s3, hidden?"hidden":"visible", "SSID broadcast");
  DASMSG_SEND();

  if (agent_registered) {
    Log::Write("unregistering agent");
    UnregisterAgent(&connectInfo);
  }

  g_variant_unref(services);
  g_variant_unref(serviceVariant);
  g_object_unref(service);

  return connectStatus;
}

ConnManBusService* GetServiceForPath(std::string objectPath) {
  GError* error = nullptr;
  ConnManBusService* service = conn_man_bus_service_proxy_new_for_bus_sync(
                                  G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  "net.connman",
                                  objectPath.c_str(),
                                  nullptr,
                                  &error);

  if(service == nullptr || error != nullptr) {
    Log::Write("Could not find service for object path: %s", objectPath.c_str());
  }

  if(error) {
    g_error_free(error);
  }

  return service;
}

ConnectWifiResult ConnectToWifiService(ConnManBusService* service) {

  if(service == nullptr) {
    return ConnectWifiResult::CONNECT_FAILURE;
  }

  GCond connectCond;
  g_cond_init(&connectCond);
  g_mutex_lock(&connectMutex);

  struct ConnectInfo data;

  data.error = nullptr;
  data.cond = &connectCond;
  data.service = service;

  conn_man_bus_service_call_connect (
    service,
    nullptr,
    ConnectCallback,
    (gpointer)&data);

  g_cond_wait(&connectCond, &connectMutex);
  g_mutex_unlock(&connectMutex);

  if(data.error != nullptr) {
    DASMSG(connman_error, "connman.error.connect", "Connman error.");
      DASMSG_SET(s1, DASMSG_ESCAPE(data.error->message), "Error message");
      DASMSG_SEND();
    Log::Write("Connect error: %s", data.error->message);
  }

  return (data.error == nullptr)? ConnectWifiResult::CONNECT_SUCCESS:
    ConnectWifiResult::CONNECT_FAILURE;
}

bool DisconnectFromWifiService(ConnManBusService* service) {
  if(service == nullptr) {
    return false;
  }

  GError* error = nullptr;
  bool success = conn_man_bus_service_call_disconnect_sync (service, nullptr, &error);

  if(error) {
    g_error_free(error);
  }

  return success;
}

std::string GetObjectPathForService(GVariant* service) {
  GVariantIter iter;
  GVariant *serviceChild;
  const gchar* objectPath = nullptr;

  g_variant_iter_init (&iter, service);
  while ((serviceChild = g_variant_iter_next_value (&iter)))
  {
    if(g_str_equal(g_variant_get_type_string(serviceChild),"o")) {
      objectPath = g_variant_get_string (serviceChild, nullptr);
    }

    g_variant_unref (serviceChild);
  }

  return std::string(objectPath);
}

WiFiState GetWiFiState() {
  // Return WiFiState object
  WiFiState wifiState;
  wifiState.ssid = "";
  wifiState.connState = WiFiConnState::UNKNOWN;

  GError* error = nullptr;

  ConnManBusManager* manager_proxy;
  manager_proxy = conn_man_bus_manager_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/",
                                                              nullptr,
                                                              &error);
  if (error) {
    loge("error getting proxy for net.connman /");
    g_error_free(error);
    return wifiState;
  }

  GVariant* services = nullptr;
  bool success = conn_man_bus_manager_call_get_services_sync(manager_proxy,
                                                        &services,
                                                        nullptr,
                                                        &error);
  g_object_unref(manager_proxy);
  if (error) {
    loge("Error getting services from connman");
    g_error_free(error);
    return wifiState;
  }

  if (!success) {
    loge("connman failed to get list of services");
    return wifiState;
  }

  for (gsize i = 0 ; i < g_variant_n_children(services); i++) {
    GVariant* child = g_variant_get_child_value(services, i);
    GVariant* attrs = g_variant_get_child_value(child, 1);

    bool isAssociated = false;
    std::string connectedSsid;
    WiFiConnState connState = WiFiConnState::UNKNOWN;

    for (gsize j = 0 ; j < g_variant_n_children(attrs); j++) {
      GVariant* attr = g_variant_get_child_value(attrs, j);
      GVariant* key_v = g_variant_get_child_value(attr, 0);
      GVariant* val_v = g_variant_get_child_value(attr, 1);
      GVariant* val = g_variant_get_variant(val_v);
      const char* key = g_variant_get_string(key_v, nullptr);

      // Make sure this is a wifi service and not something else
      if (g_str_equal(key, "Type")) {
        if (!g_str_equal(g_variant_get_string(val, nullptr), "wifi")) {
          g_variant_unref(attr);
          g_variant_unref(key_v);
          g_variant_unref(val_v);
          g_variant_unref(val);
          break;
        }
      }

      // Make sure this is for the wlan0 interface and not p2p0
      if (g_str_equal(key, "Ethernet")) {
        for (gsize k = 0 ; k < g_variant_n_children(val); k++) {
          GVariant* ethernet_attr = g_variant_get_child_value(val, k);
          GVariant* ethernet_key_v = g_variant_get_child_value(ethernet_attr, 0);
          GVariant* ethernet_val_v = g_variant_get_child_value(ethernet_attr, 1);
          GVariant* ethernet_val = g_variant_get_variant(ethernet_val_v);
          const char* ethernet_key = g_variant_get_string(ethernet_key_v, nullptr);
          if (g_str_equal(ethernet_key, "Interface")) {
            if (!g_str_equal(g_variant_get_string(ethernet_val, nullptr), WIFI_DEVICE)) {
              isAssociated = false;

              g_variant_unref(ethernet_attr);
              g_variant_unref(ethernet_key_v);
              g_variant_unref(ethernet_val_v);
              g_variant_unref(ethernet_val);
              break;
            }
          }

          g_variant_unref(ethernet_attr);
          g_variant_unref(ethernet_key_v);
          g_variant_unref(ethernet_val_v);
          g_variant_unref(ethernet_val);
        }
      }

      if (g_str_equal(key, "State")) {
        std::string state = std::string(g_variant_get_string(val, nullptr));
        std::string servicePath = GetObjectPathForService(child);
        connectedSsid = GetHexSsidFromServicePath(servicePath);

        if(state == "ready") {
          isAssociated = true;
          connState = WiFiConnState::CONNECTED;
        } else if(state == "online") {
          isAssociated = true;
          connState = WiFiConnState::ONLINE;
        }
      }

      g_variant_unref(attr);
      g_variant_unref(key_v);
      g_variant_unref(val_v);
      g_variant_unref(val);
    }

    if(isAssociated) {
      wifiState.ssid = connectedSsid;
      wifiState.connState = connState;
      g_variant_unref(attrs);
      g_variant_unref(child);
      break;
    }

    g_variant_unref(attrs);
    g_variant_unref(child);
  }

  g_variant_unref(services);

  return wifiState;
}

std::string GetHexSsidFromServicePath(const std::string& servicePath) {
  // Take a dbus wifi service path and extract the ssid HEX
  std::string wifiPrefix = "/net/connman/service/wifi";
  std::string prefix = wifiPrefix + "_000000000000_";
  std::string hexString = "";

  if(servicePath.compare(0, wifiPrefix.length(), wifiPrefix) != 0) {
    // compare strings all the way up to and including "wifi"
    return "! Invalid Ssid";
  }

  for(int i = prefix.length(); i < servicePath.length(); i++) {
    if(servicePath[i] == '_') {
      break;
    }
    hexString.push_back(servicePath[i]);
  }

  return hexString;
}

bool CanConnectToHostName(char* hostName) {
  struct sockaddr_in addr = {0};

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    return false;
  }

  // we will try to make tcp connection using IPv4
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);

  char ipAddr[100];

  if(strlen(hostName) > 100) {
    // don't allow host names larger than 100 chars
    close(sockfd);
    return false;
  }

  // Try to get IP from host name (will do DNS resolve)
  bool gotIp = GetIpFromHostName(hostName, ipAddr);

  if(!gotIp) {
    close(sockfd);
    return false;
  }

  if(inet_pton(AF_INET, ipAddr, &addr.sin_addr) <= 0) {
    // can't resolve hostname
    close(sockfd);
    return false;
  }

  if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    // can't connect to hostname
    close(sockfd);
    return false;
  }

  close(sockfd);

  // success, return true!
  return true;
}

bool GetIpFromHostName(char* hostName, char* ipAddressOut) {
  struct hostent* hostEntry;
  struct in_addr** ip;

  hostEntry = gethostbyname(hostName);

  if(hostEntry == nullptr) {
    return false;
  }

  ip = (struct in_addr**)hostEntry->h_addr_list;

  if(ip[0] == nullptr) {
    return false;
  }

  // we have an ip!
  strcpy(ipAddressOut, inet_ntoa(*ip[0]));

  return true;
}

bool IsAccessPointMode() {
  GError* error = nullptr;

  GVariant* properties;

  ConnManBusTechnology* tech_proxy = nullptr;
  tech_proxy = conn_man_bus_technology_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/net/connman/technology/wifi",
                                                              nullptr,
                                                              &error);

  if(error != nullptr) {
    g_error_free(error);
    return false;
  }

  bool success = (bool)conn_man_bus_technology_call_get_properties_sync (
    tech_proxy,
    &properties,
    nullptr,
    &error);

  if(error != nullptr) {
    g_error_free(error);
    return false;
  }

  g_object_unref(tech_proxy);

  if(!success) {
    return false;
  }

  for (gsize j = 0 ; j < g_variant_n_children(properties); j++) {
    GVariant* attr = g_variant_get_child_value(properties, j);
    GVariant* key_v = g_variant_get_child_value(attr, 0);
    GVariant* val_v = g_variant_get_child_value(attr, 1);
    GVariant* val = g_variant_get_variant(val_v);
    const char* key = g_variant_get_string(key_v, nullptr);

    g_variant_unref(attr);
    g_variant_unref(key_v);
    g_variant_unref(val_v);
    g_variant_unref(val);

    // Make sure this is a wifi service and not something else
    if (g_str_equal(key, "Tethering")) {
      bool valBool = (bool)g_variant_get_boolean(val);
      g_variant_unref(properties);
      return valBool;
    }

  }

  g_variant_unref(properties);
  return false;
}

bool EnableAccessPointMode(std::string ssid, std::string pw) {
  GError* error = nullptr;

  GVariant* ssid_s = g_variant_new_string(ssid.c_str());
  GVariant* ssid_v = g_variant_new_variant(ssid_s);

  GVariant* pw_s = g_variant_new_string(pw.c_str());
  GVariant* pw_v = g_variant_new_variant(pw_s);

  GVariant* enable_b = g_variant_new_boolean(true);
  GVariant* enable_v = g_variant_new_variant(enable_b);

  ConnManBusTechnology* tech_proxy = nullptr;
  tech_proxy = conn_man_bus_technology_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/net/connman/technology/wifi",
                                                              nullptr,
                                                              &error);

  if(error != nullptr) {
    g_error_free(error);
    return false;
  }

  conn_man_bus_technology_call_set_property_sync (
    tech_proxy,
    "TetheringIdentifier",
    ssid_v,
    nullptr,
    &error);

  if(error != nullptr) {
    g_error_free(error);
    g_object_unref(tech_proxy);
    return false;
  }

  conn_man_bus_technology_call_set_property_sync (
    tech_proxy,
    "TetheringPassphrase",
    pw_v,
    nullptr,
    &error);

  if(error != nullptr) {
    g_error_free(error);
    g_object_unref(tech_proxy);
    return false;
  }

  conn_man_bus_technology_call_set_property_sync (
    tech_proxy,
    "Tethering",
    enable_v,
    nullptr,
    &error);

  if(error != nullptr) {
    g_error_free(error);
    g_object_unref(tech_proxy);
    return false;
  }

  g_object_unref(tech_proxy);

  return true;
}

bool DisableAccessPointMode() {
  GError* error = nullptr;

  GVariant* enable_b = g_variant_new_boolean(false);
  GVariant* enable_v = g_variant_new_variant(enable_b);

  ConnManBusTechnology* tech_proxy = nullptr;
  tech_proxy = conn_man_bus_technology_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "net.connman",
                                                              "/net/connman/technology/wifi",
                                                              nullptr,
                                                              &error);

  if(error != nullptr) {
    g_error_free(error);
    return false;
  }

  conn_man_bus_technology_call_set_property_sync (
    tech_proxy,
    "Tethering",
    enable_v,
    nullptr,
    &error);

  g_object_unref(tech_proxy);

  if(error != nullptr) {
    g_error_free(error);
    return false;
  }

  return true;
}

void RecoverNetworkServices() {
  DASMSG(recover_network_services, "wifi.recover_network_services", "Attempt to recover network services");
    DASMSG_SEND();

  ExecCommandInBackground({"sudo", "/bin/systemctl", "restart", "wpa_supplicant", "connman" }, nullptr);
}

void WpaSupplicantScan() {
  // Perform a wifi scan talking to Wpa Supplicant directly
  GDBusConnection *gdbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM,
                                                nullptr,
                                                nullptr);

  FiW1Wpa_supplicant1* wpa_sup = fi_w1_wpa_supplicant1_proxy_new_sync (
      gdbusConn,
      G_DBUS_PROXY_FLAGS_NONE,
      "fi.w1.wpa_supplicant1",
      "/fi/w1/wpa_supplicant1",
      nullptr,
      nullptr);

  gchar* interface_path;
  gboolean success = fi_w1_wpa_supplicant1_call_get_interface_sync (
      wpa_sup,
      "wlan0",
      &interface_path,
      nullptr,
      nullptr);

  FiW1Wpa_supplicant1Outerface* interfacePath = fi_w1_wpa_supplicant1_outerface_proxy_new_sync(
    gdbusConn,
    G_DBUS_PROXY_FLAGS_NONE,
    "fi.w1.wpa_supplicant1",
    interface_path,
    nullptr,
    nullptr);

  GVariantBuilder *b;
  GVariant *dict;

  b = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (b, "{sv}", "Type", g_variant_new_string ("passive"));
  g_variant_builder_add (b, "{sv}", "AllowRoam", g_variant_new_boolean (false));
  dict = g_variant_builder_end (b);

  gboolean scanSuccess = fi_w1_wpa_supplicant1_outerface_call_scan_sync (
    interfacePath,
    dict,
    nullptr,
    nullptr);

  Log::Write("Dbus-WpaSupplicant interface path [%d][%d][%s]", (int)scanSuccess, (int)success, interface_path);
}

WiFiIpFlags GetIpAddress(uint8_t* ipv4_32bits, uint8_t* ipv6_128bits) {
  WiFiIpFlags wifiFlags = WiFiIpFlags::NONE;

  struct ifaddrs* ifaddrs;

  // get ifaddrs
  getifaddrs(&ifaddrs);

  struct ifaddrs* current = ifaddrs;

  // clear memory
  memset(ipv4_32bits, 0, 4);
  memset(ipv6_128bits, 0, 16);

  const char* interface = IsAccessPointMode()? "tether" : WIFI_DEVICE;

  while(current != nullptr) {
    int family = current->ifa_addr->sa_family;

    if ((family == AF_INET || family == AF_INET6) && (strcmp(current->ifa_name, interface) == 0)) {
      if(family == AF_INET) {
        // Handle IPv4
        struct sockaddr_in *sa = (struct sockaddr_in*)current->ifa_addr;
        memcpy(ipv4_32bits, &sa->sin_addr, sizeof(sa->sin_addr));
        wifiFlags = wifiFlags | WiFiIpFlags::HAS_IPV4;
      } else {
        // Handle IPv6
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)current->ifa_addr;
        memcpy(ipv6_128bits, &sa6->sin6_addr, sizeof(sa6->sin6_addr));
        wifiFlags = wifiFlags | WiFiIpFlags::HAS_IPV6;
      }
    }

    current = current->ifa_next;
  }

  // free ifaddrs
  freeifaddrs(ifaddrs);

  return wifiFlags;
}

bool GetApMacAddress(uint8_t* mac_48bits) {
  // Get IPv4 driver socket fd
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  const int MAC_SIZE = 6;

  if(sockfd == -1) {
    Log::Write("Can't connect to socket");
    return false;
  }

  // iwreq struct comes from linux/wireless.h
  struct iwreq data;

  // the iwreq struct must be populated with
  // the device name before making ioctl request
  strncpy(data.ifr_name, WIFI_DEVICE, IFNAMSIZ);

  // make ioctl request for AP mac address
  int req = ioctl(sockfd, SIOCGIWAP, &data);

  if(req == -1) {
    Log::Write("ioctl request for AP MAC addr failed: %d", errno);
    close(sockfd);
    return false;
  }

  // Copy mac address to given pointer
  memcpy(mac_48bits, &(data.u.ap_addr.sa_data), MAC_SIZE);

  close(sockfd);
  return true;
}

} // Wifi
} // Anki
