/**
 * File: animationGroupContainer.h
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


#ifndef __Cozmo_Basestation_AnimationGroup_AnimationGroupContainer_H__
#define __Cozmo_Basestation_AnimationGroup_AnimationGroupContainer_H__

#include "engine/animations/animationGroup/animationGroup.h"
#include <unordered_map>
#include <vector>

// Forward declaration
namespace Json {
  class Value;
}

namespace Anki {
  
namespace Util {
  class RandomGenerator;
}

namespace Vector {
  
  class AnimationGroupContainer
  {
  public:
    AnimationGroupContainer(Util::RandomGenerator& rng);

    // TODO: Add back check that each animation in the group is valid (VIC-370)
    Result DefineFromJson(const Json::Value& jsonRoot, const std::string& animationGroupName);
    
    Result AddAnimationGroup(const std::string& name);
    
    AnimationGroup* GetAnimationGroup(const std::string& name);
    const AnimationGroup* GetAnimationGroup(const std::string& name) const;
    bool HasGroup(const std::string& name) const;
    
    std::vector<std::string> GetAnimationGroupNames();
    
    void Clear();
    
    bool IsAnimationOnCooldown(const std::string& name, double currentTime_s) const;

    // returns how many seconds are left until the cooldown is over for name. Returns 0 if name isn't found,
    // negative if it isn't on cooldown
    float TimeUntilCooldownOver(const std::string& name, double currentTime_s) const;
    
    void SetAnimationCooldown(const std::string& name, double cooldownExpiration_s);
    
  private:
    
    std::unordered_map<std::string, AnimationGroup> _animationGroups;
    
    std::unordered_map<std::string, double> _animationCooldowns;
    
    Util::RandomGenerator& _rng;
    
  }; // class AnimationGroupContainer
  
} // namespace Vector
} // namespace Anki

#endif // __Cozmo_Basestation_AnimationGroup_AnimationGroupContainer_H__
