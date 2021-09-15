/**
 * File: CST_Localization_PoseUpdate.cpp
 *
 * Author: Matt Michini
 * Created: 3/4/19
 *
 * Description: Localize to a charger, move the charger, and ensure that poses are updated properly (e.g., ensure that
 *              the robot appropriately adjusts the _charger_ pose vs. its _own_ pose)
 *
 * Copyright: Anki, Inc. 2019
 *
 */

#include "simulator/game/cozmoSimTestController.h"

namespace Anki {
namespace Vector {
  
enum class TestState {
  Init,                    // Look forward at the charger and cube.
  ObserveChargerAndCube,   // See the charger and cube, and localize to the charger.
  ObserveChargerInNewPose, // Move the charger and allow the robot to observe it in its new position. We should update
                           // the _charger's_ position (not the robot's position) since the robot has not moved at all.
  MoveRobot,               // Now move the robot to a new position (and also moving its head)
  ObserveChargerAgain,     // Observe the charger again. This time, since the robot has moved, we should update the
                           // _robot's_ position, and the estimated charger pose should remain the same.
  TestDone
};
  
// ============ Test class declaration ============
class CST_Localization_PoseUpdate : public CozmoSimTestController
{
  virtual s32 UpdateSimInternal() override;
  
  TestState _testState = TestState::Init;
  
  virtual void HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg) override;
  
  // Amount by which to move the charger (then the robot)
  Vec3f kMoveTranslation = {25.f, 0.f, 0.f};
  
  webots::Node* _chargerNode = GetNodeByDefName("Charger");
  webots::Node* _cubeNode = GetNodeByDefName("Cube");
  
  const float kDistThreshold_mm = 5.f;
  const float kAngleThreshold_rad = DEG_TO_RAD(5.f);
  
  Pose3d _initialRobotPose;
  Pose3d _initialCubePose;
  Pose3d _initialChargerPose;
  
  ObjectID _cubeId;
  ObjectID _chargerId;
};
  
// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_Localization_PoseUpdate);

// =========== Test class implementation ===========

s32 CST_Localization_PoseUpdate::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      CST_ASSERT(_chargerNode != nullptr, "Null charger node");
      CST_ASSERT(_cubeNode != nullptr, "Null charger node");
      
      // Look straight ahead and observe the cube and charger
      SendMoveHeadToAngle(0, 100.f, 100.f);
      SET_TEST_STATE(ObserveChargerAndCube);
      break;
    }
    case TestState::ObserveChargerAndCube:
    {
      // Should have observed two objects and localized to the charger
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            GetNumObjects() == 2,
                                            IsLocalizedToObject()) {
        // Record 'initial' poses
        _initialRobotPose = GetRobotPose();
        auto result = GetObjectPose(_chargerId, _initialChargerPose);
        CST_ASSERT(result == RESULT_OK, "Failed to get initial charger pose");
        result = GetObjectPose(_cubeId, _initialCubePose);
        CST_ASSERT(result == RESULT_OK, "Failed to get initial cube pose");
        
        // Teleport the charger
        Pose3d actualChargerPose = GetPose3dOfNode(_chargerNode);
        actualChargerPose.SetTranslation(actualChargerPose.GetTranslation() + kMoveTranslation);
        SetNodePose(_chargerNode, actualChargerPose);
        SET_TEST_STATE(ObserveChargerInNewPose);
      }
      break;
    }
    case TestState::ObserveChargerInNewPose:
    {
      Pose3d chargerPose;
      GetObjectPose(_chargerId, chargerPose);
      
      Pose3d expectedChargerPose = _initialChargerPose;
      expectedChargerPose.SetTranslation(expectedChargerPose.GetTranslation() + kMoveTranslation);
      
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            chargerPose.IsSameAs(expectedChargerPose,
                                                                 kDistThreshold_mm,
                                                                 kAngleThreshold_rad)) {
        // The estimated charger pose has moved as expected. The robot's estimate of the cube pose and its own pose
        // should be exactly the same as before.
        CST_ASSERT(GetRobotPose().IsSameAs(_initialRobotPose, kDistThreshold_mm, kAngleThreshold_rad),
                   "Esimated robot pose has changed");
                                              
        Pose3d cubePose;
        GetObjectPose(_cubeId, cubePose);
        CST_ASSERT(cubePose.IsSameAs(_initialCubePose, kDistThreshold_mm, kAngleThreshold_rad),
                   "Estimated cube pose has changed");
                                 
        // Move the robot's head up so that it cannot see the charger/cube, and so that it will register the camera
        // as having moved.
        SendMoveHeadToAngle(MAX_HEAD_ANGLE, 100.f, 100.f);
        SET_TEST_STATE(MoveRobot);
      }
      break;
    }
    case TestState::MoveRobot:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING)) {
        // Teleport the robot forward and move the head level again so we see the charger.
        Pose3d actualRobotPose = GetRobotPoseActual();
        actualRobotPose.SetTranslation(actualRobotPose.GetTranslation() + kMoveTranslation);
        SetActualRobotPose(actualRobotPose);
        
        SendMoveHeadToAngle(0, 100.f, 100.f);
        SET_TEST_STATE(ObserveChargerAgain);
      }
      break;
    }
    case TestState::ObserveChargerAgain:
    {
      // We expect the robot's pose estimate to have adjusted by virtue of localizing to the charger.
      Pose3d expectedRobotPose = _initialRobotPose;
      expectedRobotPose.SetTranslation(expectedRobotPose.GetTranslation() + kMoveTranslation);
      
      // We also expect the charger pose and the cube pose to _not_ have changed from where they were last
      Pose3d chargerPose;
      GetObjectPose(_chargerId, chargerPose);
      
      Pose3d expectedChargerPose = _initialChargerPose;
      expectedChargerPose.SetTranslation(expectedChargerPose.GetTranslation() + kMoveTranslation);
      
      Pose3d cubePose;
      GetObjectPose(_cubeId, cubePose);
      
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            GetRobotPose().IsSameAs(expectedRobotPose, kDistThreshold_mm, kAngleThreshold_rad),
                                            chargerPose.IsSameAs(expectedChargerPose, kDistThreshold_mm, kAngleThreshold_rad),
                                            cubePose.IsSameAs(_initialCubePose, kDistThreshold_mm, kAngleThreshold_rad)) {
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

void CST_Localization_PoseUpdate::HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg)
{
  if (IsChargerType(msg.objectType, false)) {
    _chargerId = msg.objectID;
  } else if (IsBlockType(msg.objectType, false)) {
    _cubeId = msg.objectID;
  }
}

} // end namespace Vector
} // end namespace Anki

