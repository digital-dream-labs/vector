/**
 * File: animationGroupEntry.h
 *
 * Authors: Trevor Dasch
 * Created: 2016-01-11
 *
 * Description:
 *    Class for storing an animation selection
 *    Which defines a set of mood score graphs
 *    by which to evaluate the suitability of this animation
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Cozmo_Basestation_AnimationGroup_AnimationGroupEntry_H__
#define __Cozmo_Basestation_AnimationGroup_AnimationGroupEntry_H__

#include "clad/types/simpleMoodTypes.h"
#include "coretech/common/shared/types.h"

// Forward Declaration
namespace Json {
  class Value;
}

namespace Anki {
  namespace Vector {
    
    // Forward Declaration
    class CannedAnimationContainer;
    class MoodManager;
    
    class AnimationGroupEntry
    {
    public:
      
      AnimationGroupEntry();
      
      // For reading animation groups from files.
      // TODO: Restore check that animation actually exists somehow (VIC-370)
      // If cannedAnimations is non-null, the animation's name will be verified to exist in
      // in the container and RESULT_FAIL will be returned if it doesn't.
      Result DefineFromJson(const Json::Value& json);
      
      const std::string& GetName() const { return _name; }
      f32 GetWeight() const { return _weight; }
      SimpleMoodType  GetMood() const { return _mood; }
      
      double GetCooldown() const { return _cooldownTime_s; }
      
      bool GetUseHeadAngle() const { return _useHeadAngle; }
      f32 GetHeadAngleMin() const { return _headAngleMin; }
      f32 GetHeadAngleMax() const { return _headAngleMax; }
      
    private:
      
      // Name of this animation
      std::string _name;
      double _cooldownTime_s;
      f32 _weight;
      SimpleMoodType _mood;
      
      bool _useHeadAngle;
      f32  _headAngleMin;
      f32  _headAngleMax;
      
    }; // class AnimationGroupEntry
  } // namespace Vector
} // namespace Anki

#endif // __Cozmo_Basestation_AnimationGroup_AnimationGroupEntry_H__
