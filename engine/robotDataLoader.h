/**
 * File: robotDataLoader
 *
 * Author: baustin
 * Created: 6/10/16
 *
 * Description: Loads and holds static data robots use for initialization
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#ifndef ANKI_COZMO_BASESTATION_ROBOT_DATA_LOADER_H
#define ANKI_COZMO_BASESTATION_ROBOT_DATA_LOADER_H

#include "clad/types/behaviorComponent/behaviorIDs.h"
#include "clad/types/behaviorComponent/beiConditionTypes.h"
#include "clad/types/behaviorComponent/weatherConditionTypes.h"
#include "clad/types/cubeAnimationTrigger.h"
#include "clad/types/variableSnapshotIds.h"

#include "engine/aiComponent/behaviorComponent/behaviorTypesWrapper.h"
#include "coretech/vision/shared/compositeImage/compositeImage.h"
#include "coretech/vision/shared/spritePathMap.h"

#include "util/cladHelpers/cladEnumToStringMap.h"
#include "util/helpers/noncopyable.h"
#include <json/json.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <set>
#include <unordered_map>
#include <vector>

namespace Anki {

namespace Util {

namespace Data {
class DataPlatform;
}
}

namespace Vision{
class SpriteCache;
class SpriteSequenceContainer;
}

namespace Vector {

// forward declarations
class AnimationGroupContainer;
enum class AnimationTrigger : int32_t;
class CannedAnimationContainer;
class CubeLightAnimationContainer;
class CozmoContext;

class RobotDataLoader : private Util::noncopyable
{
public:
  RobotDataLoader(const CozmoContext* context);
  ~RobotDataLoader();

  // loads all data excluding configs, using DispatchWorker to parallelize.
  // Blocks until the data is loaded.
  void LoadNonConfigData();
  
  // Starts a thread to handle loading non-config data if it hasn't been done yet.
  // Can be repeatedly called to get an updated loading complete ratio. Returns
  // false if loading is ongoing, otherwise returns true
  bool DoNonConfigDataLoading(float& loadingCompleteRatio_out);

  // refresh individual data pieces after initial load
  void LoadRobotConfigs();

  using FileJsonMap       = std::unordered_map<std::string, const Json::Value>;
  using BehaviorIDJsonMap = std::unordered_map<BehaviorID,  const Json::Value>;

  using AnimationTriggerMap = Util::CladEnumToStringMap<AnimationTrigger>;
  using CubeAnimationTriggerMap = Util::CladEnumToStringMap<CubeAnimationTrigger>;


  const FileJsonMap& GetEmotionEventJsons()   const { return _emotionEvents; }
  const BehaviorIDJsonMap& GetBehaviorJsons() const { return _behaviors; }  
  const FileJsonMap& GetCubeLightAnimations() const { return _cubeLightAnimations; }

  CannedAnimationContainer* GetCannedAnimationContainer() const { return _cannedAnimations.get(); }
  AnimationGroupContainer* GetAnimationGroups() const { return _animationGroups.get(); }

  AnimationTriggerMap* GetAnimationTriggerMap() const { return _animationTriggerMap.get(); }
  CubeAnimationTriggerMap* GetCubeAnimationTriggerMap() const { return _cubeAnimationTriggerMap.get(); }

  bool HasAnimationForTrigger( AnimationTrigger ev ) const;
  std::string GetAnimationForTrigger( AnimationTrigger ev ) const;
  std::string GetCubeAnimationForTrigger( CubeAnimationTrigger ev ) const;
  
  const std::set<AnimationTrigger>& GetDasBlacklistedAnimationTriggers() const { return _dasBlacklistedAnimationTriggers; }
  const std::set<std::string>& GetDasBlacklistedAnimationNames() const { return _dasBlacklistedAnimationNames; }

  // Returns true if the given animation is allowed to move the body while on the charger
  bool IsAnimationAllowedToMoveBodyOnCharger(const std::string& animName) const;
  
  // all clips that are allowed to move the body while on the charger
  const std::vector<std::string>& GetAllWhitelistedChargerAnimationPrefixes() const {
    return _whitelistedChargerAnimationPrefixes;
  }

  // robot configuration json files
  const Json::Value& GetRobotMoodConfig() const              { return _robotMoodConfig; }
  const Json::Value& GetVictorFreeplayBehaviorConfig() const { return _victorFreeplayBehaviorConfig; }
  const Json::Value& GetRobotVisionConfig() const            { return _robotVisionConfig; }
  const Json::Value& GetVisionScheduleMediatorConfig() const { return _visionScheduleMediatorConfig; }
  const Json::Value& GetWebServerEngineConfig() const        { return _webServerEngineConfig; }
  const Json::Value& GetDasEventConfig() const               { return _dasEventConfig; }
  const Json::Value& GetUserIntentConfig() const             { return _userIntentsConfig; }
  const Json::Value& GetPhotographyConfig() const            { return _photographyConfig; }
  const Json::Value& GetSettingsConfig() const               { return _settingsConfig; }
  const Json::Value& GetEyeColorConfig() const               { return _eyeColorConfig; }
  const Json::Value& GetJdocsConfig() const                  { return _jdocsConfig; }
  const Json::Value& GetAccountSettingsConfig() const        { return _accountSettingsConfig; }
  const Json::Value& GetUserEntitlementsConfig() const       { return _userEntitlementsConfig; }

  // Cube Spinner game configuration
  const Json::Value& GetCubeSpinnerConfig() const             { return _cubeSpinnerConfig; }

  // User-defined behavior tree config
  using ConditionToBehaviorsMap = std::unordered_map<BEIConditionType, std::set<BehaviorID>>;
  ConditionToBehaviorsMap* GetUserDefinedConditionToBehaviorsMap() const { assert(nullptr != _conditionToBehaviorsMap); return _conditionToBehaviorsMap.get(); }
  const BEIConditionType GetUserDefinedEditCondition() const { assert(BEIConditionType::Invalid != _userDefinedEditCondition); return _userDefinedEditCondition; }

  // images are stored as a map of stripped file name (no file extension) to full path
  const Vision::SpritePathMap* GetSpritePaths()       const { assert(_spritePaths != nullptr); return _spritePaths.get(); }
  Vision::SpriteSequenceContainer* GetSpriteSequenceContainer() { return _spriteSequenceContainer.get();}
  Vision::SpriteCache* GetSpriteCache() const { assert(_spriteCache != nullptr); return _spriteCache.get();  }

  // weather response map
  using WeatherResponseMap = std::unordered_map<std::string, WeatherConditionType>;
  using WeatherConditionTTSMap = std::unordered_map<WeatherConditionType, std::string>;

  // variable snapshot json map
  using VariableSnapshotJsonMap = std::unordered_map<VariableSnapshotId, Json::Value>;

  const WeatherResponseMap* GetWeatherResponseMap() const { assert(_weatherResponseMap); return _weatherResponseMap.get();}
  const WeatherConditionTTSMap* GetWeatherConditionTTSMap() const { assert(_weatherConditionTTSMap); return _weatherConditionTTSMap.get();}
  const Json::Value& GetWeatherRemaps() const { return _weatherRemaps;}
  const Json::Value* GetWeatherRemapsPtr() const { return &_weatherRemaps;}
  VariableSnapshotJsonMap*  GetVariableSnapshotJsonMap() const { assert(_variableSnapshotJsonMap); return _variableSnapshotJsonMap.get(); }

  bool IsCustomAnimLoadEnabled() const;

  #if ANKI_DEV_CHEATS
  Json::Value& GetRobotVisionConfigUpdatableRef()           { return _robotVisionConfig; }
  #endif
  
private:
  void CollectAnimFiles();
  
  void LoadCubeLightAnimations();
  void LoadCubeLightAnimationFile(const std::string& path);

  
  void LoadAnimationGroups();
  void LoadAnimationGroupFile(const std::string& path);
  
  void LoadAnimationTriggerMap();
  void LoadCubeAnimationTriggerMap();
  
  void AddToLoadingRatio(float delta);

  using TimestampMap = std::unordered_map<std::string, time_t>;
  void WalkAnimationDir(const std::string& animationDir, TimestampMap& timestamps,
                        const std::function<void(const std::string& filePath)>& walkFunc);

  void LoadEmotionEvents();
  void LoadBehaviors();

  void LoadDasBlacklistedAnimations();
  
  void LoadSpritePaths();

  void LoadWeatherResponseMaps();
  void LoadWeatherRemaps();
  void LoadWeatherConditionTTSMap();

  void LoadVariableSnapshotJsonMap();
  
  void LoadCubeSpinnerConfig();

  void LoadUserDefinedBehaviorTreeConfig();

  void LoadAnimationWhitelist();

  // Outputs a map of file name (no path or extensions) to the full file path
  // Useful for clad mappings/lookups
  std::map<std::string, std::string> CreateFileNameToFullPathMap(const std::vector<const char*> & srcDirs, const std::string& fileExtensions) const;

  const CozmoContext* const _context;
  const Util::Data::DataPlatform* _platform;

  FileJsonMap _emotionEvents;
  BehaviorIDJsonMap _behaviors;

  enum FileType {
      Animation,
      AnimationGroup,
      CubeLightAnimation,
  };
  std::unordered_map<int, std::vector<std::string>> _jsonFiles;

  // animation data
  std::unique_ptr<CannedAnimationContainer>    _cannedAnimations;
  std::unique_ptr<AnimationGroupContainer>     _animationGroups;

  std::unique_ptr<AnimationTriggerMap>         _animationTriggerMap;
  std::unique_ptr<CubeAnimationTriggerMap>     _cubeAnimationTriggerMap;

  TimestampMap _animFileTimestamps;
  TimestampMap _groupAnimFileTimestamps;
  TimestampMap _cubeLightAnimFileTimestamps;

  std::string _test_anim;

  FileJsonMap  _cubeLightAnimations;

  // robot configs
  Json::Value _robotMoodConfig;
  Json::Value _victorFreeplayBehaviorConfig;
  Json::Value _robotVisionConfig;
  Json::Value _visionScheduleMediatorConfig;
  Json::Value _webServerEngineConfig;
  Json::Value _dasEventConfig;
  Json::Value _userIntentsConfig;
  Json::Value _photographyConfig;
  Json::Value _settingsConfig;
  Json::Value _eyeColorConfig;
  Json::Value _jdocsConfig;
  Json::Value _accountSettingsConfig;
  Json::Value _userEntitlementsConfig;

  Json::Value _cubeSpinnerConfig;

  // user-defined behavior tree config
  std::unique_ptr<ConditionToBehaviorsMap> _conditionToBehaviorsMap;
  BEIConditionType _userDefinedEditCondition;

  std::unique_ptr<Vision::SpritePathMap> _spritePaths;
  std::unique_ptr<Vision::SpriteCache>   _spriteCache;
  std::unique_ptr<Vision::SpriteSequenceContainer> _spriteSequenceContainer;

  std::unique_ptr<WeatherResponseMap>      _weatherResponseMap;
  std::unique_ptr<WeatherConditionTTSMap>  _weatherConditionTTSMap;
  Json::Value                              _weatherRemaps;
  std::unique_ptr<VariableSnapshotJsonMap> _variableSnapshotJsonMap;


  bool                  _isNonConfigDataLoaded = false;
  std::mutex            _parallelLoadingMutex;
  std::atomic<float>    _loadingCompleteRatio{0};
  std::thread           _dataLoadingThread;
  std::atomic<bool>     _abortLoad{false};
  
  std::set<AnimationTrigger> _dasBlacklistedAnimationTriggers;
  std::set<std::string> _dasBlacklistedAnimationNames;

  std::vector<std::string> _whitelistedChargerAnimationPrefixes;
};

}
}

#endif
