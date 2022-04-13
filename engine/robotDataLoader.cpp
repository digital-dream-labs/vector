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

#include "engine/robotDataLoader.h"

#include "cannedAnimLib/cannedAnims/cannedAnimationContainer.h"
#include "cannedAnimLib/cannedAnims/cannedAnimationLoader.h"
#include "cannedAnimLib/spriteSequences/spriteSequenceLoader.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/vision/shared/spriteCache/spriteCache.h"
#include "engine/actions/sayTextAction.h"

#include "engine/animations/animationGroup/animationGroupContainer.h"
#include "engine/animations/animationTransfer.h"
#include "engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior.h"

#include "engine/components/cubes/cubeLights/cubeLightComponent.h"
#include "engine/components/variableSnapshot/variableSnapshotComponent.h"
#include "engine/components/variableSnapshot/variableSnapshotEncoder.h"
#include "engine/cozmoContext.h"
#include "engine/utils/cozmoExperiments.h"
#include "engine/utils/cozmoFeatureGate.h"
#include "threadedPrintStressTester.h"
#include "util/ankiLab/ankiLab.h"
#include "util/cladHelpers/cladEnumToStringMap.h"
#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/dispatchWorker/dispatchWorker.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/string/stringUtils.h"
#include "util/threading/threadPriority.h"
#include "util/time/universalTime.h"
#include <json/json.h>
#include <string>
#include <sys/stat.h>

#define LOG_CHANNEL    "RobotDataLoader"

namespace {

CONSOLE_VAR(bool, kStressTestThreadedPrintsDuringLoad, "RobotDataLoader", false);

#if REMOTE_CONSOLE_ENABLED
static Anki::Vector::ThreadedPrintStressTester stressTester;
#endif // REMOTE_CONSOLE_ENABLED

const char* kPathToExternalIndependentSprites = "assets/sprites/independentSprites/";
const char* kPathToEngineIndependentSprites = "config/sprites/independentSprites/";
const char* kPathToExternalSpriteSequences = "assets/sprites/spriteSequences/";
const char* kPathToEngineSpriteSequences   = "config/sprites/spriteSequences/";

const std::vector<std::string> kPathsToEngineAccessibleAnimations = {
  // Dance to the beat:
  "assets/animations/anim_dancebeat_01.bin",
  "assets/animations/anim_dancebeat_02.bin",
  "assets/animations/anim_dancebeat_getin_01.bin",
  "assets/animations/anim_dancebeat_getout_01.bin",

  // Cube Spinner
  "assets/animations/anim_spinner_tap_01.bin",
  
  // Onboarding
  "assets/animations/anim_onboarding_cube_reacttocube.bin",
  
  // Robot power on/off
  "assets/animations/anim_power_offon_01.bin",
  "assets/animations/anim_power_onoff_01.bin",

};
}


namespace Anki {
namespace Vector {

RobotDataLoader::RobotDataLoader(const CozmoContext* context)
: _context(context)
, _platform(_context->GetDataPlatform())
, _animationGroups(new AnimationGroupContainer(*context->GetRandom()))
, _animationTriggerMap(new AnimationTriggerMap())
, _cubeAnimationTriggerMap(new CubeAnimationTriggerMap())
, _dasBlacklistedAnimationTriggers()
{
  _spritePaths = std::make_unique<Vision::SpritePathMap>();
}

RobotDataLoader::~RobotDataLoader()
{
  if (_dataLoadingThread.joinable()) {
    _abortLoad = true;
    _dataLoadingThread.join();
  }
}

void RobotDataLoader::LoadNonConfigData()
{
  if (_platform == nullptr) {
    return;
  }

  Anki::Util::SetThreadName(pthread_self(), "RbtDataLoader");

  ANKI_CPU_TICK_ONE_TIME("RobotDataLoader::LoadNonConfigData");

  if( kStressTestThreadedPrintsDuringLoad ) {
    REMOTE_CONSOLE_ENABLED_ONLY( stressTester.Start() );
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::CollectFiles");
    CollectAnimFiles();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadBehaviors");
    LoadBehaviors();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadWeatherResponseMaps");
    LoadWeatherResponseMaps();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadWeatherRemaps");
    LoadWeatherRemaps();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadWeatherConditionTTSMap");
    LoadWeatherConditionTTSMap();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadVariableSnapshotJsonMap");
    LoadVariableSnapshotJsonMap();
  }
  
  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadCubeSpinnerConfig");
    LoadCubeSpinnerConfig();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadUserDefinedBehaviorTreeConfig");
    LoadUserDefinedBehaviorTreeConfig();
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadSpritePaths");
    LoadSpritePaths();
    _spriteCache = std::make_unique<Vision::SpriteCache>(_spritePaths.get());
  }

  {
    ANKI_CPU_PROFILE("RobotDataLoader::LoadSpriteSequences");
    std::vector<std::string> spriteSequenceDirs = {kPathToExternalSpriteSequences, kPathToEngineSpriteSequences};
    SpriteSequenceLoader seqLoader;
    auto* sContainer = seqLoader.LoadSpriteSequences(_platform,
                                                     _spritePaths.get(),
                                                     _spriteCache.get(),
                                                     spriteSequenceDirs);
    _spriteSequenceContainer.reset(sContainer);
  }

  // After we've finished loading Sprites and SpriteSequences, retroactively verify
  // any AssetID's requested before/during loading
  _spritePaths->CheckUnverifiedAssetIDs();

  if(!FACTORY_TEST)
  {
    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadAnimationGroups");
      LoadAnimationGroups();
    }

    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadCubeLightAnimations");
      LoadCubeLightAnimations();
    }

    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadCubeAnimationTriggerMap");
      LoadCubeAnimationTriggerMap();
    }
    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadEmotionEvents");
      LoadEmotionEvents();
    }

    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadDasBlacklistedAnimations");
      LoadDasBlacklistedAnimations();
    }

    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadAnimationTriggerMap");
      LoadAnimationTriggerMap();
    }

    {
      ANKI_CPU_PROFILE("RobotDataLoader::LoadAnimationWhitelist");
      LoadAnimationWhitelist();
    }

  }
  
  {
    CannedAnimationLoader animLoader(_platform,
                                     _spriteSequenceContainer.get(),
                                     _loadingCompleteRatio, _abortLoad);

    // Create the canned animation container, but don't load any data into it
    // Engine side animations are loaded only when requested
    _cannedAnimations = std::make_unique<CannedAnimationContainer>();
    for(const auto& path : kPathsToEngineAccessibleAnimations){
      const auto fullPath =  _platform->pathToResource(Util::Data::Scope::Resources, path);
      animLoader.LoadAnimationIntoContainer(fullPath, _cannedAnimations.get());
    }
  }

  // this map doesn't need to be persistent
  _jsonFiles.clear();

  if( kStressTestThreadedPrintsDuringLoad ) {
    REMOTE_CONSOLE_ENABLED_ONLY( stressTester.Stop() );
  }

  // we're done
  _loadingCompleteRatio.store(1.0f);
}

void RobotDataLoader::AddToLoadingRatio(float delta)
{
  // Allows for a thread to repeatedly try to update the loading ratio until it gets access
  auto current = _loadingCompleteRatio.load();
  while (!_loadingCompleteRatio.compare_exchange_weak(current, current + delta));
}

void RobotDataLoader::CollectAnimFiles()
{
  // animations
  {
    std::vector<std::string> paths;
    if(FACTORY_TEST)
    {
      paths = {"config/engine/animations/"};
    }
    else
    {
      paths = {"assets/animations/", "config/engine/animations/"};
    }

    for (const auto& path : paths) {
      WalkAnimationDir(path, _animFileTimestamps, [this] (const std::string& filename) {
        _jsonFiles[FileType::Animation].push_back(filename);
      });
    }
  }

  // cube light animations
  {
    WalkAnimationDir("config/engine/lights/cubeLights", _cubeLightAnimFileTimestamps, [this] (const std::string& filename) {
      _jsonFiles[FileType::CubeLightAnimation].push_back(filename);
    });
  }


  if(!FACTORY_TEST)
  {
    // animation groups
    {
      WalkAnimationDir("assets/animationGroups/", _groupAnimFileTimestamps, [this] (const std::string& filename) {
        _jsonFiles[FileType::AnimationGroup].push_back(filename);
      });
    }
  }

  // print results
  {
    for (const auto& fileListPair : _jsonFiles) {
      PRINT_CH_INFO("Animations", "RobotDataLoader.CollectAnimFiles.Results", "Found %zu animation files of type %d",
                    fileListPair.second.size(), (int)fileListPair.first);
    }
  }
}

bool RobotDataLoader::IsCustomAnimLoadEnabled() const
{
  return (ANKI_DEV_CHEATS != 0);
}

void RobotDataLoader::LoadCubeLightAnimations()
{
  const auto& fileList = _jsonFiles[FileType::CubeLightAnimation];
  const auto size = fileList.size();

  const double startTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();

  using MyDispatchWorker = Util::DispatchWorker<3, const std::string&>;
  MyDispatchWorker::FunctionType loadFileFunc = std::bind(&RobotDataLoader::LoadCubeLightAnimationFile, 
                                                          this, std::placeholders::_1);
  MyDispatchWorker myWorker(loadFileFunc);

  for (int i = 0; i < size; i++) {
    myWorker.PushJob(fileList[i]);
  }
  
  myWorker.Process();

  const double endTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
  double loadTime = endTime - startTime;
  PRINT_CH_INFO("Animations", "RobotDataLoader.LoadCubeLightAnimations.LoadTime",
                "Time to load cube light animations = %.2f ms", loadTime);

}

void RobotDataLoader::LoadCubeLightAnimationFile(const std::string& path)
{
  Json::Value animDefs;
  const bool success = _platform->readAsJson(path.c_str(), animDefs);
  if (success && !animDefs.empty()) {
    std::lock_guard<std::mutex> guard(_parallelLoadingMutex);
    _cubeLightAnimations.emplace(path, animDefs);
  }
}

void RobotDataLoader::LoadAnimationGroups()
{
  using MyDispatchWorker = Util::DispatchWorker<3, const std::string&>;
  MyDispatchWorker::FunctionType loadFileFunc = std::bind(&RobotDataLoader::LoadAnimationGroupFile, this, std::placeholders::_1);
  MyDispatchWorker myWorker(loadFileFunc);
  const auto& fileList = _jsonFiles[FileType::AnimationGroup];
  const auto size = fileList.size();
  for (int i = 0; i < size; i++) {
    myWorker.PushJob(fileList[i]);
    //LOG_DEBUG("RobotDataLoader.LoadAnimationGroups", "loaded anim group %d of %zu", i, size);
  }
  myWorker.Process();
}

void RobotDataLoader::WalkAnimationDir(const std::string& animationDir, TimestampMap& timestamps, const std::function<void(const std::string&)>& walkFunc)
{
  const std::string animationFolder = _platform->pathToResource(Util::Data::Scope::Resources, animationDir);
  static const std::vector<const char*> fileExts = {"json", "bin"};
  auto filePaths = Util::FileUtils::FilesInDirectory(animationFolder, true, fileExts, true);

  for (const auto& path : filePaths) {
    struct stat attrib{0};
    int result = stat(path.c_str(), &attrib);
    if (result == -1) {
      LOG_WARNING("RobotDataLoader.WalkAnimationDir", "could not get mtime for %s", path.c_str());
      continue;
    }
    bool loadFile = false;
    auto mapIt = timestamps.find(path);
#ifdef __APPLE__  // TODO: COZMO-1057
    time_t tmpSeconds = attrib.st_mtimespec.tv_sec;
#else
    time_t tmpSeconds = attrib.st_mtime;
#endif
    if (mapIt == timestamps.end()) {
      timestamps.insert({path, tmpSeconds});
      loadFile = true;
    } else {
      if (mapIt->second < tmpSeconds) {
        mapIt->second = tmpSeconds;
        loadFile = true;
      }
    }
    if (loadFile) {
      walkFunc(path);
    }
  }
}

void RobotDataLoader::LoadAnimationGroupFile(const std::string& path)
{
  if (_abortLoad.load(std::memory_order_relaxed)) {
    return;
  }
  Json::Value animGroupDef;
  const bool success = _platform->readAsJson(path, animGroupDef);
  if (success && !animGroupDef.empty()) {
    std::string animationGroupName = Util::FileUtils::GetFileName(path, true, true);

    //PRINT_CH_DEBUG("Animations", "RobotDataLoader.LoadAnimationGroupFile.LoadingSpecificAnimGroupFromJson",
    //               "Loading '%s' from %s", animationGroupName.c_str(), path.c_str());

    std::lock_guard<std::mutex> guard(_parallelLoadingMutex);
    _animationGroups->DefineFromJson(animGroupDef, animationGroupName);
  }
}

void RobotDataLoader::LoadEmotionEvents()
{
  const std::string emotionEventFolder = _platform->pathToResource(Util::Data::Scope::Resources, "config/engine/emotionevents/");
  auto eventFiles = Util::FileUtils::FilesInDirectory(emotionEventFolder, true, ".json", false);
  for (const std::string& filename : eventFiles) {
    Json::Value eventJson;
    const bool success = _platform->readAsJson(filename, eventJson);
    if (success && !eventJson.empty())
    {
      _emotionEvents.emplace(std::piecewise_construct, std::forward_as_tuple(filename), std::forward_as_tuple(std::move(eventJson)));
      LOG_DEBUG("RobotDataLoader.EmotionEvents", "Loaded '%s'", filename.c_str());
    }
    else
    {
      LOG_WARNING("RobotDataLoader.EmotionEvents", "Failed to read '%s'", filename.c_str());
    }
  }
}

void RobotDataLoader::LoadBehaviors()
{
  const std::string path =  "config/engine/behaviorComponent/behaviors/";

  const std::string behaviorFolder = _platform->pathToResource(Util::Data::Scope::Resources, path);
  auto behaviorJsonFiles = Util::FileUtils::FilesInDirectory(behaviorFolder, true, ".json", true);
  for (const auto& filename : behaviorJsonFiles)
  {
    Json::Value behaviorJson;
    const bool success = _platform->readAsJson(filename, behaviorJson);
    if (success && !behaviorJson.empty())
    {
      BehaviorID behaviorID = ICozmoBehavior::ExtractBehaviorIDFromConfig(behaviorJson, filename);
      auto result = _behaviors.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(behaviorID),
                                       std::forward_as_tuple(std::move(behaviorJson)));

      DEV_ASSERT_MSG(result.second,
                     "RobotDataLoader.LoadBehaviors.FailedEmplace",
                     "Failed to insert BehaviorID %s - make sure all behaviors have unique IDs",
                     BehaviorTypesWrapper::BehaviorIDToString(behaviorID));

    }
    else if (!success)
    {
      LOG_WARNING("RobotDataLoader.Behavior", "Failed to read '%s'", filename.c_str());
    }
  }
}


void RobotDataLoader::LoadSpritePaths()
{
    // Get all independent sprites
  {
    auto spritePaths = {kPathToExternalIndependentSprites,
                        kPathToEngineIndependentSprites};
    
    const bool useFullPath = true;
    const char* extensions = "png";
    const bool recurse = true;
    for(const auto& path: spritePaths){
      const std::string& fullPathFolder = _platform->pathToResource(Util::Data::Scope::Resources, path);

      auto fullImagePaths = Util::FileUtils::FilesInDirectory(fullPathFolder, useFullPath, extensions, recurse);
      for(const auto& fullImagePath : fullImagePaths){
        const std::string& fileName = Util::FileUtils::GetFileName(fullImagePath, true, true);
        _spritePaths->AddAsset(fileName, fullImagePath, false);
      }
    }
    _spritePaths->VerifyPlaceholderAsset();
  }
}

void RobotDataLoader::LoadAnimationWhitelist()
{
  static const std::string jsonFilename = "config/engine/animation_whitelist.json";
  Json::Value whitelistConfig;
  const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, whitelistConfig);
  if(!success)
  {
    LOG_ERROR("RobotDataLoader.AnimationWhitelistConfig",
              "Animation whitelist json config file %s not found or failed to parse",
              jsonFilename.c_str());
  }
  else {
    static constexpr const char* kDriveOffChargerAnimsKey = "driveOffChargerAnims";
    
    for( const auto& clipName : whitelistConfig[kDriveOffChargerAnimsKey] ) {
      if( ANKI_VERIFY( clipName.isString(),
                       "RobotDataLoader.LoadAnimationWhitelist.DriveOffAnims.NonString",
                       "List values must be strings" ) ) {
        _whitelistedChargerAnimationPrefixes.push_back(clipName.asString());
      }
    }

    PRINT_CH_INFO("Animations", "RobotDataLoader.AnimationWhitelist.LoadedConfig",
                  "Loaded %zu charger whitelisted animation prefixes",
                  _whitelistedChargerAnimationPrefixes.size());
  }
}

void RobotDataLoader::LoadWeatherResponseMaps()
{
  _weatherResponseMap = std::make_unique<WeatherResponseMap>();
  const bool useFullPath = false;
  const char* extensions = ".json";
  const bool recurse = true;


  const std::string path =  "config/engine/behaviorComponent/weather/weatherResponseMaps/";
  const char* kAPIValueKey = "APIValue";
  const char* kCladTypeKey = "CladType";

  const std::string responseFolder = _platform->pathToResource(Util::Data::Scope::Resources, path);
  auto responseJSONFiles = Util::FileUtils::FilesInDirectory(responseFolder, useFullPath, extensions, recurse);
  for (const auto& filename : responseJSONFiles)
  {
    Json::Value responseJSON;
    const bool success = _platform->readAsJson(filename, responseJSON);
    if (success && 
        !responseJSON.empty() &&
        responseJSON.isArray())
    {
      for(const auto& pair : responseJSON){
        if(ANKI_VERIFY(pair.isMember(kAPIValueKey) && pair.isMember(kCladTypeKey),
                       "RobotDataLoader.LoadWeatherResponseMaps.PairMissingKey",
                       "File %s has an invalid pair",
                       filename.c_str())){
          WeatherConditionType cond = WeatherConditionTypeFromString(pair[kCladTypeKey].asString());

          std::string str = pair[kAPIValueKey].asString();
          std::transform(str.begin(), str.end(), str.begin(), 
                         [](const char c) { return std::tolower(c); });
          
          if(!str.empty()){
            auto resPair  = _weatherResponseMap->emplace(std::make_pair(str, cond));
            ANKI_VERIFY(resPair.second,
                        "RobotDataLoader.LoadWeatherResponseMaps.DuplicateAPIKey",
                        "Key %s already exists within the weather response map ",
                        str.c_str());
          }else{
            PRINT_NAMED_ERROR("RobotDataLoader.LoadWeatherResponseMaps.MissingAPIValue",
                              "APIValue that maps to %s in file %s is blank",
                              WeatherConditionTypeToString(cond), filename.c_str());
          }

        }
      }
    }
    else if (!success)
    {
      LOG_WARNING("RobotDataLoader.LoadWeatherResponseMap", "Failed to read '%s'", filename.c_str());
    }
  }


}

void RobotDataLoader::LoadWeatherRemaps()
{
  static const std::string jsonFilename = "config/engine/behaviorComponent/weather/condition_remaps.json";
  const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _weatherRemaps);
  if(!success)
  {
    PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherRemaps.ErrorReadingFile","");
  }
}

void RobotDataLoader::LoadWeatherConditionTTSMap()
{
  _weatherConditionTTSMap = std::make_unique<WeatherConditionTTSMap>();
  static const std::string jsonFilename = "config/engine/behaviorComponent/weather/condition_to_tts.json";
  
  Json::Value conditionList;
  const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, conditionList);
  if(!success || !conditionList.isArray())
  {
    PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherConditionTTSMap.ErrorReadingFile","");
    return;
  }

  const char* kConditionKey = "Condition";
  const char* kWhatToSayKey = "Say";
  WeatherConditionType condition = WeatherConditionType::Count;
  for(const auto& entry: conditionList){
    if(!entry.isMember(kConditionKey)){
      PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherConditionTTSMap.EntryDoesNotContainCondition","");
      continue;
    }
    if(!entry.isMember(kWhatToSayKey)){
      PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherConditionTTSMap.EntryDoesNotContainSayKey","");
      continue;
    }
    const bool conditionExists = WeatherConditionTypeFromString(entry[kConditionKey].asString(), condition);
    if(!conditionExists){
      PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherConditionTTSMap.InvalidWeatherCondition",
                          "Condition %s not found in weather condition enum",
                          entry[kConditionKey].asString().c_str());
      continue;
    }
    _weatherConditionTTSMap->emplace(std::move(condition), entry[kWhatToSayKey].asString());
  }

  if(_weatherConditionTTSMap->size() != static_cast<int>(WeatherConditionType::Count)){
    PRINT_NAMED_WARNING("RobotDataLoader.LoadWeatherConditionTTSMap.MissingConditions",
                        "There are %d weather conditions, but only %zu TTS entries",
                        static_cast<int>(WeatherConditionType::Count), 
                        _weatherConditionTTSMap->size());
  }

}


void RobotDataLoader::LoadVariableSnapshotJsonMap()
{
  _variableSnapshotJsonMap = std::make_unique<VariableSnapshotJsonMap>();
  
  std::string path = VariableSnapshotComponent::GetSavePath(_platform,
                                                            VariableSnapshotComponent::kVariableSnapshotFolder,
                                                            VariableSnapshotComponent::kVariableSnapshotFilename);
  Json::Value outLoadedJson;
  const bool success = _platform->readAsJson(path,
                                             outLoadedJson);
  // check whether the look up was successful and we got back a nonempty JSON array
  if (success && !outLoadedJson.empty() && outLoadedJson.isArray()) {
    for(const auto& loadedInfo : outLoadedJson) {
      // store the json object in the map
      const auto key = loadedInfo[VariableSnapshotEncoder::kVariableSnapshotIdKey].asString();
      VariableSnapshotId variableSnapshotId = VariableSnapshotId::Count;
      if(VariableSnapshotIdFromString(key, variableSnapshotId)){
        _variableSnapshotJsonMap->emplace(variableSnapshotId, loadedInfo);
      }else{
        PRINT_NAMED_WARNING("RobotDataLoader.LoadVariableSnapshotJsonMap.UnknownStringinJson",
                            "Key %s was not recognized as a valid snapshot value, will be dropped", key.c_str());
      }
    }
  }
}


void RobotDataLoader::LoadCubeSpinnerConfig()
{
  static const std::string jsonFilename = "config/engine/behaviorComponent/cubeSpinnerLightMaps.json";
  const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _cubeSpinnerConfig);
  if(!success)
  {
    LOG_ERROR("RobotDataLoader.LoadCubeSpinnerConfig",
              "LoadCubeSpinnerConfig Json config file %s not found or failed to parse",
              jsonFilename.c_str());
  }
}

void RobotDataLoader::LoadUserDefinedBehaviorTreeConfig()
{
  const char* kBehaviorOptionsKey = "behaviorOptions";
  const char* kConditionTypeKey = "conditionType";
  const char* kEditModeTriggerIDKey = "editModeTrigger";
  const char* kMappingOptionsListKey = "conditionToBehaviorMappingOptions";

  Json::Value _userDefinedBehaviorTreeConfig;
  static const std::string jsonFilename = "config/engine/userDefinedBehaviorTree/conditionToBehaviorMap.json";
  const bool jsonSuccess = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _userDefinedBehaviorTreeConfig);

  // if json is read properly, load the config data
  if(!jsonSuccess)
  {
    LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig",
              "LoadUserDefinedBehaviorTreeConfig Json config file %s not found or failed to parse",
              jsonFilename.c_str());
  }

  // load the behavior that triggers editing
  _userDefinedEditCondition = BEIConditionType::Invalid;

  const std::string editBehaviorIdString = JsonTools::ParseString(_userDefinedBehaviorTreeConfig,
                                                                  kEditModeTriggerIDKey,
                                                                  "RobotDataLoader.LoadUserDefinedBehaviorTreeConfig.ParseEditConditionStringFailed");
  const bool editBehaviorIdSuccess = BEIConditionTypeFromString(editBehaviorIdString, _userDefinedEditCondition);

  if(!editBehaviorIdSuccess) {
    LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig",
              "LoadUserDefinedBehaviorTreeConfig: Edit behavior %s not a valid BehaviorID.",
              editBehaviorIdString.c_str());
    return;
  }

  // load the map of possible condition to behavior mappings
  _conditionToBehaviorsMap = std::make_unique<ConditionToBehaviorsMap>();

  for(const Json::Value& mapOptionsJson : _userDefinedBehaviorTreeConfig[kMappingOptionsListKey]) {
    BEIConditionType beiCondType = BEIConditionType::Invalid;
    const std::string beiCondTypeString = JsonTools::ParseString(mapOptionsJson,
                                                                 kConditionTypeKey,
                                                                 "RobotDataLoader.LoadUserDefinedBehaviorTreeConfig.ParseConditionStringFailed");
    const bool beiCondTypeParseSuccess = BEIConditionTypeFromString(beiCondTypeString, beiCondType);
    const bool editConditionMappedToBehaviors = _userDefinedEditCondition == beiCondType;

    // edit condition should not be customizable
    if(editConditionMappedToBehaviors) {
      LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig",
                "LoadUserDefinedBehaviorTreeConfig: edit condition should not be customizable.");
      return;
    }

    // if parsing the BEIConditionType works, read the behaviorIds
    if(!beiCondTypeParseSuccess) {
      LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig",
                "LoadUserDefinedBehaviorTreeConfig: %s not a valid BEIConditionType.",
                beiCondTypeString.c_str());
      return;
    }

    // if the BehaviorID strings are parsed to strings successfully, convert them to BehaviorIDs
    std::vector<std::string> behaviorIdStrings;
    const bool behaviorIdStringsParseSuccess = JsonTools::GetVectorOptional<std::string>(mapOptionsJson, kBehaviorOptionsKey, behaviorIdStrings);

    if(!behaviorIdStringsParseSuccess) {
      LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig.ParseBehaviorStringsFailed",
                "LoadUserDefinedBehaviorTreeConfig: Could not parse list of Json BehaviorID Strings.");
      return;
    }

    // if the string->BehaviorID conversion happens successfully, add them into a set
    std::set<BehaviorID> behaviors;
    for(const auto& behaviorIdString : behaviorIdStrings) {
      BehaviorID behaviorId = BehaviorID::Anonymous;
      const bool behaviorIdSuccess = BehaviorIDFromString(behaviorIdString, behaviorId);

      if(!behaviorIdSuccess) {
        LOG_ERROR("RobotDataLoader.LoadUserDefinedBehaviorTreeConfig",
                  "LoadUserDefinedBehaviorTreeConfig: %s not a valid BehaviorID.",
                  behaviorIdString.c_str());
        return;
      }

      behaviors.emplace(behaviorId);
    }

    // add the set of behaviors into the map
    _conditionToBehaviorsMap->emplace(beiCondType, behaviors);
  }

}

std::map<std::string, std::string> RobotDataLoader::CreateFileNameToFullPathMap(const std::vector<const char*> & srcDirs, const std::string& fileExtensions) const
{
  std::map<std::string, std::string> fileNameToFullPath;
  // Get all independent sprites
  {
    const bool useFullPath = true;
    const bool recurse = true;
    for(const auto& dir: srcDirs){
      const std::string fullPathFolder = _platform->pathToResource(Util::Data::Scope::Resources, dir);

      auto fullImagePaths = Util::FileUtils::FilesInDirectory(fullPathFolder, useFullPath, fileExtensions.c_str(), recurse);
      for(auto& fullImagePath : fullImagePaths){
        const std::string fileName = Util::FileUtils::GetFileName(fullImagePath, true, true);
        fileNameToFullPath.emplace(fileName, fullImagePath);
      }
    }
  }

  return fileNameToFullPath;
}


void RobotDataLoader::LoadAnimationTriggerMap()
{
  _animationTriggerMap->Load(_platform, "assets/cladToFileMaps/AnimationTriggerMap.json", "AnimName");
}

void RobotDataLoader::LoadCubeAnimationTriggerMap()
{
  _cubeAnimationTriggerMap->Load(_platform, "assets/cladToFileMaps/CubeAnimationTriggerMap.json", "AnimName");
}



void RobotDataLoader::LoadDasBlacklistedAnimations()
{
  static const std::string kBlacklistedAnimationTriggersConfigKey = "blacklisted_animation_triggers";
  const Json::Value& blacklistedTriggers = _dasEventConfig[kBlacklistedAnimationTriggersConfigKey];
  for (int i = 0; i < blacklistedTriggers.size(); i++)
  {
    const std::string& trigger = blacklistedTriggers[i].asString();
    _dasBlacklistedAnimationTriggers.insert(AnimationTriggerFromString(trigger));
  }
  static const std::string kBlacklistedAnimationNamesConfigKey = "blacklisted_animation_names";
  const Json::Value& blacklistedAnims = _dasEventConfig[kBlacklistedAnimationNamesConfigKey];
  for (int i = 0; i < blacklistedAnims.size(); i++)
  {
    const std::string& animName = blacklistedAnims[i].asString();
    _dasBlacklistedAnimationNames.insert( animName );
  }
}


void RobotDataLoader::LoadRobotConfigs()
{
  if (_platform == nullptr) {
    return;
  }

  ANKI_CPU_TICK_ONE_TIME("RobotDataLoader::LoadRobotConfigs");
  // mood config
  {
    static const std::string jsonFilename = "config/engine/mood_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _robotMoodConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.MoodConfigJsonNotFound",
                "Mood Json config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // victor behavior systems config
  {
    static const std::string jsonFilename = "config/engine/behaviorComponent/victor_behavior_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _victorFreeplayBehaviorConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.BehaviorSystemJsonFailed",
                "Behavior Json config file %s not found or failed to parse",
                jsonFilename.c_str());
      _victorFreeplayBehaviorConfig.clear();
    }
  }

  // vision config
  {
    static const std::string jsonFilename = "config/engine/vision_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _robotVisionConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.VisionConfigJsonNotFound",
                "Vision Json config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // visionScheduleMediator config
  {
    static const std::string jsonFilename = "config/engine/visionScheduleMediator_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _visionScheduleMediatorConfig);
    if(!success)
    {
      LOG_ERROR("RobotDataLoader.VisionScheduleMediatorConfigNotFound",
                "VisionScheduleMediator Json config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // userIntentsComponent config (also maps cloud intents to user intents)
  {
    static const std::string jsonFilename = "config/engine/behaviorComponent/user_intent_map.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _userIntentsConfig);
    if(!success)
    {
      LOG_ERROR("RobotDataLoader.UserIntentsConfigNotFound",
                "UserIntents Json config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // DAS event config
  {
    static const std::string jsonFilename = "config/engine/das_event_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _dasEventConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.DasEventConfigJsonNotFound",
                "DAS Event Json config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // feature gate
  {
    const std::string filename{_platform->pathToResource(Util::Data::Scope::Resources, "config/features.json")};
    const std::string fileContents{Util::FileUtils::ReadFile(filename)};
    _context->GetFeatureGate()->Init(_context, fileContents);
  }

  // A/B testing definition
  {
    const std::string filename{_platform->pathToResource(Util::Data::Scope::Resources, "config/experiments.json")};
    const std::string fileContents{Util::FileUtils::ReadFile(filename)};
    _context->GetExperiments()->GetAnkiLab().Load(fileContents);
  }

  // Web server config
  {
    static const std::string jsonFilename = "webserver/webServerConfig_engine.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _webServerEngineConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.WebServerEngineConfigNotFound",
                "Web Server Engine Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // Photography config
  {
    static const std::string jsonFilename = "config/engine/photography_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _photographyConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.PhotographyConfigNotFound",
                "Photography Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // Settings config
  {
    static const std::string jsonFilename = "config/engine/settings_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _settingsConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.SettingsConfigNotFound",
                "Settings Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // Eye color config
  {
    static const std::string jsonFilename = "config/engine/eye_color_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _eyeColorConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.EyeColorConfigNotFound",
                "Eye Color Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // Jdocs config
  {
    static const std::string jsonFilename = "config/engine/jdocs_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _jdocsConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.JdocsConfigNotFound",
                "Jdocs Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // Account settings config
  {
    static const std::string jsonFilename = "config/engine/accountSettings_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _accountSettingsConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.AccountSettingsConfigNotFound",
                "Account Settings Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }

  // User entitlements config
  {
    static const std::string jsonFilename = "config/engine/userEntitlements_config.json";
    const bool success = _platform->readAsJson(Util::Data::Scope::Resources, jsonFilename, _userEntitlementsConfig);
    if (!success)
    {
      LOG_ERROR("RobotDataLoader.UserEntitlementsConfigNotFound",
                "User Entitlements Config file %s not found or failed to parse",
                jsonFilename.c_str());
    }
  }
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

bool RobotDataLoader::HasAnimationForTrigger( AnimationTrigger ev ) const
{
  return _animationTriggerMap->HasKey(ev);
}
std::string RobotDataLoader::GetAnimationForTrigger( AnimationTrigger ev ) const
{
  return _animationTriggerMap->GetValue(ev);
}
std::string RobotDataLoader::GetCubeAnimationForTrigger( CubeAnimationTrigger ev ) const
{
  return _cubeAnimationTriggerMap->GetValue(ev);
}

bool RobotDataLoader::IsAnimationAllowedToMoveBodyOnCharger(const std::string& animName) const
{
  for (const auto& whitelistAnimPrefix : _whitelistedChargerAnimationPrefixes) {
    if (Util::StringStartsWith(animName, whitelistAnimPrefix)) {
      return true;
    }
  }
  return false;
}


}
}
