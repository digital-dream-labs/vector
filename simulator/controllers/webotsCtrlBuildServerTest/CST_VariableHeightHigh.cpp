/**
 * File: CST_VariableHeightHigh.cpp
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
      Init,
      PickupHigh,
      TestDone
    };
    
    // ============ Test class declaration ============
    class CST_VariableHeightHigh : public CozmoSimTestController {
      
    private:
      
      virtual s32 UpdateSimInternal() override;
      
      TestState _testState = TestState::Init;
      
      ObjectID _id;
    };
    
    // Register class with factory
    REGISTER_COZMO_SIM_TEST_CLASS(CST_VariableHeightHigh);
    
    
    // =========== Test class implementation ===========
    
    s32 CST_VariableHeightHigh::UpdateSimInternal()
    {
      switch (_testState) {
        case TestState::Init:
        {
          SetActualRobotPose(Pose3d(0, Z_AXIS_3D(), {0, -400, 0}));
          StartMovieConditional("VariableHeightHigh");
          //TakeScreenshotsAtInterval("VariableHeightHigh", 1.f);
          
          
          SendMoveHeadToAngle(DEG_TO_RAD(10.f), 100, 100);
          SET_TEST_STATE(PickupHigh);
          break;
        }
        case TestState::PickupHigh:
        {
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), DEG_TO_RAD(10.f), HEAD_ANGLE_TOL),
                                                GetNumObjects() == 1)
          {
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 1;
            m.numRetries = 3;
            // Pickup object with type LIGHTCUBE3, whatever its ID happens to be
            auto objectsWithType = GetAllObjectIDsByType(ObjectType::Block_LIGHTCUBE3);
            CST_ASSERT(objectsWithType.size()==1, "Expecting 1 object of type LIGHTCUBE3");
            _id = objectsWithType.front();
            m.action.Set_pickupObject(ExternalInterface::PickupObject(_id, _defaultTestMotionProfile, 0, false, true));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TestDone);
          }
          break;
        }
        case TestState::TestDone:
        {
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(20,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                GetCarryingObjectID() == _id)
          {
            StopMovie();
            CST_EXIT();
          }
          break;
        }
      }
      return _result;
    }
  } // end namespace Vector
} // end namespace Anki

