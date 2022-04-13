/**
* File: CST_CustomObjects.cpp
*
* Author: Andrew Stein
* Created: 2/3/2017
*
* Description: 1. Define two custom objects (cube and wall) successfully, two unsuccessfully.
*              2. Observe them both in the right poses.
*              3. Turn and see them again in the right poses.
*              4. Wall should be unique and only seen once.
*              5. Cube is not unique so there should now be two.
*              6. Delocalize robot and see two custom cubes.
*              7. Undefine objects and make sure they are removed.
*
*
* Copyright: Anki, inc. 2017
*
*/

#include "engine/actions/basicActions.h"
#include "engine/charger.h"
#include "engine/customObject.h"
#include "engine/robot.h"
#include "simulator/game/cozmoSimTestController.h"


namespace Anki {
namespace Vector {

enum class TestState {
  Init,
  LookAtObjects,
  TurnAndLookDown,
  LookBackUp,
  NotifyKidnap,
  Kidnap,
  SeeCubeInNewOrigin,
  Undefine
};



// ============ Test class declaration ============
class CST_CustomObjects : public CozmoSimTestController
{
public:
  CST_CustomObjects();
  
private:
  
  virtual s32 UpdateSimInternal() override;
  virtual void HandleDefinedCustomObject(const ExternalInterface::DefinedCustomObject& msg) override;
  virtual void HandleRobotDeletedCustomMarkerObjects(const ExternalInterface::RobotDeletedCustomMarkerObjects& msg) override;
  
  void GetDimension(const webots::Node* node, const std::string& name, f32& dim);
  void DefineObjects();
  void CheckPoses();
  void CheckPoseHelper(const ObservableObject* customObj, const ObjectID& objectID);
  
  TestState _testState = TestState::Init;
  
  webots::Node* _wall      = nullptr;
  webots::Node* _cube1     = nullptr;
  webots::Node* _cube2     = nullptr;
  webots::Node* _cube3     = nullptr;
  webots::Node* _charger   = nullptr;
  
  ObjectID _wallID;
  ObjectID _chargerID;
  
  const Pose3d kPoseOrigin;
  const Pose3d kKidnappedRobotPose;
  
  Pose3d _wallPose1;
  Pose3d _wallPose2;
  Pose3d _cubePose1;
  Pose3d _cubePose2;
  Pose3d _cubePose3;
  Pose3d _chargerPose;
  
  static const size_t kNumDefinitions = 4;
  
  f32 _cubeSize_mm          = 0.f;
  f32 _cubeMarkerSize_mm    = 0.f;
  f32 _wallWidth_mm         = 0.f;
  f32 _wallHeight_mm        = 0.f;
  f32 _wallMarkerWidth_mm   = 0.f;
  f32 _wallMarkerHeight_mm  = 0.f;
  
  static constexpr const f32 kDefaultTimeout_sec   = 6.f;
  static constexpr const f32 kRobotAngleTol_deg    = 5.f;
  static constexpr const f32 kDistTolerance_mm     = 15.f;
  static constexpr const f32 kAngleTolerance_deg   = 10.f;
  static constexpr const f32 kReLocRotAngle_deg    = -31.5f;
  
  std::array<bool, kNumDefinitions> _defineResults;
  size_t _numDefinesReceived = 0;
};

// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_CustomObjects);


// =========== Test class implementation ===========

CST_CustomObjects::CST_CustomObjects()
: kPoseOrigin(0, Z_AXIS_3D(), {0.f, 0.f, 0.f})
, kKidnappedRobotPose(M_PI, Z_AXIS_3D(), {-104.f, 136.f, 0.f}, kPoseOrigin)
, _wallPose2(Radians(-2.15f), Z_AXIS_3D(), {60.f, 310.f, 60.f}, kPoseOrigin)
{
  
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
s32 CST_CustomObjects::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      StartMovieConditional("CustomObjects");
      //TakeScreenshotsAtInterval("CustomObjects", 1.f);
      
      _wall      = GetNodeByDefName("CustomWall");
      _cube1     = GetNodeByDefName("CustomCube1");
      _cube2     = GetNodeByDefName("CustomCube2");
      _cube3     = GetNodeByDefName("CustomCube3");
      _charger   = GetNodeByDefName("Charger");
     
      CST_ASSERT(nullptr != _wall,      "CST_CustomObjects.Init.MissingWallNode");
      CST_ASSERT(nullptr != _cube1,     "CST_CustomObjects.Init.MissingCube1Node");
      CST_ASSERT(nullptr != _cube2,     "CST_CustomObjects.Init.MissingCube2Node");
      CST_ASSERT(nullptr != _cube3,     "CST_CustomObjects.Init.MissingCube3Node");
      CST_ASSERT(nullptr != _charger,   "CST_CustomObjects.Init.MissingCharger");
      
      GetDimension(_wall, "width",        _wallWidth_mm);
      GetDimension(_wall, "height",       _wallHeight_mm);
      GetDimension(_wall, "markerWidth",  _wallMarkerWidth_mm);
      GetDimension(_wall, "markerHeight", _wallMarkerHeight_mm);
      
      GetDimension(_cube1, "width",       _cubeSize_mm);
      GetDimension(_cube1, "markerWidth", _cubeMarkerSize_mm);
      
      f32 otherCubeSize_mm = 0.f, otherCubeMarkerSize_mm = 0.f;
      GetDimension(_cube2, "width", otherCubeSize_mm);
      GetDimension(_cube2, "markerWidth", otherCubeMarkerSize_mm);
      
      CST_ASSERT(_cubeSize_mm == otherCubeSize_mm,             "CST_CustomObjects.Init.Cube2SizeMismatch");
      CST_ASSERT(_cubeMarkerSize_mm == otherCubeMarkerSize_mm, "CST_CustomObjects.Init.Cube2MarkerSizeMismatch");
      
      GetDimension(_cube3, "width", otherCubeSize_mm);
      GetDimension(_cube3, "markerWidth", otherCubeMarkerSize_mm);
      
      CST_ASSERT(_cubeSize_mm == otherCubeSize_mm,             "CST_CustomObjects.Init.Cube3SizeMismatch");
      CST_ASSERT(_cubeMarkerSize_mm == otherCubeMarkerSize_mm, "CST_CustomObjects.Init.Cube3MarkerSizeMismatch");
      
      _wallPose1     = GetPose3dOfNode(_wall);
      _cubePose1     = GetPose3dOfNode(_cube1);
      _cubePose2     = GetPose3dOfNode(_cube2);
      _cubePose3     = GetPose3dOfNode(_cube3);
      _chargerPose   = GetPose3dOfNode(_charger);
      
      _wallPose1.SetParent(kPoseOrigin);
      _cubePose1.SetParent(kPoseOrigin);
      _cubePose2.SetParent(kPoseOrigin);
      _cubePose3.SetParent(kPoseOrigin);
      _chargerPose.SetParent(kPoseOrigin);
      
      // Define the custom objects
      DefineObjects();
      
      // Request a cube connection
      SendForgetPreferredCube();
      SendConnectToCube();
      
      SendMoveHeadToAngle(0, 100.f, 100.f);
      SET_TEST_STATE(LookAtObjects);
      break;
    }
      
    case TestState::LookAtObjects:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            _numDefinesReceived == kNumDefinitions,
                                            _defineResults[0] == true,  // Good cube defintion
                                            _defineResults[1] == false, // Can't reuse Star marker
                                            _defineResults[2] == false, // Can't overwrite built-in types
                                            _defineResults[3] == true,  // Good wall defintion
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            NEAR(GetRobotHeadAngle_rad(), 0, HEAD_ANGLE_TOL),
                                            GetNumObjects() == 2)
      {
        CheckPoses();
        
        // Turn and look down in parallel:
        using namespace ExternalInterface;
        
        const uint32_t kIdTag = 1;
        const uint8_t kNumRetries = 0;
        const bool kIsParallel = true;
        const QueueActionPosition kPosition = QueueActionPosition::NOW;
        const std::vector<RobotActionUnion> kActions{
          RobotActionUnion(TurnInPlace(DEG_TO_RAD(90), 0.f, 0.f, POINT_TURN_ANGLE_TOL, false)),
          RobotActionUnion(SetHeadAngle(MIN_HEAD_ANGLE, 100.f, 100.f, 0.f)),
        };
        
        SendMessage(MessageGameToEngine(QueueCompoundAction(kIdTag,
                                                            kNumRetries,
                                                            kIsParallel,
                                                            kPosition,
                                                            kActions)));
        
        SET_TEST_STATE(TurnAndLookDown);
      }
      break;
    }
      
    case TestState::TurnAndLookDown:
    {
      const Radians& currentOrientation = GetRobotPose().GetRotation().GetAngleAroundZaxis();
      
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            NEAR(currentOrientation.getDegrees(), 90, kRobotAngleTol_deg),
                                            NEAR(GetRobotHeadAngle_rad(), MIN_HEAD_ANGLE, HEAD_ANGLE_TOL))
      {
        SetNodePose(_wall, _wallPose2);
        SendMoveHeadToAngle(0, 100.f, 100.f);
        SET_TEST_STATE(LookBackUp);
      }
      break;
    }
      
    case TestState::LookBackUp:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            !IsRobotStatus(RobotStatusFlag::IS_MOVING),
                                            NEAR(GetRobotHeadAngle_rad(), 0, HEAD_ANGLE_TOL),
                                            GetNumObjects() == 5,
                                            IsLocalizedToObject(),
                                            HasXSecondsPassedYet(2.0)) // Allow some time to observe the wall in its new pose
      {
        CheckPoses();
        
        // Kidnap the robot (move actual robot and just tell it to delocalize
        // as if it has been picked up -- but it doesn't know where it actually
        // is anymore)
        SetActualRobotPose(kKidnappedRobotPose);
        
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
      // Wait for delocalization to be received
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            !IsLocalizedToObject(),
                                            GetNumObjects() == 0)
      {
        // Turn to look at side-by-side custom cubes
        SendTurnInPlace(-1.26f);
        
        SET_TEST_STATE(SeeCubeInNewOrigin);
      }
      break;
    }
      
    case TestState::SeeCubeInNewOrigin:
    {
      // Wait to see the two custom cubes
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            GetNumObjects() == 2)
      {
        SendMoveHeadToAngle(0.f, 100.f, 100.f);
        
        using namespace ExternalInterface;
        SendMessage(MessageGameToEngine(UndefineAllCustomMarkerObjects()));
        
        SET_TEST_STATE(Undefine);
      }
      break;
    }
      
    case TestState::Undefine:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(kDefaultTimeout_sec,
                                            GetNumObjects()==0,
                                            _numDefinesReceived==0)
      {
        // TODO: Add test state where we look at a custom object but no longer instantiate it?
        
        StopMovie();
        CST_EXIT();
      }
      break;
    }
  }
  return _result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CST_CustomObjects::GetDimension(const webots::Node* node, const std::string& name, f32& dim_mm)
{
  CST_ASSERT(nullptr != node, "CST_CustomObjects.GetDimension.NullNode");
  
  webots::Field* field = node->getField(name);
  
  CST_ASSERT(nullptr != field, "CST_CustomObjects.GetDimension.NullField");
  
  dim_mm = M_TO_MM(field->getSFFloat());
  
  CST_ASSERT(Util::IsFltGT(dim_mm, 0.f), "CST_CustomObjects.GetDimension.ZeroDimension");
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CST_CustomObjects::DefineObjects()
{
  using namespace ExternalInterface;
  
  DefineCustomCube defineCube(ObjectType::CustomType00,
                              CustomObjectMarker::Circles2,
                              _cubeSize_mm,
                              _cubeMarkerSize_mm, _cubeMarkerSize_mm,
                              false);
  
  SendMessage(MessageGameToEngine(std::move(defineCube)));
  
  
  // This one should fail: we can't reuse TwoCircles on a different object
  DefineCustomWall bogusWall(ObjectType::CustomType02,
                             CustomObjectMarker::Circles2,
                             1.f, 1.f,
                             1.f, 1.f,
                             false);
  
  SendMessage(MessageGameToEngine(std::move(bogusWall)));
  
  // This one should also fail: bad type specified (can't overwrite built-in types)
  DefineCustomWall bogusCube(ObjectType::Block_LIGHTCUBE1,
                             CustomObjectMarker::Triangles3,
                             1.f, 1.f,
                             1.f, 1.f,
                             false);
  
  SendMessage(MessageGameToEngine(std::move(bogusCube)));
  
  // This definition should succeed
  DefineCustomWall defineWall(ObjectType::CustomType01,
                              CustomObjectMarker::Diamonds4,
                              _wallWidth_mm, _wallHeight_mm,
                              _wallMarkerWidth_mm, _wallMarkerHeight_mm,
                              true);
  
  SendMessage(MessageGameToEngine(std::move(defineWall)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CST_CustomObjects::CheckPoseHelper(const ObservableObject* object, const ObjectID& objectID)
{
  Pose3d observedPose;
  const Result result = GetObjectPose(objectID, observedPose);
  
  CST_ASSERT(RESULT_OK == result, "CST_CustomObjects.ChechPoses.FailedToGetObjectPose");
  CST_ASSERT(nullptr != object, "CST_CustomObjects.CheckPoses.NullCustomObj");
  
  const Pose3d& truePose = object->GetPose();
  
  // Assume we have no origin issues
  observedPose.SetParent(truePose.GetParent());
  
  Point3f Tdiff;
  Radians angleDiff;
  const bool isPoseSame = truePose.IsSameAs_WithAmbiguity(observedPose,
                                                          object->GetRotationAmbiguities(),
                                                          Point3f(kDistTolerance_mm),
                                                          DEG_TO_RAD(kAngleTolerance_deg),
                                                          Tdiff, angleDiff);
  
  if(!isPoseSame)
  {
    PRINT_NAMED_ERROR("CST_CustomObjects.CheckPoses.PoseMismatch",
                      "%s %d: Tdiff=(%.2f,%.2f,%.2f) (Thresh=%.1f), AngleDiff=%.1fdeg (Thresh=%.1f)",
                      EnumToString(object->GetType()), objectID.GetValue(),
                      Tdiff.x(), Tdiff.y(), Tdiff.z(), kDistTolerance_mm,
                      angleDiff.getDegrees(), kAngleTolerance_deg);
    CST_ASSERT(false, "CST_CustomObjects.CheckPoses.PoseMismatch");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - -
void CST_CustomObjects::CheckPoses()
{
  // Check wall:
  {
    auto wallIDs = GetAllObjectIDsByType(ObjectType::CustomType01);
    CST_ASSERT(wallIDs.size() == 1, "CST_CustomObjects.CheckPoses.ExpectingOneWall");
    
    CustomObject *customObj = CustomObject::CreateWall(ObjectType::CustomType01,
                                                       CustomObjectMarker::Diamonds4,
                                                       _wallWidth_mm, _wallHeight_mm,
                                                       _wallMarkerWidth_mm, _wallMarkerHeight_mm,
                                                       true);
    
    const Pose3d* whichWallPose = nullptr;
    switch(_testState)
    {
      case TestState::LookAtObjects:
        whichWallPose = &_wallPose1;
        break;
    
      case TestState::LookBackUp:
        whichWallPose = &_wallPose2;
        break;
        
      default:
        CST_ASSERT(false, "CST_CustomObjects.CheckPoses.Wall.BadTestState");
    }
    
    customObj->InitPose(*whichWallPose, PoseState::Known);
    
    if(_wallID.IsUnknown())
    {
      // First time set _wallID
      _wallID = wallIDs.front();
    }
    else
    {
      // Remaining times, verify the ID has remained consistent, since it is marked unique
      CST_ASSERT(_wallID == wallIDs.front(), "CST_CustomObject.CheckPoses.WallIDChanged");
    }
    
    CheckPoseHelper(customObj, _wallID);
    Util::SafeDelete(customObj);
  }
  
  // Check cube:
  {
    auto customCubeIDs = GetAllObjectIDsByType(ObjectType::CustomType00);
    
    struct PoseAndID {
      const Pose3d* cubePose;
      const ObjectID& objectID;
    };
    
    std::vector<PoseAndID> posesAndIDs{{ .cubePose = &_cubePose1, .objectID = customCubeIDs[0] }};
    
    if(_testState >= TestState::LookBackUp)
    {
      posesAndIDs.emplace_back(PoseAndID{ .cubePose = &_cubePose2, .objectID = customCubeIDs[1] });
    }

    for(auto & poseAndID : posesAndIDs)
    {
      CustomObject* customCube = CustomObject::CreateCube(ObjectType::CustomType00,
                                                          CustomObjectMarker::Circles2,
                                                          _cubeSize_mm,
                                                          _cubeMarkerSize_mm, _cubeMarkerSize_mm,
                                                          false);
      
      customCube->InitPose(*poseAndID.cubePose, PoseState::Known);
      CheckPoseHelper(customCube, poseAndID.objectID);
      
      Util::SafeDelete(customCube);
    }
  }
  
}

// ================ Message handler callbacks ==================

void CST_CustomObjects::HandleDefinedCustomObject(const ExternalInterface::DefinedCustomObject& msg)
{
  _defineResults[_numDefinesReceived] = msg.success;
  ++_numDefinesReceived;
}
      
void CST_CustomObjects::HandleRobotDeletedCustomMarkerObjects(const ExternalInterface::RobotDeletedCustomMarkerObjects& msg)
{
  _numDefinesReceived = 0;
}

// ================ End of message handler callbacks ==================

} // end namespace Vector
} // end namespace Anki

