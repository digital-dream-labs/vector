/**
* File: victor/dasmgr/dasManager.cpp
*
* Description: DASManager class declarations
*
* Copyright: Anki, inc. 2018
*
*/

#include "dasManager.h"
#include "dasConfig.h"

#include "DAS/DAS.h"
#include "osState/osState.h"
#include "util/fileUtils/fileUtils.h"
#include "util/jsonWriter/jsonWriter.h"
#include "util/logging/logging.h"
#include "util/logging/DAS.h"
#include "util/string/stringUtils.h"

#include <log/logger.h>
#include <log/logprint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

// Imported types
using LogLevel = Anki::Util::LogLevel;

// Log options
#define LOG_CHANNEL "DASManager"

// Local constants
namespace {

  // How often do we process statistics? Counted by log records.
  constexpr const int PROCESS_STATS_INTERVAL = 1000;

  // JSON attribute keys
  constexpr const char * kDASGlobalsKey = "dasGlobals";
  constexpr const char * kSequenceKey = "sequence";
  constexpr const char * kProfileIDKey = "profile_id";
  constexpr const char * kAllowUploadKey = "allow_upload";
  constexpr const char * kLastEventKey = "last_event_ts";

  // DAS column offsets
  // If field count changes, we need to update this code.
  // Unused columns are commented out until needed.
  static_assert(Anki::Util::DAS::FIELD_COUNT == 9, "DAS field count does not match declarations");
  constexpr const int DAS_NAME = 0;
  constexpr const int DAS_STR1 = 1;
  //constexpr const int DAS_STR2 = 2;
  constexpr const int DAS_STR3 = 3;
  constexpr const int DAS_STR4 = 4;
  constexpr const int DAS_INT1 = 5;
  //constexpr const int DAS_INT2 = 6;
  //constexpr const int DAS_INT3 = 7;
  //constexpr const int DAS_INT4 = 8;

  // Magic file used to expose state of DAS opt-in
  constexpr const char * kAllowUploadFile = "/run/das_allow_upload";
}

//
// Victor CSV log helpers
//
// Note that format here must match the format used by VictorLogger!
// Optional numeric fields are parsed as strings because they may be omitted from the log record.
// See also: util/logging/victorLogger_vicos.cpp
//
//static constexpr const char * EVENT_FORMAT = "@%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%s";
//

// Convert android log timestamp to Anki DAS equivalent
static inline int64_t GetTimeStamp(const AndroidLogEntry& logEntry)
{
  const int64_t tv_sec = logEntry.tv_sec;
  const int64_t tv_nsec = logEntry.tv_nsec;
  const int64_t ts = (tv_sec * 1000) + (tv_nsec / 1000000);
  return ts;
}

// Convert android log level to Anki DAS equivalent
inline LogLevel GetLogLevel(const AndroidLogEntry& logEntry)
{
  switch (logEntry.priority)
  {
    case ANDROID_LOG_SILENT:
    case ANDROID_LOG_DEFAULT:
    case ANDROID_LOG_VERBOSE:
    case ANDROID_LOG_DEBUG:
      return LogLevel::LOG_LEVEL_DEBUG;
    case ANDROID_LOG_INFO:
      return LogLevel::LOG_LEVEL_INFO;
    case ANDROID_LOG_WARN:
      return LogLevel::LOG_LEVEL_WARN;
    case ANDROID_LOG_ERROR:
    case ANDROID_LOG_FATAL:
    case ANDROID_LOG_UNKNOWN:
      return LogLevel::LOG_LEVEL_ERROR;
  }

  // This shouldn't happen, but if it does it's an error
  assert(false);
  return LogLevel::LOG_LEVEL_ERROR;
}

static inline void serialize(std::ostream & ostr, const std::string & key, const std::string & val)
{
  // TO DO: Guard against embedded quotes in value
  ostr << '"' << key << '"';
  ostr << ':';
  ostr << '"' << val << '"';
}

static inline void serialize(std::ostream & ostr, const std::string & key, int64_t val)
{
  ostr << '"' << key << '"';
  ostr << ':';
  ostr << val;
}

static inline void serialize(std::ostream & ostr, const std::string & key, LogLevel val)
{
  ostr << '"' << key << '"';
  ostr << ':';
  ostr << '"';
  switch (val)
  {
    case LogLevel::LOG_LEVEL_ERROR:
      ostr << "error";
      break;
    case LogLevel::LOG_LEVEL_WARN:
      ostr << "warning";
      break;
    case LogLevel::LOG_LEVEL_EVENT:
      ostr << "event";
      break;
    case LogLevel::LOG_LEVEL_INFO:
      ostr << "info";
      break;
    case LogLevel::LOG_LEVEL_DEBUG:
      ostr << "debug";
      break;
    case LogLevel::_LOG_LEVEL_COUNT:
      ostr << "count";
      break;
  }
  ostr << '"';
}

namespace Anki {
namespace Vector {

DASManager::DASManager(const DASConfig & dasConfig)
{
  _dasConfig = dasConfig;
  _logFilePath = Util::FileUtils::FullFilePath({_dasConfig.GetStoragePath(), "das.log"});
}

//
// Attempt to upload a JSON log file
// This is called on the worker thread.
// Return true on successful upload.
//
bool DASManager::PostToServer(const std::string& pathToLogFile)
{
  const std::string & json = Util::FileUtils::ReadFile(pathToLogFile);
  if (json.empty()) {
    return true;
  }

  std::string response;

  const bool success = DAS::PostToServer(_dasConfig.GetURL(), json, response);
  if (success) {
    LOG_DEBUG("DASManager.PostToServer.UploadSuccess", "Uploaded json of length %zu", json.length());
    ++_workerSuccessCount;
    if (!(_workerSuccessCount % 10)) {
      DASMSG(dasmgr_upload_success, "dasmgr.upload.stats", "Sent after every 10 successful uploads");
      DASMSG_SET(i1, _workerSuccessCount, "Worker success count");
      DASMSG_SET(i2, _workerFailCount, "Worker fail count");
      DASMSG_SET(i3, _workerDroppedCount, "Worker dropped count");
      DASMSG_SEND();
    }
    return true;
  } else {
    LOG_ERROR("DASManager.PostToServer.UploadFailed", "Failed to upload json of length %zu", json.length());
    ++_workerFailCount;
    DASMSG(dasmgr_upload_failed, "dasmgr.upload.failed", "Sent after each failed upload");
    DASMSG_SET(s1, response, "HTTP response");
    DASMSG_SET(i1, _workerSuccessCount, "Worker success count");
    DASMSG_SET(i2, _workerFailCount, "Worker fail count");
    DASMSG_SET(i3, _workerDroppedCount, "Worker dropped count");
    DASMSG_SEND();
    return false;
  }
}

void DASManager::PostLogsToServer()
{
  const auto & directories = {_dasConfig.GetStoragePath(), _dasConfig.GetBackupPath()};

  for (const auto & dir : directories) {
    const auto & jsonFiles = GetJsonFiles(dir);
    for (const auto & jsonFile : jsonFiles) {

      // Shortcut exit?
      if (_exiting) {
        LOG_DEBUG("DASManager.PostLogsToServer", "Server is exiting");
        return;
      }

      // Attempt upload
      const bool posted = PostToServer(jsonFile);
      if (!posted) {
        LOG_ERROR("DASManager.PostLogsToServer", "Failed to upload %s", jsonFile.c_str());
        return;
      }

      // Clean up file
      Util::FileUtils::DeleteFile(jsonFile);
    }
  }
}

void DASManager::BackupLogFiles()
{
  const auto & storagePath = _dasConfig.GetStoragePath();
  const auto & backupPath = _dasConfig.GetBackupPath();
  const auto backupQuota = _dasConfig.GetBackupQuota();

  const auto & jsonFiles = GetJsonFiles(storagePath);
  for (const auto & jsonFile : jsonFiles) {
    // Create the directory that will hold the json
    if (!Util::FileUtils::CreateDirectory(backupPath, false, true, S_IRWXU)) {
      LOG_ERROR("DASManager.BackupLogFiles.CreateBackupDir", "Failed to create backup path %s", backupPath.c_str());
      return;
    }
    if (Util::FileUtils::GetDirectorySize(backupPath) > (ssize_t) backupQuota) {
      LOG_INFO("DASManager.BackupLogFiles.QuotaExceeded", "Exceeded quota for %s", backupPath.c_str());
      return;
    }
    LOG_DEBUG("DASManager.BackupLogFiles.MovingFile", "Moving %s into %s", jsonFile.c_str(), backupPath.c_str());
    (void) Util::FileUtils::MoveFile(backupPath, jsonFile);
  }
}

void DASManager::PurgeBackupFiles()
{
  LOG_DEBUG("DASManager.PurgeBackupFiles", "Purge backup files");
  const auto & backupPath = _dasConfig.GetBackupPath();
  const auto & jsonFiles = GetJsonFiles(backupPath);
  for (const auto & jsonFile : jsonFiles) {
    LOG_DEBUG("DASManager.PurgeBackupFiles", "Purge %s", jsonFile.c_str());
    Util::FileUtils::DeleteFile(jsonFile);
  }
}

void DASManager::EnforceStorageQuota()
{
  //
  // Delete files to make room for incoming data.
  // GetJsonFiles() returns a sorted list so we remove the oldest files first.
  //
  const ssize_t quota = (ssize_t) _dasConfig.GetStorageQuota();
  const ssize_t fileThresholdSize = (ssize_t) _dasConfig.GetFileThresholdSize();
  const auto & path = _dasConfig.GetStoragePath();

  LOG_DEBUG("DASManager.EnforceStorageQuota", "Enforce quota %zd on path %s", quota, path.c_str());

  ssize_t directorySize = Util::FileUtils::GetDirectorySize(path);
  if (directorySize + fileThresholdSize > quota) {
    auto jsonFiles = GetJsonFiles(path);
    while (directorySize + fileThresholdSize > quota && !jsonFiles.empty()) {
      LOG_DEBUG("DASManager.EnforceQuota", "Delete %s", jsonFiles.front().c_str());
      Util::FileUtils::DeleteFile(jsonFiles.front());
      jsonFiles.erase(jsonFiles.begin());
      directorySize = Util::FileUtils::GetDirectorySize(path);
    }
  }
}

//
// Update magic state file to match current state flag
//
void DASManager::SetAllowUpload(bool allow_upload)
{
  using FileUtils = Anki::Util::FileUtils;

  LOG_DEBUG("DASManager.SetAllowUpload", "allow_upload=%d", allow_upload);

  _allow_upload = allow_upload;

  if (allow_upload && !FileUtils::FileExists(kAllowUploadFile)) {
    LOG_DEBUG("DASManager.SetAllowUpload", "Create %s", kAllowUploadFile);
    if (!FileUtils::TouchFile(kAllowUploadFile)) {
      LOG_ERROR("DASManager.SetAllowUpload", "Unable to create %s", kAllowUploadFile);
    }
  } else if (!allow_upload && FileUtils::FileExists(kAllowUploadFile)) {
    LOG_DEBUG("DASManager.SetAllowUpload", "Delete %s", kAllowUploadFile);
    FileUtils::DeleteFile(kAllowUploadFile);
  }
}

std::string DASManager::ConvertLogEntryToJson(const AndroidLogEntry & logEntry)
{
  // These values are always set by library so we don't need to check them
  DEV_ASSERT(logEntry.tag != nullptr, "DASManager.ParseLogEntry.InvalidTag");
  DEV_ASSERT(logEntry.message != nullptr, "DASManager.ParseLogEntry.InvalidMessage");

  // Anki::Util::StringSplit ignores trailing separator so don't use it
  std::vector<std::string> values;
  const char * pos = logEntry.message+1; // (skip leading event marker)
  while (1) {
    const char * end = strchr(pos, Anki::Util::DAS::FIELD_MARKER);
    if (end == nullptr) {
      values.push_back(std::string(pos));
      break;
    }
    values.push_back(std::string(pos, (size_t)(end-pos)));
    pos = end+1;
  }

  if (values.size() < Anki::Util::DAS::FIELD_COUNT) {
    LOG_ERROR("DASManager.ConvertLogEntry", "Unable to parse %s from %s (%zu != %d)",
              logEntry.message, logEntry.tag, values.size(), Anki::Util::DAS::FIELD_COUNT);
    return "";
  }

  const auto & name = values[DAS_NAME];
  if (name.empty()) {
    LOG_ERROR("DASManager.ConvertLogEntryToJson", "Missing event name");
    return "";
  }

  // Is this a recycled event?
  const auto ts = GetTimeStamp(logEntry);
  if (ts <= _first_event_ts) {
    return "";
  }

  _last_event_ts = ts;

  //
  // DAS manager uses magic event names to track global state.
  // These event names are declared in a common header (DAS.h)
  // so they can be shared with other services.
  //
  // If magic event names change, this code should be reviewed for compatibility.
  //
  if (name == DASMSG_FEATURE_START) {
    _feature_run_id = values[DAS_STR3];
    _feature_type = values[DAS_STR4];
  } else if (name == DASMSG_BLE_CONN_ID_START) {
    _ble_conn_id = values[DAS_STR1];
  } else if (name == DASMSG_BLE_CONN_ID_STOP) {
    _ble_conn_id.clear();
  } else if (name == DASMSG_WIFI_CONN_ID_START) {
    _wifi_conn_id = values[DAS_STR1];
  } else if (name == DASMSG_WIFI_CONN_ID_STOP) {
    _wifi_conn_id.clear();
  } else if (name == DASMSG_PROFILE_ID_START) {
    _profile_id = values[DAS_STR1];
  } else if (name == DASMSG_PROFILE_ID_STOP) {
    _profile_id.clear();
  } else if (name == DASMSG_DAS_ALLOW_UPLOAD) {
    const auto i1 = std::atoi(values[DAS_INT1].c_str());
    const bool allow_upload = (i1 != 0);
    if (_allow_upload && !allow_upload) {
      // User has opted out of data collection
      _purge_backup_files = true;
    }
    SetAllowUpload(allow_upload);
  }

  std::ostringstream ostr;
  ostr << '{';
  serialize(ostr, "source", logEntry.tag);
  ostr << ',';
  serialize(ostr, "ts", ts);
  ostr << ',';
  serialize(ostr, "seq", (int64_t) _seq++);
  ostr << ',';
  serialize(ostr, "level", GetLogLevel(logEntry));
  ostr << ',';
  serialize(ostr, "robot_id", _robot_id);
  ostr << ',';
  serialize(ostr, "robot_version", _robot_version);
  ostr << ',';
  serialize(ostr, "boot_id", _boot_id);
  ostr << ',';
  serialize(ostr, "profile_id", _profile_id);
  ostr << ',';
  serialize(ostr, "feature_type", _feature_type);
  ostr << ',';
  serialize(ostr, "feature_run_id", _feature_run_id);

  if (!_ble_conn_id.empty()) {
    ostr << ',';
    serialize(ostr, "ble_conn_id", _ble_conn_id);
  }

  if (!_wifi_conn_id.empty()) {
    ostr << ',';
    serialize(ostr, "wifi_conn_id", _wifi_conn_id);
  }

  static const std::vector<std::string> keys =
    {"event", "s1", "s2", "s3", "s4", "i1", "i2", "i3", "i4", "uptime_ms"};

  const size_t n = std::min(keys.size(), values.size());
  for (unsigned int i = 0 ; i < n; ++i) {
    const std::string & value = values[i];
    if (value.empty()) {
      continue;
    }
    const std::string & key = keys[i];
    ostr << ',';
    if (key[0] == 'i' || key[0] == 'u') {
      serialize(ostr, key, std::atoll(value.c_str()));
    } else {
      serialize(ostr, key, value);
    }
  }

  ostr << '}';
  return ostr.str();
}
//
// Process a log entry
//
void DASManager::ProcessLogEntry(const AndroidLogEntry & logEntry)
{
  const char * message = logEntry.message;
  assert(message != nullptr);

  ++_entryCount;

  // Does this record look like a DAS entry?
  if (*message != Anki::Util::DAS::EVENT_MARKER) {
    return;
  } else if (message[1] == Anki::Util::DAS::EVENT_MARKER) {
    _gotTerminateEvent = true;
    return;
  }

  _eventCount++;

  const std::string & json = ConvertLogEntryToJson(logEntry);
  if (json.empty()) {
    return;
  }

  // Append the JSON object to the array stored in the logfile
  if (!_logFile.is_open()) {
    _logFile.open(_logFilePath, std::ios::out | std::ofstream::binary | std::ofstream::ate);
  }

  off_t logFilePos = (off_t) _logFile.tellp();
  if (logFilePos == 0) {
    // This is a new file. Start with '[' to open the array
    _logFile << '[';
  } else {
    // Rewind 1 spot and replace the ']' with a ',' to continue the array
    _logFile.seekp(-1, std::ofstream::cur);
    _logFile << ',';
  }

  // Append the JSON object and close the array
  _logFile << json << ']';

}

void DASManager::RollLogFile()
{
  // Close current file
  _logFile.close();

  // Rename current file
  const std::string& fileName = GetPathNameForNextJsonLogFile();
  Util::FileUtils::MoveFile(fileName, _logFilePath);

  // Reset flush time
  _last_flush_time = std::chrono::steady_clock::now();

  // Enqueue upload task?
  if (_allow_upload && !_uploading && !_exiting) {
    auto uploadTask = [this]() {
      _uploading = true;
      PostLogsToServer();
      _uploading = false;
    };
    _worker.Wake(uploadTask, "uploadTask");
  }

  // Enqueue quota task?
  if (!_exiting) {
    auto quotaTask = [this]() {
      EnforceStorageQuota();
    };
    _worker.Wake(quotaTask, "quotaTask");
  }
}

//
// Log some process stats
//
void DASManager::ProcessStats()
{
  LOG_DEBUG("DASManager.ProcessStats.QueueStats",
            "entries=%llu events=%llu "
            "workerSuccess=%u workerFail=%u workerDropped=%u",
            _entryCount, _eventCount,
            (uint32_t) _workerSuccessCount,
            (uint32_t) _workerFailCount,
            (uint32_t) _workerDroppedCount);

  static struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) == 0) {
    /* maxrss = maximum resident set size */
    /* ixrss = integral shared memory size */
    /* idrss = integral unshared data size */
    /* isrss = integral unshared stack size */
    LOG_DEBUG("DASManager.ProcessStats.MemoryStats",
      "maxrss=%ld ixrss=%ld idrss=%ld isrss=%ld",
      ru.ru_maxrss, ru.ru_ixrss, ru.ru_idrss, ru.ru_isrss);
  }

}

//
// Get Seconds elapsed since last upload to server
//
uint32_t DASManager::GetSecondsSinceLastFlush()
{
  auto end = std::chrono::steady_clock::now();
  if ((TimePoint) _last_flush_time >= end) {
    return 0;
  }
  auto diff = end - (TimePoint) _last_flush_time;
  return (uint32_t) std::chrono::duration_cast<std::chrono::seconds>(diff).count();
}

std::vector<std::string> DASManager::GetJsonFiles(const std::string& path)
{
  auto jsonFiles = Util::FileUtils::FilesInDirectory(path, true, ".json", false);

  std::sort(jsonFiles.begin(), jsonFiles.end());

  return jsonFiles;
}

uint32_t DASManager::GetNextIndexForJsonFile()
{
  uint32_t index = 0;
  const auto & storagePaths = {_dasConfig.GetStoragePath(), _dasConfig.GetBackupPath()};

  for (const auto & path : storagePaths) {
    const auto & jsonFiles = GetJsonFiles(path);

    if (!jsonFiles.empty()) {
      const std::string& lastFile = jsonFiles.back();
      const std::string& filename = Util::FileUtils::GetFileName(lastFile, true, true);
      uint32_t next_index = (uint32_t) (std::atol(filename.c_str()) + 1);
      if (next_index > index) {
        index = next_index;
      }
    }
  }

  return index;
}

std::string DASManager::GetPathNameForNextJsonLogFile()
{
  uint32_t index = GetNextIndexForJsonFile();

  char filename[19] = {'\0'};
  std::snprintf(filename, sizeof(filename) - 1, "%012u.json", index);
  return Util::FileUtils::FullFilePath({_dasConfig.GetStoragePath(), filename});
}


static void LoadGlobals(Json::Value & json, const std::string & path)
{
  // Read json from persistent storage
  Json::Reader reader;
  std::ifstream ifs(path);

  if (!reader.parse(ifs, json)) {
    const auto & errors = reader.getFormattedErrorMessages();
    LOG_ERROR("DASManager.LoadGlobals", "Failed to parse [%s] (%s)",
      path.c_str(), errors.c_str());
    return;
  }
}

void DASManager::LoadTransientGlobals(const std::string & path)
{
  // Read json from transient storage
  Json::Value json;
  LoadGlobals(json, path);

  // Read transient values from json
  const auto & dasGlobals = json[kDASGlobalsKey];
  if (!dasGlobals.isObject()) {
    LOG_ERROR("DASManager.LoadTransientGlobals", "Invalid json object");
    return;
  }

  const auto & sequence = dasGlobals[kSequenceKey];
  if (sequence.isNumeric()) {
    _seq = sequence.asUInt64();
  }

  const auto & last_event_ts = dasGlobals[kLastEventKey];
  if (last_event_ts.isNumeric()) {
    _last_event_ts = last_event_ts.asInt64();
  }
}

void DASManager::LoadPersistentGlobals(const std::string & path)
{
  // Read json from path
  Json::Value json;
  LoadGlobals(json, path);

  // Read values from json
  const auto & dasGlobals = json[kDASGlobalsKey];
  if (!dasGlobals.isObject()) {
    LOG_ERROR("DASManager.LoadPersistentGlobals", "Invalid json object");
    return;
  }

  const auto & profile_id = dasGlobals[kProfileIDKey];
  if (profile_id.isString()) {
    _profile_id = profile_id.asString();
  }

  const auto & allow_upload = dasGlobals[kAllowUploadKey];
  if (allow_upload.isBool()) {
    _allow_upload = allow_upload.asBool();
  }
}

void DASManager::LoadGlobalState()
{
  // Programmatic defaults
  _feature_type = "system";
  _feature_run_id = Anki::Util::GetUUIDString();

  // Get persistent values from OS
  {
    auto * osState = Anki::Vector::OSState::getInstance();
    DEV_ASSERT(osState != nullptr, "DASManager.LoadGlobalState.InvalidOSState");
    if (osState->HasValidEMR()) {
      _robot_id = Anki::Util::StringToLower(osState->GetSerialNumberAsString());
    } else {
      LOG_ERROR("DASManager.LoadGlobalState.InvalidEMR", "INVALID EMR - NO ESN");
    }
    _robot_version = osState->GetRobotVersion();
    _boot_id = osState->GetBootID();
    Vector::OSState::removeInstance();
  }

  // Get transient globals from transient storage
  const std::string & transient_globals_path = _dasConfig.GetTransientGlobalsPath();
  if (!transient_globals_path.empty() && Util::FileUtils::FileExists(transient_globals_path)) {
    LoadTransientGlobals(transient_globals_path);
  }

  // Get persistent globals from persistent storage
  const std::string & persistent_globals_path = _dasConfig.GetPersistentGlobalsPath();
  if (!persistent_globals_path.empty() && Util::FileUtils::FileExists(persistent_globals_path)) {
    LoadPersistentGlobals(persistent_globals_path);
  }

  //
  // LAST timestamp from previous run becomes FIRST timestamp for current run.
  // This allows us to avoid processing events multiple times if the service
  // is restarted without clearing the log buffer.
  //
  // Timestamps are saved to transient storage and will be cleared automatically
  // when robot is rebooted.
  //
  // Note that android log buffer uses a realtime clock, not a steady clock, and
  // timestamps may drift backward when system clock is adjusted.  If this happens
  // during service restart, events may be dropped.
  //
  if (_last_event_ts != 0) {
    _first_event_ts = _last_event_ts;
  }

  // Call out global state for diagnostics
  LOG_DEBUG("DASManager.LoadGlobalState",
            "robot_id=%s robot_version=%s boot_id=%s sequence=%llu profile_id=%s allow_upload=%d",
            _robot_id.c_str(), _robot_version.c_str(), _boot_id.c_str(), _seq, _profile_id.c_str(), _allow_upload);

}

static void SaveGlobals(const Json::Value & json, const std::string & path)
{
  // Write json to temp file
  const auto & tmp = path + ".tmp";
  if (Util::FileUtils::FileExists(tmp)) {
    Util::FileUtils::DeleteFile(tmp);
  }

  try {
    Json::StyledWriter writer;
    std::ofstream ofs(tmp);
    ofs << writer.write(json);
  } catch (const std::exception & ex) {
    LOG_ERROR("DASManager.SaveGlobals", "Unable to write %s (%s)", tmp.c_str(), ex.what());
    return;
  }

  // Move temp file into place
  // Yes, the arguments are MoveFile(dest, src)
  const bool ok = Util::FileUtils::MoveFile(path, tmp);
  if (!ok) {
    LOG_ERROR("DASManager.SaveGlobals", "Unable to move %s to %s", tmp.c_str(), path.c_str());
  }
}

void DASManager::SaveTransientGlobals(const std::string & path)
{
  // Construct json container
  Json::Value json;
  json[kDASGlobalsKey][kSequenceKey] = _seq;
  json[kDASGlobalsKey][kLastEventKey] = _last_event_ts;

  // Save json to file
  SaveGlobals(json, path);
}

void DASManager::SavePersistentGlobals(const std::string & path)
{
  // Construct json container
  Json::Value json;
  json[kDASGlobalsKey][kProfileIDKey] = _profile_id;
  json[kDASGlobalsKey][kAllowUploadKey] = _allow_upload;

  // Save json to file
  SaveGlobals(json, path);
}

void DASManager::SaveGlobalState()
{
  // Save transient globals to transient storage
  const std::string & transient_globals_path = _dasConfig.GetTransientGlobalsPath();
  if (!transient_globals_path.empty()) {
    SaveTransientGlobals(transient_globals_path);
  }

  // Save persistent globals to persistent storage
  const std::string & persistent_globals_path = _dasConfig.GetPersistentGlobalsPath();
  if (!persistent_globals_path.empty()) {
    SavePersistentGlobals(persistent_globals_path);
  }
}

//
// Process log entries until error or the termination event is read ("@@")
//
Result DASManager::Run(const bool & shutdown)
{
  // Validate configuration
  if (_dasConfig.GetURL().empty()) {
    LOG_ERROR("DASManager.Run.InvalidURL", "Invalid URL");
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  // Validate storage path
  const auto & storagePath = _dasConfig.GetStoragePath();
  if (storagePath.empty()) {
    LOG_ERROR("DASManager.Run.InvalidStoragePath", "Invalid Storage Path");
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  // Create the directory that will hold the json
  if (!Util::FileUtils::CreateDirectory(storagePath, false, true, S_IRWXU)) {
    LOG_ERROR("DASManager.Run.CreateStoragePathFailure",
              "Failed to create storage path %s",
              storagePath.c_str());
    return RESULT_FAIL_INVALID_PARAMETER;
  }

  LoadGlobalState();

  // Initialize magic state file
  SetAllowUpload(_allow_upload);

  // Log global parameters so we can match up log data with DAS records
  LOG_INFO("DASManager.Run", "robot_id=%s robot_version=%s boot_id=%s feature_run_id=%s",
           _robot_id.c_str(), _robot_version.c_str(), _boot_id.c_str(), _feature_run_id.c_str());

  // Make sure we have room to write logs
  EnforceStorageQuota();

  // If we have unsent log files, attempt to send them now
  if (_allow_upload && !_exiting) {
    auto uploadTask = [this]() {
      _uploading = true;
      PostLogsToServer();
      _uploading = false;
    };
    _worker.Wake(uploadTask, "uploadTask");
  }

  //
  // Android log API is documented here:
  // https://android.googlesource.com/platform/system/core/+/master/liblog/README
  //

  // Open the log buffer
  struct logger_list * log = android_logger_list_open(LOG_ID_MAIN, ANDROID_LOG_RDONLY, 0, 0);
  if (log == nullptr) {
    // If we can't open the log buffer, return an appropriate error code.
    LOG_ERROR("DASManager.Run", "Unable to open android logger (errno %d)", errno);
    return RESULT_FAIL_FILE_OPEN;
  }

  LOG_DEBUG("DASManager.Run", "Begin reading loop");

  //
  // Read log records until shutdown flag becomes true.
  //
  const auto flushInterval = _dasConfig.GetFlushInterval();
  const auto fileThresholdSize = _dasConfig.GetFileThresholdSize();

  Result result = RESULT_OK;

  _last_flush_time = std::chrono::steady_clock::now();

  // Run forever until error or termination event ("@@") is read
  while (true) {
    struct log_msg logmsg;
    int rc = android_logger_list_read(log, &logmsg);
    if (rc <= 0 ) {
      // If we can't read the log buffer, return an appropriate error.
      LOG_ERROR("DASManager.Run", "Log read error %d (%s)", rc, strerror(errno));
      result = RESULT_FAIL_FILE_READ;
      break;
    }

    AndroidLogEntry logEntry;
    rc = android_log_processLogBuffer(&logmsg.entry_v1, &logEntry);
    if (rc != 0) {
      // Malformed log entry? Report the problem but keep reading.
      LOG_ERROR("DASManager.Run", "Unable to process log buffer (error %d)", rc);
      continue;
    }

    // Dispose of this log entry
    ProcessLogEntry(logEntry);

    if(_gotTerminateEvent)
    {
      if(shutdown)
      {
        LOG_INFO("DASManager.Run.Shutdown", "");
        break;
      }
      else
      {
        _gotTerminateEvent = false;
        // This can happen if, for some reason, we never parsed
        // a terminate event from a previous run and the event was
        // still sitting in the log buffer when dasManager was started.
        // We should ignore it in this case
        LOG_INFO("DASManager.Run.InvalidTerminateEvent",
                 "Got terminate event but we aren't shutting down");
      }
    }

    // If we have exceeded the threshold size, roll the log file now.
    // If we are allowed to upload and have gone over the flush interval, roll the log file now.
    // If we are NOT allowed to upload, let the file keep growing to avoid fragmentation.
    //
    bool rollNow = false;
    if (_logFile.tellp() > fileThresholdSize) {
      rollNow = true;
    } else if (_allow_upload && GetSecondsSinceLastFlush() > flushInterval) {
      rollNow = true;
    }

    if (rollNow) {
      RollLogFile();
    }

    if (_purge_backup_files) {
      auto purgeTask = [this]() {
        PurgeBackupFiles();
      };
      _worker.Wake(purgeTask, "purgeTask");
      _purge_backup_files = false;
    }

    // Print stats at regular intervals
    if (_entryCount % PROCESS_STATS_INTERVAL == 0) {
      ProcessStats();
    }

  }

  LOG_DEBUG("DASManager.Run", "Cleaning up");

  _exiting = true;

  android_logger_list_close(log);

  RollLogFile();

  //
  // If uploads are allowed, move transient logs to persistent storage
  // so they can be sent after service restarts.
  //
  // Note shutdown task is performed as a synchronous operation to
  // ensure that task queue is empty at shutdown.
  //
  auto shutdownTask = [this]() {
    if (_allow_upload) {
      BackupLogFiles();
    }
  };
  _worker.WakeSync(shutdownTask, "shutdownTask");

  // Report final stats
  ProcessStats();

  SaveGlobalState();

  sync();

  LOG_INFO("DASManager.Run", "Done(result %d)", result);
  return result;
}

} // end namespace Vector
} // end namespace Anki
