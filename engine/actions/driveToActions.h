/**
 * File: driveToActions.h
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements drive-to cozmo-specific actions, derived from the IAction interface.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_DRIVE_TO_ACTIONS_H
#define ANKI_COZMO_DRIVE_TO_ACTIONS_H

#include "coretech/common/shared/types.h"
#include "engine/actionableObject.h"
#include "engine/actions/actionInterface.h"
#include "engine/actions/compoundActions.h"
#include "coretech/planning/shared/goalDefs.h"
#include "clad/types/actionTypes.h"
#include "clad/types/animationTrigger.h"
#include "clad/types/dockingSignals.h"
#include "util/helpers/templateHelpers.h"
#include <memory>

namespace Anki {
  class Pose3d;

  namespace Vector {

    // forward declarations
    class BlockWorld;
    class IDockAction;

    class DriveToPoseAction : public IAction
    {
    public:
      DriveToPoseAction(); // Note that SetGoal(s) must be called before Update()!
      
      DriveToPoseAction(const Pose3d& pose);
      
      DriveToPoseAction(const std::vector<Pose3d>& poses);
      virtual ~DriveToPoseAction();
            
      // Set possible goal options
      Result SetGoals(const std::vector<Pose3d>& poses);
      
      // Set goal thresholds
      void SetGoalThresholds(const Point3f& distThreshold,
                             const Radians& angleThreshold);
      
      // Call this to indicate that the goal options were generated from an object's pose (predock poses). The object's
      // pose should be given as the argument.
      void SetObjectPoseGoalsGeneratedFrom(const Pose3d& objectPoseGoalsGeneratedFrom);
      
      // If true and if multiple goals were provided, only the originally-selected goal will be used
      void SetMustContinueToOriginalGoal(bool mustUse) { _mustUseOriginalGoal = mustUse; }

      // If shouldPlay, the robot will play planning animations while it computes a plan or replans,
      // for any planner that doesn't return a path immediately.
      // If !shouldPlay, the robot will plan and start driving in one fell swoop, without any logic for planning animations.
      // Default is true.
      void SetUsePlanningAnims(bool shouldPlay) { _precompute = shouldPlay; }

    protected:
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const override;
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual f32 GetTimeoutInSeconds() const override;
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
    private:
      
      ActionResult HandleComputingPath();
      ActionResult HandleFollowingPath();
      
      bool     _isGoalSet = false;
      bool     _precompute = true;
      
      std::vector<Pose3d> _goalPoses;
      std::shared_ptr<Planning::GoalID> _selectedGoalIndex;
            
      Point3f  _goalDistanceThreshold = DEFAULT_POSE_EQUAL_DIST_THRESOLD_MM;
      Radians  _goalAngleThreshold = DEFAULT_POSE_EQUAL_ANGLE_THRESHOLD_RAD;
      
      float _maxPlanningTime = DEFAULT_MAX_PLANNER_COMPUTATION_TIME_S;
      
      float _timeToAbortPlanning = -1.f;
            
      // The pose of the object that the _goalPoses were generated from
      Pose3d _objectPoseGoalsGeneratedFrom;
      bool _useObjectPose = false;
      
      bool _mustUseOriginalGoal = false;
      
    }; // class DriveToPoseAction

    
    // Uses the robot's planner to select the best pre-action pose for the
    // specified action type. Drives there using a DriveToPoseAction. Then
    // moves the robot's head to the angle indicated by the pre-action pose
    // (which may be different from the angle used for path following).
    class DriveToObjectAction : public IAction
    {
    public:
      DriveToObjectAction(const ObjectID& objectID,
                          const PreActionPose::ActionType& actionType,
                          const f32 predockOffsetDistX_mm = 0,
                          const bool useApproachAngle = false,
                          const f32 approachAngle_rad = 0);
      
      DriveToObjectAction(const ObjectID& objectID,
                          const f32 distance_mm);
      virtual ~DriveToObjectAction();
      
      // TODO: Add version where marker code is specified instead of action?
      //DriveToObjectAction(Robot& robot, const ObjectID& objectID, Vision::Marker::Code code);
      
      // If set, instead of driving to the nearest preActionPose, only the preActionPose
      // that is most closely aligned with the approach angle is considered.
      void SetApproachAngle(const f32 angle_rad);
      const bool GetUseApproachAngle() const;
      // returns a bool indicating the success or failure of setting the pose
      const bool GetClosestPreDockPose(ActionableObject* object, Pose3d& closestPose) const;
      
      // Whether or not to verify the final pose, once the path is complete,
      // according to the latest known preAction pose for the specified object.
      void DoPositionCheckOnPathCompletion(bool doCheck) { _doPositionCheckOnPathCompletion = doCheck; }

      // Set the angle tolerance to use for the pre action pose checks done by this action. Defaults to using
      // the default value specified in cozmo config
      void SetPreActionPoseAngleTolerance(f32 angle_rad) { _preActionPoseAngleTolerance_rad = angle_rad; }
            
      using GetPossiblePosesFunc = std::function<ActionResult(ActionableObject* object,
                                                              std::vector<Pose3d>& possiblePoses,
                                                              bool& alreadyInPosition)>;
      
      void SetGetPossiblePosesFunc(GetPossiblePosesFunc func)
      {
        if(IsRunning()){
          PRINT_NAMED_ERROR("DriveToActions.SetGetPossiblePosesFunc.TriedToSetWhileRunning",
                            "PossiblePosesFunc is not allowed to change while the driveToAction is running. \
                             ActionName: %s ActionTag:%i", GetName().c_str(), GetTag());
          return;
        }
        
        _getPossiblePosesFunc = func;
      }
      
      // Default GetPossiblePoses function is public in case others want to just
      // use it as the baseline and modify it's results slightly
      ActionResult GetPossiblePoses(ActionableObject* object,
                                    std::vector<Pose3d>& possiblePoses,
                                    bool& alreadyInPosition);
      
      void SetVisuallyVerifyWhenDone(const bool b) { _visuallyVerifyWhenDone = b; }
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
      
    protected:
      
      virtual bool ShouldFailOnTransitionOffTreads() const override { return true; }
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;
      
      ActionResult InitHelper(ActionableObject* object);
      
      virtual void OnRobotSet() override final;
      virtual void OnRobotSetInternalDriveToObj() {};
      
      // Not private b/c DriveToPlaceCarriedObject uses
      ObjectID                   _objectID;
      PreActionPose::ActionType  _actionType;
      f32                        _distance_mm;
      f32                        _predockOffsetDistX_mm;
      CompoundActionSequential   _compoundAction;
      
      bool                       _useApproachAngle;
      Radians                    _approachAngle_rad;

      bool                       _doPositionCheckOnPathCompletion = true;
                  
    private:

      f32 _preActionPoseAngleTolerance_rad;
      
      GetPossiblePosesFunc _getPossiblePosesFunc;
      
      bool _shouldSetCubeLights = false;
      bool _lightsSet = false;
      
      bool _visuallyVerifyWhenDone = true;
    }; // DriveToObjectAction

  
    class DriveToPlaceCarriedObjectAction : public DriveToObjectAction
    {
    public:
      // destinationObjectPadding_mm: padding around the object size at destination used if checkDestinationFree is true
      DriveToPlaceCarriedObjectAction(const Pose3d& placementPose,
                                      const bool placeOnGround,
                                      const bool useExactRotation = false,
                                      const bool checkDestinationFree = false,
                                      const float destinationObjectPadding_mm = 0.0f);
      
    protected:
    
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override; // Simplified version from DriveToObjectAction
      
      // checks if the placement destination is free (alternatively we could provide an std::function callback)
      bool IsPlacementGoalFree() const;

      virtual void OnRobotSetInternalDriveToObj() override final;
      
      Pose3d _placementPose;
      
      bool   _useExactRotation;
      bool   _checkDestinationFree; // if true the action will often check that the destination is still free to place the object
      float  _destinationObjectPadding_mm; // padding around the object size at destination if _checkDestinationFree is true
      
    }; // DriveToPlaceCarriedObjectAction()

    
    // Interface for all classes below which first drive to an object and then
    // do something with it.
    // If maxTurnTowardsFaceAngle > 0, robot will turn a maximum of that angle towards
    // last face after driving to the object (and say name if that is specified).
    class IDriveToInteractWithObject : public CompoundActionSequential
    {
    protected:
      // Not directly instantiable
      IDriveToInteractWithObject(const ObjectID& objectID,
                                 const PreActionPose::ActionType& actionType,
                                 const f32 predockOffsetDistX_mm,
                                 const bool useApproachAngle,
                                 const f32 approachAngle_rad,
                                 const Radians& maxTurnTowardsFaceAngle_rad,
                                 const bool sayName);
      
      IDriveToInteractWithObject(const ObjectID& objectID,
                                 const f32 distance);

    public:
      virtual ~IDriveToInteractWithObject();
      
      // Forces both of the turnTowards subActions to force complete (basically not run)
      void DontTurnTowardsFace();
      
      void SetMaxTurnTowardsFaceAngle(const Radians angle);
      void SetTiltTolerance(const Radians tol);

      // Set the angle tolerance to use for the pre action pose checks done by this action. Defaults to using
      // the default value specified in cozmo config
      void SetPreActionPoseAngleTolerance(f32 angle_rad);
      
      DriveToObjectAction* GetDriveToObjectAction() {
        // For debug builds do a dynamic cast for the validity checks
        DEV_ASSERT(dynamic_cast<DriveToObjectAction*>(_driveToObjectAction.lock().get()) != nullptr,
                   "DriveToObjectAction.GetDriveToObjectAction.DynamicCastFailed");
        
        return static_cast<DriveToObjectAction*>(_driveToObjectAction.lock().get());
      }
      
      // Subclasses that are a drive-to action followed by a dock action should be calling
      // this function instead of the base classes AddAction() in order to set the approriate
      // preDock pose offset for the dock action
      std::weak_ptr<IActionRunner> AddDockAction(IDockAction* dockAction, bool ignoreFailure = false);

      // Sets the animation trigger to use to say the name. Only valid if sayName was true
      void SetSayNameAnimationTrigger(AnimationTrigger trigger);
      
      // Sets the backup animation to play if the name is not known, but there is a confirmed face. Only valid
      // if sayName is true (this is because we are trying to use an animation to say the name, but if we
      // don't have a name, we want to use this animation instead)
      void SetNoNameAnimationTrigger(AnimationTrigger trigger);

      // Pass in a callback which will get called when the robot switches from driving to it's predock pose to
      // the actual docking action
      using PreDockCallback = std::function<void(Robot&)>;
      void SetPreDockCallback(PreDockCallback callback) { _preDockCallback = callback; }
      
      const bool GetUseApproachAngle() const;
      
    protected:

      virtual Result UpdateDerived() override;
      
      // If set, instead of driving to the nearest preActionPose, only the preActionPose
      // that is most closely aligned with the approach angle is considered.
      void SetApproachAngle(const f32 angle_rad);

      virtual void OnRobotSetInternalCompound() override final;
      
    private:
      // Keep weak_ptrs to each of the actions inside this compound action so they can be easily
      // modified
      // Unfortunately the weak_ptrs need to be cast to the appropriate types to use them. Static casts
      // are used for this. These are safe in this case as they are only ever casting to the
      // original type of the action
      std::weak_ptr<IActionRunner> _driveToObjectAction;
      std::weak_ptr<IActionRunner> _turnTowardsLastFacePoseAction;
      std::weak_ptr<IActionRunner> _turnTowardsObjectAction;
      std::weak_ptr<IActionRunner> _dockAction;
      ObjectID _objectID;
      bool     _shouldSetCubeLights = false;
      bool     _lightsSet = false;
      f32      _preDockPoseDistOffsetX_mm = 0;
      PreDockCallback _preDockCallback;
    }; // class IDriveToInteractWithObject
        
    
    // Compound action for driving to an object, visually verifying it can still be seen,
    // and then driving to it until it is at the specified distance (i.e. distanceFromMarker_mm)
    // from the marker.
    // @param distanceFromMarker_mm - The distance from the marker along it's normal axis that the robot should stop at.
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class DriveToAlignWithObjectAction : public IDriveToInteractWithObject
    {
    public:
      DriveToAlignWithObjectAction(const ObjectID& objectID,
                                   const f32 distanceFromMarker_mm,
                                   const bool useApproachAngle = false,
                                   const f32 approachAngle_rad = 0,
                                   const AlignmentType alignmentType = AlignmentType::CUSTOM,
                                   Radians maxTurnTowardsFaceAngle_rad = 0.f,
                                   const bool sayName = false);
      
      virtual ~DriveToAlignWithObjectAction() { }
    };
    
    
    // Common compound action for driving to an object, visually verifying we
    // can still see it, and then picking it up.
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class DriveToPickupObjectAction : public IDriveToInteractWithObject
    {
    public:
      DriveToPickupObjectAction(const ObjectID& objectID,
                                const bool useApproachAngle = false,
                                const f32 approachAngle_rad = 0,
                                Radians maxTurnTowardsFaceAngle_rad = 0.f,
                                const bool sayName = false,
                                AnimationTrigger animBeforeDock = AnimationTrigger::Count);
      
      virtual ~DriveToPickupObjectAction() { }
      
      void SetDockingMethod(DockingMethod dockingMethod);
      
      void SetPostDockLiftMovingAudioEvent(AudioMetaData::GameEvent::GenericEvent event);
      
    private:
      std::weak_ptr<IActionRunner> _pickupAction;
    };
    
    
    // Common compound action for driving to an object, visually verifying we
    // can still see it, and then placing an object on it.
    // @param objectID         - object to place carried object on
    class DriveToPlaceOnObjectAction : public IDriveToInteractWithObject
    {
    public:
      
      // Places carried object on top of objectID
      DriveToPlaceOnObjectAction(const ObjectID& objectID,
                                 const bool useApproachAngle = false,
                                 const f32 approachAngle_rad = 0,
                                 Radians maxTurnTowardsFaceAngle_rad = 0.f,
                                 const bool sayName = false);
      
      virtual ~DriveToPlaceOnObjectAction() { }
    };
    
    
    // Common compound action for driving to an object, visually verifying we
    // can still see it, and then placing an object relative to it.
    // @param placementOffsetX_mm - The desired distance between the center of the docking marker
    //                              and the center of the object that is being placed, along the
    //                              direction of the docking marker's normal.
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class DriveToPlaceRelObjectAction : public IDriveToInteractWithObject
    {
    public:
      // Place carried object on ground at specified placementOffset from objectID,
      // chooses preAction pose closest to approachAngle_rad if useApproachAngle == true.
      DriveToPlaceRelObjectAction(const ObjectID& objectID,
                                  const bool placingOnGround = true,
                                  const f32 placementOffsetX_mm = 0,
                                  const f32 placementOffsetY_mm = 0,
                                  const bool useApproachAngle = false,
                                  const f32 approachAngle_rad = 0,
                                  Radians maxTurnTowardsFaceAngle_rad = 0.f,
                                  const bool sayName = false,
                                  const bool relativeCurrentMarker = true);
      
      virtual ~DriveToPlaceRelObjectAction() { }
      
    };
    
    
    // Common compound action for driving to an object, visually verifying we
    // can still see it, and then rolling it.
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class RollObjectAction;
    class DriveToRollObjectAction : public IDriveToInteractWithObject
    {
    public:
      DriveToRollObjectAction(const ObjectID& objectID,
                              const bool useApproachAngle = false,
                              const f32 approachAngle_rad = 0,
                              Radians maxTurnTowardsFaceAngle_rad = 0.f,
                              const bool sayName = true);

      virtual ~DriveToRollObjectAction() { }
      
      // Sets the approach angle so that, if possible, the roll action will roll the block to land upright. If
      // the block is upside down or already upright, and roll action will be allowed
      void RollToUpright(const BlockWorld& blockWorld, const Pose3d& robotPose);

      // Calculate the approach angle the robot should use to drive to the pre-dock
      // pose that will result in the block being rolled upright.  Returns true
      // if the angle parameter has been set, false if the angle couldn't be
      // calculated or an approach angle to roll upright doesn't exist
      static bool GetRollToUprightApproachAngle(const BlockWorld& blockWorld,
                                                const Pose3d& robotPose,
                                                const ObjectID& objID,
                                                f32& approachAngle_rad);

      Result EnableDeepRoll(bool enable);
      
    private:
      ObjectID _objectID;
      std::weak_ptr<IActionRunner> _rollAction;
    };
    
    
    // Common compound action for driving to an object and popping a wheelie off of it
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class DriveToPopAWheelieAction : public IDriveToInteractWithObject
    {
    public:
      DriveToPopAWheelieAction(const ObjectID& objectID,
                               const bool useApproachAngle = false,
                               const f32 approachAngle_rad = 0,
                               Radians maxTurnTowardsFaceAngle_rad = 0.f,
                               const bool sayName = true);
      
      virtual ~DriveToPopAWheelieAction() { }
    };
    
    // Common compound action for driving to an object (stack) and face planting off of it by knocking the stack over
    // @param useApproachAngle  - If true, then only the preAction pose that results in a robot
    //                            approach angle closest to approachAngle_rad is considered.
    // @param approachAngle_rad - The desired docking approach angle of the robot in world coordinates.
    class DriveToFacePlantAction : public IDriveToInteractWithObject
    {
    public:
      DriveToFacePlantAction(const ObjectID& objectID,
                             const bool useApproachAngle = false,
                             const f32 approachAngle_rad = 0,
                             Radians maxTurnTowardsFaceAngle_rad = 0.f,
                             const bool sayName = false);
      
      virtual ~DriveToFacePlantAction() { }
    };
    
    class DriveToRealignWithObjectAction : public CompoundActionSequential
    {
    public:
      DriveToRealignWithObjectAction(ObjectID objectID, float dist_mm);

    protected:
      virtual void OnRobotSetInternalCompound() override final;

    private:
      ObjectID _objectID;
      float _dist_mm;
    };
  }
}

#endif /* ANKI_COZMO_DRIVE_TO_ACTIONS_H */
