/**
 * File: robotEventHandler.cpp
 *
 * Author: Lee
 * Created: 08/11/15
 *
 * Description: Class for subscribing to and handling events going to robots.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/
#include "engine/robotEventHandler.h"

#include "engine/aiComponent/aiComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviorComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviorContainer.h"
#include "engine/aiComponent/behaviorComponent/behaviorSystemManager.h"
#include "engine/ankiEventUtil.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/components/backpackLights/engineBackpackLightComponent.h"
#include "engine/components/cubes/cubeAccelComponent.h"
#include "engine/components/carryingComponent.h"
#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/components/nvStorageComponent.h"
#include "engine/components/pathComponent.h"
#include "engine/components/sensors/proxSensorComponent.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/gatewayInterface.h"
#include "engine/faceWorld.h"
#include "engine/robot.h"
#include "engine/robotManager.h"

#include "engine/actions/actionInterface.h"
#include "engine/actions/animActions.h"
#include "engine/actions/basicActions.h"
#include "engine/actions/chargerActions.h"
#include "engine/actions/dockActions.h"
#include "engine/actions/driveToActions.h"
#include "engine/actions/flipBlockAction.h"
#include "engine/actions/retryWrapperAction.h"
#include "engine/actions/sayTextAction.h"
#include "engine/actions/trackGroundPointAction.h"
#include "engine/actions/trackFaceAction.h"
#include "engine/actions/trackMotionAction.h"
#include "engine/actions/trackObjectAction.h"
#include "engine/actions/trackPetFaceAction.h"
#include "engine/actions/visuallyVerifyActions.h"

#include "engine/components/animationComponent.h"
#include "engine/components/visionComponent.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "engine/pathPlanner.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/types/poseStructs.h"
#include "util/console/consoleInterface.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/helpers/boundedWhile.h"
#include "util/helpers/fullEnumToValueArrayChecker.h"
#include "util/helpers/templateHelpers.h"

#include "generated/proto/external_interface/shared.pb.h"
#include "generated/proto/external_interface/messages.pb.h"

#define LOG_CHANNEL "RobotEventHandler"

namespace Anki {
namespace Vector {

u32 RobotEventHandler::_gameActionTagCounter = ActionConstants::FIRST_GAME_INTERNAL_TAG;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PathMotionProfile ConvertProtoPathMotionProfile(const external_interface::PathMotionProfile& protoPathMotionProfile)
{
  PathMotionProfile pathMotionProfile{
    protoPathMotionProfile.speed_mmps(),
    protoPathMotionProfile.accel_mmps2(),
    protoPathMotionProfile.decel_mmps2(),
    protoPathMotionProfile.point_turn_speed_rad_per_sec(),
    protoPathMotionProfile.point_turn_accel_rad_per_sec2(),
    protoPathMotionProfile.point_turn_decel_rad_per_sec2(),
    protoPathMotionProfile.dock_speed_mmps(),
    protoPathMotionProfile.dock_accel_mmps2(),
    protoPathMotionProfile.dock_decel_mmps2(),
    protoPathMotionProfile.reverse_speed_mmps(),
    protoPathMotionProfile.is_custom()
  };

  return pathMotionProfile;
}

// =====================================================================================================================
#pragma mark -
#pragma mark GetAction() Helpers

// Implement a specialization of this method for each action message type
template<class MessageType>
static IActionRunner* GetActionHelper(Robot& robot, const MessageType& msg);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: PlaceObjectOnGroundHereRequest
//  if any changes are made here, they should be reflected in the associated function.
//IActionRunner* GetPlaceObjectOnGroundHereAction(Robot& robot, const ExternalInterface::PlaceObjectOnGroundHere& msg)
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlaceObjectOnGroundHere& msg)
{
  return new PlaceObjectOnGroundAction();
}

// Proto equivalent of the preceding PlaceObjectOnGroundHere clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::PlaceObjectOnGroundHereRequest& msg)
{
  PlaceObjectOnGroundAction *action = new PlaceObjectOnGroundAction();
  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//IActionRunner* GetPlaceObjectOnGroundAction(Robot& robot, const ExternalInterface::PlaceObjectOnGround& msg)
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlaceObjectOnGround& msg)
{
  // Create an action to drive to specified pose and then put down the carried
  // object.
  // TODO: Better way to set the object's z height and parent? (This assumes object's origin is 22mm off the ground!)
  Rotation3d rot(UnitQuaternion(msg.qw, msg.qx, msg.qy, msg.qz));
  Pose3d targetPose(rot, Vec3f(msg.x_mm, msg.y_mm, 22.f), robot.GetWorldOrigin());
  return new PlaceObjectOnGroundAtPoseAction(targetPose,
                                             msg.useExactRotation,
                                             msg.checkDestinationFree);
}

using AnimTrackFlagType = std::underlying_type<AnimTrackFlag>::type;
std::underlying_type<AnimTrackFlag>::type GetIgnoreTracks(bool ignoreBodyTrack, bool ignoreHeadTrack, bool ignoreLiftTrack)
{
  AnimTrackFlagType ignoreTracks = Util::EnumToUnderlying(AnimTrackFlag::NO_TRACKS);

  if (ignoreBodyTrack)
  {
    ignoreTracks |= Util::EnumToUnderlying(AnimTrackFlag::BODY_TRACK);
  }
  if (ignoreHeadTrack)
  {
    ignoreTracks |= Util::EnumToUnderlying(AnimTrackFlag::HEAD_TRACK);
  }
  if (ignoreLiftTrack)
  {
    ignoreTracks |= Util::EnumToUnderlying(AnimTrackFlag::LIFT_TRACK);
  }

  return ignoreTracks;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlayAnimation& msg)
{
  AnimTrackFlagType ignoreTracks = GetIgnoreTracks(msg.ignoreBodyTrack, msg.ignoreHeadTrack, msg.ignoreLiftTrack);
  const bool kInterruptRunning = true; // TODO: expose this option in CLAD?
  return new PlayAnimationAction(msg.animationName, msg.numLoops, kInterruptRunning, ignoreTracks);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlayAnimationGroup& msg)
{
  AnimTrackFlagType ignoreTracks = GetIgnoreTracks(msg.ignoreBodyTrack, msg.ignoreHeadTrack, msg.ignoreLiftTrack);
  const bool kInterruptRunning = true; // TODO: expose this option in CLAD?
  const auto& animName = robot.GetAnimationComponent().GetAnimationNameFromGroup(msg.animationGroupName);
  return new PlayAnimationAction(animName, msg.numLoops, kInterruptRunning, ignoreTracks);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: GoToPoseRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::GotoPose& msg)
{
  // TODO: Add ability to indicate z too!
  // TODO: Better way to specify the target pose's parent
  Pose3d targetPose(msg.rad, Z_AXIS_3D(), Vec3f(msg.x_mm, msg.y_mm, 0), robot.GetWorldOrigin());
  targetPose.SetName("GotoPoseTarget");

  auto* action = new DriveToPoseAction(targetPose);

  if(msg.motionProf.isCustom)
  {
    robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
  }
  return action;
}

// Proto equivalent of the preceding GotoPose clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::GoToPoseRequest& msg)
{
  // TODO: Add ability to indicate z too!
  // TODO: Better way to specify the target pose's parent
  Pose3d targetPose(msg.rad(), Z_AXIS_3D(), Vec3f(msg.x_mm(), msg.y_mm(), 0), robot.GetWorldOrigin());
  targetPose.SetName("GotoPoseTarget");

  DriveToPoseAction* action = new DriveToPoseAction(targetPose);

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());
  if(pathMotionProfile.isCustom)
  {
    robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
  }

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::FlipBlock& msg)
{
  ObjectID selectedObjectID = msg.objectID;
  if(selectedObjectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  }

  DriveAndFlipBlockAction* action = new DriveAndFlipBlockAction(selectedObjectID);

  if(msg.motionProf.isCustom)
{
    robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
  }
  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PanAndTilt& msg)
{
  return new PanAndTiltAction(Radians(msg.bodyPan),
                              Radians(msg.headTilt),
                              msg.isPanAbsolute,
                              msg.isTiltAbsolute);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: PickupObject
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PickupObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose))
  {
    DriveToPickupObjectAction* action = new DriveToPickupObjectAction(selectedObjectID,
                                                                      msg.useApproachAngle,
                                                                      msg.approachAngle_rad);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }

    return action;
  }
  else
  {
    PickupObjectAction* action = new PickupObjectAction(selectedObjectID);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// Proto equivalent of the preceding PickupObject clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::PickupObjectRequest& msg)
{
  ObjectID selectedObjectID;
  if(msg.object_id() < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.object_id();
  }

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());
  if(static_cast<bool>(msg.use_pre_dock_pose()))
  {
    DriveToPickupObjectAction* action = new DriveToPickupObjectAction(selectedObjectID,
                                                                      msg.use_approach_angle(),
                                                                      msg.approach_angle_rad());

    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }

    return action;
  }
  else
  {
    PickupObjectAction* action = new PickupObjectAction(selectedObjectID);
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlaceRelObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {
    DriveToPlaceRelObjectAction* action = new DriveToPlaceRelObjectAction(selectedObjectID,
                                                                          true,
                                                                          msg.placementOffsetX_mm,
                                                                          0,
                                                                          msg.useApproachAngle,
                                                                          msg.approachAngle_rad);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    return action;
  } else {
    PlaceRelObjectAction* action = new PlaceRelObjectAction(selectedObjectID,
                                                            true,
                                                            msg.placementOffsetX_mm,
                                                            0);
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlaceOnObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {

    DriveToPlaceOnObjectAction* action = new DriveToPlaceOnObjectAction(selectedObjectID,
                                                                        msg.useApproachAngle,
                                                                        msg.approachAngle_rad);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    return action;
  } else {
    PlaceRelObjectAction* action = new PlaceRelObjectAction(selectedObjectID,
                                                            false,
                                                            0,
                                                            0);
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: GotoObjectRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::GotoObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  DriveToObjectAction* action;
  if(msg.usePreDockPose)
  {
    action = new DriveToObjectAction(selectedObjectID,
                                     PreActionPose::ActionType::DOCKING);
  } else {
    action = new DriveToObjectAction(selectedObjectID,
                                     msg.distanceFromObjectOrigin_mm);
  }

  if(msg.motionProf.isCustom)
  {
    robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
  }

  return action;
}

// Proto equivalent of the preceding GotoObject clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::GoToObjectRequest& msg)
{
  ObjectID selectedObjectID;
  if(msg.object_id() < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.object_id();
  }

  DriveToObjectAction* action;
  if(msg.use_pre_dock_pose())
  {
    action = new DriveToObjectAction(selectedObjectID,
                                     PreActionPose::ActionType::DOCKING);
  } else {
    action = new DriveToObjectAction(selectedObjectID,
                                     msg.distance_from_object_origin_mm());
  }

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());
  if(pathMotionProfile.isCustom)
  {
    robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
  }

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: DockWithCubeRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::AlignWithObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {
    DriveToAlignWithObjectAction* action = new DriveToAlignWithObjectAction(selectedObjectID,
                                                                            msg.distanceFromMarker_mm,
                                                                            msg.useApproachAngle,
                                                                            msg.approachAngle_rad,
                                                                            msg.alignmentType);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }

    return action;
  } else {
    AlignWithObjectAction* action = new AlignWithObjectAction(selectedObjectID,
                                                              msg.distanceFromMarker_mm,
                                                              msg.alignmentType);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about aligning with a specific marker just that we are aligning with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// Proto equivalent of the preceding AlignWithObject clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::DockWithCubeRequest& msg)
{
  ObjectID selectedObjectID;
  if(msg.object_id() < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.object_id();
  }

  // offsetting by one because in proto 0 is registered as invalid
  AlignmentType alignmentType = static_cast<AlignmentType>(msg.alignment_type() - 1);

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());

  if(msg.use_pre_dock_pose()) {
    DriveToAlignWithObjectAction* action = new DriveToAlignWithObjectAction(selectedObjectID,
                                                                            msg.distance_from_marker_mm(),
                                                                            msg.use_approach_angle(),
                                                                            msg.approach_angle_rad(),
                                                                            alignmentType);
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }

    return action;
  } else {
    AlignWithObjectAction* action = new AlignWithObjectAction(selectedObjectID,
                                                              msg.distance_from_marker_mm(),
                                                              alignmentType);
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about aligning with a specific marker just that we are aligning with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);

    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::CalibrateMotors& msg)
{
    CalibrateMotorAction* action = new CalibrateMotorAction(msg.calibrateHead,
                                                            msg.calibrateLift,
                                                            MotorCalibrationReason::Game);
    return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::CliffAlignToWhite& msg)
{
  return new CliffAlignToWhiteAction();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: DriveStraightRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::DriveStraight& msg)
{
  return new DriveStraightAction(msg.dist_mm, msg.speed_mmps, msg.shouldPlayAnimation);
}

// Proto equivalent of the preceding DriveStraight clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::DriveStraightRequest& msg)
{
  IAction* action = new DriveStraightAction(
    msg.dist_mm(),
    msg.speed_mmps(),
    msg.should_play_animation());

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: RollObjectRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::RollObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {
    DriveToRollObjectAction* action = new DriveToRollObjectAction(selectedObjectID,
                                                                  msg.useApproachAngle,
                                                                  msg.approachAngle_rad);
    action->EnableDeepRoll(msg.doDeepRoll);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    return action;
  } else {
    RollObjectAction* action = new RollObjectAction(selectedObjectID);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    action->EnableDeepRoll(msg.doDeepRoll);
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    action->EnableRollWithoutDock(msg.rollWithoutDocking);
    return action;
  }
}

// Proto equivalent of the preceding RollObject clad message handler with a couple settings removed (doDeepRoll and rollWithoutDocking).
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::RollObjectRequest& msg)
{
  ObjectID selectedObjectID;
  if(msg.object_id() < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.object_id();
  }

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());
  if(static_cast<bool>(msg.use_pre_dock_pose())) {
    DriveToRollObjectAction* action = new DriveToRollObjectAction(selectedObjectID,
                                                                  msg.use_approach_angle(),
                                                                  msg.approach_angle_rad());
    
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }
    return action;
  } else {
    RollObjectAction* action = new RollObjectAction(selectedObjectID);
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }
    
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: PopAWheelieRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PopAWheelie& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {
    DriveToPopAWheelieAction* action = new DriveToPopAWheelieAction(selectedObjectID,
                                                                    msg.useApproachAngle,
                                                                    msg.approachAngle_rad);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }

    return action;
  } else {
    PopAWheelieAction* action = new PopAWheelieAction(selectedObjectID);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::PopAWheelieRequest& msg)
{
  ObjectID selectedObjectID;
  if(msg.object_id() < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.object_id();
  }

  PathMotionProfile pathMotionProfile = ConvertProtoPathMotionProfile(msg.motion_prof());
  if(static_cast<bool>(msg.use_pre_dock_pose())) {
    DriveToPopAWheelieAction* action = new DriveToPopAWheelieAction(selectedObjectID,
                                                                    msg.use_approach_angle(),
                                                                    msg.approach_angle_rad());
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }

    return action;
  } else {
    PopAWheelieAction* action = new PopAWheelieAction(selectedObjectID);
    if(pathMotionProfile.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(pathMotionProfile, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::FacePlant& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  if(static_cast<bool>(msg.usePreDockPose)) {
    DriveToFacePlantAction* action = new DriveToFacePlantAction(selectedObjectID,
                                                                msg.useApproachAngle,
                                                                msg.approachAngle_rad);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }

    return action;
  } else {
    FacePlantAction* action = new FacePlantAction(selectedObjectID);
    if(msg.motionProf.isCustom)
    {
      robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
    }
    action->SetDoNearPredockPoseCheck(false);
    // We don't care about a specific marker just that we are docking with the correct object
    action->SetShouldVisuallyVerifyObjectOnly(true);
    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::MountCharger& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  auto action =  new DriveToAndMountChargerAction(selectedObjectID,
                                                  msg.useCliffSensorCorrection);
  if(msg.motionProf.isCustom)
  {
    robot.GetPathComponent().SetCustomMotionProfileForAction(msg.motionProf, action);
  }
  return action;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::RealignWithObject& msg)
{
  ObjectID selectedObjectID;
  if(msg.objectID < 0) {
    selectedObjectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    selectedObjectID = msg.objectID;
  }

  DriveToRealignWithObjectAction* driveToRealignWithObjectAction = new DriveToRealignWithObjectAction(selectedObjectID, msg.dist_mm);
  return driveToRealignWithObjectAction;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: TurnInPlaceRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnInPlace& msg)
{
  TurnInPlaceAction* action = new TurnInPlaceAction(msg.angle_rad, msg.isAbsolute);
  action->SetMaxSpeed(msg.speed_rad_per_sec);
  action->SetAccel(msg.accel_rad_per_sec2);
  action->SetTolerance(msg.tol_rad);
  return action;
}

// Proto equivalent of the preceding TurnInPlace clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::TurnInPlaceRequest& msg)
{
  auto isAbsolute = msg.is_absolute();
  if(isAbsolute > std::numeric_limits<uint8_t>::max())
  {
    isAbsolute = std::numeric_limits<uint8_t>::max();
  }

  TurnInPlaceAction* action = new TurnInPlaceAction(msg.angle_rad(), isAbsolute);
  action->SetMaxSpeed(msg.speed_rad_per_sec());
  action->SetAccel(msg.accel_rad_per_sec2());
  action->SetTolerance(msg.tol_rad());

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnTowardsObject& msg)
{
  ObjectID objectID;
  if(msg.objectID == std::numeric_limits<u32>::max()) {
    objectID = robot.GetBlockWorld().GetSelectedObject();
  } else {
    objectID = msg.objectID;
  }

  TurnTowardsObjectAction* action = new TurnTowardsObjectAction(objectID,
                                                                Radians(msg.maxTurnAngle_rad),
                                                                msg.visuallyVerifyWhenDone,
                                                                msg.headTrackWhenDone);

  action->SetMaxPanSpeed(msg.maxPanSpeed_radPerSec);
  action->SetPanAccel(msg.panAccel_radPerSec2);
  action->SetPanTolerance(msg.panTolerance_rad);
  action->SetMaxTiltSpeed(msg.maxTiltSpeed_radPerSec);
  action->SetTiltAccel(msg.tiltAccel_radPerSec2);
  action->SetTiltTolerance(msg.tiltTolerance_rad);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnTowardsPose& msg)
{
  Pose3d pose(0, Z_AXIS_3D(), {msg.world_x, msg.world_y, msg.world_z},
              robot.GetWorldOrigin());

  TurnTowardsPoseAction* action = new TurnTowardsPoseAction(pose, Radians(msg.maxTurnAngle_rad));

  action->SetMaxPanSpeed(msg.maxPanSpeed_radPerSec);
  action->SetPanAccel(msg.panAccel_radPerSec2);
  action->SetPanTolerance(msg.panTolerance_rad);
  action->SetMaxTiltSpeed(msg.maxTiltSpeed_radPerSec);
  action->SetTiltAccel(msg.tiltAccel_radPerSec2);
  action->SetTiltTolerance(msg.tiltTolerance_rad);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: TurnTowardsFaceRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnTowardsFace& msg)
{
  SmartFaceID smartID = robot.GetFaceWorld().GetSmartFaceID(msg.faceID);
  TurnTowardsFaceAction* action = new TurnTowardsFaceAction(smartID, Radians(msg.maxTurnAngle_rad), msg.sayName);

  if(msg.sayName)
  {
    action->SetSayNameAnimationTrigger(msg.namedTrigger);
    action->SetNoNameAnimationTrigger(msg.unnamedTrigger);
  }

  action->SetMaxPanSpeed(msg.maxPanSpeed_radPerSec);
  action->SetPanAccel(msg.panAccel_radPerSec2);
  action->SetPanTolerance(msg.panTolerance_rad);
  action->SetMaxTiltSpeed(msg.maxTiltSpeed_radPerSec);
  action->SetTiltAccel(msg.tiltAccel_radPerSec2);
  action->SetTiltTolerance(msg.tiltTolerance_rad);

  return action;
}

// Proto equivalent of the preceeding TurnTowardsFace clad message handler, with the exception that this method allows fewer settings.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::TurnTowardsFaceRequest& msg)
{
  SmartFaceID smartID = robot.GetFaceWorld().GetSmartFaceID(msg.face_id());
  TurnTowardsFaceAction* action = new TurnTowardsFaceAction(smartID, Radians(msg.max_turn_angle_rad()));

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnTowardsImagePoint& msg)
{
  TurnTowardsImagePointAction* action = new TurnTowardsImagePointAction(Point2f(msg.x, msg.y), msg.timestamp);

  action->SetMaxPanSpeed(msg.maxPanSpeed_radPerSec);
  action->SetPanAccel(msg.panAccel_radPerSec2);
  action->SetPanTolerance(msg.panTolerance_rad);
  action->SetMaxTiltSpeed(msg.maxTiltSpeed_radPerSec);
  action->SetTiltAccel(msg.tiltAccel_radPerSec2);
  action->SetTiltTolerance(msg.tiltTolerance_rad);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TurnTowardsLastFacePose& msg)
{
  TurnTowardsLastFacePoseAction* action = new TurnTowardsLastFacePoseAction(Radians(msg.maxTurnAngle_rad), msg.sayName);

  if(msg.sayName)
  {
    action->SetSayNameAnimationTrigger(msg.namedTrigger);
    action->SetNoNameAnimationTrigger(msg.unnamedTrigger);
  }

  action->SetMaxPanSpeed(msg.maxPanSpeed_radPerSec);
  action->SetPanAccel(msg.panAccel_radPerSec2);
  action->SetPanTolerance(msg.panTolerance_rad);
  action->SetMaxTiltSpeed(msg.maxTiltSpeed_radPerSec);
  action->SetTiltAccel(msg.tiltAccel_radPerSec2);
  action->SetTiltTolerance(msg.tiltTolerance_rad);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TrackToFace& trackFace)
{
  TrackFaceAction* action = new TrackFaceAction(trackFace.faceID);
  action->SetMoveEyes(trackFace.moveEyes);

  // TODO: Support body-only mode
  if(trackFace.headOnly) {
    action->SetMode(ITrackAction::Mode::HeadOnly);
  }

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TrackToLaserPoint& trackLaser)
{
  TrackGroundPointAction* action = new TrackGroundPointAction(ExternalInterface::MessageEngineToGameTag::RobotObservedLaserPoint);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TrackToObject& trackObject)
{
  TrackObjectAction* action = new TrackObjectAction(trackObject.objectID);
  action->SetMoveEyes(trackObject.moveEyes);

  // TODO: Support body-only mode
  if(trackObject.headOnly) {
    action->SetMode(ITrackAction::Mode::HeadOnly);
  }

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::TrackToPet& trackPet)
{
  TrackPetFaceAction* action = nullptr;

  if(trackPet.petID != Vision::UnknownFaceID)
  {
    action = new TrackPetFaceAction(trackPet.petID);
  }
  else
  {
    action = new TrackPetFaceAction(trackPet.petType);
  }

  action->SetUpdateTimeout(trackPet.timeout_sec);

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: SetHeadAngleRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::SetHeadAngle& setHeadAngle)
{
  MoveHeadToAngleAction* action = new MoveHeadToAngleAction(setHeadAngle.angle_rad);
  action->SetMaxSpeed(setHeadAngle.max_speed_rad_per_sec);
  action->SetAccel(setHeadAngle.accel_rad_per_sec2);
  action->SetDuration(setHeadAngle.duration_sec);
  return action;
}

// Proto equivalent of the preceding SetHeadAngle clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::SetHeadAngleRequest& msg)
{
  MoveHeadToAngleAction* action = new MoveHeadToAngleAction(msg.angle_rad());
  action->SetMaxSpeed(msg.max_speed_rad_per_sec());
  action->SetAccel(msg.accel_rad_per_sec2());
  action->SetDuration(msg.duration_sec());

  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Version for SayText message
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::SayText& sayText)
{
  SayTextAction* sayTextAction = new SayTextAction(sayText.text,
                                                   sayText.voiceStyle,
                                                   sayText.durationScalar);
  sayTextAction->SetAnimationTrigger(sayText.playEvent);

  return sayTextAction;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::SetLiftAngle& msg)
{
  // Special case if commanding low dock height while carrying a block...
  if (msg.angle_rad == MIN_LIFT_ANGLE && robot.GetCarryingComponent().IsCarryingObject())
  {
    // ...put the block down right here.
    IActionRunner* newAction = new PlaceObjectOnGroundAction();
    return newAction;
  }
  else
  {
    // In the normal case directly set the lift angle
    MoveLiftToAngleAction* action = new MoveLiftToAngleAction(msg.angle_rad);
    action->SetMaxLiftSpeed(msg.max_speed_rad_per_sec);
    action->SetLiftAccel(msg.accel_rad_per_sec2);
    action->SetDuration(msg.duration_sec);

    return action;
  }
}

// THIS FUNCTION IS A CLAD EQUIVALENT FOR THE FOLLOWING: SetLiftHeightRequest
//  if any changes are made here, they should be reflected in the associated function.
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::SetLiftHeight& msg)
{
  // Special case if commanding low dock height while carrying a block...
  if (msg.height_mm == LIFT_HEIGHT_LOWDOCK && robot.GetCarryingComponent().IsCarryingObject())
  {
    // ...put the block down right here.
    IActionRunner* newAction = new PlaceObjectOnGroundAction();
    return newAction;
  }
  else
  {
    // In the normal case directly set the lift height
    MoveLiftToHeightAction* action = new MoveLiftToHeightAction(msg.height_mm);
    action->SetMaxLiftSpeed(msg.max_speed_rad_per_sec);
    action->SetLiftAccel(msg.accel_rad_per_sec2);
    action->SetDuration(msg.duration_sec);

    return action;
  }
}

// Proto equivalent of the preceding SetLiftHeight clad message handler.
template<>
IActionRunner* GetActionHelper(Robot& robot, const external_interface::SetLiftHeightRequest& msg)
{
  // Special case if commanding low dock height while carrying a block...
  if (msg.height_mm() == LIFT_HEIGHT_LOWDOCK && robot.GetCarryingComponent().IsCarryingObject())
  {
    // ...put the block down right here.
    IActionRunner* action = new PlaceObjectOnGroundAction();
    return action;
  }
  else
  {
    // In the normal case directly set the lift height
    MoveLiftToHeightAction* action = new MoveLiftToHeightAction(msg.height_mm());
    action->SetMaxLiftSpeed(msg.max_speed_rad_per_sec());
    action->SetLiftAccel(msg.accel_rad_per_sec2());
    action->SetDuration(msg.duration_sec());

    return action;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::VisuallyVerifyFace& msg)
{
  VisuallyVerifyFaceAction* action = new VisuallyVerifyFaceAction(msg.faceID);
  action->SetNumImagesToWaitFor(msg.numImagesToWait);
  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::VisuallyVerifyObject& msg)
{
  VisuallyVerifyObjectAction* action = new VisuallyVerifyObjectAction(msg.objectID);
  action->SetNumImagesToWaitFor(msg.numImagesToWait);
  return action;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::VisuallyVerifyNoObjectAtPose& msg)
{
  Pose3d p(0, Z_AXIS_3D(), Vec3f(msg.x_mm, msg.y_mm, msg.z_mm), robot.GetWorldOrigin());
  return new VisuallyVerifyNoObjectAtPoseAction(p, {msg.x_thresh_mm, msg.y_thresh_mm, msg.z_thresh_mm});
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::PlayAnimationTrigger& msg)
{
  IActionRunner* newAction = nullptr;
  AnimTrackFlagType ignoreTracks = GetIgnoreTracks(msg.ignoreBodyTrack, msg.ignoreHeadTrack, msg.ignoreLiftTrack);
  const bool kInterruptRunning = true; // TODO: expose this option in CLAD?

  if( msg.useLiftSafe ) {
    newAction = new TriggerLiftSafeAnimationAction(msg.trigger, msg.numLoops, kInterruptRunning, ignoreTracks);
  }
  else {
    newAction = new TriggerAnimationAction(msg.trigger, msg.numLoops, kInterruptRunning, ignoreTracks);
  }
  return newAction;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::SearchForNearbyObject& msg)
{
  return new SearchForNearbyObjectAction(msg.desiredObjectID, msg.backupDistance_mm, msg.backupSpeed_mms, msg.headAngle_rad);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::Wait& msg)
{
  return new WaitAction(msg.time_s);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
IActionRunner* GetActionHelper(Robot& robot, const ExternalInterface::WaitForImages& msg)
{
  return new WaitForImagesAction(msg.numImages, msg.visionMode, msg.afterTimeStamp);
}

// =====================================================================================================================
#pragma mark -
#pragma mark ActionMessageHandler/Entry/Array

// This section of helper classes is used to guarantee all RobotActionUnionTags
// are associated with a GetAction() handler method above, AND that each corresponding
// MessageGameToEngine for an action (i.e. commanded with no queuing) also call
// the same method.

template<ExternalInterface::RobotActionUnionTag ActionUnionTag, ExternalInterface::MessageGameToEngineTag GameToEngineTag>
struct GetActionWrapper
{
  static IActionRunner* GetActionUnionFcn(Robot& robot, const ExternalInterface::RobotActionUnion& actionUnion)
  {
    return GetActionHelper(robot, actionUnion.Get_<ActionUnionTag>());
  }

  static IActionRunner* GetGameToEngineFcn(Robot& robot, const ExternalInterface::MessageGameToEngine& msg)
  {
    return GetActionHelper(robot, msg.Get_<GameToEngineTag>());
  }
};

struct ActionMessageHandler
{
  using ActionUnionTag  = ExternalInterface::RobotActionUnionTag;
  using GameToEngineTag = ExternalInterface::MessageGameToEngineTag;
  using ActionUnionFcn  = RobotEventHandler::ActionUnionFcn;
  using GameToEngineFcn = RobotEventHandler::GameToEngineFcn;

  const ActionUnionTag     actionUnionTag;
  const GameToEngineTag    gameToEngineTag;
  const ActionUnionFcn     getActionFromActionUnion;
  const GameToEngineFcn    getActionFromMessage;
  const s32                numRetries;

  constexpr ActionMessageHandler(const ActionUnionTag&     actionUnionTag,
                                 const GameToEngineTag&    gameToEngineTag,
                                 const ActionUnionFcn      getActionFromActionUnion,
                                 const GameToEngineFcn     getActionFromMessage,
                                 const s32                 defaultNumRetries)
  : actionUnionTag(actionUnionTag)
  , gameToEngineTag(gameToEngineTag)
  , getActionFromActionUnion(getActionFromActionUnion)
  , getActionFromMessage(getActionFromMessage)
  , numRetries(defaultNumRetries)
  {

  }

};

using FullActionMessageHandlerArray = Util::FullEnumToValueArrayChecker::FullEnumToValueArray<
  ActionMessageHandler::ActionUnionTag,
  ActionMessageHandler,
  ActionMessageHandler::ActionUnionTag::count>;

// =====================================================================================================================
#pragma mark -
#pragma mark RobotEventHandler Methods

RobotEventHandler::RobotEventHandler(const CozmoContext* context)
: _context(context)
{
  auto externalInterface = _context->GetExternalInterface();

  if (externalInterface != nullptr)
  {
    using namespace ExternalInterface;

    //
    // Handle action messages specially
    //

    // We'll use this callback for all action events
    auto actionEventCallback = std::bind(&RobotEventHandler::HandleActionEvents, this, std::placeholders::_1);

    // This macro makes adding handler definitions less painful/verbose by adding namespaces
    // and grabbing the right function pointer for the right method in the templated
    // GetActionWrapper helper struct above.
#   define DEFINE_HANDLER(__actionTag__, __g2eTag__, __numRetries__) \
    { \
      RobotActionUnionTag::__actionTag__ , \
      ActionMessageHandler(RobotActionUnionTag::__actionTag__, MessageGameToEngineTag::__g2eTag__, \
        &GetActionWrapper<RobotActionUnionTag::__actionTag__, MessageGameToEngineTag::__g2eTag__>::GetActionUnionFcn, \
        &GetActionWrapper<RobotActionUnionTag::__actionTag__, MessageGameToEngineTag::__g2eTag__>::GetGameToEngineFcn, \
        __numRetries__) \
    }

    constexpr static const FullActionMessageHandlerArray kActionHandlerArray {

      //
      // Create an entry pairing a RobotActionUnionTag with a MessageGameToEngineTag and
      // associating the template-specialized GetActionHelper() method here.
      // These should be added in the same order as they are defined in the
      // RobotActionUnion in messageActions.clad.
      //
      // If the order or number is not done correctly, you will get a compilation error!
      //
      // Usage:
      //   DEFINE_HANDLER(actionUnionTag, msgGameToEngineTag, defaultNumRetries)
      //
      // NOTE: numRetries is only used when action is requested via MessageGameToEngine.
      //       (Otherwise, the numRetries in the action queueing message is used.)
      //

      DEFINE_HANDLER(alignWithObject,          AlignWithObject,          0),
      DEFINE_HANDLER(calibrateMotors,          CalibrateMotors,          0),
      DEFINE_HANDLER(cliffAlignToWhite,        CliffAlignToWhite,        0),
      DEFINE_HANDLER(driveStraight,            DriveStraight,            0),
      DEFINE_HANDLER(facePlant,                FacePlant,                0),
      DEFINE_HANDLER(flipBlock,                FlipBlock,                0),
      DEFINE_HANDLER(gotoObject,               GotoObject,               0),
      DEFINE_HANDLER(gotoPose,                 GotoPose,                 2),
      DEFINE_HANDLER(mountCharger,             MountCharger,             2),
      DEFINE_HANDLER(panAndTilt,               PanAndTilt,               0),
      DEFINE_HANDLER(pickupObject,             PickupObject,             0),
      DEFINE_HANDLER(placeObjectOnGround,      PlaceObjectOnGround,      1),
      DEFINE_HANDLER(placeObjectOnGroundHere,  PlaceObjectOnGroundHere,  0),
      DEFINE_HANDLER(placeOnObject,            PlaceOnObject,            1),
      DEFINE_HANDLER(placeRelObject,           PlaceRelObject,           1),
      DEFINE_HANDLER(playAnimation,            PlayAnimation,            0),
      DEFINE_HANDLER(playAnimationGroup,       PlayAnimationGroup,       0),
      DEFINE_HANDLER(playAnimationTrigger,     PlayAnimationTrigger,     0),
      DEFINE_HANDLER(popAWheelie,              PopAWheelie,              1),
      DEFINE_HANDLER(realignWithObject,        RealignWithObject,        1),
      DEFINE_HANDLER(rollObject,               RollObject,               1),
      DEFINE_HANDLER(sayText,                  SayText,                  0),
      DEFINE_HANDLER(searchForNearbyObject,    SearchForNearbyObject,    0),
      DEFINE_HANDLER(setHeadAngle,             SetHeadAngle,             0),
      DEFINE_HANDLER(setLiftHeight,            SetLiftHeight,            0),
      DEFINE_HANDLER(setLiftAngle,             SetLiftAngle,             0),
      DEFINE_HANDLER(trackFace,                TrackToFace,              0),
      DEFINE_HANDLER(trackObject,              TrackToObject,            0),
      DEFINE_HANDLER(trackLaserPoint,          TrackToLaserPoint,        0),
      DEFINE_HANDLER(trackPet,                 TrackToPet,               0),
      DEFINE_HANDLER(turnInPlace,              TurnInPlace,              0),
      DEFINE_HANDLER(turnTowardsFace,          TurnTowardsFace,          0),
      DEFINE_HANDLER(turnTowardsImagePoint,    TurnTowardsImagePoint,    0),
      DEFINE_HANDLER(turnTowardsLastFacePose,  TurnTowardsLastFacePose,  0),
      DEFINE_HANDLER(turnTowardsObject,        TurnTowardsObject,        0),
      DEFINE_HANDLER(turnTowardsPose,          TurnTowardsPose,          0),
      DEFINE_HANDLER(visuallyVerifyFace,       VisuallyVerifyFace,       0),
      DEFINE_HANDLER(visuallyVerifyNoObjectAtPose, VisuallyVerifyNoObjectAtPose, 0),
      DEFINE_HANDLER(visuallyVerifyObject,     VisuallyVerifyObject,     0),
      DEFINE_HANDLER(wait,                     Wait,                     0),
      DEFINE_HANDLER(waitForImages,            WaitForImages,            0),
    };

    static_assert(Util::FullEnumToValueArrayChecker::IsSequentialArray(kActionHandlerArray),
                  "Duplicated or out-of-order entries in action handler array.");

    // Build lookup tables so we don't have to linearly search through the above
    // array each time we want to find the handler
    for(auto & entry : kActionHandlerArray)
    {
      const ActionMessageHandler& handler = entry.Value();

      _actionUnionHandlerLUT[handler.actionUnionTag] = handler.getActionFromActionUnion;
      _gameToEngineHandlerLUT[handler.gameToEngineTag] = std::make_pair(handler.getActionFromMessage, handler.numRetries);

      // Also subscribe to the event here:
      _signalHandles.push_back(externalInterface->Subscribe(handler.gameToEngineTag, actionEventCallback));
    }

    //
    // For all other messages, just use an AnkiEventUtil object:
    //
    auto helper = MakeAnkiEventUtil(*_context->GetExternalInterface(), *this, _signalHandles);

    // GameToEngine: (in alphabetical order)
    helper.SubscribeGameToEngine<MessageGameToEngineTag::AbortAll>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::AbortPath>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::CameraCalibration>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::CancelAction>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::CancelActionByIdTag>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::ClearCalibrationImages>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::ComputeCameraCalibration>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::ControllerGains>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::DrawPoseMarker>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::EnableCliffSensor>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::EnableStopOnCliff>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::EnableLiftPower>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::ExecuteTestPlan>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::ForceDelocalizeRobot>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::IMURequest>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::LogRawCliffData>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::LogRawProxData>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::QueueSingleAction>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::QueueCompoundAction>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::RollActionParams>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SaveCalibrationImage>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetMotionModelParams>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetRobotCarryingObject>();

    // Messages from switchboard
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetConnectionStatus>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetBLEPin>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SendBLEConnectionStatus>();

    // EngineToGame: (in alphabetical order)
    helper.SubscribeEngineToGame<MessageEngineToGameTag::AnimationAborted>();
    helper.SubscribeEngineToGame<MessageEngineToGameTag::RobotCompletedAction>();
    helper.SubscribeEngineToGame<MessageEngineToGameTag::RobotConnectionResponse>();
  }

} // RobotEventHandler Constructor


// =====================================================================================================================
#pragma mark -
#pragma mark Action Event Handlers


u32 RobotEventHandler::GetNextGameActionTag() {
  if (++_gameActionTagCounter > ActionConstants::LAST_GAME_INTERNAL_TAG) {
    _gameActionTagCounter = ActionConstants::FIRST_GAME_INTERNAL_TAG;
  }

  return _gameActionTagCounter;
}

void RobotEventHandler::HandleActionEvents(const GameToEngineEvent& event)
{
  auto const& msg = event.GetData();
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // If we don't have a valid robot there's nothing to do
  if (nullptr == robot)
  {
    return;
  }

  // Create the action
  auto handlerIter = _gameToEngineHandlerLUT.find(msg.GetTag());
  if(handlerIter == _gameToEngineHandlerLUT.end())
  {
    // This should really never happen because we are supposed to be guaranteed at
    // compile time that all action tags are inserted.
    PRINT_NAMED_ERROR("RobotEventHandler.HandleActionEvents.MissingTag",
                      "%s (%hhu)", MessageGameToEngineTagToString(msg.GetTag()), msg.GetTag());
    return;
  }

  // Now we fill out our Action and possibly update number of retries:
  IActionRunner* newAction = handlerIter->second.first(*robot, msg);
  if(nullptr == newAction)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleActionEvents.NullAction",
                        "Tag: %s", ExternalInterface::MessageGameToEngineTagToString(msg.GetTag()));
    return;
  }
  const u8 numRetries = handlerIter->second.second;
  newAction->SetTag(GetNextGameActionTag());

  // Everything's ok and we have an action, so queue it
  robot->GetActionList().QueueAction(QueueActionPosition::NOW, newAction, numRetries);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::QueueSingleAction& msg)
{
  // Can't queue actions for nonexistent robots...
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot)
  {
    return;
  }

  auto handlerIter = _actionUnionHandlerLUT.find(msg.action.GetTag());
  if(handlerIter == _actionUnionHandlerLUT.end())
  {
    // This should really never happen because we are supposed to be guaranteed at
    // compile time that all action tags are inserted.
    PRINT_NAMED_ERROR("RobotEventHandler.HandleQueueSingleAction.MissingActionTag",
                      "%s (%hhu)", RobotActionUnionTagToString(msg.action.GetTag()), msg.action.GetTag());
    return;
  }

  // If numRetries > 0, wrap in retry action
  IActionRunner* action = nullptr;
  if (msg.numRetries > 0) {
    IActionRunner* actionRunnerPtr = handlerIter->second(*robot, msg.action);
    IAction* actionPtr = dynamic_cast<IAction*>(actionRunnerPtr);
    if (actionPtr != nullptr) {
      action = new RetryWrapperAction(actionPtr, AnimationTrigger::Count, msg.numRetries);
    } else {
      ICompoundAction* compoundActionPtr = dynamic_cast<ICompoundAction*>(actionRunnerPtr);
      if (compoundActionPtr != nullptr) {
        action = new RetryWrapperAction(compoundActionPtr, AnimationTrigger::Count, msg.numRetries);
      } else {
        PRINT_NAMED_WARNING("RobotEventHandler.HandleQueueSingleAction.InvalidActionForRetries", "%s", actionRunnerPtr->GetName().c_str());
        return;
      }
    }
  } else {
    action = handlerIter->second(*robot, msg.action);
  }
  action->SetTag(msg.idTag);

  // Put the action in the given position of the specified queue
  robot->GetActionList().QueueAction(msg.position, action, 0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::QueueCompoundAction& msg)
{
  // Can't queue actions for nonexistent robots...
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleQueueCompoundAction.InvalidRobotID", "Failed to find robot. Missing 'first' robot.");
    return;
  }

  // Create an empty parallel or sequential compound action:
  ICompoundAction* compoundAction = nullptr;
  if(msg.parallel) {
    compoundAction = new CompoundActionParallel();
  } else {
    compoundAction = new CompoundActionSequential();
  }

  // Add all the actions in the message to the compound action, according
  // to their type
  for(size_t iAction=0; iAction < msg.actions.size(); ++iAction)
  {
    auto const& actionUnion = msg.actions[iAction];

    auto handlerIter = _actionUnionHandlerLUT.find(actionUnion.GetTag());
    if(handlerIter == _actionUnionHandlerLUT.end())
    {
      // This should really never happen because we are supposed to be guaranteed at
      // compile time that all action tags are inserted.
      PRINT_NAMED_ERROR("RobotEventHandler.HandleQueueCompoundAction.MissingActionTag",
                        "Action %zu: %s (%hhu)", iAction,
                        RobotActionUnionTagToString(actionUnion.GetTag()), actionUnion.GetTag());
      delete compoundAction;
      return;
    }

    IActionRunner* action = handlerIter->second(*robot, actionUnion);

    compoundAction->AddAction(action);

  } // for each action/actionType

  // If numRetries > 0, wrap in retry action
  IActionRunner* action = nullptr;
  if (msg.numRetries > 0) {
    action = new RetryWrapperAction(compoundAction, AnimationTrigger::Count, msg.numRetries);
  } else {
    action = compoundAction;
  }
  action->SetTag(msg.idTag);

  // Put the action in the given position of the specified queue
  robot->GetActionList().QueueAction(msg.position, action, 0);
}


// =====================================================================================================================
#pragma mark -
#pragma mark All Other Event Handlers

template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::EnableLiftPower& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    return;
  }

  if(robot->GetMoveComponent().AreAnyTracksLocked((u8)AnimTrackFlag::LIFT_TRACK)) {
    LOG_INFO("RobotEventHandler.HandleEnableLiftPower.LiftLocked",
                     "Ignoring ExternalInterface::EnableLiftPower while lift is locked.");
  } else {
    robot->GetMoveComponent().EnableLiftPower(msg.enable);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::EnableCliffSensor& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if (nullptr != robot)
  {
    LOG_INFO("RobotEventHandler.HandleMessage.EnableCliffSensor","Setting to %s", msg.enable ? "true" : "false");
    robot->GetCliffSensorComponent().SetEnableCliffSensor(msg.enable);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::EnableStopOnCliff& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if (nullptr != robot)
  {
    LOG_INFO("RobotEventHandler.HandleMessage.EnableStopOnCliff","Setting to %s", msg.enable ? "true" : "false");
    robot->SendRobotMessage<RobotInterface::EnableStopOnCliff>(msg.enable);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::ForceDelocalizeRobot& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleForceDelocalizeRobot.InvalidRobotID",
                        "Failed to find robot to delocalize.");

  } else if(!robot->IsPhysical()) {
    LOG_INFO("RobotMessageHandler.ProcessMessage.ForceDelocalize",
             "Forcibly delocalizing robot");

    robot->SendRobotMessage<RobotInterface::ForceDelocalizeSimulatedRobot>();
  } else {
    robot->Delocalize( robot->GetCarryingComponent().IsCarryingObject() );
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::SaveCalibrationImage& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleSaveCalibrationImage.InvalidRobotID", "Failed to find robot. Missing 'first' robot.");
  }
  else
  {
    robot->GetVisionComponent().StoreNextImageForCameraCalibration();
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::ClearCalibrationImages& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleClearCalibrationImages.InvalidRobotID", "Failed to find robot. Missing 'first' robot.");
  }
  else
  {
    robot->GetVisionComponent().ClearCalibrationImages();
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::ComputeCameraCalibration& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleComputeCameraCalibration.InvalidRobotID", "Failed to find robot. Missing 'first' robot.");
  }
  else
  {
    robot->GetVisionComponent().EnableComputingCameraCalibration(true);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const CameraCalibration& calib)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleCameraCalibration.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    std::vector<u8> calibVec(calib.Size());
    calib.Pack(calibVec.data(), calib.Size());
    robot->GetNVStorageComponent().Write(NVStorage::NVEntryTag::NVEntry_CameraCalib, calibVec.data(), calibVec.size());

    LOG_INFO("RobotEventHandler.HandleCameraCalibration.SendingCalib",
                     "fx: %f, fy: %f, cx: %f, cy: %f, nrows %d, ncols %d",
                     calib.focalLength_x, calib.focalLength_y, calib.center_x, calib.center_y, calib.nrows, calib.ncols);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::AnimationAborted& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if(nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleAnimationAborted.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->AbortAnimation();
    LOG_INFO("RobotEventHandler.HandleAnimationAborted.SendingRobotAbortAnimation", "");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::RobotCompletedAction& msg)
{
  // Log DAS events for specific action completions
  switch(msg.actionType)
  {
    case RobotActionType::ALIGN_WITH_OBJECT:
    case RobotActionType::MOUNT_CHARGER:
    case RobotActionType::PICK_AND_PLACE_INCOMPLETE:
    case RobotActionType::PICKUP_OBJECT_HIGH:
    case RobotActionType::PICKUP_OBJECT_LOW:
    case RobotActionType::PLACE_OBJECT_HIGH:
    case RobotActionType::PLACE_OBJECT_LOW:
    case RobotActionType::POP_A_WHEELIE:
    case RobotActionType::ROLL_OBJECT_LOW:
    {
      // Don't log incomplete docks -- they can happen for many reasons (such as
      // interruptions / cancellations on the way to docking) and we're most interested
      // in figuring out how successful the robot is when it gets a chance to actually
      // start trying to dock with the object
      if(msg.result != ActionResult::NOT_STARTED)
      {
        DASMSG(robot.dock_action_completed, "robot.dock_action_completed", "A dock action completed");
        DASMSG_SET(s1, EnumToString(msg.actionType), "Action type");
        DASMSG_SET(s2, EnumToString(msg.result), "Action result");
        DASMSG_SEND();
      }

      break;
    }

    default:
      break;
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::RobotConnectionResponse& msg)
{
  if (msg.result == RobotConnectionResult::Success) {
    Robot* robot = _context->GetRobotManager()->GetRobot();

    if(nullptr == robot) {
      PRINT_NAMED_WARNING("RobotEventHandler.HandleRobotConnectionResponse.InvalidRobotID", "Failed to find robot.");
    }
    else
    {
      robot->SyncRobot();
      LOG_INFO("RobotEventHandler.HandleRobotConnectionResponse.SendingSyncRobot", "");

      robot->GetAnimationComponent().Init();
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::CancelAction& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleCancelAction.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->GetActionList().Cancel((RobotActionType)msg.actionType);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::CancelActionByIdTag& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleCancelActionByIdTag.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->GetActionList().Cancel(msg.idTag);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::ControllerGains& msg)
{
  // We need a robot
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleControllerGains.InvalidRobotID", "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<RobotInterface::ControllerGains>(msg.kp, msg.ki, msg.kd, msg.maxIntegralError, msg.controller);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::DrawPoseMarker& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleDrawPoseMarker.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    if(robot->GetCarryingComponent().IsCarryingObject()) {
      Pose3d targetPose(msg.rad, Z_AXIS_3D(), Vec3f(msg.x_mm, msg.y_mm, 0));
      const ObservableObject* carryObject = robot->GetBlockWorld().GetLocatedObjectByID(robot->GetCarryingComponent().GetCarryingObjectID());
      if(nullptr == carryObject)
      {
        PRINT_NAMED_WARNING("RobotEventHandler.HandleDrawPoseMarker.NullCarryObject",
                            "Carry object set to ID=%d, but BlockWorld returned NULL",
                            robot->GetCarryingComponent().GetCarryingObjectID().GetValue());
        return;
      }
      Quad2f objectFootprint = carryObject->GetBoundingQuadXY(targetPose);
      robot->GetContext()->GetVizManager()->DrawPoseMarker(0, objectFootprint, ::Anki::NamedColors::GREEN);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const IMURequest& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleIMURequest.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->RequestIMU(msg.length_ms);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::LogRawCliffData& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleLogRawCliffData.InvalidRobotID", "Failed to find robot.");
  } else {
    robot->GetCliffSensorComponent().StartLogging(msg.length_ms);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::LogRawProxData& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleLogRawProxData.InvalidRobotID", "Failed to find robot.");
  } else {
    robot->GetProxSensorComponent().StartLogging(msg.length_ms);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::ExecuteTestPlan& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleExecuteTestPlan.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->GetPathComponent().ExecuteTestPath(msg.motionProf);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::RollActionParams& msg)
{
  // We need a robot
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleRollActionParams.InvalidRobotID", "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<RobotInterface::RollActionParams>(msg.liftHeight_mm,
                                                              msg.driveSpeed_mmps,
                                                              msg.driveAccel_mmps2,
                                                              msg.driveDuration_ms,
                                                              msg.backupDist_mm);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::SetMotionModelParams& msg)
{
  // We need a robot
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleSetMotionModelParams.InvalidRobotID", "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<RobotInterface::SetMotionModelParams>(msg.slipFactor);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::SetRobotCarryingObject& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleSetRobotCarryingObject.InvalidRobotID", "Failed to find robot.");

  }
  else
  {
    if(msg.objectID < 0) {
      robot->GetCarryingComponent().SetCarriedObjectAsUnattached();
    } else {
      robot->GetCarryingComponent().SetCarryingObject(msg.objectID, Vision::MARKER_INVALID);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::AbortPath& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleAbortPath.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->GetPathComponent().Abort();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const ExternalInterface::AbortAll& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  // We need a robot
  if (nullptr == robot)
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleAbortAll.InvalidRobotID", "Failed to find robot.");
  }
  else
  {
    robot->AbortAll();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const SwitchboardInterface::SetConnectionStatus& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.SwitchboardSetConnectionStatus.InvalidRobotID",
                        "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<SwitchboardInterface::SetConnectionStatus>(msg.status);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const SwitchboardInterface::SetBLEPin& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.SwitchboardSetBLEPin.InvalidRobotID",
                        "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<SwitchboardInterface::SetBLEPin>(msg.pin);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void RobotEventHandler::HandleMessage(const SwitchboardInterface::SendBLEConnectionStatus& msg)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();

  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.SwitchboardSendBLEConnectionStatus.InvalidRobotID",
                        "Failed to find robot");
  } else {
    // Forward to robot
    robot->SendRobotMessage<SwitchboardInterface::SendBLEConnectionStatus>(msg.connected);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class IGatewayActionRunner {
public:
  virtual void Invoke(Robot& robot, const AnkiEvent<external_interface::GatewayWrapper>& event) const = 0;
  virtual ~IGatewayActionRunner() {}
};

template <typename MessageType>
class GatewayActionRunner : public IGatewayActionRunner
{
public:
  using GatewayMessageConverterFcn = std::function<const MessageType(const AnkiEvent<external_interface::GatewayWrapper>&)>;
  const GatewayMessageConverterFcn converter;

  constexpr GatewayActionRunner(const GatewayMessageConverterFcn& converter)
  : converter(converter)
  {

  }

  virtual void Invoke(Robot& robot, const AnkiEvent<external_interface::GatewayWrapper>& event) const
  {
    const MessageType& convertedMessage = converter(event);
    IActionRunner* internalAction = GetActionHelper(robot, convertedMessage);
    LOG_INFO("RobotEventHandler.GatewayActionRunner.Invoke.ParsedMessage", "%s", internalAction->GetName().c_str());

    int numRetries = convertedMessage.num_retries();
    int idTag = convertedMessage.id_tag();

    IActionRunner* dispatchAction = nullptr;

    if (numRetries > 0) {
      IAction* actionPtr = dynamic_cast<IAction*>(internalAction);
      if (actionPtr != nullptr) {
        dispatchAction = new RetryWrapperAction(actionPtr, AnimationTrigger::Count, numRetries);
      } else {
        ICompoundAction* compoundActionPtr = dynamic_cast<ICompoundAction*>(internalAction);
        if (compoundActionPtr != nullptr) {
          dispatchAction = new RetryWrapperAction(compoundActionPtr, AnimationTrigger::Count, numRetries);
        } else {
          PRINT_NAMED_WARNING("RobotEventHandler.GatewayActionRunner.Invoke.InvalidActionForRetries", "%s", internalAction->GetName().c_str());
          delete internalAction;
          return;
        }
      }
    } else {
      dispatchAction = internalAction;
    }

    dispatchAction->SetTag(idTag);

    // Put the action in the given position of the specified queue.
    // The Queue will take responsibility for the memory management of this raw pointer - unless it fails
    //  in which case we clean up the memory ourselves.
    if( robot.GetActionList().QueueAction(QueueActionPosition::IN_PARALLEL, dispatchAction, 0) != RESULT_OK )
    {
      PRINT_NAMED_WARNING("RobotEventHandler.GatewayActionRunner.Invoke.ActionCouldNotQueue", "%s", internalAction->GetName().c_str());
    }
  }
};

static const std::map< external_interface::GatewayWrapperTag, std::unique_ptr<IGatewayActionRunner> >& GetGatewayHandlers()
{
  static std::map< external_interface::GatewayWrapperTag, std::unique_ptr<IGatewayActionRunner> > result;
  if( result.size() == 0 )
  {
#   define ADD_GATEWAY_HANDLER(__gatewayTag__, __requestType__, __extractionFunction__) \
      result[external_interface::GatewayWrapperTag::__gatewayTag__] = \
        std::make_unique<GatewayActionRunner<external_interface::__requestType__> >( \
          []( const AnkiEvent<external_interface::GatewayWrapper>& event ) { return event.GetData().__extractionFunction__(); } \
        )

    ADD_GATEWAY_HANDLER( kGoToPoseRequest,                 GoToPoseRequest,                 go_to_pose_request );
    ADD_GATEWAY_HANDLER( kDockWithCubeRequest,             DockWithCubeRequest,             dock_with_cube_request );
    ADD_GATEWAY_HANDLER( kDriveStraightRequest,            DriveStraightRequest,            drive_straight_request );
    ADD_GATEWAY_HANDLER( kTurnInPlaceRequest,              TurnInPlaceRequest,              turn_in_place_request );
    ADD_GATEWAY_HANDLER( kSetLiftHeightRequest,            SetLiftHeightRequest,            set_lift_height_request );
    ADD_GATEWAY_HANDLER( kSetHeadAngleRequest,             SetHeadAngleRequest,             set_head_angle_request );
    ADD_GATEWAY_HANDLER( kTurnTowardsFaceRequest,          TurnTowardsFaceRequest,          turn_towards_face_request );
    ADD_GATEWAY_HANDLER( kGoToObjectRequest,               GoToObjectRequest,               go_to_object_request );
    ADD_GATEWAY_HANDLER( kRollObjectRequest,               RollObjectRequest,               roll_object_request );
    ADD_GATEWAY_HANDLER( kPopAWheelieRequest,              PopAWheelieRequest,              pop_a_wheelie_request );
    ADD_GATEWAY_HANDLER( kPickupObjectRequest,             PickupObjectRequest,             pickup_object_request );
    ADD_GATEWAY_HANDLER( kPlaceObjectOnGroundHereRequest,  PlaceObjectOnGroundHereRequest,  place_object_on_ground_here_request );
  }
  return result;
}

template<>
void RobotEventHandler::HandleMessage(const AnkiEvent<external_interface::GatewayWrapper>& event)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (nullptr == robot) {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleMessage.InvalidRobotID",
                        "Failed to find robot");
    return;
  }

  const auto& handlerMap = GetGatewayHandlers();

  const auto& tag = event.GetData().GetTag();
  if( handlerMap.count(tag) == 0 )
  {
    PRINT_NAMED_WARNING("RobotEventHandler.HandleMessage.NoGatewayHandler",
                        "Gateway message received with no handler for tag %i", (int)tag);
    return;
  }

  const auto& handler = handlerMap.at(tag);
  handler->Invoke(*robot, event);
}

} // namespace Vector
} // namespace Anki
