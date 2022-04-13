/**
 * File: CST_CliffDetection.cpp
 *
 * Author: Matt Michini
 * Created: 11/10/17
 *
 * Description: This is to test that cliff detection is working properly.
 *
 * Copyright: Anki, inc. 2017
 *
 */

#include "simulator/game/cozmoSimTestController.h"
#include "anki/cozmo/shared/cozmoConfig.h"

namespace Anki {
namespace Vector {
  
enum class TestState {
  Init,
  CliffFL,
  CliffFR,
  CliffBL,
  CliffBR,
  TestDone
};
  
// ============ Test class declaration ============
class CST_CliffDetection : public CozmoSimTestController {
public:
  
  CST_CliffDetection();
  
private:
  
  virtual s32 UpdateSimInternal() override;
  
  // Move the robot to a pose near a cliff, and begin driving toward the
  // cliff, expecting the sensor with the specified cliffID to fire.
  void SetupToTestCliffSensor(CliffSensor cliffID);

  // Verify that a cliff was detected properly on the specified sensor
  bool CheckCliffDetected(CliffSensor cliffID);
  
  // Message handlers:
  virtual void HandleCliffEvent(const CliffEvent& msg) override;
  virtual void HandleSetCliffDetectThresholds(const SetCliffDetectThresholds& msg) override;
  
  TestState _testState = TestState::Init;
  
  // Store cliff thresholds. These should decrease from the default value once a
  // cliff is detected (due to auto-thresholding in CliffSensorComponent)
  std::array<uint16_t, Util::EnumToUnderlying(CliffSensor::CLIFF_COUNT)> _cliffThresholds;
  
  CliffEvent _lastCliffEvent;
  
  const Pose3d _startingPose;
  
};
  
// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_CliffDetection);

// =========== Test class implementation ===========

CST_CliffDetection::CST_CliffDetection()
  : _startingPose(0.f, Z_AXIS_3D(), {430.f, 0, 0})
{
  // initialize with default cliff values:
  _cliffThresholds.fill(CLIFF_SENSOR_THRESHOLD_DEFAULT);
}

s32 CST_CliffDetection::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      SetupToTestCliffSensor(CliffSensor::CLIFF_FL);
      SET_TEST_STATE(CliffFL);
      break;
    }
    case TestState::CliffFL:
    {
      if(CheckCliffDetected(CliffSensor::CLIFF_FL)) {
        SetupToTestCliffSensor(CliffSensor::CLIFF_FR);
        SET_TEST_STATE(CliffFR);
      }
      break;
    }
    case TestState::CliffFR:
    {
      if(CheckCliffDetected(CliffSensor::CLIFF_FR)) {
        SetupToTestCliffSensor(CliffSensor::CLIFF_BL);
        SET_TEST_STATE(CliffBL);
      }
      break;
    }
    case TestState::CliffBL:
    {
      if(CheckCliffDetected(CliffSensor::CLIFF_BL)) {
        SetupToTestCliffSensor(CliffSensor::CLIFF_BR);
        SET_TEST_STATE(CliffBR);
      }
      break;
    }
    case TestState::CliffBR:
    {
      if(CheckCliffDetected(CliffSensor::CLIFF_BR)) {
        SET_TEST_STATE(TestDone);
      }
      break;
    }
    case TestState::TestDone:
    {
      StopMovie();
      CST_EXIT();
      break;
    }
  }
  
  return _result;
}

  
void CST_CliffDetection::SetupToTestCliffSensor(CliffSensor cliffID)
{
  float poseAngle = 0.f;
  bool driveForward = true;
  switch(cliffID) {
    case CliffSensor::CLIFF_FL:
      poseAngle = DEG_TO_RAD(-10.f);
      driveForward = true;
      break;
    case CliffSensor::CLIFF_FR:
      poseAngle = DEG_TO_RAD(10.f);
      driveForward = true;
      break;
    case CliffSensor::CLIFF_BL:
      poseAngle = DEG_TO_RAD(-170.f);
      driveForward = false;
      break;
    case CliffSensor::CLIFF_BR:
      poseAngle = DEG_TO_RAD(170.f);
      driveForward = false;
      break;
    default:
      break;
  }
  
  // Rotate the robot so that it encounters the specified cliff
  Pose3d newPose = _startingPose;
  newPose.SetRotation(poseAngle, Z_AXIS_3D());
  SetActualRobotPose(newPose);
  
  // Start driving toward the cliff
  SendDriveStraight(50.f, driveForward ? 100.f : -100.f);
}
  
  
bool CST_CliffDetection::CheckCliffDetected(CliffSensor cliffID)
{
  const auto cliffInd = Util::EnumToUnderlying(cliffID);
  IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(25.0,
                                        !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                        _lastCliffEvent.detectedFlags == (1<<cliffInd),
                                        _cliffThresholds[cliffInd] < CLIFF_SENSOR_THRESHOLD_DEFAULT)
  {
    return true;
  }
  return false;
}
  

void CST_CliffDetection::HandleCliffEvent(const CliffEvent& msg)
{
  _lastCliffEvent = msg;
}


void CST_CliffDetection::HandleSetCliffDetectThresholds(const SetCliffDetectThresholds& msg)
{
  _cliffThresholds = msg.thresholds;
}


} // end namespace Vector
} // end namespace Anki

