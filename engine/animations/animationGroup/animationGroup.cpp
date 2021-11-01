/**
 * File: animationGroup.cpp
 *
 * Authors: Trevor Dasch
 * Created: 2016-01-11
 *
 * Description:
 *    Class for storing a group of animations,
 *    from which an animation can be selected
 *    for a given set of moods
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/animations/animationGroup/animationGroup.h"
#include "engine/animations/animationGroup/animationGroupContainer.h"
#include "engine/moodSystem/moodManager.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/random/randomGenerator.h"
#include "coretech/common/engine/jsonTools.h"

#define LOG_CHANNEL "Animations"

#define DEBUG_ANIMATION_GROUP_SELECTION 0

namespace Anki {
namespace Vector {
    
static const char* kAnimationsKeyName = "Animations";
    
AnimationGroup::AnimationGroup(Util::RandomGenerator& rng, const std::string& name)
: _rng(rng)
, _name(name)
{
      
}
    
Result AnimationGroup::DefineFromJson(const std::string& name, const Json::Value &jsonRoot)
{
  _name = name;
      
  const Json::Value& jsonAnimations = jsonRoot[kAnimationsKeyName];

  if(!jsonAnimations.isArray())
  {
    LOG_ERROR("AnimationGroup.DefineFromJson.NoAnimations",
              "Missing '%s' field for animation group.", kAnimationsKeyName);
    return RESULT_FAIL;
  }

  _animations.clear();
      
  const s32 numEntries = jsonAnimations.size();
      
  _animations.reserve(numEntries);
  
  bool anyFailures = false;
  for(s32 iEntry = 0; iEntry < numEntries; ++iEntry)
  {
    const Json::Value& jsonEntry = jsonAnimations[iEntry];
    
    AnimationGroupEntry newEntry;
    
    Result addResult = newEntry.DefineFromJson(jsonEntry);
    if(RESULT_OK != addResult) {
      LOG_ERROR("AnimationGroup.DefineFromJson.AddEntryFailure",
                "Adding animation %d failed.",
                iEntry);
      anyFailures = true;
    } else {
      // Only add if the new entry was defined successfully
      _animations.emplace_back(std::move(newEntry));
    }
    
  } // for each Entry
  
  if(anyFailures) {
    return RESULT_FAIL;
  } else {
    return RESULT_OK;
  }
}
    
bool AnimationGroup::IsEmpty() const
{
  return _animations.empty();
}
    
const std::string& AnimationGroup::GetAnimationName(const MoodManager& moodManager,
                                                    AnimationGroupContainer& animationGroupContainer,
                                                    float headAngleRad,
                                                    bool strictCooldown) const
{
  return GetAnimationName(moodManager.GetSimpleMood(),
                          moodManager.GetLastUpdateTime(),
                          animationGroupContainer,
                          headAngleRad,
                          strictCooldown);
}
  
const std::string& AnimationGroup::GetFirstAnimationName() const
{
  if(_animations.empty())
  {
    LOG_WARNING("AnimationGroup.GetFirstAnimationName.EmptyGroup",
                "No animations in group %s, returning empty string",
                GetName().c_str());
    static const std::string empty = "";
    return empty;
  }
  
  return _animations.front().GetName();
}
  
const std::string& AnimationGroup::GetAnimationName(SimpleMoodType mood,
                                                    float currentTime_s,
                                                    AnimationGroupContainer& animationGroupContainer,
                                                    float headAngleRad,
                                                    bool strictCooldown) const
{
  LOG_DEBUG("AnimationGroup.GetAnimation", "getting animation from group '%s', simple mood = '%s'",
            _name.c_str(),
            SimpleMoodTypeToString(mood));

  float totalWeight = 0.0f;
  bool anyAnimationsMatchingMood = false;
      
  std::vector<const AnimationGroupEntry*> availableAnimations;
      
  for (auto entry = _animations.begin(); entry != _animations.end(); entry++)
  {
    if(entry->GetMood() == mood)
    {
      anyAnimationsMatchingMood = true;
      bool validHeadAngle = true;
      if( entry->GetUseHeadAngle())
      {
        if( !(headAngleRad >= entry->GetHeadAngleMin() && headAngleRad <= entry->GetHeadAngleMax()))
        {
          validHeadAngle = false;
        }
      }
      if( validHeadAngle )
      {
        if( !animationGroupContainer.IsAnimationOnCooldown(entry->GetName(),currentTime_s))
        {
          totalWeight += entry->GetWeight();
          availableAnimations.emplace_back(&(*entry));

          if( DEBUG_ANIMATION_GROUP_SELECTION )
          {
            LOG_INFO("AnimationGroup.GetAnimation.ConsiderAnimation",
                     "%s: considering animation '%s' with weight %f",
                     _name.c_str(),
                     entry->GetName().c_str(),
                     entry->GetWeight());
          }
        }
        else if( DEBUG_ANIMATION_GROUP_SELECTION )
        {
          LOG_INFO("AnimationGroup.GetAnimation.RejectAnimation.Cooldown",
                   "%s: rejecting animation %s with mood %s is on cooldown (timer=%f)",
                   _name.c_str(),
                   entry->GetName().c_str(),
                   SimpleMoodTypeToString(entry->GetMood()),
                   entry->GetCooldown());
        }
      }
      else if( DEBUG_ANIMATION_GROUP_SELECTION ) {
        LOG_INFO("AnimationGroup.GetAnimation.RejectAnimation.HeadAngle",
                 "%s: rejecting animation %s with head angle (%f) out of range (%f,%f)",
                 _name.c_str(),
                 entry->GetName().c_str(),
                 RAD_TO_DEG(headAngleRad),
                 entry->GetHeadAngleMin(),
                 entry->GetHeadAngleMax());
      }
    }
    else if( DEBUG_ANIMATION_GROUP_SELECTION )
    {
      LOG_INFO("AnimationGroup.GetAnimation.RejectAnimation.WrongMood",
               "%s: rejecting animation %s with mood %s %son cooldown",
               _name.c_str(),
               entry->GetName().c_str(),
               SimpleMoodTypeToString(entry->GetMood()),
               animationGroupContainer.IsAnimationOnCooldown(entry->GetName(),currentTime_s) ?
               "" :
               "not ");
    }
  }
      
  float weightedSelection = Util::numeric_cast<float>(_rng.RandDbl(totalWeight));
      
  const AnimationGroupEntry* lastEntry = nullptr;
      
  for (auto entry : availableAnimations)
  {
    lastEntry = &(*entry);
    weightedSelection -= entry->GetWeight();

    if(weightedSelection < 0.0f) {
      break;
    }
  }
      
  // Possible that if weightedSelection == totalWeight, we wouldn't
  // select any, so return the last one if its not the end
  if(lastEntry != nullptr) {
    animationGroupContainer.SetAnimationCooldown(lastEntry->GetName(), currentTime_s + lastEntry->GetCooldown());

    LOG_DEBUG("AnimationGroup.GetAnimation.Found",
              "Group '%s' returning animation name '%s'",
              _name.c_str(),
              lastEntry->GetName().c_str());
  
    return lastEntry->GetName();
  }

  // we couldn't find an animation. If we were in a non-default mood, try again with the default mood
  if( mood != SimpleMoodType::Default ) {
    LOG_DEBUG("AnimationGroup.GetAnimation.NoMoodMatch",
              "No animations from group '%s' selected matching mood '%s', trying with default mood",
              _name.c_str(),
              SimpleMoodTypeToString(mood));
    
    return GetAnimationName(SimpleMoodType::Default, currentTime_s, animationGroupContainer, headAngleRad, strictCooldown);
  }

  static const std::string empty = "";
  // Since this is the backup emergency case, also ignore head angle and just play something
  if( anyAnimationsMatchingMood && !strictCooldown) {
    // choose the animation closest to being off cooldown
    const AnimationGroupEntry* bestEntry = nullptr;
    float minCooldown = std::numeric_limits<float>::max();

    LOG_INFO("AnimationGroup.GetAnimation.AllOnCooldown",
             "All animations are on cooldown. Selecting the one closest to being finished");

    for (auto entry = _animations.begin(); entry != _animations.end(); entry++)
    {
      if(entry->GetMood() == mood) {
        float timeLeft = animationGroupContainer.TimeUntilCooldownOver(entry->GetName(), currentTime_s);

        if( DEBUG_ANIMATION_GROUP_SELECTION ) {
          LOG_DEBUG("AnimationGroup.GetAnimation.ConsiderIgnoringCooldown",
                    "%s: animation %s has %f left on it's cooldown",
                    _name.c_str(),
                    entry->GetName().c_str(),
                    timeLeft);
        }

        if(timeLeft < minCooldown) {
          minCooldown = timeLeft;
          bestEntry = &(*entry);
        }
      }
    }

    if( bestEntry != nullptr ) {
      LOG_INFO("AnimationGroup.GetAnimation.BackupAnimationFound",
               "All animations in group '%s' were on cooldown / invalid, so selected '%s'",
               _name.c_str(),
               bestEntry->GetName().c_str());

      return bestEntry->GetName();
    }
    else {
      LOG_INFO("AnimationGroup.GetAnimation.NoBackup",
               "All animations in group '%s' were on cooldown / invalid nothing could be returned",
               _name.c_str());
    }
  }

  LOG_ERROR("AnimationGroup.GetAnimation.NoAnimation",
            "Could not find a single animation from group '%s' to run. Returning empty",
            _name.c_str());
  return empty;
}
    
} // namespace Vector
} // namespace Anki
