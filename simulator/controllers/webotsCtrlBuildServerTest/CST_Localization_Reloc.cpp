/**
 * File: CST_Localization_Reloc.cpp
 *
 * Author: Matt Michini
 * Created: 3/4/19
 *
 * Description: Localize to the charger, turn and deloc, see another object, then see the charger again and ensure we
 *              re-localize and bring origins together.
 *
 * Copyright: Anki, Inc. 2019
 *
 */

#include "simulator/game/cozmoSimTestController.h"

namespace Anki {
namespace Vector {
  
enum class TestState {
  Init,                     // Look forward at the charger and cube.
  ObserveChargerAndCube,    // See the charger and cube, and localize to the charger.
  TurnAndDeloc,             // Turn so that we're not seeing anything and deloc
  WaitForDeloc,             // Verify that we've delocalized
  TurnAndObserveCustomCube, // Turn to see the custom cube in the new delocalized origin
  TurnAndLocalizeToCharger, // Now turn and see just the charger. We should re-localize and rejigger origins.
  TestDone
};
  
// ============ Test class declaration ============
class CST_Localization_Reloc : public CozmoSimTestController
{
  virtual s32 UpdateSimInternal() override;
  
  TestState _testState = TestState::Init;
  
  virtual void HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg) override;
  
  void DefineCustomObject();
  
  webots::Node* _chargerNode = GetNodeByDefName("Charger");
  webots::Node* _cubeNode = GetNodeByDefName("Cube");
  webots::Node* _customCubeNode = GetNodeByDefName("CustomCube");
  
  const float kDistThreshold_mm = 5.f;
  const float kAngleThreshold_rad = DEG_TO_RAD(5.f);
  
  Pose3d _initialCubePose;
  Pose3d _initialChargerPose;
  
  ObjectID _cubeId;
  ObjectID _chargerId;
};
  
// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_Localization_Reloc);

// =========== Test class implementation ===========

s32 CST_Localization_Reloc::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      CST_ASSERT(_chargerNode != nullptr, "Null charger node");
      CST_ASSERT(_cubeNode != nullptr, "Null charger node");
      CST_ASSERT(_customCubeNode != nullptr, "Null custom cube node");
      
      // Send the definition for the custom cube
      DefineCustomObject();
      
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
        auto result = GetObjectPose(_chargerId, _initialChargerPose);
        CST_ASSERT(result == RESULT_OK, "Failed to get initial charger pose");
        result = GetObjectPose(_cubeId, _initialCubePose);
        CST_ASSERT(result == RESULT_OK, "Failed to get initial cube pose");
        
        // Turn away from objects so that we can delocalize while not seeing anything
        SendTurnInPlace(DEG_TO_RAD(90.f));
        SET_TEST_STATE(TurnAndDeloc);
      }
      break;
    }
    case TestState::TurnAndDeloc:
    {
      // Wait until we are done turning, then delocalize
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            NEAR(GetRobotPoseActual().GetRotation().GetAngleAroundZaxis().getDegrees(), 90, 20)) {
        SendForceDelocalize();
        SET_TEST_STATE(WaitForDeloc);
      }
      break;
    }
    case TestState::WaitForDeloc:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            GetNumObjects() == 0,
                                            !IsLocalizedToObject()) {
        // Turn and observe the custom cube in this new origin
        SendTurnInPlace(DEG_TO_RAD(90.f));
        SET_TEST_STATE(TurnAndObserveCustomCube);
      }
      break;
    }
    case TestState::TurnAndObserveCustomCube:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            GetNumObjects() == 1) {
        // Turn back toward the charger and observe it again. This should cause a re-localization, and both origins
        // should be brought together such that we know about all 3 objects.
        SendTurnInPlace(DEG_TO_RAD(-135.f));
        SET_TEST_STATE(TurnAndLocalizeToCharger);
      }
      break;
    }
    case TestState::TurnAndLocalizeToCharger:
    {
      
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            GetNumObjects() == 3,
                                            IsLocalizedToObject()) {
        // The robot's estimate of the charger position and the cube position should be the same as in the beginning,
        // since we've gone back into the original origin.
        Pose3d chargerPose;
        GetObjectPose(_chargerId, chargerPose);
        CST_ASSERT(chargerPose.IsSameAs(_initialChargerPose, kDistThreshold_mm, kAngleThreshold_rad),
                   "Charger pose should be the same as at the beginning");
        
        Pose3d cubePose;
        GetObjectPose(_cubeId, cubePose);
        CST_ASSERT(cubePose.IsSameAs(_initialCubePose, kDistThreshold_mm, kAngleThreshold_rad),
                   "Cube pose should be the same as at the beginning");
        
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

void CST_Localization_Reloc::HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg)
{
  if (IsChargerType(msg.objectType, false)) {
    _chargerId = msg.objectID;
  } else if (IsBlockType(msg.objectType, false)) {
    _cubeId = msg.objectID;
  }
}
  
void CST_Localization_Reloc::DefineCustomObject()
{
  const float cubeSize_mm = M_TO_MM(_customCubeNode->getField("width")->getSFFloat());
  const float cubeMarkerSize_mm = M_TO_MM(_customCubeNode->getField("markerWidth")->getSFFloat());
  
  using namespace ExternalInterface;
  DefineCustomCube defineCube(ObjectType::CustomType00,
                              CustomObjectMarker::Circles2,
                              cubeSize_mm,
                              cubeMarkerSize_mm, cubeMarkerSize_mm,
                              false);
  SendMessage(MessageGameToEngine(std::move(defineCube)));
}

} // end namespace Vector
} // end namespace Anki

