/**
 * File: CST_ChargerDocking.cpp
 *
 * Author: Matt Michini
 * Created: 11/13/17
 *
 * Description: This is to test that charger docking is working properly.
 *
 * Copyright: Anki, Inc. 2017
 *
 */

#include "simulator/game/cozmoSimTestController.h"

#include "simulator/controllers/shared/webotsHelpers.h"

namespace Anki {
namespace Vector {
  
enum class TestState {
  Init,
  ShiftChargerSlightly,
  TestDone
};
  
// ============ Test class declaration ============
class CST_ChargerDocking : public CozmoSimTestController
{
  virtual s32 UpdateSimInternal() override;
  
  TestState _testState = TestState::Init;
};
  
// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_ChargerDocking);

// =========== Test class implementation ===========

s32 CST_ChargerDocking::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      // Start freeplay mode. The robot's proto in the testWorldChargerDocking.wbt world has the battery level set to a
      // 'low' level, so the robot should immediately begin trying to dock with the charger.
      StartFreeplayMode();
      
      SET_TEST_STATE(ShiftChargerSlightly);
      break;
    }
    case TestState::ShiftChargerSlightly:
    {
      // Wait until the robot is turned around away from the charger
      // and about to dock, then move the charger a tiny bit to force
      // the robot to auto-correct with the cliff sensors
      auto* chargerNode = WebotsHelpers::GetFirstMatchingSceneTreeNode(GetSupervisor(), "VictorCharger").nodePtr;
      auto chargerPose = GetPose3dOfNode(chargerNode);
      const auto& robotPose = GetRobotPoseActual();
      float distanceAway_mm = 0.f;
      const bool result = ComputeDistanceBetween(chargerPose, robotPose, distanceAway_mm);
      CST_ASSERT(result, "Failed computing distance between charger pose and robot pose");
      const float angleBetween_deg = (chargerPose.GetRotationAngle<'Z'>() - robotPose.GetRotationAngle<'Z'>()).getDegrees();
      if (distanceAway_mm < 180.f &&
          NEAR(angleBetween_deg, -90.f, 10.f)) {
        auto chargerTranslation = chargerPose.GetTranslation();
        chargerTranslation.x() += 10.f;
        chargerPose.SetTranslation(chargerTranslation);
        SetNodePose(chargerNode, chargerPose);
        SET_TEST_STATE(TestDone);
      }
    }
    case TestState::TestDone:
    {
      const bool onCharger = IsRobotStatus(RobotStatusFlag::IS_ON_CHARGER);
      IF_CONDITION_WITH_TIMEOUT_ASSERT(onCharger, 75.f) {
        StopMovie();
        CST_EXIT();
        break;
      }
    }
  }
  
  return _result;
}


} // end namespace Vector
} // end namespace Anki

