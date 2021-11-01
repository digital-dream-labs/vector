/**
 * File: flipBlockAction.cpp
 *
 * Author: Al Chaussee
 * Date:   5/18/16
 *
 * Description: Action which flips blocks
 *              By default, when driving to the flipping preAction pose, we will drive to one of the two poses that is closest
 *              to Cozmo and farthest from the last known face. In order to maximize the chances of the person being able to see
 *              Cozmo's face and reactions while he is flipping the block
 *                - Should there not be a last known face, of the two closest preAction poses the left most will be chosen
 *
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/actions/basicActions.h"
#include "engine/actions/dockActions.h"
#include "engine/actions/flipBlockAction.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/faceWorld.h"
#include "engine/robot.h"

#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {

namespace {
// Utility function to check if the robot is within a threshold of a preAction pose
bool WithinPreActionThreshold(const Robot& robot, std::vector<Pose3d>& possiblePoses, f32 threshold)
{
  float distanceBetweenRobotAndPose;
  for(auto pose : possiblePoses){
    ComputeDistanceSQBetween(robot.GetPose(), pose, distanceBetweenRobotAndPose);
    if(threshold >= 0 && threshold > distanceBetweenRobotAndPose){
      return true;
    }
  }
  
  return false;
}
  
}

static constexpr f32 kPreDockPoseAngleTolerance = DEG_TO_RAD(5.f);

DriveAndFlipBlockAction::DriveAndFlipBlockAction(const ObjectID objectID,
                                                 const bool useApproachAngle,
                                                 const f32 approachAngle_rad,
                                                 Radians maxTurnTowardsFaceAngle_rad,
                                                 const bool sayName,
                                                 const float minAlignThreshold_mm)
: IDriveToInteractWithObject(objectID,
                             PreActionPose::FLIPPING,
                             0,
                             useApproachAngle,
                             approachAngle_rad,
                             maxTurnTowardsFaceAngle_rad,
                             sayName)
{
  FlipBlockAction* flipBlockAction = new FlipBlockAction(objectID);

  SetName("DriveToAndFlipBlock");
  
  DriveToObjectAction* driveToObjectAction = GetDriveToObjectAction();
  if(driveToObjectAction != nullptr)
  {
    driveToObjectAction->SetGetPossiblePosesFunc([this](ActionableObject* object, std::vector<Pose3d>& possiblePoses, bool& alreadyInPosition)
    {
      
      // Check to see if the robot is close enough to the preActionPose to prevent tiny re-alignments
      if(!alreadyInPosition && (_minAlignThreshold_mm >= 0)){
        bool withinThreshold = WithinPreActionThreshold(GetRobot(), possiblePoses, _minAlignThreshold_mm);
        alreadyInPosition = withinThreshold;
        static_cast<FlipBlockAction*>(_flipBlockAction.lock().get())->SetShouldCheckPreActionPose(!withinThreshold);
      }
      
      return GetPossiblePoses(GetRobot().GetPose(), GetRobot().GetCarryingComponent(), GetRobot().GetBlockWorld(), GetRobot().GetFaceWorld(), 
                              object, possiblePoses, alreadyInPosition, false);        
    });
  }
  
  _flipBlockAction = AddAction(flipBlockAction);
  SetProxyTag(flipBlockAction->GetTag()); // Use flip action's completion info
}

void DriveAndFlipBlockAction::ShouldDriveToClosestPreActionPose(bool tf)
{
  DriveToObjectAction* driveToObjectAction = GetDriveToObjectAction();
  if(driveToObjectAction != nullptr)
  {
    driveToObjectAction->SetGetPossiblePosesFunc([this, tf](ActionableObject* object, std::vector<Pose3d>& possiblePoses, bool& alreadyInPosition)
    {
      
      // Check to see if the robot is close enough to the preActionPose to prevent tiny re-alignments
      if(!alreadyInPosition && _minAlignThreshold_mm >= 0){
        bool withinThreshold = WithinPreActionThreshold(GetRobot(), possiblePoses, _minAlignThreshold_mm);
        alreadyInPosition = withinThreshold;
        static_cast<FlipBlockAction*>(_flipBlockAction.lock().get())->SetShouldCheckPreActionPose(!withinThreshold);
      }
      
      
      return GetPossiblePoses(GetRobot().GetPose(), GetRobot().GetCarryingComponent(), GetRobot().GetBlockWorld(), GetRobot().GetFaceWorld(),
                              object, possiblePoses, alreadyInPosition, tf);
    });
  }
}

ActionResult DriveAndFlipBlockAction::GetPossiblePoses(const Pose3d& robotPose,
                                                       const CarryingComponent& carryingComp,
                                                       BlockWorld& blockWorld,
                                                       FaceWorld& faceWorld,
                                                       ActionableObject* object,
                                                       std::vector<Pose3d>& possiblePoses,
                                                       bool& alreadyInPosition,
                                                       const bool shouldDriveToClosestPose)
{
  LOG_INFO("DriveAndFlipBlockAction.GetPossiblePoses", "Getting possible preActionPoses");
  const IDockAction::PreActionPoseInput preActionPoseInput(object,
                                                           PreActionPose::FLIPPING,
                                                           false,
                                                           0,
                                                           kPreDockPoseAngleTolerance,
                                                           false, 0);
  
  IDockAction::PreActionPoseOutput preActionPoseOutput;
  
  IDockAction::GetPreActionPoses(robotPose, carryingComp, blockWorld, 
                                 preActionPoseInput, preActionPoseOutput);
  
  if(preActionPoseOutput.actionResult != ActionResult::SUCCESS)
  {
    PRINT_NAMED_WARNING("DriveToFlipBlockPoseAction.Constructor", "Failed to find closest preAction pose");
    return preActionPoseOutput.actionResult;
  }
  
  Pose3d facePose;
  RobotTimeStamp_t faceTime = faceWorld.GetLastObservedFace(facePose);
  
  if(preActionPoseOutput.preActionPoses.empty())
  {
    PRINT_NAMED_WARNING("DriveToFlipBlockPoseAction.Constructor", "No preAction poses");
    return ActionResult::NO_PREACTION_POSES;
  }
  
  if(shouldDriveToClosestPose)
  {
    LOG_INFO("DriveAndFlipBlockAction.GetPossiblePoses", "Selecting closest preAction pose");
    possiblePoses.push_back(preActionPoseOutput.preActionPoses[preActionPoseOutput.closestIndex].GetPose());
    return ActionResult::SUCCESS;
  }
  
  Pose3d firstClosestPose;
  Pose3d secondClosestPose;
  f32 firstClosestDist = std::numeric_limits<float>::max();
  f32 secondClosestDist = firstClosestDist;
  
  for(auto iter = preActionPoseOutput.preActionPoses.begin(); iter != preActionPoseOutput.preActionPoses.end(); ++iter)
  {
    Pose3d poseWrtRobot;
    if(!iter->GetPose().GetWithRespectTo(robotPose, poseWrtRobot))
    {
      continue;
    }
    
    f32 dist = poseWrtRobot.GetTranslation().Length();
    
    
    if(dist < firstClosestDist)
    {
      secondClosestDist = firstClosestDist;
      secondClosestPose = firstClosestPose;
      
      firstClosestDist = dist;
      firstClosestPose = poseWrtRobot;
    }
    else if(dist < secondClosestDist)
    {
      secondClosestDist = dist;
      secondClosestPose = poseWrtRobot;
    }
  }
  
  Pose3d poseToDriveTo;
  
  // There is only one preaction pose so it will be the first closest
  if(preActionPoseOutput.preActionPoses.size() == 1)
  {
    poseToDriveTo = firstClosestPose;
  }
  else
  {
    Pose3d firstClosestPoseWRTFace = Pose3d(firstClosestPose);
    Pose3d secondClosestPoseWRTFace = Pose3d(secondClosestPose);
    const bool hasKnownFaces = faceTime != 0;
    const bool noFacesInFrame = hasKnownFaces && ((!firstClosestPose.GetWithRespectTo(facePose, firstClosestPoseWRTFace)
                                                    || !secondClosestPose.GetWithRespectTo(facePose, secondClosestPoseWRTFace)));
    
    if(!hasKnownFaces || noFacesInFrame)
    {
      // No last known face so pick the preaction pose that is left most relative to Cozmo
      if(firstClosestPose.GetTranslation().y() >= secondClosestPose.GetTranslation().y())
      {
        poseToDriveTo = firstClosestPose;
      }
      else
      {
        poseToDriveTo = secondClosestPose;
      }
    }else{
      // Otherwise pick one of the two preaction poses closest to the robot and farthest from the last known face

      if(firstClosestPoseWRTFace.GetTranslation().Length() > secondClosestPoseWRTFace.GetTranslation().Length())
      {
        poseToDriveTo = firstClosestPose;
      }
      else
      {
        poseToDriveTo = secondClosestPose;
      }
    }
  }

  possiblePoses.push_back(poseToDriveTo);
  return ActionResult::SUCCESS;
}


DriveToFlipBlockPoseAction::DriveToFlipBlockPoseAction(ObjectID objectID)
: DriveToObjectAction(objectID, PreActionPose::FLIPPING)
{
  SetName("DriveToFlipBlockPose");
  SetType(RobotActionType::DRIVE_TO_FLIP_BLOCK_POSE);
}

void DriveToFlipBlockPoseAction::ShouldDriveToClosestPreActionPose(bool tf)
{
  SetGetPossiblePosesFunc([this, tf](ActionableObject* object, std::vector<Pose3d>& possiblePoses, bool& alreadyInPosition)
  {
    return DriveAndFlipBlockAction::GetPossiblePoses(GetRobot().GetPose(), GetRobot().GetCarryingComponent(), GetRobot().GetBlockWorld(), GetRobot().GetFaceWorld(),
                                                     object, possiblePoses, alreadyInPosition, tf);
  });
}


void DriveToFlipBlockPoseAction::OnRobotSetInternalDriveToObj()
{
  SetGetPossiblePosesFunc([this](ActionableObject* object, std::vector<Pose3d>& possiblePoses, bool& alreadyInPosition)
  {
    return DriveAndFlipBlockAction::GetPossiblePoses(GetRobot().GetPose(), GetRobot().GetCarryingComponent(), GetRobot().GetBlockWorld(), GetRobot().GetFaceWorld(), 
                                                     object, possiblePoses, alreadyInPosition, false);
  });
}


FlipBlockAction::FlipBlockAction(ObjectID objectID)
: IAction("FlipBlock", 
          RobotActionType::FLIP_BLOCK,
          ((u8)AnimTrackFlag::LIFT_TRACK | (u8)AnimTrackFlag::BODY_TRACK))
, _objectID(objectID)
, _compoundAction()
, _shouldCheckPreActionPose(true)
{
}

  
FlipBlockAction::~FlipBlockAction()
{
  _compoundAction.PrepForCompletion();
  if (HasRobot() && (_flipTag != -1))
  {
    GetRobot().GetActionList().Cancel(_flipTag);
  }
}

void FlipBlockAction::SetShouldCheckPreActionPose(bool shouldCheck)
{
  _shouldCheckPreActionPose = shouldCheck;
}

void FlipBlockAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
{
  requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::Low });
}

ActionResult FlipBlockAction::Init()
{
  // Incase we are being retried
  _compoundAction.ClearActions();
  
  ActionableObject* object = dynamic_cast<ActionableObject*>(GetRobot().GetBlockWorld().GetLocatedObjectByID(_objectID));
  if(nullptr == object)
  {
    PRINT_NAMED_WARNING("FlipBlockAction.Init.NullObject", "ObjectID=%d", _objectID.GetValue());
    return ActionResult::BAD_OBJECT;
  }
  
  const IDockAction::PreActionPoseInput preActionPoseInput(object,
                                                           PreActionPose::FLIPPING,
                                                           _shouldCheckPreActionPose,
                                                           0,
                                                           kPreDockPoseAngleTolerance,
                                                           false, 0);
  
  IDockAction::PreActionPoseOutput preActionPoseOutput;
  
  IDockAction::GetPreActionPoses(GetRobot().GetPose(), GetRobot().GetCarryingComponent(), GetRobot().GetBlockWorld(),
                                 preActionPoseInput, preActionPoseOutput);
  
  if(preActionPoseOutput.actionResult != ActionResult::SUCCESS)
  {
    return preActionPoseOutput.actionResult;
  }
  
  Pose3d p;
  object->GetPose().GetWithRespectTo(GetRobot().GetPose(), p);

  // Need to suppress track locking so the two lift actions don't fail because the other locked the lift track
  // A little dangerous as animations playing in parallel to this action could move lift
  _compoundAction.ShouldSuppressTrackLocking(true);
  
  // Drive through the block
  DriveStraightAction* drive = new DriveStraightAction(p.GetTranslation().Length() + kDrivingDist_mm, kDrivingSpeed_mmps);
  
  // Need to set the initial lift height to fit lift base into block corner edge
  MoveLiftToHeightAction* initialLift = new MoveLiftToHeightAction(kInitialLiftHeight_mm);

  _compoundAction.AddAction(initialLift);
  _compoundAction.AddAction(drive);
  _compoundAction.Update();
  return ActionResult::SUCCESS;
}

ActionResult FlipBlockAction::CheckIfDone()
{
  const ActionResult result = _compoundAction.Update();
  
  // grab object now because we use regardless of result
  auto* block = GetRobot().GetBlockWorld().GetLocatedObjectByID(_objectID);
  
  if(result != ActionResult::RUNNING)
  {
    // After flipping the block, it will definitely be in a new pose, but it will be _pretty close_ to its previous
    // pose. Therefore, mark the pose as dirty, but do not remove the object entirely.
    if ( nullptr != block )
    {
      GetRobot().GetBlockWorld().MarkObjectDirty(block);
    }
    else
    {
      PRINT_NAMED_WARNING("FlipBlockAction.CheckIfDone.NotRunning.NullObject", "ObjectID=%d", _objectID.GetValue());
    }
    return result;
  }
  
  if(nullptr == block)
  {
    PRINT_NAMED_WARNING("FlipBlockAction.CheckIfDone.NullObject", "ObjectID=%d", _objectID.GetValue());
    return ActionResult::BAD_OBJECT;
  }
  
  Pose3d p;
  block->GetPose().GetWithRespectTo(GetRobot().GetPose(), p);
  if((p.GetTranslation().Length() < kDistToObjectToFlip_mm && _flipTag == -1))
  {
    IAction* action = new MoveLiftToHeightAction(MoveLiftToHeightAction::Preset::CARRY);

    // FlipBlockAction is already locking all tracks so this lift action doesn't need to lock
    action->ShouldSuppressTrackLocking(true);
    _flipTag = action->GetTag();
    GetRobot().GetActionList().QueueAction(QueueActionPosition::IN_PARALLEL, action);
  }

  return ActionResult::RUNNING;
}


void FlipBlockAction::OnRobotSet()
{
  _compoundAction.SetRobot(&GetRobot());
}

  
}
}
