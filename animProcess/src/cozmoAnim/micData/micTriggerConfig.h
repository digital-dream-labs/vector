/**
* File: micTriggerConfig.h
*
* Author: Lee Crippen
* Created: 06/02/2018
*
* Description: Loads and holds mic triggerword configuration data.
*
* Copyright: Anki, Inc. 2018
*
*/

#ifndef __AnimProcess_CozmoAnim_MicTriggerConfig_H_
#define __AnimProcess_CozmoAnim_MicTriggerConfig_H_

#include "util/environment/locale.h"

#include <map>
#include <string>
#include <vector>


namespace Json
{
  class Value;
}

namespace Anki {
namespace Vector {
namespace MicData {

class MicTriggerConfig
{
public:
  MicTriggerConfig();
  ~MicTriggerConfig();
  
  bool Init(const std::string& triggerKey, const Json::Value& initData);

  enum class ModelType {
    size_1mb,
    size_500kb,
    size_250kb,
    Count
  };

  struct TriggerDataPaths
  {
    std::string _dataDir;
    std::string _netFile;
    std::string _searchFile;

    bool operator==(const TriggerDataPaths& other) const
    {
      return _dataDir == other._dataDir && _netFile == other._netFile && _searchFile == other._searchFile;
    }
    bool operator!=(const TriggerDataPaths& other) const
    {
      return !(*this == other);
    }

    bool IsValid() const { return !_dataDir.empty() && !_netFile.empty() && !_searchFile.empty(); }
    
    std::string GenerateNetFilePath(const std::string& prefixPath = "") const;

    std::string GenerateSearchFilePath(const std::string& prefixPath = "") const;
  };

  // Note 'Count' and '-1' values indicate to use default
  TriggerDataPaths GetTriggerModelDataPaths(Util::Locale locale,
                                            ModelType modelType = ModelType::Count,
                                            int searchFileIndex = -1) const;

  std::vector<std::string> GetAllTriggerModelFiles() const;
  
private:
  using SearchFileMap = std::map<int, std::string>;
  struct ModelData {
    std::string _dataDir;
    std::string _netFile;
    int _defaultSearchFileIndex;
    SearchFileMap _searchFileMap;
  };
  using ModelDataMap = std::map<ModelType, ModelData>;
  struct LocaleTriggerData {
    ModelType _defaultModelType;
    ModelDataMap _modelDataMap;
  };
  using LocaleTriggerDataMap = std::map<Util::Locale, LocaleTriggerData>;

  LocaleTriggerDataMap _localeTriggerDataMap;

  ModelDataMap InitModelData(const Json::Value& modelDataList);
  ModelType ModelTypeFromString(const std::string& modelTypeString);
};

} // namespace MicData
} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_MicTriggerConfig_H_
