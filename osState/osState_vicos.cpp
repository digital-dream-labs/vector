/**
 * File: OSState_vicos.cpp
 *
 * Authors: Kevin Yoon
 * Created: 2017-12-11
 *
 * Description:
 *
 *   Keeps track of OS-level state, mostly for development/debugging purposes
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "osState/osState.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "util/console/consoleInterface.h"
#include "util/console/consoleInterface.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"
#include "util/string/stringUtils.h"
#include "util/time/universalTime.h"

#include "cutils/properties.h"

#include "anki/cozmo/shared/factory/emrHelper.h"

// For getting our ip address
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/wireless.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <array>
#include <iomanip>


#ifdef SIMULATOR
#error SIMULATOR should NOT be defined by any target using osState_vicos.cpp
#endif

#define LOG_CHANNEL "OsState"

namespace Anki {
namespace Vector {

CONSOLE_VAR_ENUM(int, kWebvizUpdatePeriod, "OSState.Webviz", 0, "Off,10ms,100ms,1000ms,10000ms");
CONSOLE_VAR(bool, kOSState_FakeNoTime, "OSState.Timezone", false);
CONSOLE_VAR(bool, kOSState_FakeNoTimezone, "OSState.Timezone", false);
CONSOLE_VAR(bool, kSendFakeCpuTemperature,  "OSState.Temperature", false);
CONSOLE_VAR(u32,  kFakeCpuTemperature_degC, "OSState.Temperature", 20);

namespace {

  // When memory pressure > this, report red alert
  CONSOLE_VAR_RANGED(u32, kHighMemPressureMultiple, "OSState.MemoryInfo", 10, 0, 100);

  // When memory pressure > this, report yellow alert
  CONSOLE_VAR_RANGED(u32, kMediumMemPressureMultiple, "OSState.MemoryInfo", 5, 0, 100);

 // When disk pressure > this, report red alert
  CONSOLE_VAR_RANGED(u32, kHighDiskPressureMultiple, "OSState.DiskInfo", 10, 0, 100);

  // When disk pressure > this, report yellow alert
  CONSOLE_VAR_RANGED(u32, kMediumDiskPressureMultiple, "OSState.DiskInfo", 5, 0, 100);

  // When wifi error rate > this, report red alert
  CONSOLE_VAR_RANGED(u32, kHighWifiErrorRate, "OSState.WifiInfo", 2, 0, 100);

  // When wifi error rate > this, report yellow alert
  CONSOLE_VAR_RANGED(u32, kMediumWifiErrorRate, "OSState.WifiInfo", 1, 0, 100);

  uint32_t kPeriodEnumToMS[] = {0, 10, 100, 1000, 10000};

  std::ifstream _cpuFile;
  std::ifstream _temperatureFile;
  std::ifstream _uptimeFile;
  std::ifstream _memInfoFile;
  std::ifstream _cpuTimeStatsFile;

  std::mutex _cpuMutex;
  std::mutex _temperatureMutex;
  std::mutex _MACAddressMutex;
  std::mutex _uptimeMutex;
  std::mutex _memInfoMutex;
  std::mutex _cpuTimeStatsMutex;

  const char* kNominalCPUFreqFile = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq";
  const char* kCPUFreqFile        = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq";
  const char* kCPUFreqSetFile     = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed";
  const char* kCPUGovernorFile    = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";
  const char* kTemperatureFile    = "/sys/devices/virtual/thermal/thermal_zone3/temp";
  const char* kMACAddressFile     = "/sys/class/net/wlan0/address";
  const char* kRecoveryModeFile   = "/data/unbrick";
  const char* kUptimeFile         = "/proc/uptime";
  const char* kMemInfoFile        = "/proc/meminfo";
  const char* kCPUTimeStatsFile   = "/proc/stat";
  const char* kWifiTxBytesFile    = "/sys/class/net/wlan0/statistics/tx_bytes";
  const char* kWifiRxBytesFile    = "/sys/class/net/wlan0/statistics/rx_bytes";
  const char* kWifiTxErrorsFile    = "/sys/class/net/wlan0/statistics/tx_errors";
  const char* kWifiRxErrorsFile    = "/sys/class/net/wlan0/statistics/rx_errors";
  const char* kBootIDFile         = "/proc/sys/kernel/random/boot_id";
  const char* kLocalTimeFile      = "/data/etc/localtime";
  const char* kCmdLineFile        = "/proc/cmdline";
  constexpr const char* kUniversalTimeFile = "/usr/share/zoneinfo/Universal";
  constexpr const char* kRobotVersionFile = "/anki/etc/version";
  constexpr const char* kMaintenanceRebootFile = "/run/after_maintenance_reboot";

  const char* kAutomaticGovernor = "interactive";
  const char* kManualGovernor = "userspace";

  const char* const kWifiInterfaceName = "wlan0";

  // System vars
  uint32_t _cpuFreq_kHz;      // CPU freq
  uint32_t _cpuTemp_C;        // Temperature in Celsius
  float _uptime_s;            // Uptime in seconds
  float _idleTime_s;          // Idle time in seconds
  uint32_t _totalMem_kB;      // Total memory in kB
  uint32_t _availMem_kB;      // Available memory in kB
  uint32_t _freeMem_kB;       // Free memory in kB

  std::vector<std::string> _CPUTimeStats; // CPU time stats lines


  // How often state variables are updated
  uint64_t _currentTime_ms = 0;
  uint64_t _updatePeriod_ms = 0;
  uint64_t _lastWebvizUpdateTime_ms = 0;

  std::function<void(const Json::Value&)> _webServiceCallback = nullptr;

  // helper to allow compile-time static_asserts on length of a const string
  constexpr int GetConstStrLength(const char* str)
  {
    return *str ? 1 + GetConstStrLength(str + 1) : 0;
  }

  // OS version numbers
  int _majorVersion = -1;
  int _minorVersion = -1;
  int _incrementalVersion = -1;
  int _buildVersion = -1;

} // namespace

std::string GetProperty(const std::string& key)
{
  char propBuf[PROPERTY_VALUE_MAX] = {0};
  int rc = property_get(key.c_str(), propBuf, "");
  if(rc <= 0)
  {
    LOG_WARNING("OSState.GetProperty.FailedToFindProperty",
                "Property %s not found",
                key.c_str());
  }

  return std::string(propBuf);
}

OSState::OSState()
{
  // Suppress unused function error for WriteEMR
  (void)static_cast<void(*)(size_t, void*, size_t)>(Factory::WriteEMR);
  (void)static_cast<void(*)(size_t, uint32_t)>(Factory::WriteEMR);

  // Get nominal CPU frequency for this robot
  std::ifstream file;
  file.open(kNominalCPUFreqFile, std::ifstream::in);
  if(file.is_open()) {
    file >> kNominalCPUFreq_kHz;
    LOG_INFO("OSState.Constructor.NominalCPUFreq", "%dkHz", kNominalCPUFreq_kHz);
    file.close();
  }
  else {
    LOG_ERROR("OSState.Constructor.FailedToOpenNominalCPUFreqFile", "%s", kNominalCPUFreqFile);
  }

  _cpuFreq_kHz = kNominalCPUFreq_kHz;
  _cpuTemp_C = 0;

  _buildSha = ANKI_BUILD_SHA;

  _lastWebvizUpdateTime_ms = _currentTime_ms;

  // read the OS versions once on boot up
  if(Util::FileUtils::FileExists("/etc/os-version")) {
    std::string osv = Util::FileUtils::ReadFile("/etc/os-version");
    std::vector<std::string> tokens = Util::StringSplit(osv, '.');
    if(tokens.size()==4) {
      try {
        size_t remSz;
        _majorVersion = std::stoi(tokens[0]);
        _minorVersion = std::stoi(tokens[1]);
        _incrementalVersion = std::stoi(tokens[2]);
        _buildVersion = std::stoi(tokens[3], &remSz);
      } catch(const std::invalid_argument& ia) {
        LOG_WARNING("OSState.GetOSBuildVersion.UnableToParseVersionString","%s",osv.c_str());
        _majorVersion = -1;
        _minorVersion = -1;
        _incrementalVersion = -1;
        _buildVersion = -1;
      }
    }

    // Initialize memory info
    UpdateMemoryInfo();

  }
  const bool versionsValid = _majorVersion >= 0 &&
                             _minorVersion >= 0 &&
                             _incrementalVersion >= 0 &&
                             _buildVersion >= 0;
  DEV_ASSERT_MSG(versionsValid,
                 "OSState.MajorMinorIncVersionInvalid",
                 "maj %d, min %d, inc %d, build %d",
                 _majorVersion, _minorVersion, _incrementalVersion, _buildVersion);
}

OSState::~OSState()
{
}

RobotID_t OSState::GetRobotID() const
{
  return DEFAULT_ROBOT_ID;
}

void OSState::Update(BaseStationTime_t currTime_nanosec)
{
  _currentTime_ms = currTime_nanosec/1000000;
  if (kWebvizUpdatePeriod != 0 && _webServiceCallback) {
    if (_currentTime_ms - _lastWebvizUpdateTime_ms > kPeriodEnumToMS[kWebvizUpdatePeriod]) {
      UpdateCPUTimeStats();

      Json::Value json;
      json["deltaTime_ms"] = _currentTime_ms - _lastWebvizUpdateTime_ms;

      {
        auto& usage = json["usage"];
        std::lock_guard<std::mutex> lock(_cpuTimeStatsMutex);
        for(size_t i = 0; i < _CPUTimeStats.size(); ++i) {
          usage.append( _CPUTimeStats[i] );
        }
      }

      _webServiceCallback(json);

      _lastWebvizUpdateTime_ms = _currentTime_ms;
    }
  }
}

void OSState::SetUpdatePeriod(uint32_t milliseconds)
{
  _updatePeriod_ms = milliseconds;
}

void OSState::SendToWebVizCallback(const std::function<void(const Json::Value&)>& callback) {
  _webServiceCallback = callback;
}

void OSState::UpdateCPUFreq_kHz() const
{
  // Update cpu freq
  std::lock_guard<std::mutex> lock(_cpuMutex);
  _cpuFile.open(kCPUFreqFile, std::ifstream::in);
  if (_cpuFile.is_open()) {
    _cpuFile >> _cpuFreq_kHz;
    _cpuFile.close();
  }
  else {
    LOG_ERROR("OSState.UpdateCPUFreq_kHz.FailedToOpenCPUFreqFile", "%s", kCPUFreqFile);
  }
}

void OSState::SetDesiredCPUFrequency(DesiredCPUFrequency freq)
{
  const std::string desiredGovernor = ( freq == DesiredCPUFrequency::Automatic ) ? kAutomaticGovernor : kManualGovernor;

  // write governor mode
  const bool ok1 = Util::FileUtils::WriteFile(kCPUGovernorFile, desiredGovernor);
  if( !ok1 ) {
    LOG_ERROR("OSState.SetDesiredCPUFrequency.SetGovernor.Failed",
              "Failed to write governor value '%s' to file '%s'",
              desiredGovernor.c_str(),
              kCPUGovernorFile);
    return;
  }

  if( freq != DesiredCPUFrequency::Automatic ) {
    // if Automatic, we're done once we set the governor. Otherwise we also need to write the desired freq

    // write frequency
    const unsigned int freqVal = Util::EnumToUnderlying(freq);
    if( freqVal <= 0  ){
      LOG_ERROR("OSState.SetDesiredCPUFrequency.InvalidFrequency",
                "Can't set frequency to %d",
                freqVal);
      return;
    }

    const bool ok2 = Util::FileUtils::WriteFile(kCPUFreqSetFile, std::to_string(freqVal));
    if( !ok2 ) {
      LOG_ERROR("OSState.SetDesiredCPUFrequency.SetFrequency.Failed",
                "Failed to write frequency value '%d' to file '%s'",
                freqVal,
                kCPUFreqSetFile);
      return;
    }

    LOG_INFO("OSState.SetDesiredCPUFrequency.Manual", "Set to manual cpu frequency %u",
             freqVal);
  }
  else {
    LOG_INFO("OSState.SetDesiredCPUFrequency.Automatic", "Set to automatic cpu frequency management");
  }


  // NOTE: not returning success / fail because all I know is that the file got written to. It's up to the OS
  // to actually change the frequency, and that could take some time or be overruled by something else
}


void OSState::UpdateTemperature_C() const
{
  // Update temperature reading
  std::lock_guard<std::mutex> lock(_temperatureMutex);
  _temperatureFile.open(kTemperatureFile, std::ifstream::in);
  if (_temperatureFile.is_open()) {
    _temperatureFile >> _cpuTemp_C;
    _temperatureFile.close();
  }
  else {
    LOG_ERROR("OSState.UpdateTemperature_C.FailedToOpenTemperatureFile", "%s", kTemperatureFile);
  }
}

void OSState::UpdateUptimeAndIdleTime() const
{
  // Update uptime and idle time data
  std::lock_guard<std::mutex> lock(_uptimeMutex);
  _uptimeFile.open(kUptimeFile, std::ifstream::in);
  if (_uptimeFile.is_open()) {
    _uptimeFile >> _uptime_s >> _idleTime_s;
    _uptimeFile.close();
  }
  else {
    LOG_ERROR("OSState.UpdateUptimeAndIdleTime.FailedToOpenUptimeFile", "%s", kUptimeFile);
  }
}

void OSState::UpdateMemoryInfo() const
{
  // Update total and free memory
  std::lock_guard<std::mutex> lock(_memInfoMutex);
  _memInfoFile.open(kMemInfoFile, std::ifstream::in);
  if (_memInfoFile.is_open()) {
    std::string discard;
    _memInfoFile >> discard >> _totalMem_kB >> discard >> discard >> _freeMem_kB >> discard >> discard >> _availMem_kB;
    _memInfoFile.close();
  }
  else {
    LOG_ERROR("OSState.UpdateMemoryInfo.FailedToOpenMemInfoFile", "%s", kMemInfoFile);
  }
}

void OSState::UpdateCPUTimeStats() const
{
  // Update CPU time stats lines
  std::lock_guard<std::mutex> lock(_cpuTimeStatsMutex);
  _cpuTimeStatsFile.open(kCPUTimeStatsFile, std::ifstream::in);
  if (_cpuTimeStatsFile.is_open()) {
    static const int kNumCPUTimeStatLines = 5;
    _CPUTimeStats.resize(kNumCPUTimeStatLines);
    for (int i = 0; i < kNumCPUTimeStatLines; i++) {
      std::getline(_cpuTimeStatsFile, _CPUTimeStats[i]);
    }
    _cpuTimeStatsFile.close();
  }
  else {
    LOG_ERROR("OSState.UpdateCPUTimeStats.FailedToOpenCPUTimeStatsFile", "%s", kCPUTimeStatsFile);
  }
}

uint32_t OSState::GetCPUFreq_kHz() const
{
  static uint64_t lastUpdate_ms = 0;
  if ((_currentTime_ms - lastUpdate_ms > _updatePeriod_ms) || (_updatePeriod_ms == 0)) {
    UpdateCPUFreq_kHz();
    lastUpdate_ms = _currentTime_ms;
  }

  return _cpuFreq_kHz;
}


bool OSState::IsCPUThrottling() const
{
  DEV_ASSERT(_updatePeriod_ms != 0, "OSState.IsCPUThrottling.ZeroUpdate");
  return (_cpuFreq_kHz < kNominalCPUFreq_kHz);
}

uint32_t OSState::GetTemperature_C() const
{
  static uint64_t lastUpdate_ms = 0;
  if ((_currentTime_ms - lastUpdate_ms > _updatePeriod_ms) || (_updatePeriod_ms == 0)) {
    UpdateTemperature_C();
    lastUpdate_ms = _currentTime_ms;
  }

  if(kSendFakeCpuTemperature) {
    return kFakeCpuTemperature_degC;
  }
  return _cpuTemp_C;
}

float OSState::GetUptimeAndIdleTime(float &idleTime_s) const
{
  static uint64_t lastUpdate_ms = 0;
  if ((_currentTime_ms - lastUpdate_ms > _updatePeriod_ms) || (_updatePeriod_ms == 0)) {
    UpdateUptimeAndIdleTime();
    lastUpdate_ms = _currentTime_ms;
  }

  idleTime_s = _idleTime_s;
  return _uptime_s;
}

void OSState::GetMemoryInfo(MemoryInfo & info) const
{
  // Update current stats?
  static uint64_t lastUpdate_ms = 0;
  if ((_currentTime_ms - lastUpdate_ms > _updatePeriod_ms) || (_updatePeriod_ms == 0)) {
    UpdateMemoryInfo();
    lastUpdate_ms = _currentTime_ms;
  }

  // Populate return struct
  info.totalMem_kB = _totalMem_kB;
  info.availMem_kB = _availMem_kB;
  info.freeMem_kB = _freeMem_kB;
  info.pressure = GetPressure(info.availMem_kB, info.totalMem_kB);
  info.alert = GetAlert(info.pressure, kMediumMemPressureMultiple, kHighMemPressureMultiple);

}

bool OSState::GetDiskInfo(const std::string & path, DiskInfo & info) const
{
  struct statfs fsinfo = {};

  if (statfs(path.c_str(), &fsinfo) != 0) {
    LOG_ERROR("OSState::GetDiskInfo", "Unable to get disk info for %s (errno %d)", path.c_str(), errno);
    return false;
  }

  // Populate return struct
  info.total_kB = (fsinfo.f_blocks * fsinfo.f_bsize / 1024);
  info.avail_kB = (fsinfo.f_bavail * fsinfo.f_bsize / 1024);
  info.free_kB = (fsinfo.f_bfree * fsinfo.f_bsize / 1024);
  info.pressure = GetPressure(info.avail_kB, info.total_kB);
  info.alert = GetAlert(info.pressure, kMediumDiskPressureMultiple, kHighDiskPressureMultiple);
  return true;
}

void OSState::GetCPUTimeStats(std::vector<std::string> & stats) const
{
  static uint64_t lastUpdate_ms = 0;
  if ((_currentTime_ms - lastUpdate_ms > _updatePeriod_ms) || (_updatePeriod_ms == 0)) {
    UpdateCPUTimeStats();
    lastUpdate_ms = _currentTime_ms;
  }

  {
    std::lock_guard<std::mutex> lock(_cpuTimeStatsMutex);
    stats = _CPUTimeStats;
  }
}


const std::string& OSState::GetSerialNumberAsString()
{
  if(_serialNumString.empty())
  {
    std::stringstream ss;
    ss << std::hex
       << std::setfill('0')
       << std::setw(8)
       << std::uppercase
       << Factory::GetEMR()->fields.ESN;
    _serialNumString = ss.str();
  }
  return _serialNumString;
}

const std::string& OSState::GetOSBuildVersion()
{
  if(_osBuildVersion.empty())
  {
    _osBuildVersion = GetProperty("ro.build.display.id");
  }

  return _osBuildVersion;
}

void OSState::GetOSBuildVersion(int& major, int& minor, int& incremental, int& build) const
{
  major = _majorVersion;
  minor = _minorVersion;
  incremental = _incrementalVersion;
  build = _buildVersion;
}

const std::string& OSState::GetRobotVersion()
{
  if (_robotVersion.empty()) {
    std::ifstream ifs(kRobotVersionFile);
    ifs >> _robotVersion;
    Anki::Util::StringTrimWhitespaceFromEnd(_robotVersion);
  }
  return _robotVersion;
}

const std::string& OSState::GetBuildSha()
{
  return _buildSha;
}

const std::string& OSState::GetRobotName() const
{
  static std::string name = GetProperty("anki.robot.name");
  if(name.empty())
  {
    name = GetProperty("anki.robot.name");
  }
  return  name;
}

static std::string GetIPV4AddressForInterface(const char* if_name) {
  struct ifaddrs* ifaddr = nullptr;
  struct ifaddrs* ifa = nullptr;
  char host[NI_MAXHOST] = {0};

  int rc = getifaddrs(&ifaddr);
  if (rc == -1) {
    LOG_ERROR("OSState.GetIPAddress.GetIfAddrsFailed", "%s", strerror(errno));
    return "";
  }

  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }

    if (ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    if (!strcmp(ifa->ifa_name, if_name)) {
      break;
    }
  }

  if (ifa != nullptr) {
    int s = getnameinfo(ifa->ifa_addr,
                        sizeof(struct sockaddr_in),
                        host, sizeof(host),
                        NULL, 0, NI_NUMERICHOST);
    if (s != 0) {
      LOG_ERROR("OSState.GetIPAddress.GetNameInfoFailed", "%s", gai_strerror(s));
      memset(host, 0, sizeof(host));
    }
  }

  const std::string ip(host);
  if (host[0])
  {
    static std::string sPrevIface = "";
    static std::string sPrevIP = "";
    const std::string iface(if_name);
    if(sPrevIP != ip || sPrevIface != iface)
    {
      sPrevIP = ip;
      sPrevIface = iface;
      LOG_INFO("OSState.GetIPAddress.IPV4AddressFound", "iface = %s , ip = %s",
               if_name, host);
    }
  }
  else
  {
    LOG_INFO("OSState.GetIPAddress.IPV4AddressNotFound", "iface = %s", if_name);
  }
  freeifaddrs(ifaddr);

  return ip;
}

static std::string GetWiFiSSIDForInterface(const char* if_name) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    ASSERT_NAMED_EVENT(false, "OSState.GetSSID.OpenSocketFail", "");
    return "";
  }

  iwreq req;
  memset(&req, 0, sizeof(req));
  (void) strncpy(req.ifr_name, if_name, sizeof(req.ifr_name) - 1);
  char essid[IW_ESSID_MAX_SIZE + 2] = {0};
  req.u.essid.pointer = essid;
  req.u.essid.length = sizeof(essid) - 2;

  if (ioctl(fd, SIOCGIWESSID, &req) == -1) {
    LOG_INFO("OSState.UpdateWifiInfo.FailedToGetSSID", "iface = %s , errno = %s",
             if_name, strerror(errno));
    memset(essid, 0, sizeof(essid));
  }
  (void) close(fd);
  LOG_INFO("OSState.GetSSID", "%s", essid);
  return std::string(essid);
}

const std::string& OSState::GetIPAddress(bool update)
{
  if(_ipAddress.empty() || update)
  {
    _ipAddress = GetIPV4AddressForInterface(kWifiInterfaceName);
  }

  return _ipAddress;
}

const std::string& OSState::GetSSID(bool update)
{
  if(_ssid.empty() || update)
  {
    _ssid = GetWiFiSSIDForInterface(kWifiInterfaceName);
  }

  return _ssid;
}

bool OSState::IsValidIPAddress(const std::string& ip) const
{
  struct sockaddr_in sa;
  const int result = inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
  if(result != 0)
  {
    const bool isLinkLocalIP = (ip.length() > 7) && ip.compare(0,7,"169.254") == 0;
    return !isLinkLocalIP;
  }
  return false;
}

std::string OSState::GetMACAddress() const
{
  std::lock_guard<std::mutex> lock(_MACAddressMutex);
  std::ifstream macFile;
  macFile.open(kMACAddressFile);
  if (macFile.is_open()) {
    std::string macStr;
    macFile >> macStr;
    macFile.close();
    return macStr;
  }
  else {
    LOG_ERROR("OSState.GetMACAddress.FailedToOpenMACAddressFile", "%s", kMACAddressFile);
  }
  return "";
}

static bool GetCounter(const char * path, uint64_t & val)
{
  std::ifstream rxFile;
  rxFile.open(path);
  if (rxFile.is_open()) {
    rxFile >> val;
    rxFile.close();
    return true;
  }
  else {
    LOG_ERROR("OSState.GetCounter.FailedToOpenCounterFile", "%s", path);
  }
  return false;
}

static OSState::Alert GetWifiAlert(uint64_t errors, uint64_t bytes)
{
  // Shortcut common cases
  if (errors == 0) {
    return OSState::Alert::None;
  } else if (bytes == 0) {
    return OSState::Alert::Red;
  }

  // Do the math
  const float percent = (100.0 * errors) / bytes;
  if (percent > kHighWifiErrorRate) {
    return OSState::Alert::Red;
  }
  if (percent > kMediumWifiErrorRate) {
    return OSState::Alert::Yellow;
  }
  return OSState::Alert::None;
}

uint64_t OSState::GetWifiTxBytes() const
{
  uint64_t numBytes = 0;
  GetCounter(kWifiTxBytesFile, numBytes);
  return numBytes;
}

uint64_t OSState::GetWifiRxBytes() const
{
  uint64_t numBytes = 0;
  GetCounter(kWifiRxBytesFile, numBytes);
  return numBytes;
}

bool OSState::GetWifiInfo(WifiInfo & wifiInfo) const
{
  if (!GetCounter(kWifiRxBytesFile, wifiInfo.rx_bytes)) {
    return false;
  }

  if (!GetCounter(kWifiTxBytesFile, wifiInfo.tx_bytes)) {
    return false;
  }

  if (!GetCounter(kWifiRxErrorsFile, wifiInfo.rx_errors)) {
    return false;
  }

  if (!GetCounter(kWifiTxErrorsFile, wifiInfo.tx_errors)) {
    return false;
  }

  // Determine alert level based on worst of RX and TX error stats
  const Alert rx_alert = GetWifiAlert(wifiInfo.rx_errors, wifiInfo.rx_bytes);
  const Alert tx_alert = GetWifiAlert(wifiInfo.tx_errors, wifiInfo.tx_bytes);

  wifiInfo.alert = std::max(rx_alert, tx_alert);

  return true;
}

bool OSState::IsInRecoveryMode()
{
  return Util::FileUtils::FileExists(kRecoveryModeFile);
}

bool OSState::RebootedForMaintenance() const
{
  return Util::FileUtils::FileExists(kMaintenanceRebootFile);
}

bool OSState::HasValidEMR() const
{
  const auto * emr = Factory::GetEMR();
  return (emr != nullptr);
}

const std::string & OSState::GetBootID()
{
  if (_bootID.empty()) {
    // http://0pointer.de/blog/projects/ids.html
    _bootID = Util::FileUtils::ReadFile(kBootIDFile);
    Anki::Util::StringTrimWhitespaceFromEnd(_bootID);
    if (_bootID.empty()) {
      LOG_ERROR("OSState.GetBootID", "Unable to read boot ID from %s", kBootIDFile);
    }
  }
  return _bootID;
}

bool OSState::IsWallTimeSynced() const
{
  if (kOSState_FakeNoTime) {
    return false;
  }

  struct timex txc = {};

  if (adjtimex(&txc) < 0) {
    LOG_ERROR("OSState.IsWallTimeSynced.CantGetTimex", "Invalid return from adjtimex");
    return false;
  }

  if (txc.status & STA_UNSYNC) {
    return false;
  }

  return true;
}

bool OSState::HasTimezone() const
{
  if (kOSState_FakeNoTimezone) {
    return false;
  }

  if (!Util::FileUtils::FileExists(kUniversalTimeFile)) {
    LOG_ERROR("OSState.HasTimezone.NoUniversalTimeFile",
              "Unable to find universal time file '%s', cant check for timezone (assuming none)",
              kUniversalTimeFile);
    return false;
  }

  if (!Util::FileUtils::FileExists(kLocalTimeFile)) {
    LOG_ERROR("OSState.HasTimezone.NoLocalTimeFile",
              "Missing local time file '%s'",
              kLocalTimeFile);
    return false;
  }

  // local time should be a symlink to something, either Universal (meaning we don't have a timezone) or a
  // specific timezone
  struct stat linkStatus;
  const int ok = lstat(kLocalTimeFile, &linkStatus);
  if (ok < 0) {
    LOG_ERROR("OSState.HasTimezone.CantStatLink",
              "lstat(%s) returned %d, error %s",
              kLocalTimeFile,
              ok,
              strerror(errno));
    return false;
  }

  if (!S_ISLNK(linkStatus.st_mode)) {
    LOG_ERROR("OSState.HasTimezone.LocalTimeNotALink",
              "Local time file '%s' exists but isn't a symlink",
              kLocalTimeFile);
    return false;
  }

  // check which file the kLocalTimeFile is a symlink to

  // the path string length can be variable. Rather than dynamically allocating it, just make sure our
  // statically allocated buffer is large enough
  static const size_t linkPathLen = 1024;
  static_assert (GetConstStrLength(kUniversalTimeFile) < linkPathLen,
                 "OSState.HasTimezone.InvalidFilePath");

  if( linkStatus.st_size >= linkPathLen ) {
    LOG_ERROR("OSState.HasTimezone.LinkNameTooLong",
              "Link path size is %ld, but we only made room for %zu",
              linkStatus.st_size,
              linkPathLen);
    // this means it can't be pointing to kUniversalTimeFile (we static assert that that path will fit within
    // the buffer), so it must be some really long file. It seems likely that this is a timezone with a long
    // name, so return true, but it technically could be pointing to any file
    return true;
  }

  char linkPath[linkPathLen];
  const ssize_t written = readlink(kLocalTimeFile, linkPath, linkPathLen);
  if (written < 0 || written >= linkPathLen) {
    LOG_ERROR("OSState.HasTimezone.CantReadLink",
              "File '%s' looks like a symlink, but can't be read (returned %d, error %s)",
              kLocalTimeFile,
              written,
              strerror(errno));
    return false;
  }

  linkPath[written] = '\0';

  // if timezone isn't set, path is either kUniversalTimeFile or ../../kUniversalTimeFile
  const char* found = strstr(linkPath, kUniversalTimeFile);
  if (nullptr == found) {
    // string doesn't match, so the link is pointing to some other file

    if( Util::FileUtils::FileExists(linkPath) ) {
      // valid file to link to (assume it's a time zone)
      return true;
    }
    else {
      LOG_ERROR("OSState.HasTimezone.InvalidSymLink",
                "File '%s' is a sym link to '%s' which does not exist",
                kLocalTimeFile,
                linkPath);
      return false;
    }
  }

  if (found != linkPath) {
    // double check that it's just prefixed with ../../
    if (found != linkPath + 5 ||
        strstr(linkPath, "../../") != linkPath) {
      LOG_WARNING("OSState.HasTimezone.InvalidPath",
                  "'%s' is a symlink to '%s' which doesn't meet expectations",
                  kLocalTimeFile,
                  linkPath);
    }
  }

  // since kUniversalTimeFile is being linked to, we don't have a timezone
  return false;
}

bool OSState::IsUserSpaceSecure()
{
  static bool read = false;
  if(!read)
  {
    read = true;
    std::ifstream infile(kCmdLineFile);
    std::string line;
    while(std::getline(infile, line))
    {
      static const char* kKey = "dm=";
      size_t index = line.find(kKey);
      if(index != std::string::npos)
      {
        _isUserSpaceSecure = true;
        break;
      }
    }
    infile.close();
  }

  return _isUserSpaceSecure;
}

bool OSState::IsAnkiDevRobot()
{
  static bool read = false;
  if(!read)
  {
    read = true;
    std::ifstream infile(kCmdLineFile);
    std::string line;
    while(std::getline(infile, line))
    {
      static const char* kKey = "anki.dev";
      size_t index = line.find(kKey);
      if(index != std::string::npos)
      {
        _isAnkiDevRobot = true;
        break;
      }
    }
    infile.close();
  }

  return _isAnkiDevRobot;
}

} // namespace Vector
} // namespace Anki
