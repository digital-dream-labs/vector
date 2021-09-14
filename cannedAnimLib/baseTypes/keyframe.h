/**
 * File: keyframe.h
 *
 * Authors: Andrew Stein
 * Created: 2015-06-25
 *
 * Description: 
 *   Defines the various KeyFrames used to store an animation on the
 *   the robot, all of which inherit from a common interface, 
 *   IKeyFrame.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/


#ifndef ANKI_COZMO_CANNED_KEYFRAME_H
#define ANKI_COZMO_CANNED_KEYFRAME_H

#include "coretech/common/engine/colorRGBA.h"
#include "coretech/vision/engine/image.h"
#include "cannedAnimLib/baseTypes/audioKeyFrameTypes.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/types/ledTypes.h"
#include "util/random/randomGenerator.h"
#include "json/json-forwards.h"

#ifndef CAN_STREAM
#define CAN_STREAM true
#endif

namespace CozmoAnim {
  struct HeadAngle;
  struct LiftHeight;
  struct RobotAudio;
  struct FaceAnimation;
  struct ProceduralFace;
  struct Event;
  struct BackpackLights;
  struct BodyMotion;
  struct RecordHeading;
  struct TurnToRecordedHeading;
}

namespace Anki {
  
  namespace Vision {
    class CompositeImage;
    class ImageRGB565;
    class SpriteCache;
    class SpriteSequence;
    class SpriteSequenceContainer;
  }
  
namespace Vector {
  // IKeyFrame defines an abstract interface for all KeyFrames below.
  class IKeyFrame
  {
  public:
    
    IKeyFrame();
    //IKeyFrame(const Json::Value& root);
    virtual ~IKeyFrame();
    
    // Returns true if the animation's time has reached frame's "trigger" time
    bool IsTimeToPlay(const TimeStamp_t timeSinceAnimStart_ms) const;
    
    // Returns the time to trigger whatever change is implied by the KeyFrame
    TimeStamp_t GetTriggerTime_ms() const { return _triggerTime_ms; }

    // Returns the timestamp at which the keyframe has finished performing some action on the robot
    // NOTE: This is NOT the last timestamp when the keyframe sends a message, but the last timestamp when
    // whatever action the keyframe started actually finishes
    // E.G. The lift keyframe sends a single message, but if the lift motion lasts 2000 ms, the keyframe
    // should return 2000_ms from this function so that if the track is a single keyframe long the animation doesn't
    // immediately complete and potentially stop the motion early
    TimeStamp_t GetTimestampActionComplete_ms() const { 
      if(ANKI_DEV_CHEATS){
        ANKI_VERIFY(GetKeyframeDuration_ms() != 0, 
                    "IKeyframe.GetTimestampActionComplete_ms.DurationZero", 
                    "");
      }
      return _triggerTime_ms + GetKeyframeDuration_ms();
    }
    
    
    // Set the triggert time, relative to the start time of track the animation
    // is playing in
    void SetTriggerTime_ms(TimeStamp_t triggerTime_ms) { _triggerTime_ms = triggerTime_ms; }
    
    // Set all members from Json or FlatBuffers. Calls virtual SetMembersFromJson() method so subclasses can specify
    // how to populate their members. Second argument is used to print nicer debug strings if something goes wrong
    Result DefineFromJson(const Json::Value &json, const std::string& animNameDebug = "");
    
    #if CAN_STREAM
      // Fill some kind of message for streaming and return it. Return nullptr
      // if not available.
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const = 0;
    #endif

    bool IsFirstKeyframeTick(const TimeStamp_t timeSinceAnimStart_ms) const
    {
      return GetTimeSinceTrigger(timeSinceAnimStart_ms) < ANIM_TIME_STEP_MS;
    }

  protected:
    // Populate members from Json
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") = 0;
    
    TimeStamp_t GetTimeSinceTrigger(const TimeStamp_t timeSinceAnimStart_ms) const 
    {
      return (timeSinceAnimStart_ms > GetTriggerTime_ms()) ? (timeSinceAnimStart_ms - GetTriggerTime_ms()) : 0;
    }

    virtual TimeStamp_t GetKeyframeDuration_ms() const = 0;


    //void SetIsValid(bool isValid) { _isValid = isValid; }
    
    Util::RandomGenerator& GetRNG() const;

    // The trigger time is protected instead of private so derived classes can access it.
    TimeStamp_t _triggerTime_ms  = 0;

  private:
    // A random number generator for all keyframes to share (for adding variability)
    static Util::RandomGenerator sRNG;
    
  }; // class IKeyFrame
  
  inline Util::RandomGenerator& IKeyFrame::GetRNG() const {
    return sRNG;
  }
  
  
  // A HeadAngleKeyFrame specifies the time to _start_ moving the head towards
  // a given angle (with optional variation), and how long to take to get there.
  class HeadAngleKeyFrame : public IKeyFrame
  {
  public:
    HeadAngleKeyFrame() {}
    HeadAngleKeyFrame(s8 angle_deg, u8 angle_variability_deg, TimeStamp_t duration_ms);

    bool operator ==(const HeadAngleKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_keyframeActiveDuration_ms == other._keyframeActiveDuration_ms) &&
             (_angle_deg  == other._angle_deg) &&
             (_angleVariability_deg == other._angleVariability_deg);
    }
    
    Result DefineFromFlatBuf(const CozmoAnim::HeadAngle* headAngleKeyframe, const std::string& animNameDebug);

    #if CAN_STREAM
    virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif

    static const std::string& GetClassName() {
      static const std::string ClassName("HeadAngleKeyFrame");
      return ClassName;
    }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::HeadAngle* headAngleKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return _keyframeActiveDuration_ms; }
    
  private:
    TimeStamp_t _keyframeActiveDuration_ms;
    s8          _angle_deg;
    u8          _angleVariability_deg;
    
  }; // class HeadAngleKeyFrame
  
  
  // A LiftHeightKeyFrame specifies the time to _start_ moving the lift towards
  // a given height (with optional variation), and how long to take to get there.
  class LiftHeightKeyFrame : public IKeyFrame
  {
  public:
    LiftHeightKeyFrame() { }
    LiftHeightKeyFrame(u8 height_mm, u8 heightVariability_mm, TimeStamp_t duration_ms);

    bool operator ==(const LiftHeightKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_keyframeActiveDuration_ms == other._keyframeActiveDuration_ms) &&
             (_height_mm  == other._height_mm) &&
             (_heightVariability_mm == other._heightVariability_mm);
    }

    
    Result DefineFromFlatBuf(const CozmoAnim::LiftHeight* liftHeightKeyframe, const std::string& animNameDebug);

    #if CAN_STREAM
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif

    static const std::string& GetClassName() {
      static const std::string ClassName("LiftHeightKeyFrame");
      return ClassName;
    }
    
    #if ANKI_DEV_CHEATS
    void OverrideHeight(u8 newHeight){ _height_mm = newHeight;}
    #endif
    

  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::LiftHeight* liftHeightKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return _keyframeActiveDuration_ms; }
    
  private:
    TimeStamp_t _keyframeActiveDuration_ms;
    u8          _height_mm;
    u8          _heightVariability_mm;
    
  }; // class LiftHeightKeyFrame


  // A RobotAudioKeyFrame references a single "sound" which is made of lots
  // of "samples" to be individually streamed to the robot.
  class RobotAudioKeyFrame : public IKeyFrame
  {
  public:
    
    RobotAudioKeyFrame() {}

    bool operator ==(const RobotAudioKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_audioReferences == other._audioReferences);
    }

    
    Result DefineFromFlatBuf(const CozmoAnim::RobotAudio* audioKeyframe, const std::string& animNameDebug);

    #if CAN_STREAM
      // NOTE: Always returns nullptr for RobotAudioKeyframe!
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override { return nullptr; };
    #endif

    static const std::string& GetClassName() {
      static const std::string ClassName("RobotAudioKeyFrame");
      return ClassName;
    }
    
    using AudioRefList = std::vector<AudioKeyFrameType::AudioRef>;
    
    const AudioRefList& GetAudioReferencesList() const { return _audioReferences; }
    
    Result AddAudioRef(AudioKeyFrameType::AudioRef&& audioRef);
    Result AddAudioRef(AudioKeyFrameType::AudioEventGroupRef&& eventGroupRef);
    Result AddAudioRef(AudioKeyFrameType::AudioParameterRef&& parameterRef);
    Result AddAudioRef(AudioKeyFrameType::AudioStateRef&& stateRef);
    Result AddAudioRef(AudioKeyFrameType::AudioSwitchRef&& switchRef);
    
    // Merge two RobotAudioKeyFrames together
    // Note: triggerTime_ms will not be effected
    // Note: otherFrame will be invalid after merging
    void MergeKeyFrame(RobotAudioKeyFrame&& otherFrame);
    
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    Result SetMembersFromDeprecatedJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "");
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::RobotAudio* audioKeyframe, const std::string& animNameDebug = "");
        
    // Lasts one keyframe to send audio event
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return ANIM_TIME_STEP_MS; }
    
  private:
    std::vector<AudioKeyFrameType::AudioRef> _audioReferences;
    
  }; // class RobotAudioKeyFrame


  class ProceduralFaceKeyFrame : public IKeyFrame
  {
  public:
    ProceduralFaceKeyFrame(TimeStamp_t triggerTime_ms = 0, TimeStamp_t durationTime_ms = 0);
    ProceduralFaceKeyFrame(const ProceduralFace& face, TimeStamp_t triggerTime_ms = 0, TimeStamp_t durationTime_ms = 0);

    bool operator ==(const ProceduralFaceKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_procFace == other._procFace);
    }

    Result DefineFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug);
    
    #if CAN_STREAM
      // Always returns nullptr. Use GetInterpolatedFace() to get the face stored in this
      // keyframe.
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override { return nullptr; }
    #endif
    
    // Returns message for the face interpolated between the stored face in this
    // keyframe and the one in the next keyframe.
    RobotInterface::EngineToRobot* GetInterpolatedStreamMessage(const ProceduralFaceKeyFrame& nextFrame);
    
    // Returns the interpolated face between the current keyframe and the next.
    // If the nextFrame is nullptr, then this frame's procedural face are returned.
    ProceduralFace GetInterpolatedFace(const ProceduralFaceKeyFrame& nextFrame, const TimeStamp_t currentTime_ms);
    
    void SetKeyframeActiveDuration_ms(TimeStamp_t activeDuration_ms) { _keyframeActiveDuration_ms = activeDuration_ms; }
    
    static const std::string& GetClassName() {
      static const std::string ClassName("ProceduralFaceKeyFrame");
      return ClassName;
    }
        
    const ProceduralFace& GetFace() const { return _procFace; }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return _keyframeActiveDuration_ms == 0 ? ANIM_TIME_STEP_MS : _keyframeActiveDuration_ms; }
    
  private:
    TimeStamp_t     _keyframeActiveDuration_ms = 0;
    ProceduralFace  _procFace;
    
  }; // class ProceduralFaceKeyFrame
  
  inline ProceduralFaceKeyFrame::ProceduralFaceKeyFrame(TimeStamp_t triggerTime, TimeStamp_t durationTime_ms)
  {
    SetTriggerTime_ms(triggerTime);
    _keyframeActiveDuration_ms = durationTime_ms;
  }
  
  inline ProceduralFaceKeyFrame::ProceduralFaceKeyFrame(const ProceduralFace& face,
                                                        TimeStamp_t triggerTime,
                                                        TimeStamp_t durationTime_ms)
  : _procFace(face)
  {
    SetTriggerTime_ms(triggerTime);
    _keyframeActiveDuration_ms = durationTime_ms;
  }

  
  // An EventKeyFrame simply returns an AnimEvent message from the robot
  // for higher precision event timing... like in Speed Tap.
  class EventKeyFrame : public IKeyFrame
  {
  public:
    EventKeyFrame() { }

    bool operator ==(const EventKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_event_id == other._event_id);
    }

    Result DefineFromFlatBuf(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug);
    
    #if CAN_STREAM
     virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif

    static const std::string& GetClassName() {
      static const std::string ClassName("EventKeyFrame");
      return ClassName;
    }
    
    
    Anki::Vector::AnimEvent GetAnimEvent() const { return _event_id; }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::Event* eventKeyframe, const std::string& animNameDebug = "");

    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return ANIM_TIME_STEP_MS; }
    
  private:

    Anki::Vector::AnimEvent _event_id;
    
  }; // class EventKeyFrame
  
  
  // A BackpackLightsKeyFrame sets the colors of the robot's five backpack lights
  class BackpackLightsKeyFrame : public IKeyFrame
  {
  public:
    BackpackLightsKeyFrame();

    bool operator ==(const BackpackLightsKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_keyframeActiveDuration_ms == other._keyframeActiveDuration_ms);
    }

    Result DefineFromFlatBuf(CozmoAnim::BackpackLights* backpackKeyframe, const std::string& animNameDebug);
    
    #if CAN_STREAM
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif
    
    static const std::string& GetClassName() {
      static const std::string ClassName("BackpackLightsKeyFrame");
      return ClassName;
    }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(CozmoAnim::BackpackLights* backpackKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return _keyframeActiveDuration_ms; }
    
  private:
    TimeStamp_t _keyframeActiveDuration_ms;
    RobotInterface::SetBackpackLights _streamMsg;
    
  }; // class BackpackLightsKeyFrame
  
  
  // A BodyMotionKeyFrame controls the wheels to drive straight, turn in place, or
  // drive arcs. They specify the speed and duration of the motion.
  class BodyMotionKeyFrame : public IKeyFrame
  {
  public:
    BodyMotionKeyFrame();
    BodyMotionKeyFrame(s16 speed, s16 curvatureRadius_mm, s32 duration_ms);

    bool operator ==(const BodyMotionKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             (_keyframeActiveDuration_ms == other._keyframeActiveDuration_ms);
    }
    
    Result DefineFromFlatBuf(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug);

    void CheckRotationSpeed(const std::string& animNameDebug);
    void CheckStraightSpeed(const std::string& animNameDebug);
    void CheckTurnSpeed(const std::string& animNameDebug);

    Result ProcessRadiusString(const std::string& radiusStr, const std::string& animNameDebug);

    #if CAN_STREAM
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif

    static const std::string& GetClassName() {
      static const std::string ClassName("BodyMotionKeyFrame");
      return ClassName;
    }
    
    void EnableStopMessage(bool enable) { _enableStopMessage = enable; }
    
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::BodyMotion* bodyKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override;
    
  private:
    TimeStamp_t _keyframeActiveDuration_ms;
    bool _enableStopMessage = true;
    
    RobotInterface::DriveWheelsCurvature _streamMsg;
    RobotInterface::DriveWheelsCurvature _stopMsg;
    
  }; // class BodyMotionKeyFrame
  
  
  // A RecordHeadingKeyFrame records an angular heading so that it can be returned
  // to (with an optional offset) using TurnToRecordedHeadingKeyFrame
  class RecordHeadingKeyFrame : public IKeyFrame
  {
  public:
    RecordHeadingKeyFrame();

    bool operator ==(const RecordHeadingKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms());
    }
    
    Result DefineFromFlatBuf(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug);
    
    #if CAN_STREAM
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif
    
    static const std::string& GetClassName() {
      static const std::string ClassName("RecordHeadingKeyFrame");
      return ClassName;
    }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::RecordHeading* recordHeadingKeyframe, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return ANIM_TIME_STEP_MS; }
    
  private:
    
    #if CAN_STREAM
      RobotInterface::RecordHeading _streamMsg;
    #endif
    
  }; // class RecordHeadingKeyFrame
  
  
  // A TurnToRecordedHeadingKeyFrame commands the robot to turn to the heading that was
  // previously recorded by a RecordHeadingKeyFrame
  class TurnToRecordedHeadingKeyFrame : public IKeyFrame
  {
  public:
    TurnToRecordedHeadingKeyFrame();
    TurnToRecordedHeadingKeyFrame(s16 offset_deg,
                                  s16 speed_degPerSec,
                                  s16 accel_degPerSec2,
                                  s16 decel_degPerSec2,
                                  u16 tolerance_deg,
                                  u16 numHalfRevs,
                                  bool useShortestDir,
                                  s32 duration_ms);

    bool operator ==(const TurnToRecordedHeadingKeyFrame& other) const{
      return (GetTriggerTime_ms() == other.GetTriggerTime_ms()) &&
             (GetKeyframeDuration_ms() == other.GetKeyframeDuration_ms()) &&
             _keyframeActiveDuration_ms == other._keyframeActiveDuration_ms;
    }
    
    Result DefineFromFlatBuf(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyframe, const std::string& animNameDebug);
    
    void CheckRotationSpeed(const std::string& animNameDebug);
    
    #if CAN_STREAM
      virtual RobotInterface::EngineToRobot* GetStreamMessage(const TimeStamp_t timeSinceAnimStart_ms) const override;
    #endif
    
    static const std::string& GetClassName() {
      static const std::string ClassName("TurnToRecordedHeadingKeyFrame");
      return ClassName;
    }
    
  protected:
    virtual Result SetMembersFromJson(const Json::Value &jsonRoot, const std::string& animNameDebug = "") override;
    virtual Result SetMembersFromFlatBuf(const CozmoAnim::TurnToRecordedHeading* turnToRecordedHeadingKeyFrame, const std::string& animNameDebug = "");
    
    virtual TimeStamp_t GetKeyframeDuration_ms() const override { return _keyframeActiveDuration_ms; }
    
  private:
    TimeStamp_t _keyframeActiveDuration_ms;
    RobotInterface::TurnToRecordedHeading _streamMsg;
    
  }; // class TurnToRecordedHeadingKeyFrame
  
} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_CANNED_KEYFRAME_H
