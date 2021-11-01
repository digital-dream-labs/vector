/**
 * File: animationGroup.h
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


#ifndef __Cozmo_Basestation_AnimationGroup_AnimationGroup_H__
#define __Cozmo_Basestation_AnimationGroup_AnimationGroup_H__

#include "engine/animations/animationGroup/animationGroupEntry.h"
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
    
    //Forward declaration
    class AnimationGroupContainer;
    
    class AnimationGroup
    {
    public:
      
      explicit AnimationGroup(Util::RandomGenerator& rng, const std::string& name = "");
      
      // For reading animation groups from files
      Result DefineFromJson(const std::string& name, const Json::Value& json);

      // Retrieve an animation based on the mood manager
      const std::string& GetAnimationName(const MoodManager& moodManager,
                                          AnimationGroupContainer& animationGroupContainer,
                                          float headAngleRad=0.f,
                                          bool strictCooldown=false) const;

      // Just retrieve first animation from the group
      const std::string& GetFirstAnimationName() const;
      
      // An animation group is empty if it has no animations
      bool IsEmpty() const;
      
      size_t GetNumAnimations() const { return _animations.size(); }
      
      const std::string& GetName() const { return _name; }
      
    private:
      // Retrieve an animation based on a simple mood
      const std::string& GetAnimationName(SimpleMoodType mood,
                                          float currentTime_s,
                                          AnimationGroupContainer& animationGroupContainer,
                                          float headAngleRad=0.f,
                                          bool strictCooldown=false) const;
      
      Util::RandomGenerator& _rng;
      
      // Name of this animation
      std::string _name;
      
      std::vector<AnimationGroupEntry> _animations;
      
    }; // class AnimationGroup
  } // namespace Vector
} // namespace Anki

#endif // __Cozmo_Basestation_AnimationGroup_AnimationGroup_H__
