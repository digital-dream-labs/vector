/**
 * File: CST_BasicActions.cpp
 *
 * Author: Al Chaussee
 * Created: 2/12/16
 *
 * Description: See TestStates below
 *
 * Copyright: Anki, inc. 2016
 *
 */

#include "simulator/game/cozmoSimTestController.h"
#include "engine/actions/basicActions.h"
#include "engine/robot.h"

namespace Anki {
  namespace Vector {
    
    enum class TestState {
      MoveLiftUp,
      MoveLiftDown,
      MoveHeadUp,
      MoveHeadDown,
      DriveForwards,
      DriveBackwards,
      TurnLeft,
      TurnRight,
      PanAndTilt,
      FacePose,
      VisuallyVerifyNoObjectAtPose,
      VisuallyVerifyObjectAtPose,
      TurnLeftRelative_540,
      FaceObject,
      TurnRightRelative_540,
      TurnAbsolute_90,
      TurnAbsolute_0,
      TestDone
    };
    
    // ============ Test class declaration ============
    class CST_BasicActions : public CozmoSimTestController {
      
    private:
      
      virtual s32 UpdateSimInternal() override;
      
      TestState _testState = TestState::MoveLiftUp;
      
      // Used to keep track of action results:
      RobotActionType _lastActionType = RobotActionType::UNKNOWN;
      ActionResult _lastActionResult = ActionResult::RUNNING;
      // StartingAction() sets _lastActionResult to RUNNING and sets
      //  _lastActionType to the supplied actionType:
      void StartingAction(const RobotActionType& actionType);
      
      const Point3f _poseToVerify = {200, 0, 22};
      
      // Position tolerance to use when visually verifying (no) object at pose
      const float kVisuallyVerifyTolerance_mm = 20.f;
      
      // to keep track of relative turns of more than one revolution:
      Radians _prevAngle;
      float _angularDistTraversed_deg = 0.f;
      
      // Message handlers
      virtual void HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg) override;
    };
    
    // Register class with factory
    REGISTER_COZMO_SIM_TEST_CLASS(CST_BasicActions);
    
    
    // =========== Test class implementation ===========
    
    s32 CST_BasicActions::UpdateSimInternal()
    {
      switch (_testState) {
        case TestState::MoveLiftUp:
        {
          StartMovieConditional("BasicActions");
          // TakeScreenshotsAtInterval("BasicActions", 1.f);
          
          StartingAction(RobotActionType::MOVE_LIFT_TO_HEIGHT);
          SendMoveLiftToHeight(LIFT_HEIGHT_HIGHDOCK, 100, 100);
          SET_TEST_STATE(MoveLiftDown);
          break;
        }
        case TestState::MoveLiftDown:
        {
          // Verify that lift is in up position
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetLiftHeight_mm(), LIFT_HEIGHT_HIGHDOCK, 5))
          {
            StartingAction(RobotActionType::MOVE_LIFT_TO_HEIGHT);
            SendMoveLiftToHeight(LIFT_HEIGHT_LOWDOCK, 100, 100);
            SET_TEST_STATE(MoveHeadUp);
          }
          break;
        }
        case TestState::MoveHeadUp:
        {
          // Verify that lift is in down position
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetLiftHeight_mm(), LIFT_HEIGHT_LOWDOCK, 5))
          {
            StartingAction(RobotActionType::MOVE_HEAD_TO_ANGLE);
            SendMoveHeadToAngle(MAX_HEAD_ANGLE, 100, 100);
            SET_TEST_STATE(MoveHeadDown);
          }
          break;
        }
        case TestState::MoveHeadDown:
        {
          // Verify head is up
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), MAX_HEAD_ANGLE, HEAD_ANGLE_TOL))
          {
            StartingAction(RobotActionType::MOVE_HEAD_TO_ANGLE);
            SendMoveHeadToAngle(0, 100, 100);
            SET_TEST_STATE(DriveForwards);
          }
          break;
        }
        case TestState::DriveForwards:
        {
          // Verify head is down
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), 0, HEAD_ANGLE_TOL))
          {
            StartingAction(RobotActionType::DRIVE_STRAIGHT);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 2;
            m.action.Set_driveStraight(ExternalInterface::DriveStraight(200, 50, true));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
          
            SET_TEST_STATE(DriveBackwards);
          }
          break;
        }
        case TestState::DriveBackwards:
        {
          // Verify robot is 50 mm forwards
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetTranslation().x(), 50, 10))
          {
            StartingAction(RobotActionType::DRIVE_STRAIGHT);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 3;
            m.action.Set_driveStraight(ExternalInterface::DriveStraight(200, -50, true));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            
            SET_TEST_STATE(TurnLeft);
          }
          break;
        }
        case TestState::TurnLeft:
        {
          // Verify robot is at starting point
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetTranslation().x(), 0, 10),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 0, 10))
          {
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(M_PI_F/2, DEG_TO_RAD(100), 0);
            SET_TEST_STATE(TurnRight);
          }
          break;
        }
        case TestState::TurnRight:
        {
          // Verify robot turned to 90 degrees
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 90, 10))
          {
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(-M_PI_F/2, DEG_TO_RAD(100), 0);
            SET_TEST_STATE(PanAndTilt);
          }
          break;
        }
        case TestState::PanAndTilt:
        {
          // Verify robot turned back to 0 degrees
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 0, 10))
          {
            StartingAction(RobotActionType::PAN_AND_TILT);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 6;
            m.action.Set_panAndTilt(ExternalInterface::PanAndTilt(M_PI_F, M_PI_F, true, true));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            
            SET_TEST_STATE(FacePose);
          }
          break;
        }
        case TestState::FacePose:
        {
          // Verify robot turned 180 degrees and head is at right angle
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                GetRobotPose().GetRotation().GetAngleAroundZaxis().IsNear(DEG_TO_RAD(180.f), DEG_TO_RAD(10.f)),
                                                NEAR(GetRobotHeadAngle_rad(), MAX_HEAD_ANGLE, HEAD_ANGLE_TOL))
          {
            StartingAction(RobotActionType::TURN_TOWARDS_POSE);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 7;
            // Face the position (0,-100,0) wrt robot
            m.action.Set_turnTowardsPose(ExternalInterface::TurnTowardsPose(GetRobotPose().GetTranslation().x(),
                                                              GetRobotPose().GetTranslation().y() + -1000,
                                                              NECK_JOINT_POSITION[2], M_PI_F, 0, 0, 0, 0, 0, 0));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(VisuallyVerifyNoObjectAtPose);
          }
          break;
        }
        case TestState::VisuallyVerifyNoObjectAtPose:
        {
          // Verify robot is facing pose
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), -90, 20),
                                                NEAR(GetRobotHeadAngle_rad(), DEG_TO_RAD(4.f), HEAD_ANGLE_TOL))
          {
            StartingAction(RobotActionType::VISUALLY_VERIFY_NO_OBJECT_AT_POSE);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 9;
            m.action.Set_visuallyVerifyNoObjectAtPose(ExternalInterface::VisuallyVerifyNoObjectAtPose(GetRobotPose().GetTranslation().x(),
                                                                                                      GetRobotPose().GetTranslation().y() + 100,
                                                                                                      NECK_JOINT_POSITION[2],
                                                                                                      kVisuallyVerifyTolerance_mm,
                                                                                                      kVisuallyVerifyTolerance_mm,
                                                                                                      kVisuallyVerifyTolerance_mm));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(VisuallyVerifyObjectAtPose);
          }
          break;
        }
        case TestState::VisuallyVerifyObjectAtPose:
        {
          // Verify robot is not seeing any objects at pose ~(0,100,0) which means the VisuallyVerifyNoObjectAtPose
          // succeeded
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 90, 20))
          {
            StartingAction(RobotActionType::VISUALLY_VERIFY_NO_OBJECT_AT_POSE);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 10;
            m.action.Set_visuallyVerifyNoObjectAtPose(ExternalInterface::VisuallyVerifyNoObjectAtPose(_poseToVerify.x(),
                                                                                                      _poseToVerify.y(),
                                                                                                      _poseToVerify.z(),
                                                                                                      kVisuallyVerifyTolerance_mm,
                                                                                                      kVisuallyVerifyTolerance_mm,
                                                                                                      kVisuallyVerifyTolerance_mm));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TurnLeftRelative_540);
          }
          break;
        }
        case TestState::TurnLeftRelative_540:
        {
          // Verify robot is seeing an object at pose ~(190,0,22) which means the VisuallyVerifyNoObjectAtPose action
          // failed
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 0, 10),
                                                NEAR(GetRobotPose().GetTranslation().x(), 0, 30),
                                                _lastActionResult == ActionResult::VISUAL_OBSERVATION_FAILED)
          {
            _prevAngle = GetRobotPoseActual().GetRotation().GetAngleAroundZaxis();
            _angularDistTraversed_deg = 0.f;
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(DEG_TO_RAD(540.f), DEG_TO_RAD(150), 0);
            SET_TEST_STATE(FaceObject);
          }
          break;
        }
        case TestState::FaceObject:
        {
          // Verify robot turned through 540 degress to a heading of 180 degrees
          const Radians currAngle = GetRobotPoseActual().GetRotation().GetAngleAroundZaxis();
          _angularDistTraversed_deg += (currAngle - _prevAngle).getDegrees();
          _prevAngle = currAngle;
          
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(_angularDistTraversed_deg, 540.f, 10.f))
          {
            StartingAction(RobotActionType::TURN_TOWARDS_OBJECT);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 8;
            
            // Face the Block_LIGHTCUBE1
            std::vector<s32> lightCubeIDs = GetAllObjectIDsByType(ObjectType::Block_LIGHTCUBE1);
            CST_ASSERT(!lightCubeIDs.empty(), "Found no cubes of type Block_LIGHTCUBE1");
            CST_ASSERT(lightCubeIDs.size() == 1, "Found too many cubes of type Block_LIGHTCUBE1");

            m.action.Set_turnTowardsObject(ExternalInterface::TurnTowardsObject(lightCubeIDs[0], M_PI_F, 0, 0, 0, 0, 0, 0, true, false));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TurnRightRelative_540);
          }
          break;
        }
        case TestState::TurnRightRelative_540:
        {
          // Verify robot is facing the object
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 0, 10),
                                                NEAR(GetRobotPose().GetTranslation().x(), 0, 30))
          {
            _prevAngle = GetRobotPoseActual().GetRotation().GetAngleAroundZaxis();
            _angularDistTraversed_deg = 0.f;
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(DEG_TO_RAD(-540.f), DEG_TO_RAD(150), 0);
            SET_TEST_STATE(TurnAbsolute_90);
          }
          break;
        }
        case TestState::TurnAbsolute_90:
        {
          // Verify robot turned through -540 degress to a heading of 0 degrees
          const Radians currAngle = GetRobotPoseActual().GetRotation().GetAngleAroundZaxis();
          _angularDistTraversed_deg += (currAngle - _prevAngle).getDegrees();
          _prevAngle = currAngle;
          
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(_angularDistTraversed_deg, -540.f, 10.f))
          {
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(DEG_TO_RAD(90.f), DEG_TO_RAD(150), 0, POINT_TURN_ANGLE_TOL, true);
            SET_TEST_STATE(TurnAbsolute_0);
          }
          break;
        }
        case TestState::TurnAbsolute_0:
        {
          // Verify robot turned to a heading of 90 degrees
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                GetRobotPose().GetRotation().GetAngleAroundZaxis().IsNear(DEG_TO_RAD(90.f), DEG_TO_RAD(10.f)))
          {
            StartingAction(RobotActionType::TURN_IN_PLACE);
            SendTurnInPlace(0.f, DEG_TO_RAD(150), 0, POINT_TURN_ANGLE_TOL, true);
            SET_TEST_STATE(TestDone);
          }
          break;
        }
        case TestState::TestDone:
        {
          // Verify robot turned to an absolute heading of 0 degrees
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                _lastActionResult == ActionResult::SUCCESS,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 0, 10))
          {
            StopMovie();
            CST_EXIT();
          }
          break;
        }
      }
      return _result;
    }
    
    void CST_BasicActions::StartingAction(const RobotActionType& actionType)
    {
      // Ensure that HandleRobotCompletedAction has reset _lastActionType to UNKNOWN. Otherwise
      //  we may be trying to start an action before the previous one's completion was handled.
      CST_ASSERT(_lastActionType == RobotActionType::UNKNOWN, "_lastActionType was never reset to UNKNOWN!");

      _lastActionType = actionType;
      _lastActionResult = ActionResult::RUNNING;
    }
    
    
    // ================ Message handler callbacks ==================
    void CST_BasicActions::HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg)
    {
      PRINT_NAMED_INFO("CST_BasicActions.HandleRobotCompletedAction", "completed action %s, result %s", EnumToString(msg.actionType), EnumToString(msg.result));
      
      if (msg.actionType == _lastActionType) {
        _lastActionResult = msg.result;
        // Reset _lastActionType to unknown
        _lastActionType = RobotActionType::UNKNOWN;
      } else {
        PRINT_NAMED_WARNING("CST_BasicActions.HandleRobotCompletedAction",
                            "An unexpected action completed. msg.actionType = %s, _lastActionType = %s",
                            EnumToString(msg.actionType),
                            EnumToString(_lastActionType));
      }
    }
    
    // ================ End of message handler callbacks ==================
    
  } // end namespace Vector
} // end namespace Anki

