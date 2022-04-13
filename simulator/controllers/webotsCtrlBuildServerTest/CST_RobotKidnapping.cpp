/**
* File: CST_RobotKidnapping.cpp
*
* Author: Andrew Stein
* Created: 10/20/15
*
* Description: Tests robot's ability to re-localize itself and rejigger world 
*              origins when being delocalized and then re-seeing existing light cubes.
*
* Copyright: Anki, inc. 2015
*
*/

#include "simulator/game/cozmoSimTestController.h"


namespace Anki {
namespace Vector {
  
  enum class TestState {
    Init,                // Request cube connection
    WaitForCubeConnections,      // Wait for all 2 cubes to be connected
    InitialLocalization, // Localize to Object A
    NotifyKidnap,        // Move robot to new position and delocalize
    Kidnap,              // Wait for confirmation of delocalization
    LocalizeToObjectB,   // See and localize to new Object B
    ReSeeObjectA,        // Drive to re-see Object A, to force re-jiggering of origins
    TestDone
  };
  
  // ============ Test class declaration ============
  class CST_RobotKidnapping : public CozmoSimTestController
  {
  public:
    
    CST_RobotKidnapping();
    
  private:
    
    const Pose3d  _kidnappedPose;
    const f32     _poseDistThresh_mm = 25.f;
    const Radians _poseAngleThresh;
    
    virtual s32 UpdateSimInternal() override;
    
    TestState _testState = TestState::Init;
    
    ExternalInterface::RobotState _robotState;
    
    ObjectID _objectID_A;
    ObjectID _objectID_B;
    
    u32 _numObjectsConnected = 0;
    
    // Message handlers
    virtual void HandleRobotStateUpdate(ExternalInterface::RobotState const& msg) override;
    virtual void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;
  };
  
  // Register class with factory
  REGISTER_COZMO_SIM_TEST_CLASS(CST_RobotKidnapping);
  
  
  // =========== Test class implementation ===========
  CST_RobotKidnapping::CST_RobotKidnapping()
  : _kidnappedPose(-M_PI_2_F, Z_AXIS_3D(), {150.f, -150.f, 0})
  , _poseAngleThresh(DEG_TO_RAD(5.f))
  {
    
  }
  
  s32 CST_RobotKidnapping::UpdateSimInternal()
  {
    switch (_testState) {
      case TestState::Init:
      {
        // TakeScreenshotsAtInterval("Robot Kidnapping", 1.f);

        // Request a cube connection so that we will localize to the cube
        SendForgetPreferredCube();
        SendConnectToCube();
        
        SET_TEST_STATE(WaitForCubeConnections);
        break;
      }
        
      case TestState::WaitForCubeConnections:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_numObjectsConnected == 1, 5) {
          SendMoveHeadToAngle(DEG_TO_RAD(-5), DEG_TO_RAD(360), DEG_TO_RAD(1000));
          SET_TEST_STATE(InitialLocalization);
        }
        break;
      }
        
      case TestState::InitialLocalization:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_objectID_A.IsSet(), 3)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh),
                     "Initial localization failed.");
          
          // Kidnap the robot (move actual robot and just tell it to delocalize
          // as if it has been picked up -- but it doesn't know where it actually
          // is anymore)
          SetActualRobotPose(_kidnappedPose);
        
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }

      case TestState::NotifyKidnap:
      {
        // Sending the delocalize message one tic after actually moving the robot to be sure that no images
        // from the previous pose are processed after the delocalization.
        SendForceDelocalize();
        
        SET_TEST_STATE(Kidnap);
        break;
      }
        
      case TestState::Kidnap:
      {
        // Wait until we see that the robot has gotten the delocalization message
        IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsLocalizedToObject(), 2)
        {
          // Once kidnapping occurs, tell robot to turn to see the other object
          SendTurnInPlace(DEG_TO_RAD(90));
          
          SET_TEST_STATE(LocalizeToObjectB);
        }
        break;
      }
        
      case TestState::LocalizeToObjectB:
      {
        // Wait until we see and localize to the other object
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_objectID_B.IsSet(), 6)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh, _kidnappedPose),
                     "Localization to second object failed.");
          
          // Turn back to see object A
          SendTurnInPlace(DEG_TO_RAD(90));
          
          SET_TEST_STATE(ReSeeObjectA);
        }
        break;
      }
        
      case TestState::ReSeeObjectA:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_robotState.localizedToObjectID == _objectID_A, 3)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh),
                     "Localization after re-seeing first object failed.");
          
          Pose3d poseA, poseB;
          CST_ASSERT(RESULT_OK == GetObjectPose(_objectID_A, poseA),
                     "Failed to get first object's pose.");
          CST_ASSERT(RESULT_OK == GetObjectPose(_objectID_B, poseB),
                     "Failed to get second object's pose.");
          
          const Pose3d poseA_actual(0, Z_AXIS_3D(), {150.f, 0.f, 22.f}, poseA.GetParent());
          const Pose3d poseB_actual(0, Z_AXIS_3D(), {300.f, -150.f, 0.f}, poseB.GetParent());
          CST_ASSERT(poseA.IsSameAs(poseA_actual, _poseDistThresh_mm, _poseAngleThresh),
                     "First object's pose incorrect after re-localization.");
          
          CST_ASSERT(poseB.IsSameAs(poseB_actual, _poseDistThresh_mm, _poseAngleThresh),
                     "Second object's pose incorrect after re-localization.");
          
          SET_TEST_STATE(TestDone);
        }
        break;
      }
      
      case TestState::TestDone:
      {
        
        CST_EXIT();
        break;
      }
    }
    
    return _result;
  }
  
  // ================ Message handler callbacks ==================
  
  void CST_RobotKidnapping::HandleRobotStateUpdate(const ExternalInterface::RobotState &msg)
  {
    _robotState = msg;
    if(_testState == TestState::InitialLocalization)
    {
      _objectID_A = _robotState.localizedToObjectID;
    }
    else if(_testState == TestState::LocalizeToObjectB)
    {
      _objectID_B = _robotState.localizedToObjectID;
    }
  }
  
  void CST_RobotKidnapping::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
  {
    if (msg.connected) {
      ++_numObjectsConnected;
    } else if (_numObjectsConnected > 0) {
      --_numObjectsConnected;
    }
  }
  
  // ================ End of message handler callbacks ==================

} // end namespace Vector
} // end namespace Anki

