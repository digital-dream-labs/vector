/**
* File: animationInterpolator.h
*
* Authors: Kevin M. Karol
* Created: 5/31/18
*
* Description: Every animation keyframe/track operates differently, many issuing a
* single message that moves the robot over time in "sync" with the rest of the animation
* This class allows requests to move the robot to a specific keyframe # without worrying
* about where exactly keyframes are placed.
* E.G. If the robot lift moves on frame 5 for 3 seconds, requesting frame 10 in the interpolator
* will result in a lift height message which will place the lift at the interpolated position
* it would be at in its motion
*
* Copyright: Anki, Inc. 2018
*
**/


#ifndef ANKI_COZMO_ANIMATION_INTERPOLATOR_H
#define ANKI_COZMO_ANIMATION_INTERPOLATOR_H

#include "cannedAnimLib/cannedAnims/animationMessageWrapper.h"
#include "cannedAnimLib/baseTypes/keyframe.h"

namespace Anki {
namespace Vector {

namespace Animations {
template<class FRAME_TYPE>
class Track;
}

class Animation;

class AnimationInterpolator
{
public:
  static void GetInterpolationMessages(const Animation* animation, 
                                       const int frameNum, 
                                       AnimationMessageWrapper& outMessage);
private:
  // To be implemented as time allows
  static void ExtractInterpolatedHeadMessage(const Animations::Track<HeadAngleKeyFrame>& headTrack,
                                             const int frameNum, 
                                             RobotInterface::EngineToRobot* outMessage) {};
  static void ExtractInterpolatedLiftMessage(const Animations::Track<LiftHeightKeyFrame>& liftHeightTrack,
                                             const int frameNum, 
                                             RobotInterface::EngineToRobot* outMessage) {};
  static void ExtractInterpolatedBodyMessage(const Animations::Track<BodyMotionKeyFrame>& bodyMotionTrack,
                                             const int frameNum, 
                                             RobotInterface::EngineToRobot* outMessage) {};
  static void ExtractInterpolatedBackpackMessage(const Animations::Track<BackpackLightsKeyFrame>& backpackTrack,
                                                 const int frameNum, 
                                                 RobotInterface::EngineToRobot* outMessage) {};


};
    

} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_ANIMATION_INTERPOLATOR_H
