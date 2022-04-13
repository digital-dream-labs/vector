/**
* File: CST_MultiObjectLocalization.cpp
*
* Author: Andrew Stein
* Created: 7/13/16
*
* Description: Tests robot's ability to re-localize itself and rejigger world 
*              origins when being delocalized and then re-seeing existing light cubes
*              one at a time and then all together (from pose R3).
*
*   This is the layout, showing three blocks, A, B, and C. 
*   The robot starts at R0 facing the direction indicated.
*   The poses R1-R3 are the "kidnap" poses.
*
*
*                +-+   +-+   +-+
*            R2> |C|   |A|   |B| <R1
*                +-+   +-+   +-+
*                       ^
*                       R0    __
*                            /\
*                              R3
*       
*
*
* Copyright: Anki, inc. 2016
*
*/

#include "simulator/game/cozmoSimTestController.h"


namespace Anki {
namespace Vector {
  
  enum class TestState {
    MoveHead,                    // Look up to see Object A
    InitialLocalization,         // Localize to Object A
    NotifyKidnap ,               // Move robot to new position and delocalize
    Kidnap,                      // Wait for confirmation of delocalization
    LocalizeToObjectB,           // Kidnap to R1, see and localize to new Object B
    LocalizeToObjectC,           // Kidnap to R2, turn to see C, localizing to it
    LookBackDown,
    LocalizeToAll,
    
    TestDone
  };
  
  // ============ Test class declaration ============
  class CST_MultiObjectLocalization : public CozmoSimTestController
  {
  public:
    
    CST_MultiObjectLocalization();
    
  private:
    
    bool CheckObjectPoses(std::vector<int>&& IDs, const char* debugStr);
    bool HasRelocalizedTo(const ObjectID& objectID) const;
    
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
    f32       _headAngle_deg;
    double    _kidnapStartTime;
    
    ObjectID _objectID_A;
    ObjectID _objectID_B;
    ObjectID _objectID_C;
    
    std::set<ObjectID> _objectsSeen;
    std::map<ObjectID, u8> _objectIDToIdx;
    
    bool _turnInPlaceDone = false;
    bool _moveHeadDone = false;
    
    // Message handlers
    virtual void HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg) override;
    virtual void HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction &msg) override;
    
  };
  
  // Register class with factory
  REGISTER_COZMO_SIM_TEST_CLASS(CST_MultiObjectLocalization);
  
  
  // =========== Test class implementation ===========
  CST_MultiObjectLocalization::CST_MultiObjectLocalization()
  : _kidnappedPose1( 0, Z_AXIS_3D(), {100.f, -175.f, 0.f})
  , _kidnappedPose2( 0, Z_AXIS_3D(), {100.f,  175.f, 0.f})
  , _kidnappedPose3( 0.47f, Z_AXIS_3D(),  {-56.74f, -90.0003f, 0.f})
  , _poseA_actual(0, Z_AXIS_3D(), {100.f,   0.f, 22.f}, _fakeOrigin)
  , _poseB_actual(0, Z_AXIS_3D(), {100.f, -75.f, 22.f}, _fakeOrigin)
  , _poseC_actual(0, Z_AXIS_3D(), {100.f,  75.f, 22.f}, _fakeOrigin)
  , _objectPosesActual{&_poseA_actual, &_poseB_actual, &_poseC_actual}
  , _poseAngleThresh(DEG_TO_RAD(15.f))
  {
    
  }
  
  inline bool CST_MultiObjectLocalization::HasRelocalizedTo(const ObjectID& objectID) const
  {
    const bool done = (_turnInPlaceDone &&
                       _moveHeadDone &&
                       objectID.IsSet() &&
                       GetRobotState().localizedToObjectID == objectID);
    return done;
  }
  
  s32 CST_MultiObjectLocalization::UpdateSimInternal()
  {
    switch (_testState)
    {
      case TestState::MoveHead:
      {
        SendMoveHeadToAngle(DEG_TO_RAD(-5), DEG_TO_RAD(360), DEG_TO_RAD(1000));
        // TakeScreenshotsAtInterval("MultiObjectLocalization", 1.f);
        _turnInPlaceDone = true;
        _moveHeadDone = false;
        SET_TEST_STATE(InitialLocalization);
        break;
      }
        
      case TestState::InitialLocalization:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(HasRelocalizedTo(_objectID_A), 3)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh),
                     "Initial localization failed.");
          
          // We should only know about one object now: Object A
          CST_ASSERT(CheckObjectPoses({_objectID_A}, "InitialLocalization"),
                     "InitialLocalization: Object pose checks failed");
          
          // Kidnap the robot (move actual robot and just tell it to delocalize
          // as if it has been picked up -- but it doesn't know where it actually
          // is anymore)
          SetActualRobotPose(_kidnappedPose1);
        
          _turnAngle_deg = 90;
          _headAngle_deg = -5;
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
          _moveHeadDone = false;
          SendTurnInPlace(DEG_TO_RAD(_turnAngle_deg));
          SendMoveHeadToAngle(DEG_TO_RAD(_headAngle_deg), DEG_TO_RAD(360), DEG_TO_RAD(1000));
                              
          _testState = _nextState;
        }
        break;
      }
        
      case TestState::LocalizeToObjectB:
      {
        // Wait until we see and localize to the other object
        IF_CONDITION_WITH_TIMEOUT_ASSERT(HasRelocalizedTo(_objectID_B), 6)
        {
          CST_ASSERT(IsRobotPoseCorrect(_poseDistThresh_mm, _poseAngleThresh, _kidnappedPose1),
                     "Localization to second object failed.");
          
          // We should only know about one object now: Object B
          CST_ASSERT(CheckObjectPoses({_objectID_B}, "LocalizeToObjectB"),
                     "LocalizeToObjectB: Object pose checks failed");
          
          SetActualRobotPose(_kidnappedPose2);
          _turnAngle_deg = -90;
          _headAngle_deg = -5;
          _nextState = TestState::LocalizeToObjectC;
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }
        
      case TestState::LocalizeToObjectC:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(HasRelocalizedTo(_objectID_C), 3)
        {
//          const Pose3d checkPose = _kidnappedPose2 * GetRobotPose();
//          CST_ASSERT(checkPose.IsSameAs(GetRobotPoseActual(), _poseDistThresh_mm, _poseAngleThresh),
//                     "Localization to third object failed.");
          
          // We should only know about one object now: Object C
          CST_ASSERT(CheckObjectPoses({_objectID_C}, "LocalizeToObjectC"),
                     "LocalizeToObjectC: Object pose checks failed");
          
          // Look up
          SendMoveHeadToAngle(DEG_TO_RAD(45), DEG_TO_RAD(360), DEG_TO_RAD(1000));
          _moveHeadDone = false;
          
          SET_TEST_STATE(LookBackDown);
        }
        break;
      }
                              
      case TestState::LookBackDown:
      {
        IF_CONDITION_WITH_TIMEOUT_ASSERT(_moveHeadDone, 3)
        {
          // Kidnap the robot (move actual robot and just tell it to delocalize
          // as if it has been picked up -- but it doesn't know where it actually
          // is anymore)
          SetActualRobotPose(_kidnappedPose3);
          
          
          _nextState = TestState::LocalizeToAll;
          _turnAngle_deg = 0;
          _headAngle_deg = -5;
          SET_TEST_STATE(NotifyKidnap);
        }
        break;
      }
        
        
      case TestState::LocalizeToAll:
      {
        // Should be localized to B, because it is the closest
        IF_CONDITION_WITH_TIMEOUT_ASSERT(HasRelocalizedTo(_objectID_B), 3)
        {
          // We should know about A, B, and C now
          CST_ASSERT(CheckObjectPoses({_objectID_A,_objectID_B,_objectID_C}, "LocalizeToAll"),
                     "LocalizeToAll: Object pose checks failed");
          
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
  
  
  bool CST_MultiObjectLocalization::CheckObjectPoses(std::vector<int>&& IDs, const char* debugStr)
  {
    if(_objectsSeen.size() < IDs.size())
    {
      PRINT_NAMED_WARNING("CST_MultiObjectLocalization.CheckObjectPoses",
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
  
  // ================ End of message handler callbacks ==================

  void CST_MultiObjectLocalization::HandleRobotObservedObject(ExternalInterface::RobotObservedObject const& msg)
  {
    _objectsSeen.insert(msg.objectID);
    
    switch(_testState)
    {
      case TestState::InitialLocalization:
        _objectID_A = msg.objectID;
        _objectIDToIdx[_objectID_A] = 0;
        break;
        
      case TestState::LocalizeToObjectB:
        _objectID_B = msg.objectID;
        _objectIDToIdx[_objectID_B] = 1;
        break;
        
      case TestState::LocalizeToObjectC:
        _objectID_C = msg.objectID;
        _objectIDToIdx[_objectID_C] = 2;
        break;
        
      default:
        break;
    }

  }
  
  void CST_MultiObjectLocalization::HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction &msg)
  {
    if(msg.actionType == RobotActionType::TURN_IN_PLACE)
    {
      _turnInPlaceDone = true;
    }
    else if(msg.actionType == RobotActionType::MOVE_HEAD_TO_ANGLE)
    {
      _moveHeadDone = true;
    }
    
  }
  
} // end namespace Vector
} // end namespace Anki

