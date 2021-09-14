/**
 * File: cannedAnimationContainer.cpp
 *
 * Authors: Andrew Stein
 * Created: 2014-10-22
 *
 * Description: Container for hard-coded or json-defined "canned" animations
 *              stored on the basestation and send-able to the physical robot.
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "cannedAnimLib/cannedAnims/cannedAnimationContainer.h"
#include "coretech/vision/shared/spriteSequence/spriteSequenceContainer.h"
#include "cannedAnimLib/baseTypes/track.h"
#include "cannedAnimLib/cannedAnims/cannedAnimationLoader.h"

#include "util/helpers/boundedWhile.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "Animations"

namespace Anki {
namespace Vector {

#if ANKI_DEV_CHEATS

CannedAnimationContainer* s_cubeAnimContainer = nullptr;
const char* kCubeSpinnerAnimationName = "anim_spinner_tap_01";
CONSOLE_VAR(int, kAdjustHeightOfSpinnerLift, "CubeSpinner", 81);

void SetNewTapHeight(ConsoleFunctionContextRef context)
{
  if(s_cubeAnimContainer != nullptr){
    Animation* anim = s_cubeAnimContainer->GetAnimation(kCubeSpinnerAnimationName);
    auto& track = anim->GetTrack<LiftHeightKeyFrame>();
    std::list<LiftHeightKeyFrame>& frames = track.GetAllKeyframes();
    auto iter = frames.begin();
    iter++;
    iter->OverrideHeight(kAdjustHeightOfSpinnerLift);
  }
}

CONSOLE_FUNC(SetNewTapHeight, "CubeSpinner");
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CannedAnimationContainer::CannedAnimationContainer()
{
  #if ANKI_DEV_CHEATS
  s_cubeAnimContainer = this;
  #endif
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CannedAnimationContainer::~CannedAnimationContainer()
{
  
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CannedAnimationContainer::HasAnimation(const std::string& name) const
{
  auto retVal = _animations.find(name);
  return retVal != _animations.end();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Animation* CannedAnimationContainer::GetAnimation(const std::string& name)
{
  const Animation* animPtr = const_cast<const CannedAnimationContainer *>(this)->GetAnimation(name);
  return const_cast<Animation*>(animPtr);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const Animation* CannedAnimationContainer::GetAnimation(const std::string& name) const
{
  const Animation* animPtr = nullptr;
  
  auto retVal = _animations.find(name);
  if(retVal == _animations.end()) {
    PRINT_NAMED_ERROR("CannedAnimationContainer.GetAnimation_Const.InvalidName",
                      "Animation requested for unknown animation '%s'.",
                      name.c_str());
  } else {
    animPtr = &retVal->second;
  }
  
  return animPtr;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CannedAnimationContainer::AddAnimation(Animation&& animation, bool& outOverwriting)
{
  const std::string& name = animation.GetName();

  // Replace animation with the given one because this
  // is mainly for animators testing new animations
  auto iter = _animations.find(name);
  if(iter != _animations.end()) {
    _animations.erase(iter);
    outOverwriting = true;
  }

  _animations.emplace(name, std::move(animation));
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<std::string> CannedAnimationContainer::GetAnimationNames()
{
  std::vector<std::string> v;
  v.reserve(_animations.size());
  for (std::unordered_map<std::string, Animation>::iterator i=_animations.begin(); i != _animations.end(); ++i) {
    v.push_back(i->first);
  }
  return v;
}

} // namespace Vector
} // namespace Anki
