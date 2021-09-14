/**
* File: track
*
* Author: damjan stulic
* Created: 9/16/15
*
* Description: 
*
* Copyright: Anki, inc. 2015
*
*/


#include "cannedAnimLib/baseTypes/track.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
namespace Animations {

  static void EnableStopMessageHelper(const BodyMotionKeyFrame& addedKeyFrame,
                                      BodyMotionKeyFrame* prevKeyFrame)
  {
    if(nullptr != prevKeyFrame)
    {
      // If the keyframe we just added starts within a single sample length
      // of the end of the previous keyframe, then there's no need to send
      // a stop message for the previous keyframe because the body motion
      // command for this new keyframe will handle it. This avoids delays
      // introduced by "extra" stop messages being inserted unnecessarily.
      if(prevKeyFrame->GetTimestampActionComplete_ms() > addedKeyFrame.GetTriggerTime_ms() - ANIM_TIME_STEP_MS) {
        //PRINT_NAMED_DEBUG("Animations.EnableStopMessageHelper",
        //                  "Disabling stop message for body motion keyframe at t=%d "
        //                  "with duration=%d because of next keyframe at t=%d",
        //                  prevKeyFrame->GetTriggerTime_ms(), prevKeyFrame->GetDurationTime(),
        //                  addedKeyFrame.GetTriggerTime_ms());
        prevKeyFrame->EnableStopMessage(false);
      }
    }
  }
  
  
  // Specializations for body motion to decide if we need to send a stop message
  // between the last frame and the one being added
  template<>
  Result Track<BodyMotionKeyFrame>::AddKeyFrameToBack(const BodyMotionKeyFrame& keyFrame)
  {
    BodyMotionKeyFrame* prevKeyFrame = nullptr;
    Result result = AddKeyFrameToBackHelper(keyFrame, prevKeyFrame);
    
    if(result == RESULT_OK)
    {
      EnableStopMessageHelper(keyFrame, prevKeyFrame);
    }
    
    return result;
  }
  
  // TODO: Define AddKeyFrameToBack for TurnToRecordedHeading?
  //       Shouldn't be necessary since this command doesn't make the robot move indefinitely,
  //       plus it should really only be used at the end of an animation where animationController
  //       will automatically command a stop anyway since it treats the keyframe the same
  //       as a BodyMotionKeyFrame.

  
  template<>
  Result Track<BodyMotionKeyFrame>::AddKeyFrameByTime(const BodyMotionKeyFrame& keyFrame)
  {
    BodyMotionKeyFrame* prevKeyFrame = nullptr;
    Result result = AddKeyFrameByTimeHelper(keyFrame, prevKeyFrame);
    
    if(result == RESULT_OK)
    {
      EnableStopMessageHelper(keyFrame, prevKeyFrame);
    }
    
    return result;
  }

  // Specialization for backpack lights:
  // We only care about the last keyframe's duration so that the animation doesn't
  // report that it's finished before the backpack track has finished. For all other
  // keyframes, there's no need to check duration because the lights will naturally
  // stay in whatever state the keyframe leaves them until the next one changes them.
  // So for any "previous" keyframe, we know there's another one coming and we can
  // just set its duration to 0. This avoids introducing delay when a keyframe
  // finishes at the same time the next one should trigger.
  template<>
  Result Track<BackpackLightsKeyFrame>::AddKeyFrameToBack(const BackpackLightsKeyFrame &keyFrame)
  {
    BackpackLightsKeyFrame* prevKeyFrame = nullptr;
    Result result = AddKeyFrameToBackHelper(keyFrame, prevKeyFrame);
    
    return result;
  }
  
  template<>
  Result Track<BackpackLightsKeyFrame>::AddKeyFrameByTime(const BackpackLightsKeyFrame &keyFrame)
  {
    BackpackLightsKeyFrame* prevKeyFrame = nullptr;
    Result result = AddKeyFrameByTimeHelper(keyFrame, prevKeyFrame);
    
    return result;
  }
  
  template<>
  Result Track<ProceduralFaceKeyFrame>::AddKeyFrameToBack(const ProceduralFaceKeyFrame& keyFrame)
  {
    ProceduralFaceKeyFrame* dummy = nullptr;
    const auto res = AddKeyFrameToBackHelper(keyFrame, dummy);
    auto& allKeyframes = GetAllKeyframes();
    if(allKeyframes.size() >= 2){
      auto backIter = allKeyframes.rbegin();
      auto oldBackIter = allKeyframes.rbegin();
      oldBackIter++;
      oldBackIter->SetKeyframeActiveDuration_ms(backIter->GetTriggerTime_ms() - oldBackIter->GetTriggerTime_ms());
    }
    return res;
  }
  
  template<>
  void Track<ProceduralFaceKeyFrame>::AdvanceTrack(const TimeStamp_t toTime_ms)
  {
    if(ANKI_DEV_CHEATS){
      auto allKeyframes = GetCopyOfKeyframes();
      auto safetyCheckIter = allKeyframes.begin();
      while(safetyCheckIter != allKeyframes.end()) {
        auto nextIter = safetyCheckIter;
        nextIter++;
        // Ensure tracks don't overlap
        if(nextIter != allKeyframes.end()){
          ANKI_VERIFY(safetyCheckIter->GetTimestampActionComplete_ms() == nextIter->GetTriggerTime_ms(),
                      "ITrackLayerManager.ValidateTrack.ProceduralKeyframeTimeMismatch",
                      "Previous keyframe ends at %u, but next frame does not trigger until %u, interpolation will break",
                      safetyCheckIter->GetTimestampActionComplete_ms(), nextIter->GetTriggerTime_ms());
        }
        safetyCheckIter++;
      }
    }
    AdvanceTrackHelper(toTime_ms);
  }
  
} // end namespace Animations
} // end namespace Vector
} // end namespace Anki



