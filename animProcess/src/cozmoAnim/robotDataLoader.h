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

#ifndef ANKI_COZMO_ANIM_DATA_LOADER_H
#define ANKI_COZMO_ANIM_DATA_LOADER_H

#include "cannedAnimLib/cannedAnims/cannedAnimationLoader.h"
#include "clad/types/backpackAnimationTriggers.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponentTypes.h"
#include "util/cladHelpers/cladEnumToStringMap.h"
#include "util/helpers/noncopyable.h"
#include "coretech/vision/shared/spritePathMap.h"

#include "assert.h"
#include <json/json.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Anki {

namespace Util {
namespace Data {
class DataPlatform;
}
}

namespace Vision {
class SpriteCache;
class SpriteSequenceContainer;
}

namespace Vector {

class CannedAnimationContainer;
class Animation;
namespace Anim {
class AnimContext;
class BackpackLightAnimationContainer;

class RobotDataLoader : private Util::noncopyable
{
public:
  RobotDataLoader(const AnimContext* context);
  ~RobotDataLoader();

  // Loads all static configuration data.
  // Blocks until data is loaded.
  void LoadConfigData();
  
  // Loads all data excluding configs, using DispatchWorker to parallelize.
  // Blocks until the data is loaded.
  void LoadNonConfigData();
  void LoadAnimationFile(const std::string& path);
  
  // Starts a thread to handle loading non-config data if it hasn't been done yet.
  // Can be repeatedly called to get an updated loading complete ratio. Returns
  // false if loading is ongoing, otherwise returns true
  bool DoNonConfigDataLoading(float& loadingCompleteRatio_out);

  const Json::Value & GetTextToSpeechConfig() const { return _tts_config; }
  const Json::Value & GetWebServerAnimConfig() const { return _ws_config; }
  const Json::Value & GetMicTriggerConfig() const { return _micTriggerConfig; }
  Animation* GetCannedAnimation(const std::string& name);
  std::vector<std::string> GetAnimationNames();
  
  const std::string& GetAlexaConfig() const { return _alexaConfig; }

  // images are stored as a map of stripped file name (no file extension) to full path
  const Vision::SpritePathMap* GetSpritePaths() const { assert(_spritePathMap != nullptr); return _spritePathMap.get(); }
  Vision::SpriteCache* GetSpriteCache() const { assert(_spriteCache != nullptr); return _spriteCache.get();  }

  Vision::SpriteSequenceContainer* GetSpriteSequenceContainer() { return _spriteSequenceContainer.get();}

  using BackpackAnimationTriggerMap = Util::CladEnumToStringMap<BackpackAnimationTrigger>;
  using FileJsonMap = std::unordered_map<std::string, const Json::Value>;
  const FileJsonMap& GetBackpackLightAnimations() const { return _backpackLightAnimations; }
  BackpackAnimationTriggerMap* GetBackpackAnimationTriggerMap() { return _backpackAnimationTriggerMap.get();}
  
private:
  void LoadIndependentSpritePaths();

  void NotifyAnimAdded(const std::string& animName, uint32_t animLength);
  
  void SetupProceduralAnimation();

  void LoadBackpackLightAnimations(const CannedAnimationLoader::AnimDirInfo& fileInfo);
  void LoadBackpackLightAnimationFile(const std::string& path);
  void LoadBackpackAnimationTriggerMap();
  
  const AnimContext* const _context;
  const Util::Data::DataPlatform* _platform;

  // animation data
  std::unique_ptr<CannedAnimationContainer>              _cannedAnimations;
  std::unique_ptr<Vision::SpriteSequenceContainer>       _spriteSequenceContainer;
  std::unique_ptr<Vision::SpritePathMap>                 _spritePathMap;
  std::unique_ptr<Vision::SpriteCache>                   _spriteCache;


  // loading properties shared with the animiation loader
  std::atomic<float>    _loadingCompleteRatio{0};
  std::atomic<bool>     _abortLoad{false};

  bool                  _isNonConfigDataLoaded = false;
  std::thread           _dataLoadingThread;

  
  Json::Value _tts_config;
  Json::Value _ws_config;
  Json::Value _micTriggerConfig;
  std::string _alexaConfig;

  std::unique_ptr<BackpackAnimationTriggerMap> _backpackAnimationTriggerMap;
  FileJsonMap _backpackLightAnimations;
  std::mutex _backpackLoadingMutex;
  
};

}
}
}

#endif
