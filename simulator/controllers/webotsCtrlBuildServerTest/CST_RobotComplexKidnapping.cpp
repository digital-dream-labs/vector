/**
* File: CST_RobotKidnappingComplex.cpp
*
* Author: Andrew Stein
* Created: 7/6/16
*
* Description: Tests robot's ability to re-localize itself and rejigger world 
*              origins when being delocalized and then re-seeing existing light cubes.
*
*   This is the layout, showing three blocks, A, B, and C. 
*   The robot starts at R0 facing the direction indicated.
*   The poses R1-R3 are the "kidnap" poses.
*   All blocks/poses lie on a grid as indiciated.
*
*        <R2
*
*
*        +-+      ^               +-+
*        |C|      R3       R0>    |A|
*        +-+                      +-+
*                +-+
*                |B|       R1
*                +-+       v
*
*         |<----->|
*       grid spacing
*
* Copyright: Anki, inc. 2016
*
*/

#include "simulator/game/cozmoSimTestController.h"


namespace Anki {
namespace Vector {
  
  enum class TestState {
    MoveHead,                    // Look up to see Object A
    WaitForCubeConnections,      // Wait for all 3 cubes to be connected
    InitialLocalization,         // Localize to Object A
    NotifyKidnap ,               // Move robot to new position and delocalize
    Kidnap,                      // Wait for confirmation of delocalization
    FinishTurn,                  // Wait for turn in place after kidnap to complete
    LocalizeToObjectB,           // Kidnap to R1, see and localize to new Object B
    ReSeeObjectA,                // Turn to re-see Object A, to force re-jiggering of origins
    LocalizeToObjectC,           // Kidnap to R2, turn to see C, localizing to it
    SeeObjectAWithoutLocalizing, // Kidnap to R3, see A, but don't localize (too far)
    ReLocalizeToObjectB,         // Turn towards C, relocalize and bring A into frame
    ReLocalizeToObjectC,         // Turn towards B, relocalize and bring A and C into frame
    
    TestDone
  };
  
  // ============ Test class declaration ============
  class CST_RobotKidnappingComplex : public CozmoSimTestController
  {
  public:
    
    CST_RobotKidnappingComplex();
    
  private:
    
    bool CheckObjectPoses(std::vector<int>&& IDs, const char* debugStr);
    
    const f32     _gridSpacing_mm = 150.f;
    
    const Pose3d _fakeOrigin;
    
    const Pose3d  _kidnappedPose1;
    const Pose3d  _kidnappedPose2;
    const Pose3d  _kidnappedPose3;
    
    const Pose3d _poseA_actual;
    const Pose3d _poseB_actual;
    const Pose3d _poseC_actual;
    const std::vector<const Pose3d*> _objectPosesActual;
    
    const f32     _poseDistThresh_mm = 44.f; // within one block size
    const Radians _poseAngleThresh;
    
    
    virtual s32 UpdateSimInternal() override;
    
    TestState _testState = TestState::MoveHead;
    TestState _nextState;
    f32       _turnAngle_deg;
    double    _kidnapStartTime;
    
    ExternalInterface::RobotState _robotState;
    
    ObjectID _objectID_A;
    ObjectID _objectID_B;
    ObjectID _objectID_C;
    
    std::set<ObjectID> _objectsSeen;
    std::map<ObjectID, u8> _objectIDToIdx;
    
    bool _turnInPlaceDone = false;
    bool _isMoving = false;
    
    u32  _numObjectsConnected = 0;
    
    // Message handlers
    virtual void HandleRobotStateUpdate(ExternalInterface::RobotState const& msg) override;
    virtual void HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg) override;
    virtual void HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction &msg) override;
    virtual void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;
    
  };
  
  // Register class with factory
  REGISTER_COZMO_SIM_TEST_CLASS(CST_RobotKidnappingComplex);
  
  
  // =========== Test class implementation ===========
  CST_RobotKidnappingComplex::CST_RobotKidnappingComplex()
  : _kidnappedPose1(-M_PI_2_F, Z_AXIS_3D(), {0, -_gridSpacing_mm, 0})
  , _kidnappedPose2( M_PI_F,   Z_AXIS_3D(), {-2*_gridSpacing_mm, _gridSpacing_mm, 0})
  , _kidnappedPose3( M_PI_2_F, Z_AXIS_3D(), {-_gridSpacing_mm, 0, 0})
  , _poseA_actual(0, Z_AXIS_3D(), {_gridSpacing_mm, 0.f, 22.f}, _fakeOrigin)
  , _poseB_actual(0, Z_AXIS_3D(), {-_gridSpacing_mm, -_gridSpacing_mm, 22.f}, _fakeOrigin)
  , _poseC_actual(0, Z_AXIS_3D(), {-2*_gridSpacing_mm, 0.f, 22.f}, _fakeOrigin)
  , _objectPosesActual{&_poseA_actual, &_poseB_actual, &_poseC_actual}
  , _poseAngleThresh(DEG_TO_RAD(30.f))
  {
    
  }
  
  s32 CST_RobotKidnappingComplex::UpdateSimInternal()
  {
    switch (_testState)
    {
      case TestState::MoveHead:
      {
        SendMoveHeadToAngle(DEG_TO_RAD(-5), DEG_TO_RAD(360), DEG_TO_RAD(1000));
        
        ExternalInterface::EnableLightStates m;
        m.enable = false;
        ExternalInterface::MessageGameToEngine message;
        message.Set_EnableLightStates(m);
        SendMessage(message);
        
        SET_TEST_STATE(WaitForCubeConnections);
        break;
      }
        
      case TestState::WaitForCubeConnections:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_numObjectsConnected == 3, 3) {
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
          SetActualRobotPose(_kidnappedPose1);
        
          _turnAngle_deg = -90;
          _nextState = TestState::LocalizeToObjectB;
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }

      case TestState::NotifyKidnap:
      {
        // Sending the delocalize message one tic after actually moving the robot to be sure that no images
        // from the previous pose are processed after the delocalization.
        SendForceDelocalize();
        
        _kidnapStartTime = GetSupervisor().getTime();
        SET_TEST_STATE(Kidnap);
        break;
      }
        
      case TestState::Kidnap:
      {
        // Wait until we see that the robot has gotten the delocalization message
        if(CONDITION_WITH_TIMEOUT_ASSERT(!IsLocalizedToObject(), _kidnapStartTime, 2))
        {
          // Once kidnapping occurs, tell robot to turn to see the other object
          _objectsSeen.clear();
          _turnInPlaceDone = false;
          SendTurnInPlace(DEG_TO_RAD(_turnAngle_deg));
          
          _kidnapStartTime = GetSupervisor().getTime();
          SET_TEST_STATE(FinishTurn);
        }
        break;
      }
        
      case TestState::FinishTurn:
      {
        if(CONDITION_WITH_TIMEOUT_ASSERT(_turnInPlaceDone && !_isMoving, _kidnapStartTime, 6))
        {
          _testState = _nextState;
        }
        break;
      }
        
      case TestState::LocalizeToObjectB:
      {
        // Wait until we see and localize to the other object
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_objectID_B.IsSet(), 2)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh, _kidnappedPose1),
                     "Localization to second object failed.");
          
          // We should only know about one object now: Object B
          CST_ASSERT(CheckObjectPoses({_objectID_B}, "LocalizeToObjectB"),
                     "LocalizeToObjectB: Object pose checks failed");
          
          // Turn back to see object A
          _turnInPlaceDone = false;
          SendTurnInPlace(DEG_TO_RAD(225));
          
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
          
          // We should only know about two objects now: Objects A and B
          CST_ASSERT(CheckObjectPoses({_objectID_A,_objectID_B}, "ReSeeObjectA"),
                     "ReSeeObjectA: Object pose checks failed");
          
          // Kidnap the robot (move actual robot and just tell it to delocalize
          // as if it has been picked up -- but it doesn't know where it actually
          // is anymore)
          SetActualRobotPose(_kidnappedPose2);
          
          _nextState = TestState::LocalizeToObjectC;
          _turnAngle_deg = 90;
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }
        
      case TestState::LocalizeToObjectC:
      {
        IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(3,
                                              _objectID_C.IsSet(),
                                              _robotState.localizedToObjectID == _objectID_C)
        {
          // We should only know about one object now: Object C
          CST_ASSERT(CheckObjectPoses({_objectID_C}, "LocalizeToObjectC"),
                     "LocalizeToObjectC: Object pose checks failed");
          
          // Kidnap the robot (move actual robot and just tell it to delocalize
          // as if it has been picked up -- but it doesn't know where it actually
          // is anymore)
          SetActualRobotPose(_kidnappedPose3);
          
          _nextState = TestState::SeeObjectAWithoutLocalizing;
          _turnAngle_deg = -90;
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }
        
      case TestState::SeeObjectAWithoutLocalizing:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(!_objectsSeen.empty(), 3)
        {
          CST_ASSERT(_robotState.localizedToObjectID < 0,
                     "SeeObjectAWithoutLocalizing: Should not localize to object A - should be too far");
          
          // We should only know about one object now: Objects A
          CST_ASSERT(CheckObjectPoses({_objectID_A}, "SeeObjectAWithoutLocalizing"),
                     "SeeObjectAWithoutLocalizing: Object pose checks failed");

          // Turn towards C again
          _turnInPlaceDone = false;
          SendTurnInPlace(DEG_TO_RAD(179.5f));
          
          SET_TEST_STATE(ReLocalizeToObjectC);
        }
        break;
      }
        
        
      case TestState::ReLocalizeToObjectC:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_robotState.localizedToObjectID == _objectID_C, 3)
        {
          // We should only know about A and C now
          CST_ASSERT(CheckObjectPoses({_objectID_A,_objectID_C}, "RelocalizeToObjectC"),
                     "RelocalizeToObjectC: Object pose checks failed");
          
          // Turn towards B again
          _turnInPlaceDone = false;
          SendTurnInPlace(DEG_TO_RAD(90));
          
          SET_TEST_STATE(ReLocalizeToObjectB);
        }
        break;
      }
        
      case TestState::ReLocalizeToObjectB:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_robotState.localizedToObjectID == _objectID_B, 3)
        {
          // We should know about all three objects now
          CST_ASSERT(CheckObjectPoses({_objectID_A,_objectID_B,_objectID_C}, "RelocalizeToObjectB"),
                     "RelocalizeToObjectC: Object pose checks failed");
          
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
  
  bool CST_RobotKidnappingComplex::CheckObjectPoses(std::vector<int>&& IDs, const char* debugStr)
  {
    if(_objectsSeen.size() != IDs.size())
    {
      PRINT_NAMED_WARNING("CST_RobotKidnappingComplex.CheckObjectPoses",
                          "%s: Expecting to know about %zu objects, not %zu",
                          debugStr, IDs.size(), _objectsSeen.size());
      return false;
    }
    
    for(auto & objectID : IDs)
    {
      if(!IsObjectPoseWrtRobotCorrect(objectID, *_objectPosesActual[_objectIDToIdx[objectID]], 
                                      _poseDistThresh_mm, _poseAngleThresh, debugStr))
      {
        return false;
      }
    }
    
    return true;
  }
  
  // ================ Message handler callbacks ==================
  
  void CST_RobotKidnappingComplex::HandleRobotStateUpdate(const ExternalInterface::RobotState &msg)
  {
    _robotState = msg;
    
    _isMoving = msg.status & static_cast<uint16_t>(RobotStatusFlag::IS_MOVING);
    
    switch(_testState)
    {
      case TestState::InitialLocalization:
        _objectID_A = _robotState.localizedToObjectID;
        _objectIDToIdx[_objectID_A] = 0;
        break;
        
      case TestState::LocalizeToObjectB:
        _objectID_B = _robotState.localizedToObjectID;
        _objectIDToIdx[_objectID_B] = 1;
        break;
    
      case TestState::LocalizeToObjectC:
        _objectID_C = _robotState.localizedToObjectID;
        _objectIDToIdx[_objectID_C] = 2;
        break;
        
      default:
        break;
    }
    
  }  // ================ End of message handler callbacks ==================

  void CST_RobotKidnappingComplex::HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg)
  {
    _objectsSeen.insert(msg.objectID);
  }
  
  void CST_RobotKidnappingComplex::HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction &msg)
  {
    if(msg.actionType == RobotActionType::TURN_IN_PLACE)
    {
      _turnInPlaceDone = true;
    }
  }
  
  void CST_RobotKidnappingComplex::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
  {
    if (msg.connected) {
      ++_numObjectsConnected;
    } else if (_numObjectsConnected > 0) {
      --_numObjectsConnected;
    }
  }
  
} // end namespace Vector
} // end namespace Anki

