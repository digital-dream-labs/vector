/**
 * File: cannedAnimationLoader.cpp
 *
 * Authors: Kevin M. Karol
 * Created: 1/18/18
 *
 * Description:
 *    Class that loads animations from data on worker threads and
 *    returns the final animation container
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "cannedAnimLib/cannedAnims/cannedAnimationLoader.h"

#include "cannedAnimLib/cannedAnims/cannedAnimationContainer.h"
#include "cannedAnimLib/baseTypes/cozmo_anim_generated.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/dispatchWorker/dispatchWorker.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/boundedWhile.h"
#include "util/time/universalTime.h"
#include <sys/stat.h>

#define LOG_CHANNEL   "RobotDataLoader"

namespace Anki {
namespace Vector {

namespace{
// We report some loading data info so the UI can inform the user. Ratio of time taken per section is approximate,
// based on recent profiling. Some sections below are called out specifically, the rest makes up the remainder.
// These should add up to be less than or equal to 1.0!
static constexpr float kAnimationsLoadingRatio = 0.7f;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::LoadAnimationsIntoContainer(const AnimDirInfo& info, CannedAnimationContainer* container)
{
  {
    ANKI_CPU_PROFILE("CannedAnimationLoader::LoadAnimations");
    LoadAnimationsInternal(info, container);
    // The threaded animation loading workers each add to the loading ratio
  }

  // we're done
  _loadingCompleteRatio.store(1.0f);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::LoadAnimationIntoContainer(const std::string& path, CannedAnimationContainer* container)
{
  AnimDirInfo info;
  info.jsonFiles.push_back(path);
  {
    ANKI_CPU_PROFILE("CannedAnimationLoader::LoadAnimationFile");
    LoadAnimationsInternal(info, container);
  }

  // TODO: be able to load a face animation

  // we're done
  _loadingCompleteRatio.store(1.0f);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CannedAnimationLoader::AnimDirInfo CannedAnimationLoader::CollectAnimFiles(const std::vector<std::string>& paths)
{
  ANKI_CPU_PROFILE("CannedAnimationLoader::CollectFiles");
  AnimDirInfo info;
  // animations
  {
    for (const auto& path : paths) {
      CannedAnimationLoader::WalkAnimationDir(_platform, path, info.animFileTimestamps, 
        [&info] (const std::string& filename) {
          info.jsonFiles.push_back(filename);
        });
    }
  }

  // print results
  LOG_INFO("CannedAnimationLoader.CollectAnimFiles.Results", "Found %zu animation files", info.jsonFiles.size());
 
  return info;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::WalkAnimationDir(const Util::Data::DataPlatform* platform,
                                             const std::string& animationDir, AnimDirInfo::TimestampMap& timestamps, 
                                             const std::function<void(const std::string&)>& walkFunc)
{
  const std::string animationFolder = platform->pathToResource(Util::Data::Scope::Resources, animationDir);
  const std::vector<const char*> fileExts = {"json", "bin"};
  auto filePaths = Util::FileUtils::FilesInDirectory(animationFolder, true, fileExts, true);

  for (const auto& path : filePaths) {
    struct stat attrib{0};
    int result = stat(path.c_str(), &attrib);
    if (result == -1) {
      LOG_WARNING("CannedAnimationLoader.WalkAnimationDir", "could not get mtime for %s", path.c_str());
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


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::AddToLoadingRatio(float delta)
{
  // Allows for a thread to repeatedly try to update the loading ratio until it gets access
  auto current = _loadingCompleteRatio.load();
  while (!_loadingCompleteRatio.compare_exchange_weak(current, current + delta));
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::LoadAnimationsInternal(const AnimDirInfo& info, CannedAnimationContainer* container)
{
#if ALLOW_DEBUG_LOGGING
  const double startTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
#endif
  
  // Disable super-verbose warnings about clipping face parameters in json files
  // To help find bad/deprecated animations, try removing this.
  ProceduralFace::EnableClippingWarning(false);

  using MyDispatchWorker = Util::DispatchWorker<3, const std::string&, CannedAnimationContainer*>;
  MyDispatchWorker::FunctionType loadFileFunc = std::bind(&CannedAnimationLoader::LoadAnimationFile, this, 
                                                          std::placeholders::_1, std::placeholders::_2);
  MyDispatchWorker myWorker(loadFileFunc);

  unsigned long size = info.jsonFiles.size();
  for (int i = 0; i < size; i++) {
    myWorker.PushJob(info.jsonFiles[i], container);
    //LOG_DEBUG("CannedAnimationLoader.LoadAnimations", "loaded regular anim %d of %zu", i, size);
  }

  _perAnimationLoadingRatio = kAnimationsLoadingRatio * 1.0f / Util::numeric_cast<float>(size);
  myWorker.Process();

  ProceduralFace::EnableClippingWarning(true);

#if ALLOW_DEBUG_LOGGING
  const double endTime = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
  const double loadTime = endTime - startTime;

  LOG_DEBUG("CannedAnimationLoader.LoadAnimationsInternal.LoadTime",
            "Time to load animations = %.2f ms",
            loadTime);

  const auto & animNames = container->GetAnimationNames();

  LOG_DEBUG("CannedAnimationLoader.LoadAnimations.CannedAnimationsCount",
            "Total number of canned animations available = %zu",
            animNames.size());
#endif
}





// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationLoader::LoadAnimationFile(const std::string& path, CannedAnimationContainer* container)
{
  if (_abortLoad.load(std::memory_order_relaxed)) {
    return;
  }

  //PRINT_CH_DEBUG("Animations", "CannedAnimationLoader.LoadAnimationFile.LoadingAnimationsFromBinaryOrJson",
  //               "Loading animations from %s", path.c_str());

  const bool binFile = Util::FileUtils::FilenameHasSuffix(path.c_str(), "bin");

  if (binFile) {

    // Read the binary file
    auto binFileContents = Util::FileUtils::ReadFileAsBinary(path);
    if (binFileContents.size() == 0) {
      LOG_ERROR("CannedAnimationLoader.LoadAnimationFile.BinaryDataEmpty", "Found no data in %s", path.c_str());
      return;
    }
    unsigned char *binData = binFileContents.data();
    if (nullptr == binData) {
      LOG_ERROR("CannedAnimationLoader.LoadAnimationFile.BinaryDataNull", "Found no data in %s", path.c_str());
      return;
    }
    auto animClips = CozmoAnim::GetAnimClips(binData);
    if (nullptr == animClips) {
      LOG_ERROR("CannedAnimationLoader.LoadAnimationFile.AnimClipsNull", "Found no animations in %s", path.c_str());
      return;
    }
    auto allClips = animClips->clips();
    if (nullptr == allClips) {
      LOG_ERROR("CannedAnimationLoader.LoadAnimationFile.AllClipsNull", "Found no animations in %s", path.c_str());
      return;
    }
    if (allClips->size() == 0) {
      LOG_ERROR("CannedAnimationLoader.LoadAnimationFile.AnimClipsEmpty", "Found no animations in %s", path.c_str());
      return;
    }

    for (int clipIdx=0; clipIdx < allClips->size(); clipIdx++) {
      auto animClip = allClips->Get(clipIdx);
      auto animName = animClip->Name()->c_str();
      //PRINT_CH_DEBUG("Animations", "CannedAnimationLoader.LoadAnimationFile.LoadingSpecificAnimFromBinary",
      //              "Loading '%s' from %s", animName, path.c_str());
      std::string strName = animName;

      // TODO: Should this mutex lock happen here or immediately before this for loop (COZMO-8766)?
      std::lock_guard<std::mutex> guard(_parallelLoadingMutex);

      DefineFromFlatBuf(animClip, strName, container);
    }

  } else {
    Json::Value animDefs;
    // add json filename and callback (to perform load) here?
    const bool success = _platform->readAsJson(path.c_str(), animDefs);
    std::string animationId;
    if (success && !animDefs.empty()) {
      std::lock_guard<std::mutex> guard(_parallelLoadingMutex);
      DefineFromJson(animDefs, animationId, container);

      // TODO: This warning is useful, but it causes a crash when we use the current mechanism for
      //       animators to preview their work in Maya on the robot. We plan on changing that
      //       preview-on-robot to use the SDK, so this warning should be tested and potentially
      //       enabled after that. See COZMO-9251 for some related info (nishkar, 2/2/2017).
      //
      //if(path.find(animationId) == std::string::npos) {
      //  PRINT_NAMED_WARNING("CannedAnimationLoader.LoadAnimationFile.AnimationNameMismatch",
      //                      "Animation name '%s' does not seem to match filename '%s'",
      //                      animationId.c_str(), path.c_str());
      //}

    }
  }
  AddToLoadingRatio(_perAnimationLoadingRatio);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result CannedAnimationLoader::DefineFromFlatBuf(const CozmoAnim::AnimClip* animClip, std::string& animName,
                                                CannedAnimationContainer* container)
{
  Animation animation(animName);

  Result lastResult = animation.DefineFromFlatBuf(animName, animClip, _spriteSequenceContainer);

  const Result res = SanityCheck(lastResult, animation, animName);
  if(res == Result::RESULT_OK){
    bool outOverwriting = false;
    container->AddAnimation(std::move(animation), outOverwriting);
    if(outOverwriting){
      PRINT_NAMED_WARNING("CannedAnimationLoader.DefineFromFlatBuf.OverwritingExistingAnimation",
                          "Container already had an animation named %s, overwriting",
                          animName.c_str());
    }
  }
  return res;

} // CannedAnimationLoader::DefineFromFlatBuf()


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result CannedAnimationLoader::DefineFromJson(const Json::Value& jsonRoot, std::string& animationName,
                                             CannedAnimationContainer* container)
{

  Json::Value::Members animationNames = jsonRoot.getMemberNames();
  
  /*
  // Add _all_ the animations first to register the IDs, so Trigger keyframes
  // which specify another animation's name will still work (because that
  // name should already exist, no matter the order the Json is parsed)
  for(auto const& animationName : animationNames) {
    AddAnimation(animationName);
  }
  */
  
  if(animationNames.empty()) {
    PRINT_NAMED_ERROR("CannedAnimationLoader.DefineFromJson.EmptyFile",
                      "Found no animations in JSON");
    return RESULT_FAIL;
  } else if(animationNames.size() != 1) {
    PRINT_NAMED_WARNING("CannedAnimationLoader.DefineFromJson.TooManyAnims",
                        "Expecting only one animation per json file, found %lu. "
                        "Will use first: %s",
                        (unsigned long)animationNames.size(), animationNames[0].c_str());
  }
  
  animationName = animationNames[0];

  PRINT_CH_DEBUG(LOG_CHANNEL, "CannedAnimationLoader::DefineFromJson", "Loading '%s'", animationName.c_str());

  Animation animation(animationName);
  Result lastResult = animation.DefineFromJson(animationName, jsonRoot[animationName], _spriteSequenceContainer);

  const Result res = SanityCheck(lastResult, animation, animationName);
  if(res == Result::RESULT_OK){
    bool outOverwriting = false;
    container->AddAnimation(std::move(animation), outOverwriting);
    if(outOverwriting){
      PRINT_NAMED_WARNING("CannedAnimationLoader.DefineFromJson.OverwritingExistingAnimation",
                          "Container already had an animation named %s, overwriting",
                          animationName.c_str());
    }
  }
  return res;
} // CannedAnimationLoader::DefineFromJson()



// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result CannedAnimationLoader::SanityCheck(Result lastResult, Animation& animation, std::string& animationName) const
{
  if(animation.GetName() != animationName) {
    PRINT_NAMED_ERROR("CannedAnimationContainer.DefineFromJson",
                      "Animation's internal name ('%s') doesn't match container's name for it ('%s').",
                      animation.GetName().c_str(),
                      animationName.c_str());
    return RESULT_FAIL;
  }
  
  if(lastResult != RESULT_OK) {
    PRINT_NAMED_ERROR("CannedAnimationContainer.DefineFromJson",
                      "Failed to define animation '%s' from Json.",
                      animationName.c_str());
    return lastResult;
  }

  return RESULT_OK;
} // CannedAnimationLoader::SanityCheck()


} // namespace Vector
} // namespace Anki
