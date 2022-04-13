/**
 * File: chargerActions.cpp
 *
 * Author: Matt Michini
 * Date:   8/10/2017
 *
 * Description: Implements charger-related actions, e.g. docking with the charger.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "engine/actions/chargerActions.h"

#include "engine/actions/animActions.h"
#include "engine/actions/dockActions.h"
#include "engine/actions/driveToActions.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/charger.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/robot.h"

#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {
  
#pragma mark ---- MountChargerAction ----
  
MountChargerAction::MountChargerAction(ObjectID chargerID,
                                       const bool useCliffSensorCorrection)
  : IAction("MountCharger",
            RobotActionType::MOUNT_CHARGER,
            (u8)AnimTrackFlag::BODY_TRACK | (u8)AnimTrackFlag::HEAD_TRACK | (u8)AnimTrackFlag::LIFT_TRACK)
  , _chargerID(chargerID)
  , _useCliffSensorCorrection(useCliffSensorCorrection)
{
  
}


MountChargerAction::~MountChargerAction()
{  
  if (_mountAction != nullptr) {
    _mountAction->PrepForCompletion();
  }
  if (_driveForRetryAction != nullptr) {
    _driveForRetryAction->PrepForCompletion();
  }
}

  
ActionResult MountChargerAction::Init()
{
  // Reset the compound actions to ensure they get re-configured:
  _mountAction.reset();
  _driveForRetryAction.reset();
  
  // Verify that we have a charger in the world that matches _chargerID
  const auto* charger = GetRobot().GetBlockWorld().GetLocatedObjectByID(_chargerID);
  if ((charger == nullptr) ||
      (charger->GetType() != ObjectType::Charger_Basic)) {
    PRINT_NAMED_WARNING("MountChargerAction.Init.InvalidCharger",
                        "No charger object with ID %d in block world!",
                        _chargerID.GetValue());
    return ActionResult::BAD_OBJECT;
  }
  
  // Tell robot which charger it will be using
  GetRobot().SetCharger(_chargerID);

  // Set up the turnAndMount compound action
  ActionResult result = ConfigureMountAction();
  
  return result;
}


ActionResult MountChargerAction::CheckIfDone()
{ 
  auto result = ActionResult::RUNNING;
  
  // Tick the turnAndMount action (if needed):
  if (_mountAction != nullptr) {
    result = _mountAction->Update();
    // If the action fails with a retry code and the robot has already
    // turned away from the charger, then position for a retry
    if (IActionRunner::GetActionResultCategory(result) == ActionResultCategory::RETRY) {
      bool isFacingAwayFromCharger = true;
      const auto* charger = GetRobot().GetBlockWorld().GetLocatedObjectByID(_chargerID);
      if (charger != nullptr) {
        const auto& chargerAngle = charger->GetPose().GetRotation().GetAngleAroundZaxis();
        const auto& robotAngle = GetRobot().GetPose().GetRotation().GetAngleAroundZaxis();
        isFacingAwayFromCharger = (chargerAngle - robotAngle).getAbsoluteVal().ToFloat() > M_PI_2_F;
      }

      if (isFacingAwayFromCharger) {
        PRINT_NAMED_WARNING("MountChargerAction.CheckIfDone.PositionForRetry",
                            "Turning and mounting the charger failed (action result = %s). Driving forward to position for a retry.",
                            EnumToString(result));
        // Finished with turnAndMountAction
        _mountAction.reset();
        // We need to add the driveForRetryAction:
        result = ConfigureDriveForRetryAction();
        if (result != ActionResult::SUCCESS) {
          return result;
        }
      }
    }
  }
  
  // Tick the _driveForRetryAction action (if needed):
  if (_driveForRetryAction != nullptr) {
    result = _driveForRetryAction->Update();
    
    // If the action finished successfully, this parent action
    // should return a NOT_ON_CHARGER_RETRY to cause a retry.
    if (result == ActionResult::SUCCESS) {
      return ActionResult::NOT_ON_CHARGER_RETRY;
    }
  }
  
  return result;
}


void MountChargerAction::SetDockingAnimTriggers(const AnimationTrigger& start,
                                                const AnimationTrigger& loop,
                                                const AnimationTrigger& end)
{
  _dockingStartTrigger = start;
  _dockingLoopTrigger  = loop;
  _dockingEndTrigger   = end;
  _dockingAnimTriggersSet = true;
}


ActionResult MountChargerAction::ConfigureMountAction()
{
  DEV_ASSERT(_mountAction == nullptr, "MountChargerAction.ConfigureMountAction.AlreadyConfigured");
  _mountAction.reset(new CompoundActionSequential());
  _mountAction->ShouldSuppressTrackLocking(true);
  _mountAction->SetRobot(&GetRobot());
  
  // Raise lift slightly so it doesn't drag against the ground (if necessary)
  const float backingUpLiftHeight_mm = 45.f;
  if (GetRobot().GetLiftHeight() < backingUpLiftHeight_mm) {
    _mountAction->AddAction(new MoveLiftToHeightAction(backingUpLiftHeight_mm));
  }
  
  // Back up into the charger
  auto* backupAction = new BackupOntoChargerAction(_chargerID, _useCliffSensorCorrection);
  if (_dockingAnimTriggersSet) {
    backupAction->SetDockAnimations(_dockingStartTrigger, _dockingLoopTrigger, _dockingEndTrigger);
  }
  _mountAction->AddAction(backupAction);
  
  return ActionResult::SUCCESS;
}
  
ActionResult MountChargerAction::ConfigureDriveForRetryAction()
{
  DEV_ASSERT(_driveForRetryAction == nullptr, "MountChargerAction.ConfigureDriveForRetryAction.AlreadyConfigured");
  const float distanceToDriveForward_mm = 120.f;
  const float driveForwardSpeed_mmps = 100.f;
  auto* driveAction = new DriveStraightAction(distanceToDriveForward_mm,
                                              driveForwardSpeed_mmps,
                                              false);
  driveAction->SetCanMoveOnCharger(true);
  _driveForRetryAction.reset(driveAction);
  _driveForRetryAction->ShouldSuppressTrackLocking(true);
  _driveForRetryAction->SetRobot(&GetRobot());

  return ActionResult::SUCCESS;
}

#pragma mark ---- TurnToAlignWithChargerAction ----
  
TurnToAlignWithChargerAction::TurnToAlignWithChargerAction(ObjectID chargerID,
                                                           AnimationTrigger leftTurnAnimTrigger,
                                                           AnimationTrigger rightTurnAnimTrigger)
  : IAction("TurnToAlignWithCharger",
            RobotActionType::TURN_TO_ALIGN_WITH_CHARGER,
            (u8)AnimTrackFlag::BODY_TRACK)
  , _chargerID(chargerID)
  , _leftTurnAnimTrigger(leftTurnAnimTrigger)
  , _rightTurnAnimTrigger(rightTurnAnimTrigger)
{
}

void TurnToAlignWithChargerAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::Low });
}

ActionResult TurnToAlignWithChargerAction::Init()
{
  _compoundAction.reset(new CompoundActionParallel());
  _compoundAction->ShouldSuppressTrackLocking(true);
  _compoundAction->SetRobot(&GetRobot());
  
  const auto* charger = GetRobot().GetBlockWorld().GetLocatedObjectByID(_chargerID);
  if ((charger == nullptr) ||
      (charger->GetType() != ObjectType::Charger_Basic)) {
    PRINT_NAMED_WARNING("TurnToAlignWithChargerAction.Init.InvalidCharger",
                        "No charger object with ID %d in block world!",
                        _chargerID.GetValue());
    return ActionResult::BAD_OBJECT;
  }
  // Compute the angle to turn.
  //
  // The charger's origin is at the 'front' edge of the ramp (furthest from the marker).
  // This value is the distance from the origin into the charger of the point that the
  // robot should angle towards. Setting this distance to 0 means the robot will angle
  // itself toward the charger origin.
  const float distanceIntoChargerToAimFor_mm = 50.f;
  Pose3d poseToAngleToward(0.f, Z_AXIS_3D(),
                           {distanceIntoChargerToAimFor_mm, 0.f, 0.f},
                           charger->GetPose());

  // Get the vector from the target pose to the drive center pose, expressed in the world origin frame
  Vec3f targetToRobotVec;
  if (!ComputeVectorBetween(GetRobot().GetDriveCenterPose(),
                            poseToAngleToward,
                            GetRobot().GetWorldOrigin(),
                            targetToRobotVec)) {
    PRINT_NAMED_WARNING("TurnToAlignWithChargerAction.Init.CouldNotComputeVector",
                        "Failed to compute vector from target pose to robot pose");
    return ActionResult::BAD_POSE;
  }
  const float angleToTurnTo = atan2f(targetToRobotVec.y(), targetToRobotVec.x());
  
  auto* turnAction = new TurnInPlaceAction(angleToTurnTo, true);
  turnAction->SetMaxSpeed(DEG_TO_RAD(100.f));
  turnAction->SetAccel(DEG_TO_RAD(300.f));
  
  _compoundAction->AddAction(turnAction);
  
  // Play the left/right turn animation depending on the anticipated direction of the turn
  const auto& robotAngle = GetRobot().GetPose().GetRotation().GetAngleAroundZaxis();
  const bool clockwise = (angleToTurnTo - robotAngle).ToFloat() < 0.f;
  
  const auto animationTrigger = clockwise ? _rightTurnAnimTrigger : _leftTurnAnimTrigger;
  
  if (animationTrigger != AnimationTrigger::Count) {
    _compoundAction->AddAction(new TriggerAnimationAction(animationTrigger));
  }
  
  // Go ahead and do the first Update for the compound action so we don't
  // "waste" the first CheckIfDone call doing so. Proceed so long as this
  // first update doesn't _fail_
  const auto& compoundResult = _compoundAction->Update();
  if((ActionResult::SUCCESS == compoundResult) ||
     (ActionResult::RUNNING == compoundResult)) {
    return ActionResult::SUCCESS;
  } else {
    return compoundResult;
  }
}
  
  
ActionResult TurnToAlignWithChargerAction::CheckIfDone()
{
  DEV_ASSERT(_compoundAction != nullptr, "TurnToAlignWithChargerAction.CheckIfDone.NullCompoundAction");
  return _compoundAction->Update();
}


#pragma mark ---- BackupOntoChargerAction ----

BackupOntoChargerAction::BackupOntoChargerAction(ObjectID chargerID,
                                                 bool useCliffSensorCorrection)
  : IDockAction(chargerID,
                "BackupOntoCharger",
                RobotActionType::BACKUP_ONTO_CHARGER)
  , _useCliffSensorCorrection(useCliffSensorCorrection)
{
  // We don't expect to be near the pre-action pose of the charger when we
  // begin backing up onto it, so don't check for it. We aren't even seeing
  // the marker at this point anyway.
  SetDoNearPredockPoseCheck(false);
  
  // Don't turn toward the object since we're expected to be facing away from it
  SetShouldFirstTurnTowardsObject(false);
}


ActionResult BackupOntoChargerAction::InitInternal()
{
  _initialPitchAngle = GetRobot().GetPitchAngle();
  return ActionResult::SUCCESS;
}

  
ActionResult BackupOntoChargerAction::SelectDockAction(ActionableObject* object)
{
  auto objType = object->GetType();
  if (objType != ObjectType::Charger_Basic) {
    PRINT_NAMED_ERROR("BackupOntoChargerAction.SelectDockAction.NotChargerObject",
                      "Object is not a charger! It's a %s.", EnumToString(objType));
    return ActionResult::BAD_OBJECT;
  }
  
  _dockAction = _useCliffSensorCorrection ?
                  DockAction::DA_BACKUP_ONTO_CHARGER_USE_CLIFF :
                  DockAction::DA_BACKUP_ONTO_CHARGER;
  
  
  // Tell robot which charger it will be using
  GetRobot().SetCharger(_dockObjectID);

  return ActionResult::SUCCESS;
}
  
  
ActionResult BackupOntoChargerAction::Verify()
{
  // Verify that robot is on charger
  if (GetRobot().GetBatteryComponent().IsOnChargerContacts()) {
    LOG_INFO("BackupOntoChargerAction.Verify.MountingChargerComplete",
             "Robot has mounted charger.");
    return ActionResult::SUCCESS;
  }
  
  // We're not on the charger contacts - but why? Let's find out.
  const auto& currPitchAngle = GetRobot().GetPitchAngle();
  const auto& pitchAngleChange = currPitchAngle - _initialPitchAngle;
  
  const bool pitchSuggestsOnCharger = pitchAngleChange.IsNear(-kChargerSlopeAngle_rad, DEG_TO_RAD(2.f));
  const bool pitchSuggestsStillOnGround = pitchAngleChange.IsNear(0.f, DEG_TO_RAD(2.f));
  
  if (pitchSuggestsOnCharger) {
    // The difference in pitch angle suggests that we are
    // indeed on the charger platform, but we have not sensed
    // the contacts. It's likely that the charger is unplugged.
    PRINT_NAMED_WARNING("BackupOntoChargerAction.Verify.ChargerUnplugged",
                        "Pitch angle says we're on the charger platform, but not sensing contacts. Charger may be unplugged."
                        "(starting pitch %.2f deg, current pitch %.2f deg)",
                        _initialPitchAngle.getDegrees(),
                        currPitchAngle.getDegrees());
    return ActionResult::CHARGER_UNPLUGGED_ABORT;
    
  }
  
  if (pitchSuggestsStillOnGround) {
    // We probably completely missed the charger or otherwise ended
    // up flat on the ground again, so something is wrong.
    PRINT_NAMED_WARNING("BackupOntoChargerAction.Verify.StillOnGround",
                        "Pitch angles says we are still on the ground and not on the charger platform. "
                        "(starting pitch %.2f deg, current pitch %.2f deg)",
                        _initialPitchAngle.getDegrees(),
                        currPitchAngle.getDegrees());
    return ActionResult::NOT_ON_CHARGER_ABORT;
  }
  
  // We are neither confidently on the charger nor confidently
  // on the ground, so we should just retry.
  PRINT_NAMED_WARNING("BackupOntoChargerAction.Verify.Failed",
                      "We are not sensing the charger contacts, and pitch angle suggests that"
                      "we are neither on the charger platform nor flat on the ground."
                      "(starting pitch %.2f deg, current pitch %.2f deg)",
                      _initialPitchAngle.getDegrees(),
                      currPitchAngle.getDegrees());
  return ActionResult::NOT_ON_CHARGER_RETRY;
}
  
  
#pragma mark ---- DriveToAndMountChargerAction ----
  
DriveToAndMountChargerAction::DriveToAndMountChargerAction(const ObjectID& objectID,
                                                           const bool useCliffSensorCorrection,
                                                           const bool enableDockingAnims,
                                                           const bool doPositionCheckOnPathCompletion)
: CompoundActionSequential()
{
  // Get DriveToObjectAction
  auto driveToAction = new DriveToObjectAction(objectID,
                                               PreActionPose::ActionType::DOCKING,
                                               0,
                                               false,
                                               0);
  driveToAction->SetPreActionPoseAngleTolerance(DEG_TO_RAD(15.f));
  driveToAction->DoPositionCheckOnPathCompletion(doPositionCheckOnPathCompletion);
  AddAction(driveToAction);
  AddAction(new TurnToAlignWithChargerAction(objectID));

  auto mountAction = new MountChargerAction(objectID, useCliffSensorCorrection);
  if(!enableDockingAnims)
  {
    mountAction->SetDockingAnimTriggers(AnimationTrigger::Count,
                                        AnimationTrigger::Count,
                                        AnimationTrigger::Count);
  }
  AddAction(mountAction);
}
  
} // namespace Vector
} // namespace Anki

