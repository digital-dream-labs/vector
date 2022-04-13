/**
 * File: basicActions.h
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements basic cozmo-specific actions, derived from the IAction interface.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_BASIC_ACTIONS_H
#define ANKI_COZMO_BASIC_ACTIONS_H

#include "coretech/common/engine/math/pose.h"
#include "engine/actions/actionInterface.h"
#include "engine/actions/compoundActions.h"
#include "engine/components/movementComponent.h"
#include "engine/smartFaceId.h"
#include "anki/cozmo/shared/animationTag.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "coretech/vision/engine/faceIdTypes.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/visionMarker.h"
#include "clad/externalInterface/messageActions.h"
#include "clad/types/actionTypes.h"
#include "clad/types/animationTypes.h"
#include "clad/types/visionModes.h"
#include "util/bitFlags/bitFlags.h"
#include "util/helpers/templateHelpers.h"
#include "util/signals/simpleSignal_fwd.h"

#include <vector>

namespace Anki {
  
  // Forward declaration
  namespace Vision {
    struct SalientPoint;
  }
  
namespace Vector {
  
    // Forward declaration
    struct ImageSaverParams;
    class ObservableObject;
    class SayNameProbabilityTable;
  
    // Turn in place by a given angle, wherever the robot is when the action is executed.
    //
    // If isAbsolute==true, then angle_rad specifies the absolute body angle to turn to,
    // and the robot will take the shortest path to the desired angle.
    //
    // If isAbsolute==false, then the robot will turn by the amount specified by angle_rad
    // (which can be any arbitrarily large angular displacement, possibly greater than 180
    // degrees or possibly multiple turns)
    class TurnInPlaceAction : public IAction
    {
    public:
      explicit TurnInPlaceAction(const float angle_rad, // float instead of Radians to allow angles > 180 deg.
                                 const bool isAbsolute);
      virtual ~TurnInPlaceAction();
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
      
      // Modify default parameters (must be called before Init() to have an effect)
      void SetRequestedTurnAngle(const f32 turnAngle_rad);
      void SetMaxSpeed(f32 maxSpeed_radPerSec);
      void SetAccel(f32 accel_radPerSec2);
      void SetTolerance(const Radians& angleTol_rad);
      void SetVariability(const Radians& angleVar_rad)   { _variability = angleVar_rad; }
      void SetValidOffTreadsStates(const std::set<OffTreadsState>& states) { _validTreadStates = states; }

      virtual bool SetMotionProfile(const PathMotionProfile& motionProfile) override;
      virtual f32 GetTimeoutInSeconds() const override { return _timeout_s; }
      
      // Note: PROCEDURAL_EYE_LEADING is a compile-time option to enable/disable eye leading
      void SetMoveEyes(bool enable) { _moveEyes = (enable && PROCEDURAL_EYE_LEADING); }
      
    protected:
      
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      
      float RecalculateTimeout();
      bool IsBodyInPosition(Radians& currentAngle) const;
      Result SendSetBodyAngle();
      bool IsOffTreadsStateValid() const;
      bool IsActionMakingProgress() const;
      
      const f32 _kDefaultSpeed        = MAX_BODY_ROTATION_SPEED_RAD_PER_SEC;
      const f32 _kDefaultAccel        = 10.f;
      const f32 _kDefaultTimeoutFactor = 1.5f;
      const f32 _kMaxRelativeTurnRevs = 25.f; // Maximum number of revolutions allowed for a relative turn.
      const Radians _kHeldInPalmAngleTolerance = DEG_TO_RAD(5.f);
      const std::string _kEyeShiftLayerName = "TurnInPlaceEyeShiftLayer";
      
      bool       _inPosition = false;
      bool       _turnStarted = false;
      float      _requestedAngle_rad = 0.f;
      Radians    _currentAngle;
      Radians    _previousAngle;
      Radians    _currentTargetAngle;
      float      _angularDistExpected_rad           = 0.f;
      float      _angularDistTraversed_rad          = 0.f;
      float      _absAngularDistToRemoveEyeDart_rad = 0.f;
      Radians    _angleTolerance = POINT_TURN_ANGLE_TOL;
      Radians    _variability;
      const bool _isAbsoluteAngle;
      f32        _maxSpeed_radPerSec = _kDefaultSpeed;
      f32        _accel_radPerSec2 = _kDefaultAccel;
      bool       _motionProfileManuallySet = false;
      float      _timeout_s;
      float      _expectedTotalAccelTime_s = 0.f;
      float      _expectedMaxSpeedTime_s = 0.f;
      std::set<OffTreadsState> _validTreadStates = {OffTreadsState::OnTreads, OffTreadsState::InAir};
      
      // To keep track of PoseFrameId changes mid-turn:
      PoseFrameID_t _prevPoseFrameId = 0;
      u32 _relocalizedCnt = 0;
      
      bool    _moveEyes = (true && PROCEDURAL_EYE_LEADING);
      
      bool _isInitialized = false;
      
      MovementComponent::MotorActionID _actionID = 0;
      bool       _motionCommanded = false;
      bool       _motionCommandAcked = false;
      
      Signal::SmartHandle _signalHandle;
      
    }; // class TurnInPlaceAction

    // A simple compound action which is useful for identifying blocks that are close
    // to Cozmo's current frame of view.  Cozmo drives backwards slightly, looks left and right
    // and up and down slightly to identify blocks that may be slightly outside the camera
    // optionally an objectID can be passed in for the action to complete immediately on finding the object
    class SearchForNearbyObjectAction : public IAction
    {
    public:
      using SFNOD = ExternalInterface::SearchForNearbyObjectDefaults;
      
      SearchForNearbyObjectAction(const ObjectID& desiredObjectID = ObjectID(),
                                  f32 backupDistance_mm = Util::numeric_cast<f32>(Util::EnumToUnderlying(SFNOD::BackupDistance_mm)),
                                  f32 backupSpeed_mms = Util::numeric_cast<f32>(Util::EnumToUnderlying(SFNOD::BackupSpeed_mms)),
                                  f32 headAngle_rad = Util::numeric_cast<f32>(DEG_TO_RAD(Util::EnumToUnderlying(SFNOD::HeadAngle_deg))));
      virtual ~SearchForNearbyObjectAction();

      void SetSearchAngle(f32 minSearchAngle_rads, f32 maxSearchAngle_rads);
      void SetSearchWaitTime(f32 minWaitTime_s, f32 maxWaitTime_s);

    protected:
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      virtual void OnRobotSet() override final;

    private:
      CompoundActionSequential _compoundAction;
      ObjectID                 _desiredObjectID;
      bool                     _objectObservedDuringSearch;
      std::vector<Signal::SmartHandle> _eventHandlers;

      void AddToCompoundAction(IActionRunner* action);
      
      f32 _minWaitTime_s = 0.8f;
      f32 _maxWaitTime_s = 1.2f;
      f32 _minSearchAngle_rads = DEG_TO_RAD(15.0f);
      f32 _maxSearchAngle_rads = DEG_TO_RAD(20.0f);
      f32 _backupDistance_mm = 0.0f;
      f32 _backupSpeed_mms = 0.0f;
      f32 _headAngle_rad = 0.0f;
    };

    // A simple action for drving a straight line forward or backward, without
    // using the planner
    class DriveStraightAction : public IAction
    {
    public:
      // Positive distance for forward, negative for backward.
      // Speed should be positive if specified
      DriveStraightAction(f32 dist_mm);
      DriveStraightAction(f32 dist_mm, f32 speed_mmps, bool shouldPlayAnimation = true);
      virtual ~DriveStraightAction();

      void SetShouldPlayAnimation(bool shouldPlay) { _shouldPlayDrivingAnimation = shouldPlay; }

      // By default, this action cannot move while on the charger (platform). This function can be used to
      // override this setting, and must be called before the action has started
      void SetCanMoveOnCharger(bool canMove);
      
      void SetAccel(f32 accel_mmps2);
      void SetDecel(f32 decel_mmps2);

      virtual bool SetMotionProfile(const PathMotionProfile& motionProfile) override;
      
      virtual f32 GetTimeoutInSeconds() const override { return _timeout_s; }
      void SetTimeoutInSeconds(float timeout_s);
      
    protected:
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>&requests) const override;
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      
      f32 _dist_mm = 0.f;
      f32 _speed_mmps  = DEFAULT_PATH_MOTION_PROFILE.speed_mmps;
      f32 _accel_mmps2 = DEFAULT_PATH_MOTION_PROFILE.accel_mmps2;
      f32 _decel_mmps2 = DEFAULT_PATH_MOTION_PROFILE.decel_mmps2;
      bool _motionProfileManuallySet = false;
      
      bool _hasStarted = false;
      
      bool _shouldPlayDrivingAnimation = true;

      bool _canMoveOnCharger = false;
      
      float _timeout_s;
      
    }; // class DriveStraightAction
    
    
    class PanAndTiltAction : public IAction
    {
    public:
      // Rotate the body according to bodyPan angle and tilt the head according
      // to headTilt angle. Angles are considered relative to current robot pose
      // if isAbsolute==false.
      // If an angle is less than AngleTol, then no movement occurs but the
      // eyes will dart to look at the angle.
      PanAndTiltAction(Radians bodyPan, Radians headTilt,
                       bool isPanAbsolute, bool isTiltAbsolute);
      virtual ~PanAndTiltAction();
      
      // Modify default parameters (must be called before Init() to have an effect)
      void SetMaxPanSpeed(f32 maxSpeed_radPerSec);
      void SetPanAccel(f32 accel_radPerSec2);
      void SetPanTolerance(const Radians& angleTol_rad);
      void SetMaxTiltSpeed(f32 maxSpeed_radPerSec);
      void SetTiltAccel(f32 accel_radPerSec2);
      void SetTiltTolerance(const Radians& angleTol_rad);
      void SetMoveEyes(bool enable) { _moveEyes = (enable && PROCEDURAL_EYE_LEADING); }
      void SetValidOffTreadsStates(const std::set<OffTreadsState>& states);
      
      Radians GetBodyPanAngleTolerance() const { return _panAngleTol; }
      Radians GetHeadTiltAngleTolerance() const { return _tiltAngleTol; }

    protected:
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
      void SetBodyPanAngle(Radians angle) { _bodyPanAngle = angle; }
      void SetHeadTiltAngle(Radians angle) { _headTiltAngle = angle; }
      
      virtual void OnRobotSet() override final;
      virtual void OnRobotSetInternalPan() {}

    private:
      CompoundActionParallel _compoundAction;
      
      Radians _bodyPanAngle   = 0.f;
      Radians _headTiltAngle  = 0.f;
      bool    _isPanAbsolute  = false;
      bool    _isTiltAbsolute = false;
      bool    _moveEyes       = true;
      
      const f32 _kDefaultPanAngleTol  = DEG_TO_RAD(5);
      const f32 _kDefaultMaxPanSpeed  = MAX_BODY_ROTATION_SPEED_RAD_PER_SEC;
      const f32 _kDefaultPanAccel     = 10.f;
      const f32 _kDefaultTiltAngleTol = DEG_TO_RAD(5);
      const f32 _kDefaultMaxTiltSpeed = 15.f;
      const f32 _kDefaultTiltAccel    = 20.f;
      
      Radians _panAngleTol            = _kDefaultPanAngleTol;
      f32     _maxPanSpeed_radPerSec  = _kDefaultMaxPanSpeed;
      f32     _panAccel_radPerSec2    = _kDefaultPanAccel;
      Radians _tiltAngleTol           = _kDefaultTiltAngleTol;
      f32     _maxTiltSpeed_radPerSec = _kDefaultMaxTiltSpeed;
      f32     _tiltAccel_radPerSec2   = _kDefaultTiltAccel;
      bool    _panSpeedsManuallySet   = false;
      bool    _tiltSpeedsManuallySet  = false;

      
    }; // class PanAndTiltAction
    
  
  
    class CalibrateMotorAction : public IAction
    {
    public:
      CalibrateMotorAction(bool calibrateHead,
                           bool calibrateLift,
                           const MotorCalibrationReason& reason);

      // Template for all events we subscribe to
      template<typename T>
      void HandleMessage(const T& msg);
      
    protected:
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      bool _calibHead;
      bool _calibLift;
      
      MotorCalibrationReason _calibReason;
      
      bool _headCalibStarted;
      bool _liftCalibStarted;
      
      std::vector<Signal::SmartHandle> _signalHandles;
      
    };
  
      
  
    class MoveHeadToAngleAction : public IAction
    {
    public:
      enum class Preset : u8 {
        GROUND_PLANE_VISIBLE,      // at this head angle, the whole ground plane (or the max amount) will be visible
        IDEAL_BLOCK_VIEW           // ideal angle for looking at blocks
      };
    
      MoveHeadToAngleAction(const Radians& headAngle,
                            const Radians& tolerance = HEAD_ANGLE_TOL,
                            const Radians& variability = 0);

      MoveHeadToAngleAction(const Preset preset,
                            const Radians& tolerance = HEAD_ANGLE_TOL,
                            const Radians& variability = 0);
      
      virtual ~MoveHeadToAngleAction();
      
      // Modify default parameters (must be called before Init() to have an effect)
      // TODO: Use setters for variability and tolerance too
      void SetMaxSpeed(f32 maxSpeed_radPerSec)   { _maxSpeed_radPerSec = maxSpeed_radPerSec; }
      void SetAccel(f32 accel_radPerSec2)        { _accel_radPerSec2 = accel_radPerSec2; }
      void SetDuration(f32 duration_sec)         { _duration_sec = duration_sec; }
      
      // Enable/disable eye movement while turning. If hold is true, the eyes will
      // remain in their final position until the next time something moves the head.
      // Note: PROCEDURAL_EYE_LEADING is a compile-time option to enable/disable eye leading
      void SetMoveEyes(bool enable, bool hold=false) { _moveEyes = (enable && PROCEDURAL_EYE_LEADING); _holdEyes = hold; }
      
    protected:
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
    
      static f32 GetPresetHeadAngle(Preset preset);
      static const char* GetPresetName(Preset preset);

      const std::string _kEyeShiftLayerName = "MoveHeadToAngleEyeShiftLayer";
      
      bool IsHeadInPosition() const;
      
      Radians     _headAngle;
      Radians     _angleTolerance;
      Radians     _variability;
      
      f32         _maxSpeed_radPerSec = 15.f;
      f32         _accel_radPerSec2   = 20.f;
      f32         _duration_sec = 0.f;
      bool        _moveEyes = (true && PROCEDURAL_EYE_LEADING);
      bool        _holdEyes = false;
      Radians     _halfAngle;

      MovementComponent::MotorActionID _actionID = 0;
      bool        _motionCommanded = false;
      bool        _motionCommandAcked = false;
      
      bool        _inPosition;
      bool        _motionStarted = false;
      
      Signal::SmartHandle _signalHandle;
      
    };  // class MoveHeadToAngleAction
    
    
    // Set the lift to specified angle with a given tolerance. Note that setting
    // the tolerance too small will likely lead to an action timeout.
    class MoveLiftToAngleAction : public IAction
    {
    public:
      
      MoveLiftToAngleAction(const f32 angle_rad,
                            const f32 tolerance_rad = DEG_TO_RAD(3.f), 
                            const f32 variability = 0.f);
      
      // how long this action should take (which, in turn, effects lift speed)
      void SetDuration(float duration_sec) { _duration = duration_sec; }
      
      void SetMaxLiftSpeed(float speedRadPerSec) { _maxLiftSpeedRadPerSec = speedRadPerSec; }
      void SetLiftAccel(float accelRadPerSec2) { _liftAccelRacPerSec2 = accelRadPerSec2; }
      
    protected:
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      
      bool IsLiftInPosition() const;
      
      f32         _angle_rad;
      f32         _angleTolerance_rad;
      f32         _variability;
      f32         _angleWithVariation;
      f32         _duration = 0.0f; // 0 means "as fast as it can"
      f32         _maxLiftSpeedRadPerSec = 10.0f;
      f32         _liftAccelRacPerSec2 = 20.0f;

      MovementComponent::MotorActionID _actionID;
      bool        _motionCommanded = false;
      bool        _motionCommandAcked = false;
      
      bool        _inPosition;
      bool        _motionStarted = false;
      
      Signal::SmartHandle _signalHandle;
      
    }; // class MoveLiftToAngleAction


    // Set the lift to specified height with a given tolerance. Note that setting
    // the tolerance too small will likely lead to an action timeout.
    class MoveLiftToHeightAction : public IAction
    {
    public:
      
      // Named presets:
      enum class Preset : u8 {
        LOW_DOCK,
        HIGH_DOCK,
        CARRY,
        OUT_OF_FOV, // Moves to low or carry, depending on which is closer to current height
        JUST_ABOVE_PROX, // High enough to avoid the prox sensor, and improves driving over cluttered spaces
      };
      
      MoveLiftToHeightAction(const f32 height_mm,
                             const f32 tolerance_mm = 5.f, 
                             const f32 variability = 0);
      MoveLiftToHeightAction(const Preset preset, const f32 tolerance_mm = 5.f);
      
      // how long this action should take (which, in turn, effects lift speed)
      void SetDuration(float duration_sec) { _duration = duration_sec; }
      
      void SetMaxLiftSpeed(float speedRadPerSec) { _maxLiftSpeedRadPerSec = speedRadPerSec; }
      void SetLiftAccel(float accelRadPerSec2) { _liftAccelRacPerSec2 = accelRadPerSec2; }
      
    protected:
      
      static f32 GetPresetHeight(Preset preset);
      static const std::string& GetPresetName(Preset preset);
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      
      bool IsLiftInPosition() const;
      
      f32         _height_mm;
      f32         _heightTolerance;
      f32         _variability;
      f32         _heightWithVariation;
      f32         _duration = 0.0f; // 0 means "as fast as it can"
      f32         _maxLiftSpeedRadPerSec = 10.0f;
      f32         _liftAccelRacPerSec2 = 20.0f;

      MovementComponent::MotorActionID _actionID;
      bool        _motionCommanded = false;
      bool        _motionCommandAcked = false;
      
      bool        _inPosition;
      bool        _motionStarted = false;
      
      Signal::SmartHandle _signalHandle;
      
    }; // class MoveLiftToHeightAction
    
    
    // Tilt head and rotate body to face the given pose.
    // Use angles specified at construction to control the body rotation.
    class TurnTowardsPoseAction : public PanAndTiltAction
    {
    public:
      // Note that the rotation information in pose will be ignored
      TurnTowardsPoseAction(const Pose3d& pose,
                            Radians maxTurnAngle = M_PI_F);
      
      void SetMaxTurnAngle(Radians angle) { _maxTurnAngle = angle; }

      // compute the turn angles. Can be useful to check what will happen before this action
      // executes. Arguments are translations with respect to the robot
      static Radians GetAbsoluteHeadAngleToLookAtPose(const Point3f& translationWrtRobot);
      static Radians GetRelativeBodyAngleToLookAtPose(const Point3f& translationWrtRobot);
      
    protected:
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
      TurnTowardsPoseAction(Radians maxTurnAngle);
      
      void SetPose(const Pose3d& pose);
      
      Pose3d    _poseWrtRobot;
      
      const Radians& GetMaxTurnAngle() const { return _maxTurnAngle; }
      
    private:
      Radians   _maxTurnAngle;
      bool      _isPoseSet   = false;
      bool      _nothingToDo = false;
      
      static constexpr f32 kHeadAngleDistBias_rad = DEG_TO_RAD(5.f);
      static constexpr f32 kHeadAngleHeightBias_rad = DEG_TO_RAD(7.5f);
      
    }; // class TurnTowardsPoseAction
  
  
    // Tilt head and rotate body to face the given image coordinate.
    // Note that this makes the simplifying approximation that the robot
    // turns around the camera center, which is not actually true.
    class TurnTowardsImagePointAction : public PanAndTiltAction
    {
    public:

      TurnTowardsImagePointAction(const Point2f& imgPoint, const RobotTimeStamp_t imgTimeStamp);
      
      // Constructor for turning towards a SalientPoint, whose (x,y) location is in normalized
      // coordinates (and which has its own timestamp)
      TurnTowardsImagePointAction(const Vision::SalientPoint& salientPoint);
      
    protected:
      virtual ActionResult Init() override;
      
    private:
      
      Point2f     _imgPoint;
      RobotTimeStamp_t _timestamp;
      bool        _isPointNormalized;
      
    }; // class TurnTowardsImagePointAction
    
    
    // Wait for some number of images to be processed by the robot.
    // Optionally specify to only start counting images after a given timestamp.
    class WaitForImagesAction : public IAction
    {
    public:
      
      // NumFrames is the number of times this action will wait for a mode
      //  to be marked as processed, before completing.
      // VisionMode indicates the vision mode(s) that this action wants to wait for
      //
      // If the specified visionMode takes more than one camera frame to complete, 
      // or is not scheduled to run on every frame, then several images may go 
      // through the vision system before this mode is marked as "processed".
      WaitForImagesAction(u32 numFrames, VisionMode visionMode = VisionMode::Count, RobotTimeStamp_t afterTimeStamp = 0);

      struct UseDefaultNumImages_t {};
      static constexpr UseDefaultNumImages_t UseDefaultNumImages = UseDefaultNumImages_t{};
      
      // use a default number of images to give the robot a good chance to see something with the given vision modes
      WaitForImagesAction(UseDefaultNumImages_t, VisionMode visionMode);
      
      // Set save params, assuming VisionMode::SaveImages is active
      // If Mode is SingleShot, will save one image at the start of this action.
      // If Mode is Stream, will save all images (irrespective of visionMode above) while the action
      //   is running. In this case, the mode will be set back to off when the action ends, so anything
      //   that was previously saving will be disabled.
      // This is primarily useful for debugging.
      void SetSaveParams(const ImageSaverParams& params);
      
      virtual ~WaitForImagesAction();
      
      virtual f32 GetTimeoutInSeconds() const override { return std::numeric_limits<f32>::max(); }
      
    protected:
      
      // This action will automatically subscribe to whatever VisionMode it is asked to wait on
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;

      virtual ActionResult Init() override;
      
      virtual ActionResult CheckIfDone() override;
      
    private:
      u32 _numFramesToWaitFor;
      RobotTimeStamp_t _afterTimeStamp;
      
      Signal::SmartHandle             _imageProcSignalHandle;
      VisionMode                      _visionMode = VisionMode::Count;
      EVisionUpdateFrequency          _updateFrequency;
      u32                             _numModeFramesSeen = 0;
      
      std::unique_ptr<ImageSaverParams> _saveParams;
      
    }; // WaitForImagesAction()
  
    // Tilt head and rotate body to face the specified (marker on an) object.
    // Use angles specified at construction to control the body rotation.
    class TurnTowardsObjectAction : public TurnTowardsPoseAction
    {
    public:
      // If facing the object requires less than turnAngleTol turn, then no
      // turn is performed. If a turn greater than maxTurnAngle is required,
      // the action does nothing but succeeds. For angles in between, the robot
      // will first turn to face the object, then tilt its head. To disallow turning,
      // set maxTurnAngle to zero.
      
      TurnTowardsObjectAction(ObjectID objectID,
                              Radians maxTurnAngle = M_PI_F,
                              bool visuallyVerifyWhenDone = false,
                              bool headTrackWhenDone = false);
      
      TurnTowardsObjectAction(ObjectID objectID,
                              Vision::Marker::Code whichCode,
                              Radians maxTurnAngle,
                              bool visuallyVerifyWhenDone = false,
                              bool headTrackWhenDone = false);
      
      virtual ~TurnTowardsObjectAction();

      // usually, an object ID should be passed in in the constructor, but this function can be called to
      // specify an object pointer which may live outside of blockworld
      void UseCustomObject(ObservableObject* objectPtr);
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
      
      void ShouldDoRefinedTurn(const bool tf) { _doRefinedTurn = tf; }
      void SetRefinedTurnAngleTol(const f32 tol) { _refinedTurnAngleTol_rad = tol; }
      
    protected:
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;

      bool                       _facePoseCompoundActionDone = false;
      
      std::unique_ptr<IActionRunner> _visuallyVerifyAction = nullptr;
      bool                       _visuallyVerifyWhenDone = false;
      bool                       _refinedTurnTowardsDone = false;
      
      ObjectID                   _objectID;
      ObservableObject*          _objectPtr = nullptr;
      Vision::Marker::Code       _whichCode;
      bool                       _headTrackWhenDone;
      bool                       _doRefinedTurn = true;
      f32                        _refinedTurnAngleTol_rad = DEG_TO_RAD(5.f);
      
    }; // TurnTowardsObjectAction
    
    
    // Turn towards the last known face pose. Note that this action "succeeds" without doing
    // anything if there is no face (unless requireFace is set to true)
    // If a face is seen after we stop turning, "fine tune" the turn a bit and
    // say the face's name if we recognize it (and sayName=true).
    class TurnTowardsFaceAction : public TurnTowardsPoseAction
    {
    public:
      TurnTowardsFaceAction(const SmartFaceID& faceID, Radians maxTurnAngle = M_PI_F, bool sayName = false);
      
      // Use SayNameProbabilityTable to decide if the name, if any, should be said
      TurnTowardsFaceAction(const SmartFaceID& faceID, Radians maxTurnAngle,
                            std::shared_ptr<SayNameProbabilityTable>& sayNameProbTable);
      
      virtual ~TurnTowardsFaceAction();
      
      // Set the maximum number of frames we are will to wait to see a face after
      // the initial blind turn to the last face pose.
      void SetMaxFramesToWait(u32 N) { _maxFramesToWait = N; }

      // Sets the animation trigger to use to say the name. Only valid if sayName was true. Cannot be used
      // with SetAnyFaceAnimationTrigger
      void SetSayNameAnimationTrigger(AnimationTrigger trigger);
      
      // Sets the backup animation to play if the name is not known, but there is a confirmed face. Only valid
      // if sayName is true (this is because we are trying to use an animation to say the name, but if we
      // don't have a name, we want to use this animation instead). Cannot be used with SetAnyFaceAnimationTrigger.
      void SetNoNameAnimationTrigger(AnimationTrigger trigger);
      
      // Play an animation after turning to the face
      // (Mutually exclusive with SetSayNameAnimationTrigger/SetNoNameAnimationTrigger)
      void SetAnyFaceAnimationTrigger(AnimationTrigger trigger);

      // instead of manually specifying a trigger, this function allows a lambda to be called when the face is
      // turned to. It is called right before the animation should be played, only if the face is named. Input
      // is the face we are reacting to, and the return value should be the animation to play. If
      // AnimationTrigger::Count is returned, no animation will play. Cannot be used with SetAnyFaceTriggerCallback
      using AnimTriggerForFaceCallback = std::function<AnimationTrigger(const Robot& robot, const SmartFaceID& faceID)>;
      void SetSayNameTriggerCallback(AnimTriggerForFaceCallback&& callback);      

      // same as above, but for the case when the face has no associated name. Cannot be used with SetAnyFaceTriggerCallback
      void SetNoNameTriggerCallback(AnimTriggerForFaceCallback&& callback);
      
      // same as above, but runs for any face.
      // (Mutually exclusive with SetSayNameTriggerCallback and SetNoNameTriggerCallback)
      void SetAnyFaceTriggerCallback(AnimTriggerForFaceCallback&& callback);
      
      // For SayName/NoName/AnyFace animations, these tracks will be locked
      void SetAnimTracksToLock(u8 tracksToLock) { _animTracksToLock = tracksToLock; }

      // Sets whether or not we require a face. Default is false (it will play animations and return success
      // even if no face is found). If set to true and no face is found, the action will fail with
      // NO_FACE and no animations will be played
      void SetRequireFaceConfirmation(bool isRequired) { _requireFaceConfirmation = isRequired; }
      
      // After turning toward the supplied faceID, this sets whether the action locks onto the closest
      // face in that direction (true) or if it should only lock onto the supplied faceID (false). Defaults to false.
      void SetLockOnClosestFaceAfterTurn(bool shouldLock) { _lockOnClosestFace = shouldLock; }
      
      // Template for all events we subscribe to
      template<typename T>
      void HandleMessage(const T& msg);
      
    protected:
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
      virtual void OnRobotSetInternalPan() override final;

    private:
      enum class State : u8 {
        Turning,
        WaitingForFace,
        FineTuning,
        WaitingForRecognition,
        PlayingAnimation, // playing an recognition animation, possibly including TTS for the name
      };
      
      SmartFaceID       _faceID;
      std::unique_ptr<IActionRunner> _action     = nullptr;
      f32               _closestDistSq           = std::numeric_limits<f32>::max();
      u32               _maxFramesToWait         = 10;
      SmartFaceID       _obsFaceID;
      State             _state                   = State::Turning;
      bool              _sayName                 = false;
      bool              _tracksLocked            = false;
      bool              _requireFaceConfirmation = false;
      bool              _lockOnClosestFace       = false;
      u8                _animTracksToLock        = (u8) AnimTrackFlag::NO_TRACKS;

      AnimTriggerForFaceCallback _sayNameTriggerCallback;
      AnimTriggerForFaceCallback _noNameTriggerCallback;
      AnimTriggerForFaceCallback _anyFaceTriggerCallback;
      
      std::vector<Signal::SmartHandle> _signalHandles;

      std::shared_ptr<SayNameProbabilityTable> _sayNameProbTable;
      f32 _startedWaitingForRecognition = 0.f;
      
      bool MightSayName() const;
      bool ShouldSayName(const std::string& name);
      
      void CreateFineTuneAction();
      void SetAction(IActionRunner* action, bool suppressTrackLocking = true);
      bool CreateNameAnimationAction(const Vision::TrackedFace* face);
      
    }; // TurnTowardsFaceAction

  
    class TurnTowardsLastFacePoseAction : public TurnTowardsFaceAction
    {
    public:
      TurnTowardsLastFacePoseAction(Radians maxTurnAngle = M_PI_F, bool sayName = false)
      : TurnTowardsFaceAction(SmartFaceID(), maxTurnAngle, sayName)
      {
        // must see face for action to succeed
        SetRequireFaceConfirmation(true); 
      }
    };
  
    // Turn towards the last face before or after another action
    class TurnTowardsFaceWrapperAction : public CompoundActionSequential
    {
    public:

      // Create a wrapper around the given action which looks towards a face before and/or after (default
      // before) the action. This takes ownership of action, the pointer should not be used after this call
      TurnTowardsFaceWrapperAction(IActionRunner* action,
                                   bool turnBeforeAction = true,
                                   bool turnAfterAction = false,
                                   Radians maxTurnAngle = M_PI_F,
                                   bool sayName = false);
    };
    
    
    // Waits for a specified amount of time in seconds, from the time the action
    // is begun. Returns RUNNING while waiting and SUCCESS when the time has
    // elapsed.
    class WaitAction : public IAction
    {
    public:
      WaitAction(f32 waitTimeInSeconds);

      virtual f32 GetTimeoutInSeconds() const override;

    protected:
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
      f32         _waitTimeInSeconds;
      f32         _doneTimeInSeconds;
      
    };

  
    // Dummy action that just never finishes, can be useful for testing or holding the queue
    class HangAction : public IAction
    {
    public:
      HangAction() : IAction("Hang",
                             RobotActionType::HANG,
                             (u8)AnimTrackFlag::NO_TRACKS) {}

      virtual f32 GetTimeoutInSeconds() const override { return std::numeric_limits<f32>::max(); }
      
    protected:
      
      virtual ActionResult Init() override { return ActionResult::SUCCESS; }
      virtual ActionResult CheckIfDone() override { return ActionResult::RUNNING; }

    };

    class WaitForLambdaAction : public IAction
    {
    public:
      WaitForLambdaAction(std::function<bool(Robot&)> lambda,
                          f32 timeout_sec = std::numeric_limits<f32>::max())
        : IAction("WaitForLambda",
                  RobotActionType::WAIT_FOR_LAMBDA,
                  (u8)AnimTrackFlag::NO_TRACKS)
        , _lambda(lambda)
        , _timeout_sec(timeout_sec)
        {
        }
      
      virtual ~WaitForLambdaAction() { }

      virtual f32 GetTimeoutInSeconds() const override { return _timeout_sec; }
      
    protected:
      
      virtual ActionResult Init() override { return ActionResult::SUCCESS; }
      virtual ActionResult CheckIfDone() override {
        if(_lambda(GetRobot()) ){
          return ActionResult::SUCCESS;
        }
        else {
          return ActionResult::RUNNING;
        }
      }

    private:
      
      std::function<bool(Robot&)> _lambda;
      f32 _timeout_sec;
    };
    
    
    // CliffAlignToWhite
    //
    // Uses cliff sensors to align both front sensors with the
    // white border line of the habitat.
    // Requires that one front cliff sensor is already on a white line.
    class CliffAlignToWhiteAction : public IAction
    {
    public:
      CliffAlignToWhiteAction();
      
      virtual ~CliffAlignToWhiteAction();
      
    protected:
      
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      enum class State : u8 {
        Waiting,
        Success,
        FailedTimeout,
        FailedNoTurning,
        FailedOverturning,
        FailedNoWhite,
        FailedStopped,
      };

      State _state = State::Waiting;

      // Handler for the cliff align action completing
      Signal::SmartHandle        _signalHandle;
      
      // Whether or not to restore stopOnWhite setting when
      // action completes since it must be disabled for this
      // action to work.
      bool _resumeStopOnWhite = false;

    }; // class CliffAlignToWhiteAction


} // namespace Vector
} // namespace Anki

#endif /* ANKI_COZMO_BASIC_ACTIONS_H */
