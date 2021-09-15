/**
 * File: CST_MotionCommands.cpp
 *
 * Author: Matt Michini
 * Created: 1/4/19
 *
 * Description: Test the lowest level motion commands to the robot
 *
 * Copyright: Anki, inc. 2019
 *
 */

#include "simulator/game/cozmoSimTestController.h"

namespace Anki {
namespace Vector {
  
enum class TestState {
  Init,
  MovingHead,
  MovingLift,
  DrivingWheels,
  Stopping1,
  DrivingStraight,
  Stopping2,
  DrivingArc,
  TestDone,
};
  
// ============ Test class declaration ============
class CST_MotionCommands : public CozmoSimTestController {
public:
  
  CST_MotionCommands() {}
  
private:
  
  virtual s32 UpdateSimInternal() override;
  
  TestState _testState = TestState::Init;
  
  const float driveSpeed_mmps = 100.f;
  const float driveAccel = 100.f;
};
  
// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_MotionCommands);

// =========== Test class implementation ===========

s32 CST_MotionCommands::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      SendMoveHead(DEG_TO_RAD(45.f));
      SET_TEST_STATE(MovingHead);
      break;
    }
    
    case TestState::MovingHead:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(NEAR(GetRobotHeadAngle_rad(), MAX_HEAD_ANGLE, HEAD_ANGLE_TOL),
                                       DEFAULT_TIMEOUT) {
        SendMoveHead(0.f);
        SendMoveLift(DEG_TO_RAD(45.f));
        SET_TEST_STATE(MovingLift);
      }
      break;
    }
      
    case TestState::MovingLift:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(NEAR(GetLiftHeight_mm(), LIFT_HEIGHT_HIGHDOCK, 2.f),
                                       DEFAULT_TIMEOUT) {
        SendMoveLift(0.f);
        
        // Drive backwards
        SendDriveWheels(-driveSpeed_mmps, -driveSpeed_mmps, driveAccel, driveAccel);
        SET_TEST_STATE(DrivingWheels);
      }
      break;
    }
    
    case TestState::DrivingWheels:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(NEAR(GetRobotPose().GetTranslation().x(), -50, 10),
                                       DEFAULT_TIMEOUT) {
        SendStopAllMotors();
        SET_TEST_STATE(Stopping1);
      }
      break;
    }
    
    case TestState::Stopping1:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                       DEFAULT_TIMEOUT) {
        SendDriveStraight(driveSpeed_mmps, 100.f);
        SET_TEST_STATE(DrivingStraight);
      }
      break;
    }
      
    case TestState::DrivingStraight:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(NEAR(GetRobotPose().GetTranslation().x(), 0, 10),
                                       DEFAULT_TIMEOUT) {
        SendStopAllMotors();
        SET_TEST_STATE(Stopping2);
      }
      break;
    }
      
    case TestState::Stopping2:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                       DEFAULT_TIMEOUT) {
        // Drive a slow tight-ish curve
        const float speed_mmps = 25.f;
        const float curvature_mm = 50.f;
        SendDriveArc(speed_mmps, driveAccel, curvature_mm);
        SET_TEST_STATE(DrivingArc);
      }
      break;
    }
      
    case TestState::DrivingArc:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(GetRobotPose().GetRotation().GetAngleAroundZaxis().IsNear(DEG_TO_RAD(90.f), DEG_TO_RAD(5.f)),
                                       DEFAULT_TIMEOUT) {
        SET_TEST_STATE(TestDone);
      }
      break;
    }
      
    case TestState::TestDone:
    {
      CST_EXIT();
      break;
    }
      
    default:
      break;
  }
  
  return _result;
}


} // end namespace Vector
} // end namespace Anki

