/**
* File: victor/dasmgr/dasConfig.h
*
* Description: DASConfig class declarations
*
* Copyright: Anki, inc. 2018
*
*/

#ifndef __victor_dasmgr_dasConfig_h
#define __victor_dasmgr_dasConfig_h

#include <string>

// Forward declarations
namespace Json {
  class Value;
}

namespace Anki {
namespace Vector {

class DASConfig
{
public:
  DASConfig() {}
  DASConfig(const std::string & url,
            size_t file_threshold_size,
            uint32_t flush_interval,
            const std::string& storage_path,
            size_t storage_quota,
            const std::string& backup_path,
            size_t backup_quota,
            const std::string & persistent_globals_path,
            const std::string & transient_globals_path);

  // DAS endpoint URL
  const std::string & GetURL() const { return _url; }

  // How big of a JSON log file do we create before we try to upload it
  size_t GetFileThresholdSize() const { return _file_threshold_size; }

  // How many seconds should we collect events before trying to upload to the server
  uint32_t GetFlushInterval() const { return _flush_interval; }

  // Where should we store the JSON log files as we create them
  // We expect this to be a tmpfs (RAM) instead of an on-disk location as we
  // don't want to wear out the eMMC in the robot
  const std::string & GetStoragePath() const { return _storage_path; }

  // How much space can we use for our JSON log files before we must start dropping
  // events.  This should only happen in extreme cases where we can't reach the DAS
  // server.
  size_t GetStorageQuota() const { return _storage_quota; }

  // Where should we store the JSON log files when we are shutting down.
  // We expect this to be an on-disk location that we can backup to
  // before a reboot.
  const std::string & GetBackupPath() const { return _backup_path; }

  // How much space can we use at the backup location before we have to stop accepting
  // new log files
  size_t GetBackupQuota() const { return _backup_quota; }

  // Where should we store persistent global variables?
  // We expect this to be an on-disk location that we can read at
  // startup and write at shutdown. We expect this storage to persist
  // when the robot reboots. If robot's persistent data is cleared,
  // these values are lost.
  const std::string & GetPersistentGlobalsPath() const { return _persistent_globals_path; }

  // Where should we store transient global variables?
  // We expect this to be a temporary file system that will be
  // initialized when robot boots but can be used to save
  // values when service restarts without reboot.
  const std::string & GetTransientGlobalsPath() const { return _transient_globals_path; }

  // Helper methods to parse DAS configuration file.
  // Helper methods return nullptr on error.
  // Configuration format looks like this:
  // {
  //   "dasConfig" : {
  //     "url": "string",
  //     "file_threshold_size" : uint,
  //     "flush_interval" : uint,
  //     "storage_path": string,
  //     "storage_quota": uint,
  //     "backup_path": string,
  //     "backup_quota": uint,
  //     "persistent_globals_path": string
  //     "transient_globals_path": string
  //   }
  // }
  //
  static std::unique_ptr<DASConfig> GetDASConfig(const Json::Value & json);
  static std::unique_ptr<DASConfig> GetDASConfig(const std::string & path);

private:
  std::string _url;
  size_t _file_threshold_size;
  uint32_t _flush_interval;
  std::string _storage_path;
  size_t _storage_quota;
  std::string _backup_path;
  size_t _backup_quota;
  std::string _persistent_globals_path;
  std::string _transient_globals_path;
};

} // end namespace Vector
} // end namespace Anki

#endif // __platform_dasmgr_dasConfig_h
