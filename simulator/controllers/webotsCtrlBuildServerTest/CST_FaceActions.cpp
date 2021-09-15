/**
 * File: CST_FaceActions.cpp
 *
 * Author: Al Chaussee
 * Created: 2/29/16
 *
 * Description: See TestStates below
 *
 * Copyright: Anki, inc. 2016
 *
 */

#include "simulator/game/cozmoSimTestController.h"
#include "engine/actions/basicActions.h"
#include "engine/robot.h"
#include "engine/components/visionScheduleMediator/iVisionModeSubscriber.h"
#include "util/bitFlags/bitFlags.h"

namespace Anki {
  namespace Vector {
    
    namespace
    {
      const float kTestDoneGoalTilt_deg = 41.0f;
      const float kTestDoneGoalTiltTol_deg = 6.0f;
    }
    
    enum class TestState {
      SetupVisionMode,
      TurnToFace,
      TurnAwayFromFace,
      TurnBackToFace,
      TestDone
    };
    
    // ============ Test class declaration ============
    class CST_FaceActions : public CozmoSimTestController {
      
    private:
      
      virtual s32 UpdateSimInternal() override;
      
      TestState _testState = TestState::SetupVisionMode;
      
      bool _lastActionSucceeded = false;
      
      RobotTimeStamp_t _prevFaceSeenTime = 0;
      RobotTimeStamp_t _faceSeenTime = 0;
      
      // Message handlers
      virtual void HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg) override;
      virtual void HandleRobotObservedFace(ExternalInterface::RobotObservedFace const& msg) override;
    };
    
    // Register class with factory
    REGISTER_COZMO_SIM_TEST_CLASS(CST_FaceActions);
    
    
    // =========== Test class implementation ===========
    
    s32 CST_FaceActions::UpdateSimInternal()
    {
      switch (_testState) {
        case TestState::SetupVisionMode:
        {
          // enable the correct vision modes (using the console var message for this will also ensure
          // the right schedule is used as well)
          using namespace ExternalInterface;
          MessageGameToEngine wrap(SetDebugConsoleVarMessage("Faces", "1"));
          
          if(SendMessage(wrap)==Anki::RESULT_OK) {
            _testState = TestState::TurnToFace;
            break;
          } else {
            PRINT_NAMED_ERROR("CST_FaceActions.SetupVisionMode.Failed","");
            _result = 255;
            QuitWebots(_result);
          }
        }
        case TestState::TurnToFace:
        {
          SendMoveHeadToAngle(MAX_HEAD_ANGLE, 100, 100);
          
          ExternalInterface::QueueSingleAction m;
          m.position = QueueActionPosition::AT_END;
          m.idTag = 2;
          m.action.Set_turnInPlace(ExternalInterface::TurnInPlace(-M_PI_F/2, DEG_TO_RAD(100), 0, POINT_TURN_ANGLE_TOL, false));
          ExternalInterface::MessageGameToEngine message;
          message.Set_QueueSingleAction(m);
          SendMessage(message);
          SET_TEST_STATE(TurnAwayFromFace);
          break;
        }
        case TestState::TurnAwayFromFace:
        {
          // Verify robot has turned and has seen the face
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), MAX_HEAD_ANGLE, HEAD_ANGLE_TOL),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), -90, 10),
                                                _faceSeenTime != 0)
          {
            SendMoveHeadToAngle(0, 20, 20);
            
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::AT_END;
            m.idTag = 3;
            m.action.Set_turnInPlace(ExternalInterface::TurnInPlace(-M_PI_F/2, DEG_TO_RAD(100), 0, POINT_TURN_ANGLE_TOL, false));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TurnBackToFace);
          }
          break;
        }
        case TestState::TurnBackToFace:
        {
          // Verify robot has turned away from face
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), 0, HEAD_ANGLE_TOL),
                                                (NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), -180, 10) ||
                                                 NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), 180, 10)))
          {
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 10;
            
            // note: we set the tolerance of the tilt-angle for the action
            //       to be half the tolerance of the test-expected tilt angle
            //       because there is noise in the estimation of pose from vision
            m.action.Set_turnTowardsLastFacePose(
              ExternalInterface::TurnTowardsLastFacePose(
                M_PI_F,
                0,
                0,
                0,
                0,
                0,
                DEG_TO_RAD(kTestDoneGoalTiltTol_deg)/2, // action's tilt tolerance
                false,
                AnimationTrigger::Count,
                AnimationTrigger::Count));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TestDone);
          }
          break;
        }
        case TestState::TestDone:
        {
          // Verify robot has turned back towards the face
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), DEG_TO_RAD(kTestDoneGoalTilt_deg), DEG_TO_RAD(kTestDoneGoalTiltTol_deg)),
                                                NEAR(GetRobotPose().GetRotation().GetAngleAroundZaxis().getDegrees(), -90, 10),
                                                _prevFaceSeenTime < _faceSeenTime,
                                                _prevFaceSeenTime != 0)
          {
            CST_EXIT();
          }
          break;
        }
      }
      return _result;
    }
    
    
    // ================ Message handler callbacks ==================
    void CST_FaceActions::HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg)
    {
      if (msg.result == ActionResult::SUCCESS) {
        _lastActionSucceeded = true;
      }
    }
    
    void CST_FaceActions::HandleRobotObservedFace(ExternalInterface::RobotObservedFace const& msg)
    {
      _prevFaceSeenTime = _faceSeenTime;
      _faceSeenTime = msg.timestamp;
    }
    
    // ================ End of message handler callbacks ==================
    
  } // end namespace Vector
} // end namespace Anki

