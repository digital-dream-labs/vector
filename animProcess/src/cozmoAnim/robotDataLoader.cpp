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

#include "cozmoAnim/robotDataLoader.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/vision/shared/spriteCache/spriteCache.h"

#include "cannedAnimLib/cannedAnims/animation.h"
#include "cannedAnimLib/cannedAnims/cannedAnimationContainer.h"
#include "cannedAnimLib/baseTypes/cozmo_anim_generated.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"
#include "cannedAnimLib/spriteSequences/spriteSequenceLoader.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"
//#include "anki/cozmo/basestation/animations/animationTransfer.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"

#include "cozmoAnim/backpackLights/backpackLightAnimationContainer.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"

#include "osState/osState.h"

#include "util/console/consoleInterface.h"
#include "util/dispatchWorker/dispatchWorker.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/threading/threadPriority.h"
#include "util/time/universalTime.h"
#include <json/json.h>
#include <string>
#include <sys/stat.h>

#define LOG_CHANNEL   "RobotDataLoader"

namespace Anki {
namespace Vector {
namespace Anim {

namespace{
const char* kPathToExternalIndependentSprites = "assets/sprites/independentSprites/";
const char* kPathToEngineIndependentSprites = "config/sprites/independentSprites/";
const char* kPathToExternalSpriteSequences = "assets/sprites/spriteSequences/";
const char* kPathToEngineSpriteSequences   = "config/sprites/spriteSequences/";
const char* kProceduralAnimName = "_PROCEDURAL_";
}

RobotDataLoader::RobotDataLoader(const AnimContext* context)
: _context(context)
, _platform(_context->GetDataPlatform())
, _cannedAnimations(nullptr)
, _backpackAnimationTriggerMap(new BackpackAnimationTriggerMap())
{
  _spritePathMap = std::make_unique<Vision::SpritePathMap>();
  _spriteCache = std::make_unique<Vision::SpriteCache>(_spritePathMap.get());
}

RobotDataLoader::~RobotDataLoader()
{
  if (_dataLoadingThread.joinable()) {
    _abortLoad = true;
    _dataLoadingThread.join();
  }
}

void RobotDataLoader::LoadConfigData()
{
  // Text-to-speech config
  {
    static const std::string & tts_config = "config/engine/tts_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, tts_config, _tts_config);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.TextToSpeechConfigNotFound",
                "Text-to-speech config file %s not found or failed to parse",
                  tts_config.c_str());
    }
  }
  // Web server config
  {
    static const std::string & ws_config = "webserver/webServerConfig_anim.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, ws_config, _ws_config);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.WebServerAnimConfigNotFound",
                "Web server anim config file %s not found or failed to parse",
                  ws_config.c_str());
    }
  }
  // Mic data config
  {
    const std::string& triggerConfigFile = "config/micData/micTriggerConfig.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, triggerConfigFile, _micTriggerConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.MicTriggerConfigNotFound",
                "Mic trigger config file %s not found or failed to parse",
                triggerConfigFile.c_str());
    }
  }

  {
    const std::string& alexaConfigFile = "config/alexa.json";
    const auto path = _platform->GetResourcePath(alexaConfigFile);

    if (Util::FileUtils::FileExists(path)) {
      _alexaConfig = Util::FileUtils::ReadFile(path);
    } else {
      LOG_ERROR("RobotDataLoader.AlexaConfigNotFound",
                "Alexa config file %s not found or failed to parse",
                path.c_str());
    }
  }
}

void RobotDataLoader::LoadNonConfigData()
{
  if (_platform == nullptr) {
    return;
  }
  
  // Dependency Order:
  //  1) Load map of sprite filenames to asset paths
  //  2) SpriteSequences use sprite map to load sequenceName -> all images in sequence directory
  //  3) Canned animations use SpriteSequences for their FaceAnimation keyframe
  LoadIndependentSpritePaths();
  {
    std::vector<std::string> spriteSequenceDirs = {kPathToExternalSpriteSequences, kPathToEngineSpriteSequences};
    SpriteSequenceLoader seqLoader;
    auto* sContainer = seqLoader.LoadSpriteSequences(_platform,
                                                     _spritePathMap.get(),
                                                     _spriteCache.get(),
                                                     spriteSequenceDirs);
    _spriteSequenceContainer.reset(sContainer);
  }

  {
    // Set up container
    _cannedAnimations = std::make_unique<CannedAnimationContainer>();

    // Gather the files to load into the animation container
    CannedAnimationLoader animLoader(_platform,
                                     _spriteSequenceContainer.get(), 
                                     _loadingCompleteRatio, _abortLoad);

    std::vector<std::string> paths;
    if(FACTORY_TEST)
    {
      // Only need to load engine animations
      paths = {"config/engine/animations/"};
    }
    else
    {
      paths = {"assets/animations/", "config/engine/animations/"};
    }

    // Load the gathered files into the container
    const auto& fileInfo = animLoader.CollectAnimFiles(paths);
    animLoader.LoadAnimationsIntoContainer(fileInfo, _cannedAnimations.get());
  }

  // After we've finished loading Sprites and SpriteSequences, retroactively verify
  // any AssetID's requested before/during loading
  _spritePathMap->CheckUnverifiedAssetIDs();

  // Backpack light animations
  {
    // Use the CannedAnimationLoader to collect the backpack light json files
    CannedAnimationLoader animLoader(_platform,
                                     _spriteSequenceContainer.get(), 
                                     _loadingCompleteRatio, _abortLoad);

    const auto& fileInfo = animLoader.CollectAnimFiles({"config/engine/lights/backpackLights"});
    LoadBackpackLightAnimations(fileInfo);
  }

  {
    LoadBackpackAnimationTriggerMap();
  }
  
  SetupProceduralAnimation();
}

void RobotDataLoader::LoadAnimationFile(const std::string& path)
{
  if (_platform == nullptr) {
    return;
  }
  CannedAnimationLoader animLoader(_platform,
                                   _spriteSequenceContainer.get(),
                                   _loadingCompleteRatio, _abortLoad);

  animLoader.LoadAnimationIntoContainer(path, _cannedAnimations.get());

  const auto animName = Util::FileUtils::GetFileName(path, true, true);
  const auto * anim = _cannedAnimations->GetAnimation(animName);
  if (anim == nullptr) {
    LOG_ERROR("RobotDataLoader.LoadAnimationFile", "Failed to load %s from %s", animName.c_str(), path.c_str());
    return;
  }
  NotifyAnimAdded(animName, anim->GetLastKeyFrameEndTime_ms());
}

void RobotDataLoader::LoadIndependentSpritePaths()
{
  // Get all independent sprites
  {
    auto spritePaths = {kPathToExternalIndependentSprites,
                        kPathToEngineIndependentSprites};
    
    const bool useFullPath = true;
    const char* extensions = "png";
    const bool recurse = true;
    for(const auto& path: spritePaths){
      const std::string fullPathFolder = _platform->pathToResource(Util::Data::Scope::Resources, path);

      auto fullImagePaths = Util::FileUtils::FilesInDirectory(fullPathFolder, useFullPath, extensions, recurse);
      for(auto& fullImagePath : fullImagePaths){
        const std::string fileName = Util::FileUtils::GetFileName(fullImagePath, true, true);
        _spritePathMap->AddAsset(fileName, fullImagePath, false);
      }
    }
  }
}


Animation* RobotDataLoader::GetCannedAnimation(const std::string& name)
{
  DEV_ASSERT(_cannedAnimations != nullptr, "_cannedAnimations");
  return _cannedAnimations->GetAnimation(name);
}

std::vector<std::string> RobotDataLoader::GetAnimationNames()
{
  DEV_ASSERT(_cannedAnimations != nullptr, "_cannedAnimations");
  return _cannedAnimations->GetAnimationNames();
}

void RobotDataLoader::NotifyAnimAdded(const std::string& animName, uint32_t animLength)
{
  AnimationAdded msg;
  memcpy(msg.animName, animName.c_str(), animName.length());
  msg.animName_length = animName.length();
  msg.animLength = animLength;
  AnimProcessMessages::SendAnimToEngine(msg);
}
  
void RobotDataLoader::SetupProceduralAnimation()
{
  // TODO: kevink - This should probably live somewhere else but since robot data loader
  // currently maintains control of both canned animations and sprite sequences this
  // is the best spot to put it for the time being
  Animation proceduralAnim(kProceduralAnimName);
  bool outOverwrite = false;
  _cannedAnimations->AddAnimation(std::move(proceduralAnim), outOverwrite);
  
  assert(_cannedAnimations->GetAnimation(kProceduralAnimName) != nullptr);
}

bool RobotDataLoader::DoNonConfigDataLoading(float& loadingCompleteRatio_out)
{
  loadingCompleteRatio_out = _loadingCompleteRatio.load();
  
  if (_isNonConfigDataLoaded)
  {
    return true;
  }
  
  // loading hasn't started
  if (!_dataLoadingThread.joinable())
  {
    // start loading
    _dataLoadingThread = std::thread(&RobotDataLoader::LoadNonConfigData, this);
    return false;
  }
  
  // loading has started but isn't complete
  if (loadingCompleteRatio_out < 1.0f)
  {
    return false;
  }
  
  // loading is now done so lets clean up
  _dataLoadingThread.join();
  _dataLoadingThread = std::thread();
  _isNonConfigDataLoaded = true;
  
  return true;
}

void RobotDataLoader::LoadBackpackAnimationTriggerMap()
{
  _backpackAnimationTriggerMap->Load(_platform, "assets/cladToFileMaps/BackpackAnimationTriggerMap.json", "AnimName");
}

void RobotDataLoader::LoadBackpackLightAnimations(const CannedAnimationLoader::AnimDirInfo& fileInfo)
{
  const double startTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();

  using MyDispatchWorker = Util::DispatchWorker<3, const std::string&>;
  MyDispatchWorker::FunctionType loadFileFunc = std::bind(&RobotDataLoader::LoadBackpackLightAnimationFile,
                                                          this, std::placeholders::_1);
  MyDispatchWorker myWorker(loadFileFunc);

  const auto& fileList = fileInfo.jsonFiles;
  const auto size = fileList.size();
  for (int i = 0; i < size; i++) {
    myWorker.PushJob(fileList[i]);
  }

  myWorker.Process();

  const double endTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
  double loadTime = endTime - startTime;
  PRINT_CH_INFO("Animations", "RobotDataLoader.LoadBackpackLightAnimations.LoadTime",
                "Time to load backpack light animations = %.2f ms", loadTime);
}

void RobotDataLoader::LoadBackpackLightAnimationFile(const std::string& path)
{
  Json::Value animDefs;
  const bool success = _platform->readAsJson(path.c_str(), animDefs);
  if (success && !animDefs.empty()) {
    std::lock_guard<std::mutex> guard(_backpackLoadingMutex);
    _backpackLightAnimations.emplace(path, animDefs);
  }
}

}
}
}
