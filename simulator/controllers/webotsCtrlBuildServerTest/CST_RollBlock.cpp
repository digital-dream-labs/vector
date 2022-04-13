/**
 * File: CST_RollBlock.cpp
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
      RollObject,
      TestDone
    };
    
    // ============ Test class declaration ============
    class CST_RollBlock : public CozmoSimTestController {
      
    private:
      
      virtual s32 UpdateSimInternal() override;
      
      TestState _testState = TestState::Init;
      s32       _cubeID = 0;
    };
    
    // Register class with factory
    REGISTER_COZMO_SIM_TEST_CLASS(CST_RollBlock);
    
    
    // =========== Test class implementation ===========
    
    s32 CST_RollBlock::UpdateSimInternal()
    {
      switch (_testState) {
        case TestState::Init:
        {
          StartMovieConditional("RollBlock");
          // TakeScreenshotsAtInterval("RollBlock", 1.f);
          
          SendMoveHeadToAngle(0, 100, 100);
          SET_TEST_STATE(RollObject);
          break;
        }
        case TestState::RollObject:
        {
          std::vector<s32> objIds = GetAllLightCubeObjectIDs();
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(DEFAULT_TIMEOUT,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                NEAR(GetRobotHeadAngle_rad(), 0, HEAD_ANGLE_TOL),
                                                !objIds.empty())
          {
            ExternalInterface::QueueSingleAction m;
            m.position = QueueActionPosition::NOW;
            m.idTag = 11;
            m.numRetries = 3;
            
            // Roll first LightCube
            _cubeID = objIds[0];
            
            m.action.Set_rollObject(ExternalInterface::RollObject(_cubeID, _defaultTestMotionProfile, 0, false, false, true, false));
            ExternalInterface::MessageGameToEngine message;
            message.Set_QueueSingleAction(m);
            SendMessage(message);
            SET_TEST_STATE(TestDone);
          }
          break;
        }
        case TestState::TestDone:
        {
          // Verify robot has rolled the block
          Pose3d pose;
          GetObjectPose(_cubeID, pose);
          IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(25,
                                                !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                                GetCarryingObjectID() == -1,
                                                pose.GetRotationAngle().IsNear(-1.5f, 0.2f))
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

