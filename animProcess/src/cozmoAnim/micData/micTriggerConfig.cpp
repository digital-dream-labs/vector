/**
* File: micTriggerConfig.cpp
*
* Author: Lee Crippen
* Created: 06/02/2018
*
* Description: Loads and holds mic triggerword configuration data.
*
* Copyright: Anki, Inc. 2018
*
*/

#include "cozmoAnim/micData/micTriggerConfig.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "json/json.h"

namespace Anki {
namespace Vector {
namespace MicData {

namespace {
  const std::string kModelTypeStrings[] = {
    "size_1mb",
    "size_500kb",
    "size_250kb",
    "INVALID"
  };
  constexpr auto kNumModelTypeStrings = sizeof(kModelTypeStrings)/sizeof(kModelTypeStrings[0]);
  static_assert(kNumModelTypeStrings == ((int)MicTriggerConfig::ModelType::Count) + 1,
                "Number of model types does not match length of corresponding string list");

  const std::string& kLocaleKey = "locale";
  const std::string& kModelListKey = "modelList";
  const std::string& kModelTypeKey = "modelType";
  const std::string& kDefaultModelTypeKey = "defaultModelType";
  const std::string& kDataDirectoryKey = "dataDirectory";
  const std::string& kNetFileNameKey = "netFileName";
  const std::string& kSearchFileListKey = "searchFileList";
  const std::string& kSearchFileIndexKey = "searchFileIndex";
  const std::string& kDefaultSearchFileIndexKey = "defaultSearchFileIndex";
  const std::string& kSearchFileNameKey = "searchFileName";
}

// unique_ptr of a forward-declared class type demands this
MicTriggerConfig::MicTriggerConfig() = default;
MicTriggerConfig::~MicTriggerConfig() = default;

bool MicTriggerConfig::Init(const std::string& triggerKey, const Json::Value& initData)
{
  _localeTriggerDataMap.clear();
  
  // Verify this is a object of triggers
  if (!initData.isObject())
  {
    LOG_ERROR("MicTriggerConfig.Init.JsonData", "Mic init data is not an object.");
    return false;
  }
  
  const Json::Value& triggerData = initData[triggerKey];
  
  // Verify this is a list of locale data
  if (!triggerData.isArray())
  {
    LOG_ERROR("MicTriggerConfig.Init.JsonData", "Mic trigger data is not an array.");
    return false;
  }

  for (const Json::Value& localeData : triggerData)
  {
    if (!localeData.isObject())
    {
      LOG_ERROR("MicTriggerConfig.Init.JsonData", "Locale config data is not an object.");
      continue;
    }

    // Get the Locale type
    if (!localeData.isMember(kLocaleKey) || !localeData[kLocaleKey].isString())
    {
      LOG_ERROR("MicTriggerConfig.LocaleJsonData",
                "Locale data item does not contain locale type.\n%s",
                Json::StyledWriter().write(localeData).c_str());
      continue;
    }
    const auto& nextLocale = Util::Locale::LocaleFromString(localeData[kLocaleKey].asString());
    if (_localeTriggerDataMap.find(nextLocale) != _localeTriggerDataMap.end())
    {
      LOG_ERROR("MicTriggerConfig.LocaleTypeUnique",
                "Data for locale %s already added. Ignoring.",
                nextLocale.ToString().c_str());
      continue;
    }

    // Load the default model type for this locale
    if (!localeData.isMember(kDefaultModelTypeKey) || !localeData[kDefaultModelTypeKey].isString())
    {
      LOG_ERROR("MicTriggerConfig.LocaleDefaultModelType",
                "Locale data item does not contain default model type.\n%s",
                Json::StyledWriter().write(localeData).c_str());
      continue;
    }
    const auto& defaultModelType = ModelTypeFromString(localeData[kDefaultModelTypeKey].asString());

    // Load the model data list, make sure there's at least one entry
    if (!localeData.isMember(kModelListKey) || !localeData[kModelListKey].isArray())
    {
      LOG_ERROR("MicTriggerConfig.LocaleModelData",
                "Locale data item does not contain model data.\n%s",
                Json::StyledWriter().write(localeData).c_str());
      continue;
    }
    const Json::Value& modelDataList = localeData[kModelListKey];
    auto newModelDataMap = InitModelData(modelDataList);
    if (newModelDataMap.empty())
    {
      LOG_ERROR("MicTriggerConfig.LocaleModelData",
                "Locale data item model data is empty, ignoring.\n%s",
                Json::StyledWriter().write(localeData).c_str());
      continue;
    }

    LocaleTriggerData nextLocaleData{};
    nextLocaleData._defaultModelType = defaultModelType;
    nextLocaleData._modelDataMap = std::move(newModelDataMap);
    _localeTriggerDataMap[nextLocale] = std::move(nextLocaleData);
  }
  
  return !_localeTriggerDataMap.empty();
}

MicTriggerConfig::ModelDataMap MicTriggerConfig::InitModelData(const Json::Value& modelDataList)
{
  // Verify this is a list of model data
  if (!modelDataList.isArray())
  {
    LOG_ERROR("MicTriggerConfig.InitModelData.JsonData", "Model data is not an array.");
    return ModelDataMap{};
  }

  ModelDataMap newModelDataMap{};
  // Try to add each of the model data entries
  for (int i=0; i < modelDataList.size(); ++i)
  {
    const Json::Value& modelData = modelDataList[i];
    if (!modelData.isObject())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.JsonData", "Model data is not an object.");
      continue;
    }

    // Verify the model type
    if (!modelData.isMember(kModelTypeKey) || !modelData[kModelTypeKey].isString())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.ModelType",
                "Model data item does not contain model type.\n%s",
                Json::StyledWriter().write(modelData).c_str());
      continue;
    }
    auto nextModelType = ModelTypeFromString(modelData[kModelTypeKey].asString());
    if (nextModelType == ModelType::Count || newModelDataMap.find(nextModelType) != newModelDataMap.end())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.ModelType",
                "Model type %d(%s)not valid or already used",
                (int) nextModelType,
                kModelTypeStrings[(int) nextModelType].c_str());
      continue;
    }

    // Verify the data directory is specified
    if (!modelData.isMember(kDataDirectoryKey) || !modelData[kDataDirectoryKey].isString())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.DataDirectory",
                "Model data item does not contain DataDirectory.\n%s",
                Json::StyledWriter().write(modelData).c_str());
      continue;
    }
    const auto& dataDir = modelData[kDataDirectoryKey].asString();

    // Verify the net file name is specified
    if (!modelData.isMember(kNetFileNameKey) || !modelData[kNetFileNameKey].isString())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.NetFileName",
                "Model data item does not contain NetFileName.\n%s",
                Json::StyledWriter().write(modelData).c_str());
      continue;
    }
    const auto& netFileName = modelData[kNetFileNameKey].asString();

    // Verify the default search file index is specified
    if (!modelData.isMember(kDefaultSearchFileIndexKey) || !modelData[kDefaultSearchFileIndexKey].isInt())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.DefaultSearchFileIndex",
                "Model data item does not contain DefaultSearchFileIndex.\n%s",
                Json::StyledWriter().write(modelData).c_str());
      continue;
    }
    const auto defaultSearchFileIndex = modelData[kDefaultSearchFileIndexKey].asInt();

    // Verify the search file list is specified
    if (!modelData.isMember(kSearchFileListKey) || !modelData[kSearchFileListKey].isArray())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.SearchFileList",
                "Model data item does not contain SearchFileList.\n%s",
                Json::StyledWriter().write(modelData).c_str());
      continue;
    }
    
    auto newSearchFileMap = SearchFileMap{};
    const Json::Value& searchFileList = modelData[kSearchFileListKey];
    for (int i=0; i < searchFileList.size(); ++i)
    {
      const Json::Value& searchFileData = searchFileList[i];
      if (!searchFileData.isObject())
      {
        LOG_ERROR("MicTriggerConfig.InitModelData.SearchFileData", "SearchFile data is not an object.");
        continue;
      }

      // Verify the search file index
      if (!searchFileData.isMember(kSearchFileIndexKey) || !searchFileData[kSearchFileIndexKey].isIntegral())
      {
        LOG_ERROR("MicTriggerConfig.InitModelData.SearchFileIndex",
                  "Search file data item does not contain index.");
        continue;
      }
      auto searchFileIndex = searchFileData[kSearchFileIndexKey].asInt();
      if (newSearchFileMap.find(searchFileIndex) != newSearchFileMap.end())
      {
        LOG_ERROR("MicTriggerConfig.InitModelData.SearchFileIndex",
                  "SearchFileIndex %d already used", searchFileIndex);
        continue;
      }

      // Verify the search file name
      if (!searchFileData.isMember(kSearchFileNameKey) || !searchFileData[kSearchFileNameKey].isString())
      {
        LOG_ERROR("MicTriggerConfig.InitModelData.SearchFileName",
                  "Search file data item does not contain file name.");
        continue;
      }
      const auto& searchFileName = searchFileData[kSearchFileNameKey].asString();
      newSearchFileMap[searchFileIndex] = searchFileName;
    }

    if (newSearchFileMap.empty())
    {
      LOG_ERROR("MicTriggerConfig.InitModelData.SearchFiles",
                "Model data item does not contain SearchFiles.");
      continue;
    }

    auto newModelData = ModelData{};
    newModelData._dataDir = dataDir;
    newModelData._netFile = netFileName;
    newModelData._searchFileMap = std::move(newSearchFileMap);
    newModelData._defaultSearchFileIndex = defaultSearchFileIndex;
    newModelDataMap[nextModelType] = std::move(newModelData);
  }

  return newModelDataMap;
}

MicTriggerConfig::ModelType MicTriggerConfig::ModelTypeFromString(const std::string& modelTypeString)
{
  for (uint32_t i=0; i < kNumModelTypeStrings; ++i)
  {
    if (kModelTypeStrings[i] == modelTypeString)
    {
      return (ModelType) i;
    }
  }
  return ModelType::Count;
}

MicTriggerConfig::TriggerDataPaths MicTriggerConfig::GetTriggerModelDataPaths(Util::Locale locale,
                                                                              MicTriggerConfig::ModelType modelType,
                                                                              int searchFileIndex) const
{
  const auto& localeDataIter = _localeTriggerDataMap.find(locale);
  if (localeDataIter == _localeTriggerDataMap.end())
  {
    return TriggerDataPaths{};
  }

  const auto& modelDataMap = localeDataIter->second._modelDataMap;
  modelType = (modelType != ModelType::Count) ? modelType : localeDataIter->second._defaultModelType;
  const auto& modelDataIter = modelDataMap.find(modelType);
  if (modelDataIter == modelDataMap.end())
  {
    return TriggerDataPaths{};
  }

  const auto& searchFileMap = modelDataIter->second._searchFileMap;
  searchFileIndex = (searchFileIndex != -1) ? searchFileIndex : modelDataIter->second._defaultSearchFileIndex;
  const auto& searchFileIter = searchFileMap.find(searchFileIndex);
  if (searchFileIter == searchFileMap.end())
  {
    return TriggerDataPaths{};
  }

  auto triggerData = TriggerDataPaths{};
  triggerData._dataDir = modelDataIter->second._dataDir;
  triggerData._netFile = modelDataIter->second._netFile;
  triggerData._searchFile = searchFileIter->second;
  return triggerData;
}

std::vector<std::string> MicTriggerConfig::GetAllTriggerModelFiles() const
{
  std::vector<std::string> triggerDataList;
  for (const auto& localeDataKV : _localeTriggerDataMap)
  {
    for (const auto& modelDataKV : localeDataKV.second._modelDataMap)
    {
      auto netFilePath = Util::FileUtils::FullFilePath({modelDataKV.second._dataDir, modelDataKV.second._netFile});
      triggerDataList.push_back(std::move(netFilePath));
      for (const auto& searchFileKV : modelDataKV.second._searchFileMap)
      {
        auto searchFilePath = Util::FileUtils::FullFilePath({modelDataKV.second._dataDir, searchFileKV.second});
        triggerDataList.push_back(std::move(searchFilePath));
      }
    }
  }
  return triggerDataList;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string MicTriggerConfig::TriggerDataPaths::GenerateNetFilePath(const std::string& prefixPath) const
{
  return Util::FileUtils::FullFilePath( {prefixPath, _dataDir, _netFile} );
}

std::string MicTriggerConfig::TriggerDataPaths::GenerateSearchFilePath(const std::string& prefixPath) const
{
  return Util::FileUtils::FullFilePath( {prefixPath, _dataDir, _searchFile} );
}

} // namespace MicData
} // namespace Vector
} // namespace Anki
