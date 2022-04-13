/**
 * File: dockActions.cpp
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements cozmo-specific actions, derived from the IAction interface.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/actions/dockActions.h"

#include "clad/types/animationTypes.h"
#include "clad/types/behaviorComponent/behaviorStats.h"
#include "coretech/common/engine/utils/timer.h"
#include "engine/actions/animActions.h"
#include "engine/actions/driveToActions.h"
#include "engine/actions/visuallyVerifyActions.h"
#include "engine/aiComponent/aiComponent.h"
#include "engine/ankiEventUtil.h"
#include "engine/audio/engineRobotAudioClient.h"
#include "engine/block.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/charger.h"
#include "engine/components/carryingComponent.h"
#include "engine/components/cubes/cubeLights/cubeLightComponent.h"
#include "engine/components/dockingComponent.h"
#include "engine/components/habitatDetectorComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/components/pathComponent.h"
#include "engine/components/robotStatsTracker.h"
#include "engine/components/visionComponent.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/faceWorld.h"
#include "engine/robot.h"
#include "engine/robotDataLoader.h"
#include "engine/robotInterface/messageHandler.h"
#include "util/cladHelpers/cladEnumToStringMap.h"
#include "util/console/consoleInterface.h"
#include "util/helpers/templateHelpers.h"


namespace{
// This max negative offset is limited mainly by kBodyDistanceOffset_mm used in
// AlignWithObject which defines the closest that a block can be approached (with lift raised).
// Doesn't make much sense for PlaceRelObject but it doesn't really hurt except that
// the robot would bump into the block it was docking to.
static const float kMaxNegativeXPlacementOffset = 16.f;

// use a fairly large distance offset and tighter angle to try to rule out current pose
static const f32 kSamePreactionPoseDistThresh_mm = 100.f;
static const f32 kSamePreactionPoseAngleThresh_deg = 30.f;
}

#define LOG_CHANNEL "Actions"

namespace Anki {
  namespace Vector {

    // Which docking method actions should use
    CONSOLE_VAR(u32, kDefaultDockingMethod,"DockingMethod(B:0 T:1 H:2)", (u8)DockingMethod::BLIND_DOCKING);
    CONSOLE_VAR(u32, kPickupDockingMethod, "DockingMethod(B:0 T:1 H:2)", (u8)DockingMethod::HYBRID_DOCKING_BEELINE);
    CONSOLE_VAR(u32, kRollDockingMethod,   "DockingMethod(B:0 T:1 H:2)", (u8)DockingMethod::BLIND_DOCKING);
    CONSOLE_VAR(u32, kStackDockingMethod,  "DockingMethod(B:0 T:1 H:2)", (u8)DockingMethod::BLIND_DOCKING);

    // Whether or not to calculate the max preDock pose offset for PlaceRelObjectAction
    CONSOLE_VAR(bool, kPlaceRelUseMaxOffset, "PlaceRelObjectAction", true);

    // Helper function for computing the distance-to-preActionPose threshold,
    // given how far preActionPose is from actionObject
    Point2f ComputePreActionPoseDistThreshold(const Pose3d& preActionPose,
                                              const Pose3d& actionObjectPose,
                                              const Radians& preActionPoseAngleTolerance)
    {
      if(preActionPoseAngleTolerance > 0.f) {
        // Compute distance threshold for preaction pose based on distance to the
        // object: the further away, the more slop we're allowed.
        Pose3d objectWrtPreActionPose;
        if(false == actionObjectPose.GetWithRespectTo(preActionPose, objectWrtPreActionPose)) {
          PRINT_NAMED_WARNING("ComputePreActionPoseDistThreshold.ObjectPoseOriginProblem",
                              "Could not get object pose w.r.t. preActionPose.");
          return -1.f;
        }

        const f32 objectDistance = objectWrtPreActionPose.GetTranslation().Length();
        const f32 thresh = objectDistance * std::sin(preActionPoseAngleTolerance.ToFloat());

        // We don't care so much about the distance to the object (x threshold) so scale it
        const Point2f preActionPoseDistThresh(thresh * PREACTION_POSE_X_THRESHOLD_SCALAR, thresh);

        PRINT_CH_INFO("Actions",
                      "ComputePreActionPoseDistThreshold.DistThresh",
                      "At a distance of %.1fmm, will use pre-dock pose distance threshold of (%.1fmm, %.1fmm)",
                      objectDistance, preActionPoseDistThresh.x(), preActionPoseDistThresh.y());

        return preActionPoseDistThresh;
      } else {
        return -1.f;
      }
    }

    #pragma mark ---- IDockAction ----

    IDockAction::IDockAction(ObjectID objectID,
                             const std::string name,
                             const RobotActionType type)
    : IAction(name,
              type,
              ((u8)AnimTrackFlag::HEAD_TRACK |
               (u8)AnimTrackFlag::LIFT_TRACK |
               (u8)AnimTrackFlag::BODY_TRACK))
    , _dockObjectID(objectID)
    , _dockingMethod((DockingMethod)kDefaultDockingMethod)
    {
      _getInDockTrigger  = AnimationTrigger::DockStartDefault;
      _loopDockTrigger   = AnimationTrigger::DockLoopDefault;
      _getOutDockTrigger = AnimationTrigger::DockEndDefault;
      _curDockTrigger    = AnimationTrigger::Count;
    }

    IDockAction::~IDockAction()
    {
      if(!HasRobot()){
        return;
      }

      // the action automatically selects the block, deselect now to remove Viz
      GetRobot().GetBlockWorld().DeselectCurrentObject();

      // Abort anything that shouldn't still be running
      if(GetRobot().GetPathComponent().IsActive()) {
        GetRobot().GetPathComponent().Abort();
      }
      if(_lightsSet)
      {
        LOG_INFO("IDockAction.UnsetInteracting", "%s[%d] Unsetting interacting object to %d",
                 GetName().c_str(), GetTag(),
                 _dockObjectID.GetValue());
        GetRobot().GetCubeLightComponent().StopLightAnimAndResumePrevious(CubeAnimationTrigger::Interacting, _dockObjectID);
      }

      if(_dockingComponentPtr != nullptr){
        if(_dockingComponentPtr->IsPickingOrPlacing()) {
          _dockingComponentPtr->AbortDocking();
        }

        _dockingComponentPtr->UnsetDockObjectID();
      }

      if(_faceAndVerifyAction != nullptr)
      {
        _faceAndVerifyAction->PrepForCompletion();
      }

      if(_dockAnim != nullptr)
      {
        _dockAnim->PrepForCompletion();
      }
    }

    void IDockAction::OnRobotSet()
    {
      _dockingComponentPtr = &GetRobot().GetDockingComponent();
      _carryingComponentPtr = &GetRobot().GetCarryingComponent();
    }

    bool IDockAction::VerifyDockingComponentValid() const{
      if( _dockingComponentPtr == nullptr ) {
        // action may be getting destroyed before init
        ANKI_VERIFY(!HasRobot(),
                    "IDockAction.VerifyDockingComponentValid.DockingComponentNotSet","");
        return false;
      } else {
        return true;
      }
    }

    bool IDockAction::VerifyCarryingComponentValid() const{
      if( _carryingComponentPtr == nullptr ) {
        // action may be getting destroyed before init
        ANKI_VERIFY(!HasRobot(),
                    "IDockAction.VerifyCarryingComponentValid.CarryingComponentNotSet","");
        return false;
      } else {
        return true;
      }
    }

    void IDockAction::SetSpeedAndAccel(f32 speed_mmps, f32 accel_mmps2, f32 decel_mmps2)
    {
      _dockSpeed_mmps = speed_mmps;
      _dockAccel_mmps2 = accel_mmps2;
      _dockDecel_mmps2 = decel_mmps2;
      _motionProfileManuallySet = true;
    }

    void IDockAction::SetSpeed(f32 speed_mmps)
    {
      _dockSpeed_mmps = speed_mmps;
      _motionProfileManuallySet = true;
    }

    void IDockAction::SetAccel(f32 accel_mmps2, f32 decel_mmps2)
    {
      _dockAccel_mmps2 = accel_mmps2;
      _dockDecel_mmps2 = decel_mmps2;
      _motionProfileManuallySet = true;
    }

    bool IDockAction::SetMotionProfile(const PathMotionProfile& profile)
    {
      if( _motionProfileManuallySet ) {
        return false;
      }
      else {
        _dockSpeed_mmps = profile.dockSpeed_mmps;
        _dockAccel_mmps2 = profile.dockAccel_mmps2;
        _dockDecel_mmps2 = profile.dockDecel_mmps2;
        return true;
      }
    }


    void IDockAction::SetPlacementOffset(f32 offsetX_mm, f32 offsetY_mm, f32 offsetAngle_rad)
    {
      if(FLT_LT(offsetX_mm, -kMaxNegativeXPlacementOffset)) {
        DEV_ASSERT_MSG(false,
                       "IDockAction.SetPlacementOffset.InvalidOffset",
                       "x offset %f cannot be negative (through block)",
                       offsetX_mm);
        // for release set offset to 0 so that Cozmo doesn't look stupid plowing through a block
        offsetX_mm = 0;
      }
      _placementOffsetX_mm = offsetX_mm;
      _placementOffsetY_mm = offsetY_mm;
      _placementOffsetAngle_rad = offsetAngle_rad;
    }

    void IDockAction::SetPlaceOnGround(bool placeOnGround)
    {
      _placeObjectOnGroundIfCarrying = placeOnGround;
    }

    void IDockAction::SetPreActionPoseAngleTolerance(Radians angleTolerance)
    {
      _preActionPoseAngleTolerance = angleTolerance;
    }

    void IDockAction::SetPostDockLiftMovingAudioEvent(AudioMetaData::GameEvent::GenericEvent event)
    {
      _liftMovingAudioEvent = event;
    }

    ActionResult IDockAction::ComputePlacementApproachAngle(const Robot& robot,
                                                            const Pose3d& placementPose,
                                                            f32& approachAngle_rad)
    {
      const CarryingComponent& carryingComponentRef = robot.GetCarryingComponent();

      if (!carryingComponentRef.IsCarryingObject()) {
        LOG_INFO("ComputePlacementApproachAngle.NoCarriedObject", "");
        return ActionResult::NOT_CARRYING_OBJECT_ABORT;
      }

      // Get carried object
      const ObservableObject* object = robot.GetBlockWorld().GetLocatedObjectByID(carryingComponentRef.GetCarryingObjectID());
      if(nullptr == object)
      {
        PRINT_NAMED_WARNING("DriveToActions.ComputePlacementApproachAngle.NullObject",
                            "ObjectID=%d", carryingComponentRef.GetCarryingObjectID().GetValue());
        return ActionResult::BAD_OBJECT;
      }

      // Check that up axis of carried object and the desired placementPose are the same.
      // Otherwise, it's impossible for the robot to place it there!
      const AxisName targetUpAxis = placementPose.GetRotationMatrix().GetRotatedParentAxis<'Z'>();
      const AxisName currentUpAxis = object->GetPose().GetRotationMatrix().GetRotatedParentAxis<'Z'>();
      if (currentUpAxis != targetUpAxis) {
        PRINT_NAMED_WARNING("ComputePlacementApproachAngle.MismatchedUpAxes",
                            "Carried up axis: %d , target up axis: %d",
                            currentUpAxis, targetUpAxis);
        return ActionResult::MISMATCHED_UP_AXIS;
      }

      // Get pose of carried object wrt robot
      Pose3d poseObjectWrtRobot;
      if (!object->GetPose().GetWithRespectTo(robot.GetPose(), poseObjectWrtRobot)) {
        PRINT_NAMED_WARNING("ComputePlacementApproachAngle.FailedToComputeObjectWrtRobotPose", "");
        return ActionResult::BAD_POSE;
      }

      // Get pose of robot if the carried object were aligned with the placementPose and the robot was still carrying it
      Pose3d poseRobotIfPlacingObject = poseObjectWrtRobot.GetInverse();
      poseRobotIfPlacingObject.PreComposeWith(placementPose);

      approachAngle_rad = poseRobotIfPlacingObject.GetRotationMatrix().GetAngleAroundParentAxis<'Z'>().ToFloat();

      return ActionResult::SUCCESS;
    }

    void IDockAction::GetPreActionPoses(const Pose3d& robotPose,
                                        const CarryingComponent& carryingComp,
                                        BlockWorld& blockWorld,
                                        const PreActionPoseInput& input,
                                        PreActionPoseOutput& output)
    {
      const ActionableObject* dockObject          = input.object;
      PreActionPose::ActionType preActionPoseType = input.preActionPoseType;
      bool doNearPredockPoseCheck                 = input.doNearPreDockPoseCheck;
      Radians preActionPoseAngleTolerance         = input.preActionPoseAngleTolerance;
      f32 preDockPoseDistOffsetX_mm               = input.preDockPoseDistOffsetX_mm;

      std::vector<PreActionPose>& preActionPoses  = output.preActionPoses;
      size_t& closestIndex                        = output.closestIndex;
      Point2f& closestPoint                       = output.closestPoint;

      // Make sure the object we were docking with is not null
      if(dockObject == nullptr) {
        PRINT_NAMED_WARNING("IsCloseEnoughToPreActionPose.NullObject", "");
        output.actionResult = ActionResult::BAD_OBJECT;
        return;
      }

      if(dockObject->GetID() == carryingComp.GetCarryingObjectID())
      {
        PRINT_NAMED_WARNING("IsCloseEnoughToPreActionPose.CarryingSelectedObject",
                            "Robot is currently carrying action object with ID=%d",
                            dockObject->GetID().GetValue());
        output.actionResult = ActionResult::BAD_OBJECT;
        return;
      }

      // select the object so it shows up properly in viz
      blockWorld.SelectObject(dockObject->GetID());

      // Verify that we ended up near enough a PreActionPose of the right type
      std::vector<std::pair<Quad2f, ObjectID> > obstacles;
      blockWorld.GetObstacles(obstacles);

      LOG_DEBUG("IsCloseEnoughToPreActionPose.GetCurrentPreActionPoses",
                "Using preDockPoseOffset_mm %f and %s",
                preDockPoseDistOffsetX_mm,
                (doNearPredockPoseCheck ? "checking if near pose" : "NOT checking if near pose"));
      dockObject->GetCurrentPreActionPoses(preActionPoses,
                                           robotPose,
                                           {preActionPoseType},
                                           std::set<Vision::Marker::Code>(),
                                           obstacles,
                                           preDockPoseDistOffsetX_mm);

      const Pose3d& robotPoseParent = robotPose.GetParent();

      // If using approach angle remove any preAction poses that aren't close to the desired approach angle
      if(input.useApproachAngle)
      {
        for(auto iter = preActionPoses.begin(); iter != preActionPoses.end();)
        {
          Pose3d preActionPose;
          if(iter->GetPose().GetWithRespectTo(robotPoseParent, preActionPose) == false)
          {
            PRINT_NAMED_WARNING("IsCloseEnoughToPreActionPose.PreActionPoseOriginProblem",
                                "Could not get pre-action pose w.r.t. world origin.");
            iter = preActionPoses.erase(iter);
            continue;
          }

          Radians headingDiff = preActionPose.GetRotationAngle<'Z'>() - input.approachAngle_rad;
          // If the heading difference between our desired approach angle and the preAction pose's heading is
          // greater than 45 degrees this preAction pose will not be the one of the poses closest to approach angle
          if(FLT_GE(std::abs(headingDiff.ToFloat()), DEG_TO_RAD(45.f)))
          {
            iter = preActionPoses.erase(iter);
          }
          else
          {
            ++iter;
          }
        }
      }

      if(preActionPoses.empty()) {
        PRINT_NAMED_WARNING("IsCloseEnoughToPreActionPose.NoPreActionPoses",
                            "Action object with ID=%d returned no pre-action poses of the given type.",
                            dockObject->GetID().GetValue());
        output.actionResult = ActionResult::NO_PREACTION_POSES;
        return;
      }

      const Point2f currentXY(robotPose.GetTranslation().x(),
                              robotPose.GetTranslation().y());

      closestIndex = preActionPoses.size();
      float closestDistSq = std::numeric_limits<float>::max();

      for(size_t index = 0; index < preActionPoses.size(); ++index)
      {
        Pose3d preActionPose;
        if(preActionPoses[index].GetPose().GetWithRespectTo(robotPoseParent, preActionPose) == false)
        {
          PRINT_NAMED_WARNING("IsCloseEnoughToPreActionPose.PreActionPoseOriginProblem",
                              "Could not get pre-action pose w.r.t. world origin.");
          continue;
        }

        const Point2f preActionXY(preActionPose.GetTranslation().x(),
                                  preActionPose.GetTranslation().y());
        const Point2f dist = (currentXY - preActionXY);
        const float distSq = dist.LengthSq();

        LOG_DEBUG("IsCloseEnoughToPreActionPose.CheckPoint",
                  "considering point (%f, %f) dist = %f",
                  dist.x(), dist.y(),
                  dist.Length());

        if(distSq < closestDistSq)
        {
          closestPoint = dist.GetAbs();
          closestIndex  = index;
          closestDistSq = distSq;
        }
      }

      // If closestIndex was never changed
      if(closestIndex == preActionPoses.size())
      {
        PRINT_NAMED_WARNING("IDockAction.GetPreActionPose.NoClosestPose",
                            "Could not find a closest preAction pose for object %d",
                            dockObject->GetID().GetValue());
        output.actionResult = ActionResult::BAD_POSE;
        return;
      }

      LOG_INFO("IsCloseEnoughToPreActionPose.ClosestPoint",
               "Closest point (%f, %f) robot pose (%f, %f) dist = %f",
               preActionPoses[closestIndex].GetPose().GetTranslation().x(),
               preActionPoses[closestIndex].GetPose().GetTranslation().y(),
               currentXY.x(), currentXY.y(),
               closestPoint.Length());

      output.distThresholdUsed = ComputePreActionPoseDistThreshold(preActionPoses[closestIndex].GetPose(),
                                                                   dockObject->GetPose(),
                                                                   preActionPoseAngleTolerance);

      output.robotAtClosestPreActionPose = false;

      if(output.distThresholdUsed > 0)
      {
        if(closestPoint.AnyGT(output.distThresholdUsed))
        {
          // If we are checking that we are close enough to the preDock pose and our closestPoint is
          // outside the distThreshold then fail saying we are too far away
          // Otherwise we will succeed but robotAtClosestPreActionPose will stay false
          if(doNearPredockPoseCheck)
          {
            LOG_INFO("IsCloseEnoughToPreActionPose.TooFarFromGoal",
                     "Robot is too far from pre-action pose (%.1fmm, %.1fmm).",
                     closestPoint.x(), closestPoint.y());
            output.actionResult = ActionResult::DID_NOT_REACH_PREACTION_POSE;
            return;
          }
        }
        // Else closestPoint is within the distThreshold and if the angle of the closest preAction pose is within
        // preActionPoseAngleTolerance to the current angle of the robot then set robotAtClosestPreActionPose to true
        else
        {
          Pose3d p;
          preActionPoses[closestIndex].GetPose().GetWithRespectTo(robotPose, p);

          if(FLT_LT(std::abs(p.GetRotation().GetAngleAroundZaxis().ToFloat()), preActionPoseAngleTolerance.ToFloat()))
          {
            PRINT_CH_INFO("Actions",
                          "IsCloseEnoughToPreActionPose.AtClosestPreActionPose",
                          "Robot is close enough to closest preAction pose (%.1fmm, %.1fmm) with threshold (%.1fmm, %.1fmm)",
                          closestPoint.x(),
                          closestPoint.y(),
                          output.distThresholdUsed.x(),
                          output.distThresholdUsed.y());
            output.robotAtClosestPreActionPose = true;
          }
        }
      }

      output.actionResult = ActionResult::SUCCESS;
    }

    bool IDockAction::RemoveMatchingPredockPose(const Pose3d& pose, std::vector<Pose3d>& possiblePoses)
    {
      bool removed = false;
      for(auto iter = possiblePoses.begin(); iter != possiblePoses.end(); )
      {
        if(iter->IsSameAs(pose,
                          kSamePreactionPoseDistThresh_mm,
                          DEG_TO_RAD(kSamePreactionPoseAngleThresh_deg))) {
          iter = possiblePoses.erase(iter);
          removed = true;
        }
        else {
          ++iter;
        }
      }

      return removed;
    }

    void IDockAction::GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const
    {
      requests.insert({ VisionMode::Markers, EVisionUpdateFrequency::High });
    }

    ActionResult IDockAction::Init()
    {
      _waitToVerifyTimeSecs = -1.f;
      _curDockTrigger = AnimationTrigger::Count;

      // In case of action restart, need to reset the dock animation
      if(_dockAnim != nullptr)
      {
        _dockAnim->PrepForCompletion();
      }
      _dockAnim.reset(nullptr);

      ActionableObject* dockObject = dynamic_cast<ActionableObject*>(GetRobot().GetBlockWorld().GetLocatedObjectByID(_dockObjectID));

      if(dockObject == nullptr)
      {
        PRINT_NAMED_WARNING("IDockAction.NullDockObject", "Dock object is null");
        return ActionResult::BAD_OBJECT;
      }

      // Only set cube lights if the dock object is a light cube
      _shouldSetCubeLights = IsValidLightCube(dockObject->GetType(), false);

      PreActionPoseOutput preActionPoseOutput;

      if(_doNearPredockPoseCheck)
      {
        PreActionPoseInput preActionPoseInput(dockObject,
                                              GetPreActionType(),
                                              _doNearPredockPoseCheck,
                                              _preDockPoseDistOffsetX_mm,
                                              _preActionPoseAngleTolerance.ToFloat(),
                                              false, 0);

        GetPreActionPoses(GetRobot().GetPose(),
                          GetRobot().GetCarryingComponent(),
                          GetRobot().GetBlockWorld(),
                          preActionPoseInput,
                          preActionPoseOutput);

        if(preActionPoseOutput.actionResult != ActionResult::SUCCESS)
        {
          return preActionPoseOutput.actionResult;
        }
      }

      ActionResult result = SelectDockAction(dockObject);
      if(result != ActionResult::SUCCESS) {
        PRINT_NAMED_WARNING("IDockAction.Init.DockActionSelectionFailure", "");
        return result;
      }

      // Specify post-dock lift motion callback to play sound
      using namespace RobotInterface;
      auto liftSoundLambda = [this](const AnkiEvent<RobotToEngine>& event)
      {
        if(_curDockTrigger != _getOutDockTrigger)
        {
          _curDockTrigger = _getOutDockTrigger;

          // If _dockAnim is not null, cancel it so we can play
          // the get out anim
          if(_dockAnim != nullptr)
          {
            _dockAnim->Cancel();
            _dockAnim->PrepForCompletion();
          }
          if (ShouldPlayDockingAnimations() && _getOutDockTrigger != AnimationTrigger::Count) {
            _dockAnim.reset(new TriggerAnimationAction(_getOutDockTrigger));
            _dockAnim->SetRobot(&GetRobot());
          }
        }

        using GE = AudioMetaData::GameEvent::GenericEvent;
        if (_liftMovingAudioEvent != GE::Invalid)
        {
          // Check that the action matches the current action
          DockAction recvdAction = event.GetData().Get_movingLiftPostDock().action;
          if (_dockAction != recvdAction)
          {
            PRINT_NAMED_WARNING("IDockAction.MovingLiftPostDockHandler.ActionMismatch",
                                "Expected %u, got %u. Ignoring.",
                                (u32)_dockAction, (u32)recvdAction);
            return;
          }

          using GO = AudioMetaData::GameObjectType;
          GetRobot().GetAudioClient()->PostEvent(_liftMovingAudioEvent,
                                                 GO::Behavior);
        }
      };

      _liftLoadState = LiftLoadState::UNKNOWN;
      auto liftLoadLambda = [this](const AnkiEvent<RobotToEngine>& event)
      {
        _liftLoadState = event.GetData().Get_liftLoad().hasLoad ? LiftLoadState::HAS_LOAD : LiftLoadState::HAS_NO_LOAD;
      };

      _liftMovingSignalHandle = GetRobot().GetRobotMessageHandler()->Subscribe(RobotToEngineTag::movingLiftPostDock,
                                                                               liftSoundLambda);
      _liftLoadSignalHandle = GetRobot().GetRobotMessageHandler()->Subscribe(RobotToEngineTag::liftLoad, liftLoadLambda);

      if (GetRobot().HasExternalInterface() )
      {
        using namespace ExternalInterface;
        auto helper = MakeAnkiEventUtil(*(GetRobot().GetExternalInterface()), *this, _signalHandles);
        helper.SubscribeEngineToGame<MessageEngineToGameTag::RobotDeletedLocatedObject>();
      }

      const Vision::KnownMarker* dockMarkerPtr = nullptr;
      const Vision::KnownMarker* dockMarkerPtr2 = nullptr;
      _dockMarkerCode  = Vision::MARKER_INVALID; // clear until we grab them below
      _dockMarkerCode2 = Vision::MARKER_INVALID;

      if (_doNearPredockPoseCheck) {
        LOG_INFO("IDockAction.Init.BeginDockingFromPreActionPose",
                 "Robot is within (%.1fmm,%.1fmm) of the nearest pre-action pose, "
                 "proceeding with docking.", preActionPoseOutput.closestPoint.x(), preActionPoseOutput.closestPoint.y());

        // Set dock markers
        dockMarkerPtr = preActionPoseOutput.preActionPoses[preActionPoseOutput.closestIndex].GetMarker();
        dockMarkerPtr2 = GetDockMarker2(preActionPoseOutput.preActionPoses, preActionPoseOutput.closestIndex);

      } else {
        std::vector<const Vision::KnownMarker*> markers;
        dockObject->GetObservedMarkers(markers);

        if(markers.empty())
        {
          PRINT_NAMED_ERROR("IDockAction.Init.NoMarkers",
                            "Using currently observed markers instead of preDock pose but no currently visible marker");
          return ActionResult::VISUAL_OBSERVATION_FAILED;
        }
        else if(markers.size() == 1)
        {
          dockMarkerPtr = markers.front();
        }
        else
        {
          f32 distToClosestMarker = std::numeric_limits<f32>::max();
          for(const Vision::KnownMarker* marker : markers)
          {
            Pose3d p;
            if(!marker->GetPose().GetWithRespectTo(GetRobot().GetPose(), p))
            {
              LOG_INFO("IDockAction.Init.GetMarkerWRTRobot",
                       "Failed to get marker %s's pose wrt to robot",
                       marker->GetCodeName());
              continue;
            }

            if(p.GetTranslation().LengthSq() < distToClosestMarker*distToClosestMarker)
            {
              distToClosestMarker = p.GetTranslation().Length();
              dockMarkerPtr = marker;
            }
          }
        }
        LOG_INFO("IDockAction.Init.BeginDockingToMarker",
                 "Proceeding with docking to marker %s", dockMarkerPtr->GetCodeName());
      }

      if(dockMarkerPtr == nullptr)
      {
        PRINT_NAMED_WARNING("IDockAction.Init.NullDockMarker", "Dock marker is null returning failure");
        return ActionResult::BAD_MARKER;
      }

      // cache marker codes (required before SetupTurnAndVerifyAction)
      _dockMarkerCode  = (nullptr != dockMarkerPtr ) ? dockMarkerPtr->GetCode()  : Vision::MARKER_INVALID;
      _dockMarkerCode2 = (nullptr != dockMarkerPtr2) ? dockMarkerPtr2->GetCode() : Vision::MARKER_INVALID;

      SetupTurnAndVerifyAction(dockObject);

      if(_shouldSetCubeLights && !_lightsSet)
      {
        LOG_INFO("IDockAction.SetInteracting", "%s[%d] Setting interacting object to %d",
                 GetName().c_str(), GetTag(),
                 _dockObjectID.GetValue());
        GetRobot().GetCubeLightComponent().PlayLightAnimByTrigger(_dockObjectID, CubeAnimationTrigger::Interacting);
        _lightsSet = true;
      }

      // Allow actions the opportunity to check or set any properties they need to
      // this allows actions that are part of driveTo or wrappers a chance to check data
      // when they know they're at the pre-dock pose
      ActionResult internalActionResult = InitInternal();
      if(internalActionResult != ActionResult::SUCCESS){
        return internalActionResult;
      }

      // Go ahead and Update the FaceObjectAction once now, so we don't
      // waste a tick doing so in CheckIfDone (since this is the first thing
      // that will be done in CheckIfDone anyway)
      ActionResult faceObjectResult = _faceAndVerifyAction->Update();

      if(ActionResult::SUCCESS == faceObjectResult ||
         ActionResult::RUNNING == faceObjectResult)
      {
        return ActionResult::SUCCESS;
      } else {
        return faceObjectResult;
      }
    } // Init()

    ActionResult IDockAction::CheckIfDone()
    {
      ActionResult actionResult = ActionResult::RUNNING;

      if(_dockObjectID.IsUnknown())
      {
        return ActionResult::BAD_OBJECT;
      }

      // Wait for visual verification to complete successfully before telling
      // robot to dock and continuing to check for completion
      if(_faceAndVerifyAction != nullptr) {
        actionResult = _faceAndVerifyAction->Update();
        if(actionResult == ActionResult::RUNNING) {
          return actionResult;
        } else {
          if(actionResult == ActionResult::SUCCESS) {
            // Finished with visual verification:
            _faceAndVerifyAction.reset();
            actionResult = ActionResult::RUNNING;

            LOG_INFO("IDockAction.DockWithObjectHelper.BeginDocking",
                     "Docking with marker %d (%s) using action %s.",
                     _dockMarkerCode, Vision::Marker::GetNameForCode(_dockMarkerCode), DockActionToString(_dockAction));
            if(VerifyDockingComponentValid() &&
               _dockingComponentPtr->DockWithObject(_dockObjectID,
                                                    _dockSpeed_mmps,
                                                    _dockAccel_mmps2,
                                                    _dockDecel_mmps2,
                                                    _dockMarkerCode,
                                                    _dockMarkerCode2,
                                                    _dockAction,
                                                    _placementOffsetX_mm,
                                                    _placementOffsetY_mm,
                                                    _placementOffsetAngle_rad,
                                                    _numDockingRetries,
                                                    _dockingMethod,
                                                    _doLiftLoadCheck,
                                                    _backUpWhileLiftingCube) == RESULT_OK)
            {
              //NOTE: Any completion (success or failure) after this point should tell
              // the robot to stop tracking and go back to looking for markers!
              _wasPickingOrPlacing = false;
            } else {
              return ActionResult::SEND_MESSAGE_TO_ROBOT_FAILED;
            }

          } else {
            PRINT_NAMED_WARNING("IDockAction.CheckIfDone.VisualVerifyFailed",
                                "VisualVerification of object failed, stopping IDockAction.");
            return actionResult;
          }
        }
      }

      if (!_wasPickingOrPlacing && VerifyDockingComponentValid()) {
        // We have to see the robot went into pick-place mode once before checking
        // to see that it has finished picking or placing below. I.e., we need to
        // know the robot got the DockWithObject command sent in Init().
        _wasPickingOrPlacing = _dockingComponentPtr->IsPickingOrPlacing();

        if(_wasPickingOrPlacing && ShouldPlayDockingAnimations())
        {
          // If we haven't started playing any dock anim triggers, play the get in
          if(_curDockTrigger == AnimationTrigger::Count)
          {
            _curDockTrigger = _getInDockTrigger;

            if(_curDockTrigger != AnimationTrigger::Count)
            {
              // Init docking anim
              _dockAnim.reset(new TriggerAnimationAction(_getInDockTrigger));
              _dockAnim->SetRobot(&GetRobot());
            }

            UpdateDockingAnim();
          }
        }
      }
      else if (VerifyDockingComponentValid() &&
               !_dockingComponentPtr->IsPickingOrPlacing() &&
               !GetRobot().GetMoveComponent().IsMoving())
      {
        const f32 currentTime = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();

        // While head is moving to verification angle, this shouldn't count towards the waitToVerifyTime
        if (GetRobot().GetMoveComponent().IsHeadMoving()) {
          _waitToVerifyTimeSecs = -1;
        }

        // Set the verification time if not already set
        if(_waitToVerifyTimeSecs < 0.f) {
          _waitToVerifyTimeSecs = currentTime + GetVerifyDelayInSeconds();
        }

        // Stopped executing docking path, and should have backed out by now,
        // and have head pointed at an angle to see where we just placed or
        // picked up from. So we will check if we see a block with the same
        // ID/Type as the one we were supposed to be picking or placing, in the
        // right position.
        if(currentTime >= _waitToVerifyTimeSecs) {
          //LOG_INFO("IDockAction.CheckIfDone",
          //              "Robot has stopped moving and picking/placing. Will attempt to verify success.");

          actionResult = Verify();
        }
      }
      else
      {
        // If dock anim is null then it means the get in finished so time to start the loop
        if(_dockAnim == nullptr && ShouldPlayDockingAnimations())
        {
          _curDockTrigger = _loopDockTrigger;

          if(_curDockTrigger != AnimationTrigger::Count)
          {
            _dockAnim.reset(new TriggerAnimationAction(_loopDockTrigger));
            _dockAnim->SetRobot(&GetRobot());
          }
        }

        // Still docking so update dock anim
        UpdateDockingAnim();
      }

      return actionResult;
    } // CheckIfDone()


    void IDockAction::SetupTurnAndVerifyAction(const ObservableObject* dockObject)
    {
      _faceAndVerifyAction.reset(new CompoundActionSequential());
      _faceAndVerifyAction->ShouldSuppressTrackLocking(true);
      _faceAndVerifyAction->SetRobot(&GetRobot());

      if(_firstTurnTowardsObject)
      {
        // Set up a visual verification action to make sure we can still see the correct
        // marker of the selected object before proceeding
        // NOTE: This also disables tracking head to object if there was any
        IAction* turnTowardsDockObjectAction = new TurnTowardsObjectAction(_dockObjectID,
                                                                           (_visuallyVerifyObjectOnly ? Vision::Marker::ANY_CODE : _dockMarkerCode),
                                                                           0, true, false);

        // Disable the turn towards action from issuing a completion signal
        turnTowardsDockObjectAction->ShouldSuppressTrackLocking(true);

        _faceAndVerifyAction->AddAction(turnTowardsDockObjectAction);
      }
    }

    void IDockAction::UpdateDockingAnim()
    {
      if(_dockAnim != nullptr)
      {
        const ActionResult res = _dockAnim->Update();
        const ActionResultCategory resCat = IActionRunner::GetActionResultCategory(res);
        // If dock animation isn't running (failed or completed)
        if(resCat != ActionResultCategory::RUNNING)
        {
          // If dock animation action failed print warning
          if(resCat != ActionResultCategory::SUCCESS)
          {
            PRINT_NAMED_WARNING("IDockAction.UpdateDockingAnim.AnimFailed",
                                "%s [%d]'s dock anim %s [%d] failed %s",
                                GetName().c_str(),
                                GetTag(),
                                _dockAnim->GetName().c_str(),
                                _dockAnim->GetTag(),
                                EnumToString(res));

          }

          _dockAnim->PrepForCompletion();
          _dockAnim.reset(nullptr);
        }
      }
    }

    template<>
    void IDockAction::HandleMessage(const ExternalInterface::RobotDeletedLocatedObject& msg)
    {
      if(msg.objectID == _dockObjectID)
      {
        LOG_INFO("IDockAction.RobotDeletedLocatedObject",
                 "Dock object was deleted from current origin stopping dock action");
        _dockObjectID.UnSet();
      }
    }

    void IDockAction::SetDockAnimations(const AnimationTrigger& getIn,
                                        const AnimationTrigger& loop,
                                        const AnimationTrigger& getOut)
    {
      _getInDockTrigger  = getIn;
      _loopDockTrigger   = loop;
      _getOutDockTrigger = getOut;
    }

#pragma mark ---- PopAWheelieAction ----

    PopAWheelieAction::PopAWheelieAction(ObjectID objectID)
    : IDockAction(objectID,
                  "PopAWheelie",
                  RobotActionType::POP_A_WHEELIE)
    {

    }

    void PopAWheelieAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;
      switch(_dockAction)
      {
        case DockAction::DA_POP_A_WHEELIE:
        {
          if(VerifyCarryingComponentValid() && _carryingComponentPtr->IsCarryingObject()) {
            PRINT_NAMED_WARNING("PopAWheelieAction.EmitCompletionSignal.ExpectedNotCarryingObject", "");
          } else {
            info.objectID = _dockObjectID;
          }
          break;
        }
        default:
        {
          if(GetState() != ActionResult::NOT_STARTED)
          {
            PRINT_NAMED_WARNING("PopAWheelieAction.EmitCompletionSignal.DockActionNotSet",
                                "Dock action not set before filling completion signal.");
          }
        }
      }
      completionUnion.Set_objectInteractionCompleted(std::move( info ));
      IDockAction::GetCompletionUnion(completionUnion);
    }

    ActionResult PopAWheelieAction::SelectDockAction(ActionableObject* object)
    {
      Pose3d objectPose;
      if(object->GetPose().GetWithRespectTo(GetRobot().GetPose().GetParent(), objectPose) == false) {
        PRINT_NAMED_WARNING("PopAWheelieAction.SelectDockAction.PoseWrtFailed",
                            "Could not get pose of dock object w.r.t. robot's parent.");
        return ActionResult::BAD_OBJECT;
      }

      // Choose docking action based on block's position and whether we are
      // carrying a block
      const f32 dockObjectHeightWrtRobot = objectPose.GetTranslation().z() - GetRobot().GetPose().GetTranslation().z();
      _dockAction = DockAction::DA_POP_A_WHEELIE;


      // TODO: Stop using constant ROBOT_BOUNDING_Z for this
      // TODO: There might be ways to roll high blocks when not carrying object and low blocks when carrying an object.
      //       Do them later.
      if (dockObjectHeightWrtRobot > 0.5f*ROBOT_BOUNDING_Z) { //  dockObject->GetSize().z()) {
        LOG_INFO("PopAWheelieAction.SelectDockAction.ObjectTooHigh", "Object is too high to pop-a-wheelie. Aborting.");
        return ActionResult::BAD_OBJECT;
      } else if (VerifyCarryingComponentValid() && _carryingComponentPtr->IsCarryingObject()) {
        LOG_INFO("PopAWheelieAction.SelectDockAction.CarryingObject", "");
        return ActionResult::STILL_CARRYING_OBJECT;
      }

      return ActionResult::SUCCESS;
    } // SelectDockAction()

    ActionResult PopAWheelieAction::Verify()
    {
      ActionResult result = ActionResult::ABORT;

      switch(_dockAction)
      {
        case DockAction::DA_POP_A_WHEELIE:
        {
          if(VerifyDockingComponentValid() && _dockingComponentPtr->GetLastPickOrPlaceSucceeded()) {
            // Check that the robot is sufficiently pitched up
            if (GetRobot().GetPitchAngle() < 1.f) {
              LOG_INFO("PopAWheelieAction.Verify.PitchAngleTooSmall",
                       "Robot pitch angle expected to be higher (measured %f rad)",
                       GetRobot().GetPitchAngle().ToDouble());
              result = ActionResult::UNEXPECTED_PITCH_ANGLE;
            } else {
              result = ActionResult::SUCCESS;
            }

          } else {
            // If the robot thinks it failed last pick-and-place, it is because it
            // failed to dock/track.
            LOG_INFO("PopAWheelieAction.Verify.DockingFailed",
                     "Robot reported pop-a-wheelie failure. Assuming docking failed");
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }

          break;
        } // DA_POP_A_WHEELIE


        default:
          PRINT_NAMED_WARNING("PopAWheelieAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      return result;

    } // Verify()


#pragma mark ---- FacePlantAction ----

    FacePlantAction::FacePlantAction(ObjectID objectID)
    : IDockAction(objectID, "FacePlant", RobotActionType::FACE_PLANT)
    {
    }

    void FacePlantAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;
      switch(_dockAction)
      {
        case DockAction::DA_FACE_PLANT:
        {
          if(VerifyCarryingComponentValid() && _carryingComponentPtr->IsCarryingObject()) {
            PRINT_NAMED_WARNING("FacePlantAction.EmitCompletionSignal.ExpectedNotCarryingObject", "");
          } else {
            info.objectID = _dockObjectID;
          }
          break;
        }
        default:
          PRINT_NAMED_WARNING("FacePlantAction.EmitCompletionSignal.DockActionNotSet",
                              "Dock action not set before filling completion signal.");
      }
      completionUnion.Set_objectInteractionCompleted(std::move( info ));
      IDockAction::GetCompletionUnion(completionUnion);
    }

    ActionResult FacePlantAction::SelectDockAction(ActionableObject* object)
    {
      Pose3d objectPose;
      if(object->GetPose().GetWithRespectTo(GetRobot().GetPose().GetParent(), objectPose) == false) {
        PRINT_NAMED_WARNING("FacePlantAction.SelectDockAction.PoseWrtFailed",
                            "Could not get pose of dock object w.r.t. robot's parent.");
        return ActionResult::BAD_OBJECT;
      }

      const f32 dockObjectHeightWrtRobot = objectPose.GetTranslation().z() - GetRobot().GetPose().GetTranslation().z();
      _dockAction = DockAction::DA_FACE_PLANT;

      // TODO: Stop using constant ROBOT_BOUNDING_Z for this
      if (dockObjectHeightWrtRobot > 0.5f*ROBOT_BOUNDING_Z) { //  dockObject->GetSize().z()) {
        LOG_INFO("FacePlantAction.SelectDockAction.ObjectTooHigh", "");
        return ActionResult::BAD_OBJECT;
      }

      if (VerifyCarryingComponentValid() &&
          _carryingComponentPtr->IsCarryingObject()) {
        LOG_INFO("FacePlantAction.SelectDockAction.CarryingObject", "");
        return ActionResult::STILL_CARRYING_OBJECT;
      }

      return ActionResult::SUCCESS;
    } // SelectDockAction()

    ActionResult FacePlantAction::Verify()
    {
      ActionResult result = ActionResult::ABORT;

      switch(_dockAction)
      {
        case DockAction::DA_FACE_PLANT:
        {
          if(VerifyDockingComponentValid() && _dockingComponentPtr->GetLastPickOrPlaceSucceeded()) {
            // Check that the robot is sufficiently pitched down
            if (GetRobot().GetPitchAngle() > kMaxSuccessfulPitchAngle_rad) {
              LOG_INFO("FacePlantAction.Verify.PitchAngleTooSmall",
                       "Robot pitch angle expected to be lower (measured %f deg)",
                       GetRobot().GetPitchAngle().getDegrees() );
              result = ActionResult::UNEXPECTED_PITCH_ANGLE;
            } else {
              result = ActionResult::SUCCESS;
            }

          } else {
            // If the robot thinks it failed last pick-and-place, it is because it
            // failed to dock/track.
            LOG_INFO("FacePlantAction.Verify.DockingFailed",
                     "Robot reported face plant failure. Assuming docking failed");
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }

          break;
        } // DA_FACE_PLANT


        default:
          PRINT_NAMED_WARNING("FacePlantAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      return result;

    } // Verify()


#pragma mark ---- AlignWithObjectAction ----

    PreActionPose::ActionType AlignWithObjectAction::GetPreActionTypeFromAlignmentType(AlignmentType alignmentType) {
      switch (alignmentType) {
        case AlignmentType::LIFT_FINGER:
          return PreActionPose::ActionType::PLACE_RELATIVE;
        case AlignmentType::LIFT_PLATE:
          // Assumption is that robot is setting up for pickup so only dockable
          // sides should be considered
          return PreActionPose::ActionType::DOCKING;
        case AlignmentType::BODY:
          return PreActionPose::ActionType::PLACE_RELATIVE;
        case AlignmentType::CUSTOM:
          // Normally this action uses the DOCKING preAction poses but if we are aligning
          // to a custom distance then the DOCKING poses could be too close so use the PLACE_RELATIVE
          // preAction poses. Plus, we want to be able to align with non-pickupable sides.
          return PreActionPose::ActionType::PLACE_RELATIVE;
        default:
          PRINT_NAMED_ERROR("AlignWithObjectAction.GetPreActionTypeByAlignmentType.InvalidAlignmentType", "%s", EnumToString(alignmentType));
          return PreActionPose::ActionType::PLACE_RELATIVE;
      }
    }


    AlignWithObjectAction::AlignWithObjectAction(ObjectID objectID,
                                                 const f32 distanceFromMarker_mm,
                                                 const AlignmentType alignmentType)
    : IDockAction(objectID,
                  "AlignWithObject",
                  RobotActionType::ALIGN_WITH_OBJECT)
    , _alignmentType(alignmentType)
    {
      f32 distance = 0;
      switch(alignmentType)
      {
        case(AlignmentType::LIFT_FINGER):
        {
          distance = kLiftFingerDistanceOffset_mm;
          break;
        }
        case(AlignmentType::LIFT_PLATE):
        {
          distance = 0;

          // If we are aligning to the LIFT_PLATE then assume that we want the lift fingers in the
          // object grooves (as if to pickup the object) so use the same docking method as pickup
          _dockingMethod = (DockingMethod)kPickupDockingMethod;
          break;
        }
        case(AlignmentType::BODY):
        {
          distance = kBodyDistanceOffset_mm;
          break;
        }
        case(AlignmentType::CUSTOM):
        {
          distance = distanceFromMarker_mm - kCustomDistanceOffset_mm;
          break;
        }
      }
      _preActionPoseActionType = GetPreActionTypeFromAlignmentType(alignmentType);
      SetPlacementOffset(distance, 0, 0);
    }

    AlignWithObjectAction::~AlignWithObjectAction()
    {

    }

    void AlignWithObjectAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;
      info.objectID = _dockObjectID;
      completionUnion.Set_objectInteractionCompleted(std::move( info ));

      IDockAction::GetCompletionUnion(completionUnion);
    }


    ActionResult AlignWithObjectAction::SelectDockAction(ActionableObject* object)
    {
      _dockAction = DockAction::DA_ALIGN;

      // If we are aligning to the LIFT_PLATE then assume that we want the lift fingers in the
      // object grooves (as if to pickup the object) so use a special align dock action
      // which basically functions the same as pickup (does the Hanns Manuever)
      // except doesn't move the lift
      if(_alignmentType == AlignmentType::LIFT_PLATE)
      {
        _dockAction = DockAction::DA_ALIGN_SPECIAL;
      }

      return ActionResult::SUCCESS;
    } // SelectDockAction()


    ActionResult AlignWithObjectAction::Verify()
    {
      ActionResult result = ActionResult::ABORT;

      switch(_dockAction)
      {
        case DockAction::DA_ALIGN:
        case DockAction::DA_ALIGN_SPECIAL:
        {
          if(VerifyDockingComponentValid() && _dockingComponentPtr->IsPickingOrPlacing())
          {
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }
          else if(GetRobot().GetPathComponent().IsActive())
          {
            result = ActionResult::FAILED_TRAVERSING_PATH;
          }
          else if(VerifyDockingComponentValid() &&
                  !_dockingComponentPtr->GetLastPickOrPlaceSucceeded())
          {
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }
          else
          {
            LOG_INFO("AlignWithObjectAction.Verify", "Align with object SUCCEEDED!");
            result = ActionResult::SUCCESS;
          }
          break;
        } // ALIGN

        default:
          PRINT_NAMED_WARNING("AlignWithObjectAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      return result;

    } // Verify()

#pragma mark ---- PickupObjectAction ----

    PickupObjectAction::PickupObjectAction(ObjectID objectID)
    : IDockAction(objectID,
                  "PickupObject",
                  RobotActionType::PICK_AND_PLACE_INCOMPLETE)
    {
      _dockingMethod = (DockingMethod)kPickupDockingMethod;
      using GE = AudioMetaData::GameEvent::GenericEvent;
      SetPostDockLiftMovingAudioEvent(GE::Play__Robot_Vic_Sfx__Lift_High_Up_Short_Excited);

      _doLiftLoadCheck = true; // Do lift load check by default
    }

    PickupObjectAction::~PickupObjectAction()
    {
      if(_verifyAction != nullptr)
      {
        _verifyAction->PrepForCompletion();
      }
    }

    void PickupObjectAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;

      switch(_dockAction)
      {
        case DockAction::DA_PICKUP_HIGH:
        case DockAction::DA_PICKUP_LOW:
        {
          if(VerifyCarryingComponentValid() && !_carryingComponentPtr->IsCarryingObject()) {
            LOG_INFO("PickupObjectAction.GetCompletionUnion.ExpectedCarryingObject", "");
          } else if(VerifyCarryingComponentValid()) {
            info.objectID = _dockObjectID;
          }
          break;
        }
        default:
        {
          // Not setting dock action is only an issue if the action has started
          if(GetState() != ActionResult::NOT_STARTED)
          {
            PRINT_NAMED_WARNING("PickupObjectAction.EmitCompletionSignal.DockActionNotSet",
                                "Dock action not set before filling completion signal");
          }
        }
      }

      completionUnion.Set_objectInteractionCompleted(std::move( info ));
    }

    ActionResult PickupObjectAction::SelectDockAction(ActionableObject* object)
    {
      // Record the object's original pose (before picking it up) so we can
      // verify later whether we succeeded.
      // Make it w.r.t. robot's parent so we can compare heights fairly.
      if(object->GetPose().GetWithRespectTo(GetRobot().GetPose().GetParent(), _dockObjectOrigPose) == false) {
        PRINT_NAMED_WARNING("PickupObjectAction.SelectDockAction.PoseWrtFailed",
                            "Could not get pose of dock object w.r.t. robot parent.");
        return ActionResult::BAD_OBJECT;
      }

      // Choose docking action based on block's position and whether we are
      // carrying a block
      const f32 dockObjectHeightWrtRobot = _dockObjectOrigPose.GetTranslation().z() - GetRobot().GetPose().GetTranslation().z();
      _dockAction = DockAction::DA_PICKUP_LOW;
      SetType(RobotActionType::PICKUP_OBJECT_LOW);

      if (VerifyCarryingComponentValid() &&
          _carryingComponentPtr->IsCarryingObject()) {
        LOG_INFO("PickupObjectAction.SelectDockAction.CarryingObject", "Already carrying object. Can't pickup object. Aborting.");
        return ActionResult::STILL_CARRYING_OBJECT;
      } else if (dockObjectHeightWrtRobot > 0.5f*ROBOT_BOUNDING_Z) { // TODO: Stop using constant ROBOT_BOUNDING_Z for this
        _dockAction = DockAction::DA_PICKUP_HIGH;
        SetType(RobotActionType::PICKUP_OBJECT_HIGH);
      }

      // If we are either in the habitat or unsure, we should do the version of cube pickup where instead of driving
      // forward at the same time as raising the lift, we drive backward. This improves the cube pickup success rate
      // in case the cube is pressed against the wall of the habitat.
      const auto habitatBeliefState = GetRobot().GetComponent<HabitatDetectorComponent>().GetHabitatBeliefState();
      const bool possiblyInHabitat = (habitatBeliefState == HabitatBeliefState::InHabitat) ||
                                     (habitatBeliefState == HabitatBeliefState::Unknown);
      SetBackUpWhileLiftingCube(possiblyInHabitat);

      return ActionResult::SUCCESS;
    } // SelectDockAction()

    ActionResult PickupObjectAction::Verify()
    {
      ActionResult result = ActionResult::ABORT;
      const RobotTimeStamp_t currentTime = GetRobot().GetLastMsgTimestamp();

      if (_firstVerifyCallTime == 0) {
        _firstVerifyCallTime = currentTime;
      }

      if (VerifyDockingComponentValid() &&
          _dockingComponentPtr->GetLastPickOrPlaceSucceeded()) {

        bool checkObjectMotion = false;
        
        // Determine whether or not we should do a SearchForNearbyObject instead of TurnTowardsPose
        // depending on if the liftLoad test resulted in HAS_NO_LOAD since this could be due to sticky lift.
        if (_doLiftLoadCheck) {
          if (_liftLoadState == LiftLoadState::UNKNOWN) {
            // If liftLoad message hasn't come back yet, wait a little longer
            if (_liftLoadWaitTime_ms == 0) {
              _liftLoadWaitTime_ms = currentTime + kLiftLoadTimeout_ms;
              return ActionResult::RUNNING;
            } else if (currentTime > _liftLoadWaitTime_ms) {
              // If LiftLoadCheck times out for some reason -- lift probably just couldn't get into
              // position fast enough -- then just proceed to motion check.
              PRINT_NAMED_WARNING("PickupObjectAction.Verify.LiftLoadTimeout", "");
              checkObjectMotion = true;
            } else {
              return ActionResult::RUNNING;
            }
          } else if (_liftLoadState == LiftLoadState::HAS_NO_LOAD) {
            checkObjectMotion = true;
          }
        } else {
          // If not doing liftLoadCheck, at least do motion check
          checkObjectMotion = true;
        }

        // If the liftLoadCheck failed then look at lastMoved time.
        // Assuming that the robot stopping coincides closely with the first call to Verify().
        // If the cube is moving too long after the first call to Verify() the cube is probably in someone's hand.
        // If it hasn't moved at all for some period before the first call to Verify() the cube probably
        // wasn't in the lift during pickup.
        if (checkObjectMotion) {
          BlockWorld& blockWorld = GetRobot().GetBlockWorld();
          ObservableObject* obj = blockWorld.GetLocatedObjectByID(_dockObjectID);
          if (nullptr == obj) {
            PRINT_NAMED_WARNING("PickupObjectAction.Verify.nullObject", "ObjectID %d", _dockObjectID.GetValue());
            return ActionResult::BAD_OBJECT;
          }

          // Only do this motion check if connected
          if (obj->GetActiveID() >= 0) {
            RobotTimeStamp_t lastMovingTime;

            // Check that object is not moving for longer than expected following the first call to Verify().
            // If it's moving for too long it's probably being handled by someone.
            if (obj->IsMoving(&lastMovingTime)) {
              if (VerifyCarryingComponentValid() &&
                  (currentTime > _firstVerifyCallTime + kMaxObjectStillMovingAfterRobotStopTime_ms)) {
                _carryingComponentPtr->SetCarriedObjectAsUnattached(true);
                LOG_INFO("PickupObjectAction.Verify.ObjectStillMoving", "");
                return ActionResult::PICKUP_OBJECT_UNEXPECTEDLY_MOVING;
              }
              return ActionResult::RUNNING;
            }

            // Check that the object has moved at all in certain time window before we started calling Verify().
            // If it hasn't moved at all we probably missed, note the outcome and retry.
            else if (VerifyCarryingComponentValid() &&
                     (_firstVerifyCallTime > lastMovingTime + (_dockAction == DockAction::DA_PICKUP_LOW ? kMaxObjectHasntMovedBeforeRobotStopTime_ms : kMaxObjectHasntMovedBeforeRobotStopTimeForHighPickup_ms))) {
              _carryingComponentPtr->SetCarriedObjectAsUnattached(true);
              LOG_INFO("PickupObjectAction.Verify.ObjectDidntMoveAsExpected", "lastMovedTime %d, firstTime: %d", (TimeStamp_t)lastMovingTime, (TimeStamp_t)_firstVerifyCallTime);
              return ActionResult::PICKUP_OBJECT_UNEXPECTEDLY_NOT_MOVING;
            }
          }
        }

      } // if (GetRobot().GetLastPickOrPlaceSucceeded())


      if(_verifyAction == nullptr)
      {
        _verifyAction.reset(new VisuallyVerifyNoObjectAtPoseAction(_dockObjectOrigPose));
        _verifyAction->ShouldSuppressTrackLocking(true);
        _verifyAction->SetRobot(&GetRobot());
        _verifyActionDone = false;
      }

      if(!_verifyActionDone)
      {
        ActionResult res = _verifyAction->Update();
        if(res != ActionResult::RUNNING)
        {
          _verifyActionDone = true;
        }
        else
        {
          return ActionResult::RUNNING;
        }
      }

      switch(_dockAction)
      {
        case DockAction::DA_PICKUP_LOW:
        case DockAction::DA_PICKUP_HIGH:
        {
          if(VerifyCarryingComponentValid() &&
             (_carryingComponentPtr->IsCarryingObject() == false)) {
            PRINT_NAMED_WARNING("PickupObjectAction.Verify.ExpectedCarryingObject",
                                "Expecting robot to think it's carrying an object at this point.");
            result = ActionResult::NOT_CARRYING_OBJECT_RETRY;
            break;
          }

          BlockWorld& blockWorld = GetRobot().GetBlockWorld();

          // We should _not_ still see an object with the
          // same type as the one we were supposed to pick up in that
          // block's original position because we should now be carrying it.
          ObservableObject* carryObject = nullptr;
          if(VerifyCarryingComponentValid()){
            carryObject = blockWorld.GetLocatedObjectByID(_carryingComponentPtr->GetCarryingObjectID());
          }

          if(carryObject == nullptr) {
            PRINT_NAMED_WARNING("PickupObjectAction.Verify.CarryObjectNoLongerExists",
                                "Object %d we were carrying no longer exists in the world.",
                                VerifyCarryingComponentValid() ? _carryingComponentPtr->GetCarryingObjectID().GetValue() : -1);
            result = ActionResult::BAD_OBJECT;
            break;
          }

          BlockWorldFilter filter;
          filter.SetAllowedTypes({carryObject->GetType()});
          std::vector<ObservableObject*> objectsWithType;
          blockWorld.FindLocatedMatchingObjects(filter, objectsWithType);

          // Robot's pose parent could have changed due to delocalization.
          // Assume it's actual pose is relatively accurate w.r.t. that original
          // pose (when dockObjectOrigPose was stored) and update the parent so
          // that we can do IsSameAs checks below.
          _dockObjectOrigPose.SetParent(GetRobot().GetPose().GetParent());

          Radians angleDiff;
          for(const auto& object : objectsWithType)
          {
            // TODO: is it safe to always have useAbsRotation=true here?
            Vec3f Tdiff;
            Radians angleDiff;
            if(object->GetPose().IsSameAs_WithAmbiguity(_dockObjectOrigPose, // dock obj orig pose is w.r.t. robot
                                                        carryObject->GetRotationAmbiguities(),
                                                        carryObject->GetSameDistanceTolerance()*0.5f,
                                                        carryObject->GetSameAngleTolerance(),
                                                        Tdiff, angleDiff))
            {
              LOG_INFO("PickupObjectAction.Verify.ObjectInOrigPose",
                       "Seeing object %d in original pose. (Tdiff = (%.1f,%.1f,%.1f), "
                       "AngleDiff=%.1fdeg), carrying object %d",
                       object->GetID().GetValue(),
                       Tdiff.x(), Tdiff.y(), Tdiff.z(), angleDiff.getDegrees(),
                       carryObject->GetID().GetValue());
              break;
            }
          }

          LOG_INFO("PickupObjectAction.Verify.Success", "Object pick-up SUCCEEDED!");
          result = ActionResult::SUCCESS;
          break;
        } // PICKUP

        default:
          PRINT_NAMED_WARNING("PickupObjectAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      if( result == ActionResult::SUCCESS ) {
        GetRobot().GetComponent<RobotStatsTracker>().IncrementBehaviorStat(BehaviorStat::PickedUpCube);
      }

      return result;

    } // Verify()

#pragma mark ---- PlaceObjectOnGroundAction ----

    PlaceObjectOnGroundAction::PlaceObjectOnGroundAction()
    : IAction("PlaceObjectOnGround",
              RobotActionType::PLACE_OBJECT_LOW,
              ((u8)AnimTrackFlag::LIFT_TRACK |
               (u8)AnimTrackFlag::BODY_TRACK |
               (u8)AnimTrackFlag::HEAD_TRACK))
    {

    }

    PlaceObjectOnGroundAction::~PlaceObjectOnGroundAction()
    {
      if(_faceAndVerifyAction != nullptr)
      {
        _faceAndVerifyAction->PrepForCompletion();
      }
    }

    ActionResult PlaceObjectOnGroundAction::Init()
    {
      CarryingComponent& carryingComponentRef = GetRobot().GetCarryingComponent();
      ActionResult result = ActionResult::RUNNING;

      _startedPlacing = false;

      // Robot must be carrying something to put something down!
      if(carryingComponentRef.IsCarryingObject() == false) {
        PRINT_NAMED_WARNING("PlaceObjectOnGroundAction.CheckPreconditions.NotCarryingObject",
                            "Executing PlaceObjectOnGroundAction but not carrying object.");
        result = ActionResult::NOT_CARRYING_OBJECT_ABORT;
      } else {

        _carryingObjectID  = carryingComponentRef.GetCarryingObjectID();

        if(carryingComponentRef.PlaceObjectOnGround() == RESULT_OK)
        {
          result = ActionResult::SUCCESS;
        } else {
          PRINT_NAMED_WARNING("PlaceObjectOnGroundAction.CheckPreconditions.SendPlaceObjectOnGroundFailed",
                              "Robot's SendPlaceObjectOnGround method reported failure.");
          result = ActionResult::SEND_MESSAGE_TO_ROBOT_FAILED;
        }

        const Vision::KnownMarker::Code carryObjectMarkerCode = carryingComponentRef.GetCarryingMarkerCode();
        _faceAndVerifyAction.reset(new TurnTowardsObjectAction(_carryingObjectID,
                                                               carryObjectMarkerCode,
                                                               0, true, false));
        _faceAndVerifyAction->SetRobot(&GetRobot());
        _faceAndVerifyAction->ShouldSuppressTrackLocking(true);

      } // if/else IsCarryingObject()

      // If we were moving, stop moving.
      GetRobot().GetMoveComponent().StopAllMotors();

      _startedPlacing = false;

      return result;

    } // CheckPreconditions()

    ActionResult PlaceObjectOnGroundAction::CheckIfDone()
    {
      ActionResult actionResult = ActionResult::RUNNING;

      // Wait for robot to report it is done picking/placing and that it's not
      // moving

      const bool isPickingAndPlacing = GetRobot().GetDockingComponent().IsPickingOrPlacing();

      if(isPickingAndPlacing)
      {
        _startedPlacing = true;
      }

      if (_startedPlacing &&
          !isPickingAndPlacing &&
          !GetRobot().GetMoveComponent().IsMoving())
      {
        // Stopped executing docking path, and should have placed carried block
        // and backed out by now, and have head pointed at an angle to see
        // where we just placed or picked up from.
        // So we will check if we see a block with the same
        // ID/Type as the one we were supposed to be picking or placing, in the
        // right position.

        actionResult = _faceAndVerifyAction->Update();

        if(actionResult != ActionResult::RUNNING && actionResult != ActionResult::SUCCESS) {
          PRINT_NAMED_WARNING("PlaceObjectOnGroundAction.CheckIfDone.FaceAndVerifyFailed",
                              "FaceAndVerify action reported failure, just clearing object %d.",
                              _carryingObjectID.GetValue());
          // rsam: it's arguably whether the action should do this. _carryingObjectID may
          // no longer be equal robot.GetCarryingObjectID(), and be the reason why the actionResult
          // is != Success, which would make this operation useless if the object doesn't exist anymore.
          // I'm not sure this should be clearing or totally deleting (from PoseState refactor)
          GetRobot().GetBlockWorld().ClearLocatedObjectByIDInCurOrigin(_carryingObjectID);
        }

      } // if robot is not picking/placing or moving

      return actionResult;

    } // CheckIfDone()

    void  PlaceObjectOnGroundAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;
      info.objectID = _carryingObjectID;
      completionUnion.Set_objectInteractionCompleted(std::move(info));
    }

#pragma mark ---- PlaceObjectOnGroundAtPoseAction ----

    PlaceObjectOnGroundAtPoseAction::PlaceObjectOnGroundAtPoseAction(const Pose3d& placementPose,
                                                                     const bool useExactRotation,
                                                                     const bool checkFreeDestination,
                                                                     const float destinationObjectPadding_mm)
    : CompoundActionSequential()
    {
      auto* driveAction = new DriveToPlaceCarriedObjectAction(placementPose,
                                                              true,
                                                              useExactRotation,
                                                              checkFreeDestination,
                                                              destinationObjectPadding_mm);
      _driveAction = AddAction(driveAction);

      PlaceObjectOnGroundAction* action = new PlaceObjectOnGroundAction();
      AddAction(action);
      SetProxyTag(action->GetTag());
    }

#pragma mark ---- PlaceRelObjectAction ----

    PlaceRelObjectAction::PlaceRelObjectAction(ObjectID objectID,
                                               const bool placeOnGround,
                                               const f32 placementOffsetX_mm,
                                               const f32 placementOffsetY_mm,
                                               const bool relativeCurrentMarker)
    : IDockAction(objectID,
                  "PlaceRelObject",
                  RobotActionType::PICK_AND_PLACE_INCOMPLETE)
    , _relOffsetX_mm(placementOffsetX_mm)
    , _relOffsetY_mm(placementOffsetY_mm)
    , _relativeCurrentMarker(relativeCurrentMarker)
    {
      SetPlaceOnGround(placeOnGround);
      using GE = AudioMetaData::GameEvent::GenericEvent;
      SetPostDockLiftMovingAudioEvent(GE::Play__Robot_Vic_Sfx__Lift_High_Down_Short_Excited);

      // SetPlacementOffset set in InitInternal
      if(!(FLT_NEAR(placementOffsetX_mm, 0.f) &&
           FLT_NEAR(placementOffsetY_mm, 0.f))){
        SetDoNearPredockPoseCheck(false);
        PRINT_CH_INFO("Actions",
                      "PlaceRelObjectAction.Constructor.WillNotCheckPreDockPoses",
                      "Pre-dock pose is at an offset, so preDock pose check won't run");
      }
    }

    ActionResult  PlaceRelObjectAction::InitInternal()
    {
      ActionResult result = ActionResult::SUCCESS;

      if(!_relativeCurrentMarker){
        result = TransformPlacementOffsetsRelativeObject();
      }

      // If attempting to place the block off to the side of the target, do it even blinder
      // so that Cozmo doesn't fail when he inevitably looses sight of the tracker
      if(!NEAR_ZERO(_relOffsetY_mm)){
        SetDockingMethod(DockingMethod::EVEN_BLINDER_DOCKING);
      }

      SetPlacementOffset(_relOffsetX_mm, _relOffsetY_mm, _placementOffsetAngle_rad);

      return result;
    }

    void PlaceRelObjectAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;

      switch(_dockAction)
      {
        case DockAction::DA_PLACE_HIGH:
        case DockAction::DA_PLACE_LOW:
        {
          ObservableObject* object = GetRobot().GetBlockWorld().GetLocatedObjectByID(_dockObjectID);
          if(object == nullptr) {
            PRINT_NAMED_WARNING("PlaceRelObjectAction.EmitCompletionSignal.NullObject",
                                "Docking object %d not found in world after placing.",
                                _dockObjectID.GetValue());
          } else {
            info.objectID = _dockObjectID;
          }
          break;
        }
        default:
        {
          // Not setting dock action is only an issue if the action has started
          if(GetState() != ActionResult::NOT_STARTED)
          {
            PRINT_NAMED_WARNING("PlaceRelObjectAction.EmitCompletionSignal.DockActionNotSet",
                                "Dock action not set before filling completion signal.");
          }
        }
      }

      completionUnion.Set_objectInteractionCompleted(std::move( info ));
      IDockAction::GetCompletionUnion(completionUnion);
    }

    ActionResult PlaceRelObjectAction::SelectDockAction(ActionableObject* object)
    {
      if (VerifyCarryingComponentValid() &&
          !_carryingComponentPtr->IsCarryingObject()) {
        LOG_INFO("PlaceRelObjectAction.SelectDockAction.NotCarryingObject", "Can't place if not carrying an object. Aborting.");
        return ActionResult::NOT_CARRYING_OBJECT_ABORT;
      }

      if(!_placeObjectOnGroundIfCarrying &&
         VerifyDockingComponentValid() &&
         !_dockingComponentPtr->CanStackOnTopOfObject(*object))
      {
        PRINT_NAMED_WARNING("PlaceRelObjectAction.SelectDockAction.CantStackOnObject", "");
        return ActionResult::BAD_OBJECT;
      }

      _dockAction = _placeObjectOnGroundIfCarrying ? DockAction::DA_PLACE_LOW : DockAction::DA_PLACE_HIGH;

      if(_dockAction == DockAction::DA_PLACE_HIGH) {
        SetType(RobotActionType::PLACE_OBJECT_HIGH);
        _dockingMethod = (DockingMethod)kStackDockingMethod;
      }
      else
      {
        SetType(RobotActionType::PLACE_OBJECT_LOW);
      }

      // Need to record the object we are currently carrying because it
      // will get unset when the robot unattaches it during placement, and
      // we want to be able to verify that we're seeing what we just placed.
      if(VerifyCarryingComponentValid()){
        _carryObjectID = _carryingComponentPtr->GetCarryingObjectID();
      }

      return ActionResult::SUCCESS;
    } // SelectDockAction()

    ActionResult PlaceRelObjectAction::Verify()
    {
      ActionResult result = ActionResult::ABORT;

      switch(_dockAction)
      {
        case DockAction::DA_PLACE_LOW:
        case DockAction::DA_PLACE_HIGH:
        {
          if(GetRobot().GetDockingComponent().GetLastPickOrPlaceSucceeded()) {

            if(VerifyCarryingComponentValid() &&
               (_carryingComponentPtr->IsCarryingObject() == true)) {
              PRINT_NAMED_WARNING("PlaceRelObjectAction.Verify.ExpectedNotCarryingObject",
                                  "Expecting robot to think it's NOT carrying an object at this point.");
              return ActionResult::STILL_CARRYING_OBJECT;
            }

            // If the physical robot thinks it succeeded, move the lift out of the
            // way, and attempt to visually verify
            if(_placementVerifyAction == nullptr) {
              _placementVerifyAction.reset(new TurnTowardsObjectAction(_carryObjectID,
                                                                       Radians(0),
                                                                       true,
                                                                       false));
              _placementVerifyAction->ShouldSuppressTrackLocking(true);
              _placementVerifyAction->SetRobot(&GetRobot());
              _verifyComplete = false;

              // Go ahead do the first update of the FaceObjectAction to get the
              // init "out of the way" rather than wasting a tick here
              result = _placementVerifyAction->Update();
              if(ActionResult::SUCCESS != result && ActionResult::RUNNING != result) {
                return result;
              }
            }

            result = _placementVerifyAction->Update();

            if(result != ActionResult::RUNNING) {

              // Visual verification is done
              _placementVerifyAction.reset();

              if(result != ActionResult::SUCCESS)
              {
                PRINT_NAMED_WARNING("PlaceRelObjectAction.Verify.VerifyFailed",
                                    "Robot thinks it placed the object %s, but verification of placement "
                                    "failed. Not sure where carry object %d is, so clearing it.",
                                    _dockAction == DockAction::DA_PLACE_LOW ? "low" : "high",
                                    _carryObjectID.GetValue());

                GetRobot().GetBlockWorld().ClearLocatedObjectByIDInCurOrigin(_carryObjectID);
              }
              else if(_dockAction == DockAction::DA_PLACE_HIGH && !_verifyComplete) {

                // If we are placing high and verification succeeded, lower the lift
                _verifyComplete = true;

                if(result == ActionResult::SUCCESS) {
                  // Visual verification succeeded, drop lift (otherwise, just
                  // leave it up, since we are assuming we are still carrying the object)
                  _placementVerifyAction.reset(new MoveLiftToHeightAction(MoveLiftToHeightAction::Preset::LOW_DOCK));
                  _placementVerifyAction->ShouldSuppressTrackLocking(true);
                  _placementVerifyAction->SetRobot(&GetRobot());

                  result = ActionResult::RUNNING;
                }

              }
            } else {
              // Mostly for debugging when placement verification is taking too long
              LOG_INFO("PlaceRelObjectAction.Verify.Waiting", "");
            } // if(result != ActionResult::RUNNING)

          } else {
            // If the robot thinks it failed last pick-and-place, it is because it
            // failed to dock/track, so we are probably still holding the block
            PRINT_NAMED_WARNING("PlaceRelObjectAction.Verify.DockingFailed",
                                "Robot reported placement failure. Assuming docking failed "
                                "and robot is still holding same block.");
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }

          break;
        } // PLACE

        default:
          PRINT_NAMED_WARNING("PlaceRelObjectAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      return result;

    } // Verify()

    ActionResult PlaceRelObjectAction::TransformPlacementOffsetsRelativeObject()
    {
      ObservableObject* dockObject = GetRobot().GetBlockWorld().GetLocatedObjectByID(_dockObjectID);
      if(dockObject == nullptr){
        return ActionResult::BAD_OBJECT;
      }

      Pose3d dockObjectWRTRobot;
      const Pose3d& topPose = dockObject->GetZRotatedPointAboveObjectCenter(0.5f);
      const bool success = topPose.GetWithRespectTo(GetRobot().GetPose(), dockObjectWRTRobot);

      DEV_ASSERT(success, "PlaceRelObjectAction.Verify.GetWrtRobotPoseFailed");

      const float robotObjRelRotation_rad = dockObjectWRTRobot.GetRotation().GetAngleAroundZaxis().ToFloat();

      // consts for comparing relative robot/block alignment
      const float kRotationTolerance_rad = DEG_TO_RAD(15.f);
      const float kInAlignment_rad = DEG_TO_RAD(0.f);
      const float kClockwise_rad = DEG_TO_RAD(-90.f);
      const float kCounterClockwise_rad = -kClockwise_rad;
      const float kOppposite_rad = DEG_TO_RAD(180.f);
      const float kOppposite_rad_neg = -kOppposite_rad;

      //values to set placement offset with
      f32 xAbsolutePlacementOffset_mm;
      f32 yAbsolutePlacementOffset_mm;

      if(Util::IsNear(robotObjRelRotation_rad, kInAlignment_rad, kRotationTolerance_rad)){
        xAbsolutePlacementOffset_mm = -_relOffsetX_mm;
        yAbsolutePlacementOffset_mm = _relOffsetY_mm;
      }else if(Util::IsNear(robotObjRelRotation_rad, kCounterClockwise_rad, kRotationTolerance_rad)){
        xAbsolutePlacementOffset_mm = _relOffsetY_mm;
        yAbsolutePlacementOffset_mm = _relOffsetX_mm;
      }else if(Util::IsNear(robotObjRelRotation_rad, kClockwise_rad, kRotationTolerance_rad)){
        xAbsolutePlacementOffset_mm = -_relOffsetY_mm;
        yAbsolutePlacementOffset_mm = -_relOffsetX_mm;
      }else if( Util::IsNear(robotObjRelRotation_rad, kOppposite_rad, kRotationTolerance_rad)
               ||  Util::IsNear(robotObjRelRotation_rad, kOppposite_rad_neg, kRotationTolerance_rad)){
        xAbsolutePlacementOffset_mm = _relOffsetX_mm;
        yAbsolutePlacementOffset_mm = -_relOffsetY_mm;
      }else{
        PRINT_NAMED_WARNING("PlaceRelObjectAction.CalculatePlacementOffset.InvalidOrientation",
                            "Robot and block are not within alignment threshold - rotation:%f threshold:%f",
                            RAD_TO_DEG(robotObjRelRotation_rad), kRotationTolerance_rad);
        return ActionResult::DID_NOT_REACH_PREACTION_POSE;
      }

      if(FLT_LT(xAbsolutePlacementOffset_mm, -kMaxNegativeXPlacementOffset)){
        PRINT_NAMED_ERROR("PlaceRelObjectAction.TransformPlacementOffsetsRelativeObject.InvalidNegativeOffset",
                          "Attempted to set negative xOffset. xOffset:%f, yOffset:%f", xAbsolutePlacementOffset_mm, yAbsolutePlacementOffset_mm);
        return ActionResult::ABORT;
      }

      _relOffsetX_mm = xAbsolutePlacementOffset_mm;
      _relOffsetY_mm = yAbsolutePlacementOffset_mm;

      return ActionResult::SUCCESS;
    }

    // The max lateral offset an object at a given distance from a pose can be such that it is still
    // visible by the camera with x and y offsets applied
    // maxOffset is a lateral distance from pose relative to object
    static Result GetMaxOffsetObjectStillVisible(const Vision::Camera& camera,
                                                 const ObservableObject* object,
                                                 const f32 distanceToObject,
                                                 const f32 desiredOffsetX_mm,
                                                 const f32 desiredOffsetY_mm,
                                                 const Pose3d& pose,
                                                 f32& maxOffset)
    {
      // Find the width of the closest marker to pose
      Pose3d objectPoseWrtRobot_unused;
      Vision::Marker closestMarker(Vision::Marker::ANY_CODE);
      const Result res = object->GetClosestMarkerPose(pose, true, objectPoseWrtRobot_unused, closestMarker);
      if(res != RESULT_OK)
      {
        PRINT_NAMED_WARNING("GetMaxOffsetObjectStillVisible.GetClosestMarkerPose",
                            "Could not get closest marker pose");
        return RESULT_FAIL;
      }

      const auto markers = object->GetMarkersWithCode(closestMarker.GetCode());
      if(markers.size() != 1)
      {
        // This should not happen we just found this marker by calling GetClosestMarkerPose
        PRINT_NAMED_WARNING("GetMaxOffsetObjectStillVisible.GetMarkersWithCode",
                            "No markers with code %s found on object %s",
                            closestMarker.GetCodeName(),
                            EnumToString(object->GetType()));
        return RESULT_FAIL;
      }

      const f32     markerSize = markers.back()->GetSize().x();
      const Radians fov        = camera.GetCalibration()->ComputeHorizontalFOV();
      const f32     tanHalfFov = tanf(fov.ToFloat()*0.5f);
      const f32     distance   = fabsf(distanceToObject) + desiredOffsetX_mm;

      const f32 minDistance = markerSize / tanHalfFov;
      if(distance < minDistance)
      {
        PRINT_NAMED_WARNING("GetMaxOffsetObjectStillVisible.InvalidDistance",
                            "Total distance to object %f < min possible distance %f to see the object",
                            distance,
                            minDistance);
        return RESULT_FAIL;
      }

      // Find the distance between the center of the camera's fov and the edge of it at the given
      // distance + x_offset
      const f32 y = tanHalfFov * distance;

      // Subtract the width of the marker so that it will be completely visible
      maxOffset = y - markers.back()->GetSize().x();

      return RESULT_OK;
    }

    ActionResult PlaceRelObjectAction::ComputePlaceRelObjectOffsetPoses(const ActionableObject* object,
                                                                        const f32 placementOffsetX_mm,
                                                                        const f32 placementOffsetY_mm,
                                                                        const Pose3d& robotPose,
                                                                        const Pose3d& worldOrigin,
                                                                        const CarryingComponent& carryingComp,
                                                                        BlockWorld& blockWorld,
                                                                        const VisionComponent& visionComp,
                                                                        std::vector<Pose3d>& possiblePoses,
                                                                        bool& alreadyInPosition)
    {
      // Guilty until proven innocent - since we might clear some pre-dock poses
      // we should not assume we're in position b/c it returned true above
      // instead we should prove we're in a valid pose below
      alreadyInPosition = false;
      possiblePoses.clear();

      const IDockAction::PreActionPoseInput preActionPoseInput(object,
                                                               PreActionPose::ActionType::PLACE_RELATIVE,
                                                               false,
                                                               0,
                                                               DEFAULT_PREDOCK_POSE_ANGLE_TOLERANCE,
                                                               false,
                                                               0);
      IDockAction::PreActionPoseOutput preActionPoseOutput;

      IDockAction::GetPreActionPoses(robotPose,
                                     carryingComp,
                                     blockWorld,
                                     preActionPoseInput,
                                     preActionPoseOutput);

      if(preActionPoseOutput.actionResult == ActionResult::SUCCESS)
      {
        // Add the pre-action poses to the possible poses list
        for(const auto& preActPose: preActionPoseOutput.preActionPoses){
          possiblePoses.push_back(preActPose.GetPose());
        }

        // Now determine if any of those are invalid and remove them
        using PoseIter = std::vector<Pose3d>::iterator;

        for(PoseIter fullIter = possiblePoses.begin(); fullIter != possiblePoses.end(); )
        {
          const Pose3d& idealCenterPose = object->GetZRotatedPointAboveObjectCenter(0.f);
          Pose3d preDocWRTUnrotatedBlock;
          const bool posesOk = fullIter->GetWithRespectTo(idealCenterPose, preDocWRTUnrotatedBlock);
          if (!posesOk ) {
            // this should not be possible at all, since the predock poses and the object have to be in same origin
            PRINT_NAMED_ERROR("DriveToPlaceRelObjectAction.GetPossiblePosesFunc.InvalidPoses",
                              "FullIter Pose and idealCenterPose not related!");
            continue;
          }

          const float poseX = preDocWRTUnrotatedBlock.GetTranslation().x();
          const float poseY = preDocWRTUnrotatedBlock.GetTranslation().y();
          const float minIllegalOffset = 1.f;

          const bool xOffsetRelevant =
            !IN_RANGE(placementOffsetX_mm, -minIllegalOffset, minIllegalOffset) &&
            !IN_RANGE(poseX, -minIllegalOffset, minIllegalOffset);

          const bool yOffsetRelevant =
            !IN_RANGE(placementOffsetY_mm, -minIllegalOffset, minIllegalOffset) &&
            !IN_RANGE(poseY, -minIllegalOffset, minIllegalOffset);

          const bool isPoseInvalid =
            (xOffsetRelevant && (FLT_GT(poseX, 0) != FLT_GT(placementOffsetX_mm, 0))) ||
            (yOffsetRelevant && (FLT_GT(poseY, 0) != FLT_GT(placementOffsetY_mm, 0)));

          if(isPoseInvalid)
          {
            fullIter = possiblePoses.erase(fullIter);

            LOG_INFO("DriveToPlaceRelObjectAction.PossiblePosesFunc.RemovingInvalidPose",
                     "Removing pose x:%f y:%f because Cozmo can't place at offset x:%f, y:%f, xRelevant:%d, yRelevant:%d",
                     poseX, poseY,
                     placementOffsetX_mm, placementOffsetY_mm,
                     xOffsetRelevant, yOffsetRelevant);
          }
          else
          {
            // We need to visually verify placement since there are high odds
            // that we will bump objects while placing relative to them, so if possible
            // place using a y offset
            const bool onlyOnePlacementDirection = xOffsetRelevant != yOffsetRelevant;
            const bool currentXPoseIdeal = xOffsetRelevant && IN_RANGE(poseY, -minIllegalOffset, minIllegalOffset);
            const bool currentYPoseIdeal = yOffsetRelevant && IN_RANGE(poseX, -minIllegalOffset, minIllegalOffset);

            if(onlyOnePlacementDirection &&
               possiblePoses.size() > 2 &&
               (currentXPoseIdeal || currentYPoseIdeal))
            {
              fullIter = possiblePoses.erase(fullIter);
            }
            else
            {
              const auto& trans = preDocWRTUnrotatedBlock.GetTranslation();

              const Radians angle = preDocWRTUnrotatedBlock.GetRotation().GetAngleAroundZaxis();
              f32 preDockOffsetX = placementOffsetX_mm;
              f32 preDockOffsetY = placementOffsetY_mm;
              f32 distanceToObject = trans.x();

              // we expect the Z angle to be a quarter (0,90,180,270). Check below with a small epsilon
              const Radians kAngleEpsilonRad( DEG_TO_RAD(2.0f) );
              DEV_ASSERT( angle.IsNear( DEG_TO_RAD(  0.0f), kAngleEpsilonRad) ||
                          angle.IsNear( DEG_TO_RAD( 90.0f), kAngleEpsilonRad) ||
                          angle.IsNear( DEG_TO_RAD(180.0f), kAngleEpsilonRad) ||
                          angle.IsNear( DEG_TO_RAD(270.0f), kAngleEpsilonRad),
                          "PlaceRelObjectAction.ComputePlaceRelObjectOffsetPoses.PreDockPoseAngleNotNearQuarter");

              const bool isAlignedWithYAxis = angle.IsNear( DEG_TO_RAD( 90.0f), kAngleEpsilonRad) ||
                                              angle.IsNear( DEG_TO_RAD(270.0f), kAngleEpsilonRad);

              // Flip the x and y offset and use the y translation should this preDock pose
              // be at 90 or 270 degrees relative to the object
              if(isAlignedWithYAxis)
              {
                preDockOffsetX = placementOffsetY_mm;
                preDockOffsetY = placementOffsetX_mm;
                distanceToObject = trans.y();
              }

              // Find the max lateral offset from the preDock pose that the object will still be visible
              // This is to ensure we will be seeing the object when we are at the preDock pose
              f32 maxOffset_mm = 0;
              const Result res = GetMaxOffsetObjectStillVisible(visionComp.GetCamera(),
                                                                object,
                                                                distanceToObject,
                                                                preDockOffsetX,
                                                                preDockOffsetY,
                                                                *fullIter,
                                                                maxOffset_mm);
              if(res != RESULT_OK)
              {
                PRINT_NAMED_WARNING("DriveToPlaceRelObjectAction.GetPossiblePosesFunc.GetMaxYOffset",
                                    "Failed to get max offset where %s is still visible from distance %f with placement offsets (%f, %f)",
                                    EnumToString(object->GetType()),
                                    trans.x(),
                                    placementOffsetX_mm,
                                    placementOffsetY_mm);

                fullIter = possiblePoses.erase(fullIter);
                continue;
              }

              // Subtract a bit of padding from maxOffset to account for errors in path following should
              // we actually decide to drive to this predock pose
              // Still doesn't guarantee that we will be seeing the object once we get to the preDock pose
              // but greatly increases our chances
              const static f32 padding_mm = 20;
              if(maxOffset_mm > padding_mm)
              {
                maxOffset_mm -= padding_mm;
              }

              // Depending on which preDock pose this is, either the x or y placementOffset
              // (whichever corresponds to horizontal distance relative to the preDock pose) will need
              // to be clipped to the maxOffset
              f32 clipX_mm = placementOffsetX_mm;
              f32 clipY_mm = placementOffsetY_mm;
              if(isAlignedWithYAxis)
              {
                clipX_mm = CLIP(placementOffsetX_mm, -maxOffset_mm, maxOffset_mm);
              }
              else
              {
                clipY_mm = CLIP(placementOffsetY_mm, -maxOffset_mm, maxOffset_mm);
              }

              // If we don't want to use the maxOffset then set clipX/Y to 0
              if(!kPlaceRelUseMaxOffset)
              {
                clipX_mm = 0;
                clipY_mm = 0;
              }

              preDocWRTUnrotatedBlock.SetTranslation({trans.x() + clipX_mm,
                                                      trans.y() + clipY_mm,
                                                      trans.z()});

              const bool poseOriginOk = preDocWRTUnrotatedBlock.GetWithRespectTo(worldOrigin, *fullIter);
              if ( !poseOriginOk ) {
                // this should not be possible at all, since the predock poses are in robot origin
                PRINT_NAMED_ERROR("DriveToPlaceRelObjectAction.GetPossiblePosesFunc.UnrotatedBlockPoseBadOrigin",
                                  "Could not obtain predock pose from unrotated wrt origin.");
                continue;
              }

              Point2f distThreshold = ComputePreActionPoseDistThreshold(*fullIter,
                                                                        object->GetPose(),
                                                                        DEFAULT_PREDOCK_POSE_ANGLE_TOLERANCE);

              // If the new preAction pose is close enough to the robot's current pose mark as
              // alreadyInPosition
              // Don't really care about z
              static const f32 kDontCareZThreshold = 100;
              if(robotPose.IsSameAs(*fullIter,
                                    {distThreshold.x(), distThreshold.y(), kDontCareZThreshold},
                                    DEFAULT_PREDOCK_POSE_ANGLE_TOLERANCE))
              {
                alreadyInPosition = true;
              }

              ++fullIter;
            }
          }
        }// end for(PoseIter)
      }// end if(possiblePosesResult == ActionResult::Success)
      else
      {
        PRINT_CH_INFO("Actions",
                      "DriveToPlaceRelObjectAction.PossiblePosesFunc.PossiblePosesResultNotSuccess",
                      "Received possible poses result:%u", preActionPoseOutput.actionResult);
      }

      if(possiblePoses.size() > 0)
      {
        return preActionPoseOutput.actionResult;
      }
      else
      {
        PRINT_CH_INFO("Actions",
                      "PlaceRelObjectAction.PossiblePosesFunc.NoValidPoses",
                      "After filtering invalid pre-doc poses none remained for placement offset x:%f, y%f",
                      placementOffsetX_mm, placementOffsetY_mm);

        return ActionResult::NO_PREACTION_POSES;
      }
    }


#pragma mark ---- RollObjectAction ----

    RollObjectAction::RollObjectAction(ObjectID objectID)
    : IDockAction(objectID,
                  "RollObject",
                  RobotActionType::ROLL_OBJECT_LOW)
    {
      _dockingMethod = (DockingMethod)kRollDockingMethod;
      _dockAction = DockAction::DA_ROLL_LOW;
      using GE = AudioMetaData::GameEvent::GenericEvent;
      SetPostDockLiftMovingAudioEvent(GE::Play__Robot_Vic_Sfx__Lift_High_Down_Long_Excited);
    }

    void RollObjectAction::EnableDeepRoll(bool enable)
    {
      _dockAction = enable ? DockAction::DA_DEEP_ROLL_LOW : DockAction::DA_ROLL_LOW;
      SetName(enable ? "DeepRollObject" : "RollObject");
    }

    void RollObjectAction::EnableRollWithoutDock(bool enable)
    {
      _dockAction = enable ? DockAction::DA_POST_DOCK_ROLL : DockAction::DA_ROLL_LOW;
      SetName(enable ? "RollWithoutDock" : "RollObject");

      // Don't check if we are near a predock pose because we won't actually be docking
      SetDoNearPredockPoseCheck(!enable);

      // We are likely right next to the object to roll so don't bother turning towards it/trying to verify
      // it is in front of us
      SetShouldFirstTurnTowardsObject(!enable);
    }

    bool RollObjectAction::CanActionRollObject(const DockingComponent& dockingComponent,
                                               const ObservableObject* object)
    {
      return dockingComponent.CanPickUpObjectFromGround(*object);
    }


    void RollObjectAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      ObjectInteractionCompleted info;
      switch(_dockAction)
      {
        case DockAction::DA_ROLL_LOW:
        case DockAction::DA_DEEP_ROLL_LOW:
        case DockAction::DA_POST_DOCK_ROLL:
        {
          if(VerifyCarryingComponentValid() &&
             _carryingComponentPtr->IsCarryingObject()) {
            PRINT_NAMED_WARNING("RollObjectAction.EmitCompletionSignal.ExpectedNotCarryingObject", "");
          }
          else {
            info.objectID = _dockObjectID;
          }
          break;
        }
        default:
        {
          // Not setting dock action is only an issue if the action has started
          if(GetState() != ActionResult::NOT_STARTED)
          {
            PRINT_NAMED_WARNING("RollObjectAction.EmitCompletionSignal.DockActionNotSet",
                                "Dock action not set before filling completion signal.");
          }
        }
      }

      completionUnion.Set_objectInteractionCompleted(std::move( info ));
      IDockAction::GetCompletionUnion(completionUnion);
    }

    ActionResult RollObjectAction::SelectDockAction(ActionableObject* object)
    {
      // Record the object's original pose (before picking it up) so we can
      // verify later whether we succeeded.
      // Make it w.r.t. robot's parent so we don't have to worry about differing origins later.
      if(object->GetPose().GetWithRespectTo(GetRobot().GetPose().GetParent(), _dockObjectOrigPose) == false) {
        PRINT_NAMED_WARNING("RollObjectAction.SelectDockAction.PoseWrtFailed",
                            "Could not get pose of dock object w.r.t. robot's parent.");
        return ActionResult::BAD_OBJECT;
      }

      // Choose docking action based on block's position and whether we are
      // carrying a block
      const f32 dockObjectHeightWrtRobot = _dockObjectOrigPose.GetTranslation().z() - GetRobot().GetPose().GetTranslation().z();

      // Get the top marker as this will be what needs to be seen for verification
      Block* block = dynamic_cast<Block*>(object);
      if (block == nullptr) {
        PRINT_NAMED_WARNING("RollObjectAction.SelectDockAction.NonBlock", "Only blocks can be rolled");
        return ActionResult::BAD_OBJECT;
      }
      Pose3d junk;
      _expectedMarkerPostRoll = &(block->GetTopMarker(junk));

      // TODO: Stop using constant ROBOT_BOUNDING_Z for this
      // TODO: There might be ways to roll high blocks when not carrying object and low blocks when carrying an object.
      //       Do them later.
      if (dockObjectHeightWrtRobot > 0.5f*ROBOT_BOUNDING_Z) { //  dockObject->GetSize().z()) {
        LOG_INFO("RollObjectAction.SelectDockAction.ObjectTooHigh", "Object is too high to roll. Aborting.");
        return ActionResult::BAD_OBJECT;
      } else if (VerifyCarryingComponentValid() && _carryingComponentPtr->IsCarryingObject()) {
        LOG_INFO("RollObjectAction.SelectDockAction.CarryingObject", "");
        return ActionResult::STILL_CARRYING_OBJECT;
      }

      return ActionResult::SUCCESS;
    } // SelectDockAction()

    ActionResult RollObjectAction::Verify()
    {
      ActionResult result = ActionResult::RUNNING;

      switch(_dockAction)
      {
        case DockAction::DA_ROLL_LOW:
        case DockAction::DA_DEEP_ROLL_LOW:
        case DockAction::DA_POST_DOCK_ROLL:
        {
          if(VerifyDockingComponentValid() && _dockingComponentPtr->GetLastPickOrPlaceSucceeded()) {

            if(VerifyCarryingComponentValid() && (_carryingComponentPtr->IsCarryingObject() == true)) {
              PRINT_NAMED_WARNING("RollObjectAction.Verify.ExpectedNotCarryingObject", "");
              result = ActionResult::STILL_CARRYING_OBJECT;
              break;
            }

            // If the physical robot thinks it succeeded, verify that the expected marker is being seen
            if(_rollVerifyAction == nullptr) {
              // Since rolling is the only action that moves the block and then immediately needs to visually verify
              // The head needs to look down more to account for the fact the block pose moved towards us and then we can
              // do the verification
              _rollVerifyAction.reset(new CompoundActionSequential({
                new MoveHeadToAngleAction(kAngleToLookDown),
                new VisuallyVerifyObjectAction(_dockObjectID, _expectedMarkerPostRoll->GetCode())
              }));

              _rollVerifyAction->ShouldSuppressTrackLocking(true);
              _rollVerifyAction->SetRobot(&GetRobot());

              // Do one update step immediately after creating the action to get Init done
              result = _rollVerifyAction->Update();
            }

            if(result == ActionResult::RUNNING) {
              result = _rollVerifyAction->Update();
            }

            if(result != ActionResult::RUNNING) {

              // Visual verification is done
              _rollVerifyAction.reset();

              if(result != ActionResult::SUCCESS) {
                LOG_INFO("RollObjectAction.Verify.VisualVerifyFailed",
                         "Robot thinks it rolled the object, but verification failed. ");

                // Automatically set to deep roll in case the action is retried
                EnableDeepRoll(true);

                result = ActionResult::VISUAL_OBSERVATION_FAILED;
              }
            } else {
              // Mostly for debugging when verification takes too long
              LOG_INFO("RollObjectAction.Verify.Waiting", "");
            } // if(result != ActionResult::RUNNING)

          } else {
            // If the robot thinks it failed last pick-and-place, it is because it
            // failed to dock/track.
            PRINT_NAMED_WARNING("RollObjectAction.Verify.DockingFailed",
                                "Robot reported roll failure. Assuming docking failed");
            // retry, since the block is hopefully still there
            result = ActionResult::LAST_PICK_AND_PLACE_FAILED;
          }

          break;
        } // ROLL_LOW


        default:
          PRINT_NAMED_WARNING("RollObjectAction.Verify.ReachedDefaultCase",
                              "Don't know how to verify unexpected dockAction %s.", DockActionToString(_dockAction));
          result = ActionResult::UNEXPECTED_DOCK_ACTION;
          break;

      } // switch(_dockAction)

      if( result == ActionResult::SUCCESS ) {
        GetRobot().GetComponent<RobotStatsTracker>().IncrementBehaviorStat(BehaviorStat::RolledCube);
      }

      return result;

    } // Verify()
  }
}
