/**
 * File: trackActionInterface.h
 *
 * Author: Andrew Stein
 * Date:   12/11/2015
 *
 * Description: Vision system component for benchmarking operations. Add new methods as needed to
 *              compare performance of various ways of implementing operations on images (e.g. using OpenCV,
 *              directly looping over the pixels, using a lookup table, etc. Each method is tied to a "Mode"
 *              which can be enabled/disabled via console vars as well.
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef __Anki_Cozmo_Basestation_TrackActionInterface_H__
#define __Anki_Cozmo_Basestation_TrackActionInterface_H__

#include "anki/cozmo/shared/animationTag.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "engine/actions/actionInterface.h"

#include "clad/types/actionTypes.h"
#include "clad/externalInterface/messageEngineToGame.h"

namespace Anki {
namespace Vector {
  
enum class AnimationTrigger : int32_t;

// Forward Declarations:
class Robot;

template <typename Type>
class AnkiEvent;

class ITrackAction : public IAction
{
public:
  
  enum class Mode {
    HeadAndBody,
    HeadOnly,
    BodyOnly
  };
  
  // Choose whether to track with head, body, or both (default)
  void SetMode(Mode newMode);
  Mode GetMode() const { return _mode; }
  
  // Tracking is meant to be ongoing, so "never" timeout
  virtual f32 GetTimeoutInSeconds() const override { return std::numeric_limits<f32>::max(); }
  
  //
  // The setters below should only be called before the action is started!
  //
  
  // Stop this action after maintaining the target within tolerances for the given amount of time.
  // If interruptDrivingAnim=true (and driving animations are enabled), then if stop criteria are met,
  // SUCCESS is returned immediately and the end driving animation is not played. This presumes the caller
  // wants to more quickly play their own final animation.
  // Set time to 0 to disable (default).
  void SetStopCriteria(const Radians& panTol, const Radians& tiltTol, f32 minDist_mm, f32 maxDist_mm, f32 time_sec,
                       bool interruptDrivingAnim = false);
  
  // Set how long the tracker will run without seeing whatever it is trying to track.
  // Set to 0 to disable timeout (default).
  // If there is no StopCriteria, a timeout is a "successful" completion of this action.
  // If StopCriteria are provided, a timeout will result in this action completing with a timeout failure.
  void SetUpdateTimeout(float timeout_sec);

  // Tells this action to keep running until another action (being run separately) stops. As soon as this
  // other action completes, this action will complete as well
  void StopTrackingWhenOtherActionCompleted( u32 otherActionTag );
    
  // Instead of setting pan _speed_, set the desired duration of the pan to turn towards
  // the target and compute the speed internally. So small turns will move more slowly and
  // large turns will be quicker. If this duration is set (non-zero), it will take precedence
  // over any pan speeds specified above.
  void SetPanDuration(f32 panDuration_sec);
  void SetTiltDuration(f32 tiltDuration_sec);
  
  // Set desired time to reach target if forward motion is supported by the derived class
  // The shorter this is, the faster the robot will drive to try to reach the target distance
  // returned by UpdateTracking().
  void SetDesiredTimeToReachTarget(f32 time_sec);
  
  void EnableDrivingAnimation(bool enable);
  
  // TODO: Remove this in favor of using driving animations
  // Sound settings: which animation (should be sound only), how frequent, and
  // minimum angle required to play sound. Use AnimationTrigger::Count for sound to
  // disable (default).
  void SetSound(const AnimationTrigger animName);
  void SetSoundSpacing(f32 spacingMin_sec, f32 spacingMax_sec);
  void SetMinPanAngleForSound(const Radians& angle);
  void SetMinTiltAngleForSound(const Radians& angle);
  
  // Angles returned by GetAngles() method must be greater than these tolerances
  // to actually trigger movement.
  void SetPanTolerance(const Radians& panThreshold);
  void SetTiltTolerance(const Radians& tiltThreshold);

  // If enabled, angles returned by GetAngles() below tolerance (which would generally
  // be ignored) are clamped to the tolerances so that the robot _always_ moves
  // by at least the tolerance amounts (in the correct direction). This creates
  // extra (technically, unncessary) movement of the robot, but keeps him looking
  // more alive while tracking. Disabled by default.
  void SetClampSmallAnglesToTolerances(bool tf);

  // If we are clamping small angles, this setting can be set to minimize how "often" we do the clamp. If max
  // is > 0, a random number will be rolled between min and max and the "clamping" will next happen after that
  // interval has elapsed
  void SetClampSmallAnglesPeriod(float min_sec, float max_sec);
  
  void SetMaxHeadAngle(const Radians& maxHeadAngle_rads);

  // Enable/disable moving of eyes while tracking. Default is false.
  void SetMoveEyes(bool moveEyes);
  
  // Enable/disable action in certain tread-states. Default is only OnTreads.
  void SetValidOffTreadsStates(const std::set<OffTreadsState>& states) { _validTreadStates = states; }

protected:

  ITrackAction(const std::string name, const RobotActionType type);
  virtual ~ITrackAction();
  
  // Anything which inherits from track action needs to have appropriate VisionModes enabled.
  virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override = 0;

  // Note that derived classes should override InitInternal, which is called by Init
  virtual ActionResult Init() override final;
  virtual ActionResult InitInternal() = 0;
  
  // Derived classes must implement InitInternal(), but cannot implement Init() or CheckIfDone().
  virtual ActionResult CheckIfDone() override final;
  
  enum UpdateResult : u8 {
    NoNewInfo,
    NewInfo,
    PredictedInfo,
    ShouldStop
  };

  // These are the methods that children classes should call to use the stop/continue
  // criteria of their choice. However, as it is implemented right now you can have
  // some unintended consquences. As an example if SetStopCriteria is called
  // followed by UseContinueCriteria, stop criteria will not be used and instead
  // continue criteria will be. VIC-5821 further describes these potential
  // issues.
  void UseContinueCriteria(bool useContinueCriteria) { _useStopCriteria = !useContinueCriteria; }
  
  // Implementation-specific method for computing the absolute angles needed
  // to turn and face whatever is being tracked and the distance to target.
  // Note: distance will be ignored if using head only tracking
  virtual UpdateResult UpdateTracking(Radians& absPanAngle, Radians& absTiltAngle, f32& distance_mm) = 0;
  
  virtual bool InterruptInternal() override final;

  // This method is intended to be overridden by child
  // classes. With the goal of having the child class
  // incorporate appliation specific logic to override
  // the stop criteria in this base class. For an example
  // see TrackFaceAction.
  virtual bool AreContinueCriteriaMet(const f32 currentTime_sec) {return false;};

  // Stop criteria is only valid if duration_sec is non-zero or
  // earliestStoppingTime_sec is greater than zero.
  struct {
    Radians panTol                      = -1.f;
    Radians tiltTol                     = -1.f;
    f32     minDist_mm                  = -1.f;
    f32     maxDist_mm                  = -1.f;
    f32     duration_sec                = 0.f;
    f32     withinTolSince_sec          = 0.f;
    bool    interruptDrivingAnim        = false;
  } _stopCriteria;
  
private:

  // sets internal values to track clamping small angles. Returns true if we should clamp, false otherwise
  bool UpdateSmallAngleClamping();
  
  bool AreStopCriteriaMet(const f32 relPanAngle_rad, const f32 relTiltAngle_rad,
                          const f32 dist_mm, const f32 currentTime_sec);
  bool IsWithinTolerances(const f32 relPanAngle_rad, const f32 relTiltAngle_rad,
                          const f32 dist_mm, const f32 currentTime_sec) const;
  bool IsTimeToStop(const f32 relPanAngle_rad, const f32 relTiltAngle_rad,
                    const f32 dist_mm, const f32 currentTime_sec);
  
  Mode     _mode = Mode::HeadAndBody;
  float    _updateTimeout_sec = 0.0f;
  float    _lastUpdateTime = 0.0f;
  Radians  _panTolerance  = POINT_TURN_ANGLE_TOL;
  Radians  _tiltTolerance = HEAD_ANGLE_TOL;
  Radians  _maxHeadAngle  = MAX_HEAD_ANGLE;
  u32      _stopOnOtherActionTag = ActionConstants::INVALID_TAG;
  
  bool     _moveEyes      = (false && PROCEDURAL_EYE_LEADING);
  
  bool     _shouldPlayDrivingAnimation = false;

  const std::string _kEyeShiftLayerName = "ITrackActionEyeShiftLayer";

  // This member variable determines whether the tracker should use stop
  // criteria or continue criteria, the children classes are expected
  // to override via public setters as neccesary. See TrackFaceAction
  // for example.
  bool _useStopCriteria = true;
  
  // When driving animations are used, we have to wait until the End animation is complete
  // before returning whatever actual final result for the action we wanted. In the mean time
  // we have to return RUNNING. So we use this member variable to store the return value
  // we want to use once driving animations finish (if _shouldPlayDrivingAnimation=true).
  ActionResult _finalActionResult = ActionResult::NOT_STARTED;
  
  // TODO: Remove this old sound stuff?
  AnimationTrigger _turningSoundAnimTrigger;
  f32      _soundSpacingMin_sec = 0.5f;
  f32      _soundSpacingMax_sec = 1.0f;
  f32      _nextSoundTime = 0.f;
  Radians  _minPanAngleForSound = DEG_TO_RAD(10);
  Radians  _minTiltAngleForSound = DEG_TO_RAD(10);
  
  f32      _tiltDuration_sec = 0.15f;
  f32      _panDuration_sec  = 0.25f;
  f32      _timeToReachTarget_sec  = 0.5f;
  
  u32      _soundAnimTag = (u32)ActionConstants::INVALID_TAG;
  bool     _clampSmallAngles = false;
  f32      _clampSmallAnglesMinPeriod_s = -1.0f;
  f32      _clampSmallAnglesMaxPeriod_s = -1.0f;
  f32      _nextTimeToClampSmallAngles_s = -1.0f;
  
  // Tread states in which this action is allowed to run, can be modified.
  std::set<OffTreadsState> _validTreadStates = {OffTreadsState::OnTreads};
  
  const std::string _kKeepFaceAliveITrackActionName = "ITrackAction";

  bool HaveStopCriteria() const;

  // Helper for storing the return result if we are using driving animations and just
  // returning result immediately if not
  ActionResult CheckIfDoneReturnHelper(ActionResult result, bool stopCriteriaMet);
  
}; // class ITrackAction
    
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_TrackActionInterface_H__ */
