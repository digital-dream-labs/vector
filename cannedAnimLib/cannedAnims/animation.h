/**
 * File: animation.h
 *
 * Authors: Andrew Stein
 * Created: 2015-06-25
 *
 * Description:
 *    Class for storing a single animation, which is made of
 *    tracks of keyframes. Also manages streaming those keyframes
 *    to a robot.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/


#ifndef ANKI_COZMO_CANNED_ANIMATION_H
#define ANKI_COZMO_CANNED_ANIMATION_H

#include "coretech/common/engine/jsonTools.h"
#include "anki/cozmo/shared/animationTag.h"
#include "cannedAnimLib/baseTypes/keyframe.h"
#include "cannedAnimLib/baseTypes/track.h"
#include "cannedAnimLib/cannedAnims/spriteBoxCompositor.h"
#include <list>
#include <queue>

namespace CozmoAnim {
  struct AnimClip;
}

namespace Anki {

// Fwd Declaration
namespace Vision{
  class SpriteCache;
  class SpriteSequenceContainer;
}

namespace Vector {

class Animation
{
public:

  Animation(const std::string& name = "");

  bool operator==(const Animation &other) const;

  // For reading canned animations from files
  Result DefineFromFlatBuf(const std::string& name, const CozmoAnim::AnimClip* animClip, 
                           Vision::SpriteSequenceContainer* seqContainer);
  Result DefineFromJson(const std::string& name, const Json::Value& json,
                        Vision::SpriteSequenceContainer* seqContainer);

  // For defining animations at runtime (e.g. live animation)
  template<class KeyFrameType>
  Result AddKeyFrameToBack(const KeyFrameType& kf);
  
  template<class KeyFrameType>
  Result AddKeyFrameByTime(const KeyFrameType& kf);

  Result AddSpriteBoxKeyFrame(Vision::SpriteBoxKeyFrame&& keyFrame)
  {
    return _spriteBoxCompositor.AddKeyFrame(std::move(keyFrame));
  }

  void SetFaceImageOverride(const Vision::SpriteHandle& spriteHandle,
                            const TimeStamp_t relativeStreamTime_ms,
                            const TimeStamp_t duration_ms)
  {
    return _spriteBoxCompositor.SetFaceImageOverride(spriteHandle, relativeStreamTime_ms, duration_ms);
  }

  void SetOverrideAllSpritesToEyeHue()
  {
    _spriteBoxCompositor.SetOverrideAllSpritesToEyeHue();
  }

  void ClearOverrides()
  {
    _spriteBoxCompositor.ClearOverrides();
  }

  void AddSpriteBoxRemap(const Vision::SpriteBoxName& spriteBoxName,
                         const Vision::SpritePathMap::AssetID remappedAssetID)
  {
    return _spriteBoxCompositor.AddSpriteBoxRemap(spriteBoxName, remappedAssetID);
  }

  // Get a track by KeyFrameType
  template<class KeyFrameType>
  Animations::Track<KeyFrameType>& GetTrack();
  
  // Const version of GetTrack
  template<class KeyFrameType>
  const Animations::Track<KeyFrameType>& GetTrack() const
  {
    // Normally I hate using const_cast, but GetTrack is a template function where the actual implementation
    // is to have a different specialization for each type, in order to return the correct Track member.
    return const_cast<Animation*>(this)->GetTrack<KeyFrameType>();
  }

  const Animations::SpriteBoxCompositor& GetSpriteBoxCompositor() const { return _spriteBoxCompositor; }
  
  // Calls all tracks' Init() methods
  Result Init(Vision::SpriteCache* cache);

  bool IsInitialized() const { return _isInitialized; }
  
  // An animation is Empty if *all* its tracks are empty
  bool IsEmpty() const;

  // True if any track has has frames left to play
  bool HasFramesLeft() const;
  
  void Clear();

  void ClearUpToCurrent();

  // If the animation has any sprites (the sprite sequence track) cache them for the duration
  // of the animation so that they're not being loaded from disk during playback
  void CacheAnimationSprites(Vision::SpriteCache* cache);


  void SetName(const std::string& name) { _name = name; }
  const std::string& GetName() const { return _name; }
  
  // Append Animation with another animation starting on the next key frame
  void AppendAnimation(const Animation& appendAnim);
  
  // Get last key frame time_ms
  uint32_t GetLastKeyFrameTime_ms() const;
  
  // Get last key frame + duration of keyframe
  uint32_t GetLastKeyFrameEndTime_ms() const;

  // Advance all tracks to the keyframe that should play in ms
  // NOTE: This function only moves tracks forwards
  void AdvanceTracks(const TimeStamp_t toTime_ms);

  // Takes a CompositeImage for rendering to the face. If this animation has
  // any SpriteBoxKeyFrames, they will be added to the provided CompositeImage
  // and we'll return true. Else returns false.
  bool PopulateCompositeImage(Vision::SpriteCache& spriteCache,
                              Vision::SpriteSequenceContainer& spriteSeqContainer,
                              TimeStamp_t timeSinceAnimStart_ms,
                              Vision::CompositeImage& outCompImg){
    return _spriteBoxCompositor.PopulateCompositeImage(spriteCache,
                                                       spriteSeqContainer,
                                                       timeSinceAnimStart_ms,
                                                       outCompImg);
  }

private:

  // Name of this animation
  std::string _name;
  bool _isInitialized;

  // All the animation tracks, storing different kinds of KeyFrames
  Animations::Track<HeadAngleKeyFrame>      _headTrack;
  Animations::Track<LiftHeightKeyFrame>     _liftTrack;
  Animations::Track<ProceduralFaceKeyFrame> _proceduralFaceTrack;
  Animations::Track<EventKeyFrame>          _eventTrack;
  Animations::Track<BackpackLightsKeyFrame> _backpackLightsTrack;
  Animations::Track<BodyMotionKeyFrame>     _bodyPosTrack;
  Animations::Track<RecordHeadingKeyFrame>  _recordHeadingTrack;
  Animations::Track<TurnToRecordedHeadingKeyFrame> _turnToRecordedHeadingTrack;
  Animations::Track<RobotAudioKeyFrame>     _robotAudioTrack;
  
  Animations::SpriteBoxCompositor _spriteBoxCompositor;

  // Compare if the track's last key frame time is gerater then the lastFrameTime_ms argument
  // Return the greater time
  template<class KeyFrameType>
  TimeStamp_t CompareLastFrameTime(const TimeStamp_t lastFrameTime_ms) const;
  
  // Compare if the track's last key frame + duration time is greater then the lastFrameTime_ms argument
  // Return the greater time
  template<class KeyFrameType>
  TimeStamp_t CompareLastFrameEndTime(const TimeStamp_t lastFrameTime_ms) const;
  
}; // class Animation


template<class KeyFrameType>
Result Animation::AddKeyFrameToBack(const KeyFrameType& kf)
{
  auto* oldKF = GetTrack<KeyFrameType>().GetLastKeyFrame();
  Result addResult = GetTrack<KeyFrameType>().AddKeyFrameToBack(kf);
  if(RESULT_OK != addResult) {
    PRINT_NAMED_ERROR("Animation.AddKeyFrameToBack.Failed", "AnimationName:%s",
                      GetName().c_str());
    return addResult;
  }

  auto* newKF = GetTrack<KeyFrameType>().GetLastKeyFrame();

  if((oldKF != nullptr) &&
     (newKF->GetTriggerTime_ms() == 0)){
    newKF->SetTriggerTime_ms(oldKF->GetTimestampActionComplete_ms());
  }

  return addResult;
}


template<class KeyFrameType>
Result Animation::AddKeyFrameByTime(const KeyFrameType& kf)
{
  Result addResult = GetTrack<KeyFrameType>().AddKeyFrameByTime(kf);
  if(RESULT_OK != addResult) {
    PRINT_NAMED_ERROR("Animation.AddKeyFrameByTime.Failed", "AnimationName:%s",
                      GetName().c_str());
  }

  return addResult;
}
    

} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_CANNED_ANIMATION_H
