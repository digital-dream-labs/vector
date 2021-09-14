/**
* File: animationInterpolator.cpp
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

#include "cannedAnimLib/cannedAnims/animationInterpolator.h"

#include "cannedAnimLib/cannedAnims/animation.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationInterpolator::GetInterpolationMessages(const Animation* animation, 
                                                     const int frameNum, 
                                                     AnimationMessageWrapper& outMessage)
{
  if(animation == nullptr){
    return;
  }

  ExtractInterpolatedHeadMessage(animation->GetTrack<HeadAngleKeyFrame>(), frameNum, outMessage.moveHeadMessage);
  ExtractInterpolatedLiftMessage(animation->GetTrack<LiftHeightKeyFrame>(), frameNum, outMessage.moveLiftMessage);
  ExtractInterpolatedBodyMessage(animation->GetTrack<BodyMotionKeyFrame>(), frameNum, outMessage.bodyMotionMessage);
  ExtractInterpolatedBackpackMessage(animation->GetTrack<BackpackLightsKeyFrame>(), frameNum, outMessage.backpackLightsMessage);
}

} // namespace Vector
} // namespace Anki
