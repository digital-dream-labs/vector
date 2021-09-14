/**
* File: victor/dasmgr/dasConfig.cpp
*
* Description: DASConfig class implementation
*
* Copyright: Anki, inc. 2018
*
*/

#include "dasConfig.h"
#include "json/json.h"
#include "util/logging/logging.h"
#include <fstream>

namespace Anki {
namespace Vector {

DASConfig::DASConfig(const std::string & url,
                     size_t file_threshold_size,
                     uint32_t flush_interval,
                     const std::string& storage_path,
                     size_t storage_quota,
                     const std::string& backup_path,
                     size_t backup_quota,
                     const std::string& persistent_globals_path,
                     const std::string& transient_globals_path) :
  _url(url),
  _file_threshold_size(file_threshold_size),
  _flush_interval(flush_interval),
  _storage_path(storage_path),
  _storage_quota(storage_quota),
  _backup_path(backup_path),
  _backup_quota(backup_quota),
  _persistent_globals_path(persistent_globals_path),
  _transient_globals_path(transient_globals_path)
{
}

std::unique_ptr<DASConfig> DASConfig::GetDASConfig(const Json::Value & json)
{
  if (!json.isObject()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidJSON", "Invalid json object");
    return nullptr;
  }

  const auto & dasConfig = json["dasConfig"];
  if (!dasConfig.isObject()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidDASConfig", "Invalid dasConfig");
    return nullptr;
  }

  const auto & url = dasConfig["url"];
  if (!url.isString()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidURL", "Invalid url attribute");
    return nullptr;
  }

  const auto & file_threshold_size = dasConfig["file_threshold_size"];
  if (!file_threshold_size.isUInt()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidFileThresholdSize", "Invalid file_threshold_size attribute");
    return nullptr;
  }

  const auto & flush_interval = dasConfig["flush_interval"];
  if (!flush_interval.isUInt()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidFlushInterval", "Invalid flush_interval attribute");
    return nullptr;
  }

  const auto & storage_path = dasConfig["storage_path"];
  if (!storage_path.isString()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidStoragePath", "Invalid storage_path attribute");
    return nullptr;
  }

  const auto & storage_quota = dasConfig["storage_quota"];
  if (!storage_quota.isUInt()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidStorageQuota", "Invalid storage_quota attribute");
    return nullptr;
  }

  const auto & backup_path = dasConfig["backup_path"];
  if (!backup_path.isString()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidBackupPath", "Invalid backup_path attribute");
    return nullptr;
  }

  const auto & backup_quota = dasConfig["backup_quota"];
  if (!backup_quota.isUInt()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidBackupQuota", "Invalid backup_quota attribute");
    return nullptr;
  }

  const auto & persistent_globals_path = dasConfig["persistent_globals_path"];
  if (!persistent_globals_path.isString()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidPersistentGlobalsPath", "Invalid persistent_globals_path attribute");
    return nullptr;
  }

  const auto & transient_globals_path = dasConfig["transient_globals_path"];
  if (!transient_globals_path.isString()) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidTransientGlobalsPath", "Invalid transient_globals_path attribute");
    return nullptr;
  }

  return std::make_unique<DASConfig>(url.asString(),
                                     file_threshold_size.asUInt(),
                                     flush_interval.asUInt(),
                                     storage_path.asString(),
                                     storage_quota.asUInt(),
                                     backup_path.asString(),
                                     backup_quota.asUInt(),
                                     persistent_globals_path.asString(),
                                     transient_globals_path.asString());

}

std::unique_ptr<DASConfig> DASConfig::GetDASConfig(const std::string & path)
{
  std::ifstream ifstream(path);
  Json::Reader reader;
  Json::Value json;

  if (!reader.parse(ifstream, json)) {
    LOG_ERROR("DASConfig.GetDASConfig.InvalidJsonFile", "Unable to parse json from %s", path.c_str());
    return nullptr;
  }

  return GetDASConfig(json);
}

} // end namespace Vector
} // end namespace Anki
