/**
 * File: OSState.h
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

#ifndef __Victor_OSState_H__
#define __Victor_OSState_H__

#include "coretech/common/shared/types.h"
#include "util/singleton/dynamicSingleton.h"
#include "json/json.h"

#include <functional>
#include <string>

// Forward declarations
namespace webots {
  class Supervisor;
}
namespace Anki {
  namespace Vector {
    class DASManager;
  }
}

namespace Anki {
namespace Vector {

enum class DesiredCPUFrequency : uint32_t {
  Automatic = 0,
  Manual200MHz = 200000,
  Manual400Mhz = 400000,
  Manual533Mhz = 533333,
};

class OSState : public Util::DynamicSingleton<OSState>
{
  ANKIUTIL_FRIEND_SINGLETON(OSState);

public:
  // Public types
  typedef enum Alert {
    None = 0,
    Yellow = 1,
    Red = 2
  } Alert;

  // System-wide memory stats
  typedef struct MemoryInfo {
    // Total memory, in kB
    uint32_t totalMem_kB = 0;
    // Memory available to processes, in kB
    uint32_t availMem_kB = 0;
    // Unused memory, in kB
    uint32_t freeMem_kB = 0;
    // "Memory pressure" aka (total / avail)
    uint32_t pressure = 0;
    // Alert level for current pressure
    Alert alert = Alert::None;
  } MemoryInfo;

  // Wifi info stats
  typedef struct WifiInfo {
    uint64_t rx_bytes = 0;
    uint64_t tx_bytes = 0;
    uint64_t rx_errors = 0;
    uint64_t tx_errors = 0;
    Alert alert = Alert::None;
  } WifiInfo;

  // Filesystem space stats
  typedef struct DiskInfo {
    // Total space, in kB
    uint32_t total_kB = 0;
    // Space available to non-root users, in kB
    uint32_t avail_kB = 0;
    // Unused space, in kB
    uint32_t free_kB = 0;
    // "Disk pressure" aka (total / avail)
    uint32_t pressure = 0;
    // Alert level for current pressure
    Alert alert = Alert::None;
  } DiskInfo;

// Public destructor
~OSState();

  void Update(BaseStationTime_t currTime_nanosec);

  RobotID_t GetRobotID() const;
  void SetRobotID(RobotID_t robotID);

  // Set how often state should be updated.
  // Affects how often the freq and temperature is updated.
  // Default is 0 ms which means never update.
  // You should leave this at zero only if you don't
  // ever care about CPU freq and temperature.
  void SetUpdatePeriod(uint32_t milliseconds);

  void SendToWebVizCallback(const std::function<void(const Json::Value&)>& callback);

  // Returns true if CPU frequency falls below kNominalCPUFreq_kHz
  bool IsCPUThrottling() const;

  // Returns current CPU frequency
  uint32_t GetCPUFreq_kHz() const;

  // Returns temperature in Celsius
  uint32_t GetTemperature_C() const;

  // set specific CPU frequency, or reset to automatic
  void SetDesiredCPUFrequency(DesiredCPUFrequency freq);

  // Returns uptime (and idle time) in seconds
  float GetUptimeAndIdleTime(float &idleTime_s) const;

  // Get system-wide memory info
  // Values are fetched once per update period
  void GetMemoryInfo(MemoryInfo & info) const;

  // Get current wifi info.
  // Values are fetched from wlan device on each call.
  // Returns true on success, false on error.
  bool GetWifiInfo(WifiInfo & info) const;

  // Get disk info for given path.
  // Values are fetched from file system on each call.
  // Returns true on success, false on error.
  bool GetDiskInfo(const std::string & path, DiskInfo & info) const;

  // Returns data about CPU times
  void GetCPUTimeStats(std::vector<std::string> & stats) const;

  // Returns our ip address
  const std::string& GetIPAddress(bool update = false);

  // Returns whether or not the IP address is "valid"
  // which currently just means it is an ipv4 address and it's not a link-local IP
  bool IsValidIPAddress(const std::string& ip) const;

  // Returns the SSID of the connected wifi network
  const std::string& GetSSID(bool update = false);

  // Returns our mac address
  std::string GetMACAddress() const;

  uint64_t GetWifiTxBytes() const;
  uint64_t GetWifiRxBytes() const;

  // Returns the ESN (electronic serial number) as a u32
  u32 GetSerialNumber()
  {
    const std::string& serialNum = GetSerialNumberAsString();
    if(!serialNum.empty())
    {
      return static_cast<u32>(std::stoul(serialNum, nullptr, 16));
    }

    return 0;
  }

  // Returns the ESN (electronic serial number) as a string
  const std::string& GetSerialNumberAsString();

  // Returns the os build version (time of build)
  const std::string& GetOSBuildVersion();

  void GetOSBuildVersion(int& major, int& minor, int& incremental, int& build) const;

  // Returns "major.minor.build" for reporting to DAS
  const std::string& GetRobotVersion();

  const std::string& GetBuildSha();

  // Returns the semi-unique name of this robot, Vector_XYXY
  // Where X is a letter and Y is a digit
  // The name can change over the lifetime of the robot
  const std::string& GetRobotName() const;

  // Return GUID string generated each time robot boots
  const std::string& GetBootID();

  // Returns whether or not the robot has booted in recovery mode
  // which is done by holding the backpack button down for ~12 seconds
  // while robot is on charger
  bool IsInRecoveryMode();

  // True if this current boot of the robot was the result of an automatic reboot, as opposed to the user
  // turning off the robot or it powering off for some other reason, like a dead battery
  bool RebootedForMaintenance() const;

  // True if we've synced time with a time server
  bool IsWallTimeSynced() const;

  // True if timezone is set (and therefore we can get local time)
  bool HasTimezone() const;

  // True if user space is secure
  bool IsUserSpaceSecure();

  // True if this is a disclaimer bot for internal Anki Dev use
  bool IsAnkiDevRobot();

  // Reads CPU times data
  void UpdateCPUTimeStats() const;
  
protected:
   // Return true if robot has a valid EMR.
   // This function is "off limits" to normal robot services
   // but allows vic-dasmgr to check for ESN without crashing.
  friend class DASManager;
  bool HasValidEMR() const;

private:
  // private ctor
  OSState();

  void UpdateWifiInfo();

  // Reads the current CPU frequency
  void UpdateCPUFreq_kHz() const;

  // Reads the temperature in Celsius
  void UpdateTemperature_C() const;

  // Reads the battery voltage in microvolts
  void UpdateBatteryVoltage_uV() const;

  // Reads uptime (and idle time) data
  void UpdateUptimeAndIdleTime() const;

  // Reads memory info data
  void UpdateMemoryInfo() const;

  uint32_t kNominalCPUFreq_kHz = 800000;

  std::string _ipAddress       = "";
  std::string _ssid            = "";
  std::string _serialNumString = "";
  std::string _osBuildVersion  = "";
  std::string _robotVersion    = "";
  std::string _buildSha        = "";
  std::string _bootID          = "";
  bool        _isUserSpaceSecure = false;
  bool        _hasValidIPAddress = false;
  bool        _isAnkiDevRobot = false;

  inline uint32_t GetPressure(uint32_t avail, uint32_t total) const
  {
    return (avail > 0 ? total / avail : std::numeric_limits<uint32_t>::max());
  }

  inline Alert GetAlert(uint32_t pressure, uint32_t yellow, uint32_t red) const
  {
    if (pressure > red) {
      return Alert::Red;
    }
    if (pressure > yellow) {
      return Alert::Yellow;
    }
    return Alert::None;
  }

}; // class OSState

} // namespace Vector
} // namespace Anki

#endif /* __Victor_OSState_H__ */
