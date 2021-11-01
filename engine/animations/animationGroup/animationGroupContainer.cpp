/**
 * File: animationGroupContainer.cpp
 *
 * Authors: Trevor Dasch
 * Created: 2016-01-12
 *
 * Description: Container for hard-coded or json-defined animations groups
 *              used to determine which animations to send to cozmo
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/animations/animationGroup/animationGroupContainer.h"

#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/random/randomGenerator.h"

namespace Anki {
namespace Vector {
    
AnimationGroupContainer::AnimationGroupContainer(Util::RandomGenerator& rng)
: _rng(rng)
{
}
    
Result AnimationGroupContainer::AddAnimationGroup(const std::string& name)
{
  Result lastResult = RESULT_OK;
      
  auto retVal = _animationGroups.find(name);
  if(retVal == _animationGroups.end()) {
    _animationGroups.emplace(name,AnimationGroup(_rng, name));
  }
      
  return lastResult;
}
    
AnimationGroup* AnimationGroupContainer::GetAnimationGroup(const std::string& name)
{
  const AnimationGroup* animGroupPtr = const_cast<const AnimationGroupContainer *>(this)->GetAnimationGroup(name);
  return const_cast<AnimationGroup*>(animGroupPtr);
}
    
const AnimationGroup* AnimationGroupContainer::GetAnimationGroup(const std::string& name) const
{
  const AnimationGroup* animPtr = nullptr;
      
  auto retVal = _animationGroups.find(name);
  if(retVal == _animationGroups.end()) {
    PRINT_NAMED_ERROR("AnimationGroupContainer.GetAnimationGroup_Const.InvalidName",
                      "AnimationGroup requested for unknown animation group '%s'.",
                      name.c_str());
  } else {
    animPtr = &retVal->second;
  }
      
  return animPtr;
}
  
bool AnimationGroupContainer::HasGroup(const std::string& name) const
{
  auto retVal = _animationGroups.find(name);
  return retVal != _animationGroups.end();
}
  
std::vector<std::string> AnimationGroupContainer::GetAnimationGroupNames()
{
  std::vector<std::string> v;
  v.reserve(_animationGroups.size());
  for (std::unordered_map<std::string, AnimationGroup>::iterator i=_animationGroups.begin();
       i != _animationGroups.end();
       ++i) {
    v.push_back(i->first);
  }
  return v;
}
    
    
Result AnimationGroupContainer::DefineFromJson(const Json::Value& jsonRoot,
                                               const std::string& animationGroupName)
{
      
  if(RESULT_OK != AddAnimationGroup(animationGroupName)) {
    PRINT_CH_INFO("Animations", "AnimationGroupContainer.DefineAnimationGroupFromJson.ReplaceName",
                     "Replacing existing animation group named '%s'.",
                     animationGroupName.c_str());
  }
      
  AnimationGroup* animationGroup = GetAnimationGroup(animationGroupName);
  if(animationGroup == nullptr) {
    PRINT_NAMED_ERROR("AnimationGroupContainer.DefineAnimationGroupFromJson",
                      "Could not GetAnimationGroup named '%s'.",
                      animationGroupName.c_str());
    return RESULT_FAIL;
  }
      
  Result result = animationGroup->DefineFromJson(animationGroupName, jsonRoot);
      
      
  if(result != RESULT_OK) {
    PRINT_NAMED_ERROR("AnimationGroupContainer.DefineAnimationGroupFromJson",
                      "Failed to define animation group '%s' from Json.",
                      animationGroupName.c_str());
  }
      
  return result;
} // AnimationGroupContainer::DefineAnimationGroupFromJson()
    
    
void AnimationGroupContainer::Clear()
{
  _animationGroups.clear();
} // Clear()
    
    
    
bool AnimationGroupContainer::IsAnimationOnCooldown(const std::string& name, double currentTime_s) const
{
  auto retVal = _animationCooldowns.find(name);
  if(retVal == _animationCooldowns.end()) {
    return false;
  } else {
    return currentTime_s < retVal->second;
  }
}

float AnimationGroupContainer::TimeUntilCooldownOver(const std::string& name, double currentTime_s) const
{
  auto retVal = _animationCooldowns.find(name);
  if(retVal == _animationCooldowns.end()) {
    return 0.0f;
  } else {
    return Util::numeric_cast<float>(retVal->second - currentTime_s);
  }      
}
  
void AnimationGroupContainer::SetAnimationCooldown(const std::string& name, double cooldownExpiration_s)
{
  _animationCooldowns[name] = cooldownExpiration_s;
}
    
} // namespace Vector
} // namespace Anki
