#include "simulator/game/cozmoSimTestController.h"
#include "coretech/common/shared/math/rotation.h"

namespace Anki {
namespace Vector {
enum class TestState {
  Init,
  ExecuteStraightPath,
  IntroduceObstacle,
  VerifyDriveToPoseCompleted,
  VerifyObstacleAvoidance,
  Exit
};

class CST_IntroducedObstacleAvoidance : public CozmoSimTestController
{
public:
  CST_IntroducedObstacleAvoidance();

private:
  s32 UpdateSimInternal() override;

  TestState _testState = TestState::Init;
  s32 _result = 0;

  bool _driveToPoseSucceeded = false;

  webots::Node* _duckNode = nullptr;
  
  void HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg) override;
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_IntroducedObstacleAvoidance);

CST_IntroducedObstacleAvoidance::CST_IntroducedObstacleAvoidance() {}

s32 CST_IntroducedObstacleAvoidance::UpdateSimInternal()
{
  const f32 kHeadLookupAngle_rad = DEG_TO_RAD(7);

  const int kDuckHeight_mm = 25;
  const Pose3d kObstructingPose = {M_PI_2_F, {1, 0, 0}, {200, 0, kDuckHeight_mm}, _webotsOrigin};
  const Pose3d kRobotDestination = {0, {0, 0, 1}, {600, 0, 0}, _webotsOrigin};

  switch (_testState) {
    case TestState::Init:
    {
      SendMoveHeadToAngle(kHeadLookupAngle_rad, 100, 100);

      _duckNode = GetNodeByDefName("duck");
      CST_ASSERT(_duckNode != nullptr, "null duck");
      
      // We do not want to play driving animations, so push empty anims
      SendPushDrivingAnimations("webots_test",
                                AnimationTrigger::Count,
                                AnimationTrigger::Count,
                                AnimationTrigger::Count);
      
      SET_TEST_STATE(ExecuteStraightPath);
      break;
    }

    case TestState::ExecuteStraightPath:
    {
      // Note that this does not drive cozmo to the ground truth pose of kRobotDestination, but
      // rather it will be relative to where it starts. That is, if cozmo starts at x=-200mm and the
      // translation in the pose is x=+500mm, he will travel to a ground truth position of +300mm
      // (500-200), and his robot estimated pose will be +500mm.
      SendExecutePathToPose(kRobotDestination, _defaultTestMotionProfile);
      SET_TEST_STATE(IntroduceObstacle);
      break;
    }

    case TestState::IntroduceObstacle:
    {
      float distanceToObstructingPose_mm = 0.f;
      const bool result = ComputeDistanceBetween(GetRobotPoseActual(), kObstructingPose, distanceToObstructingPose_mm);
      CST_ASSERT(result, "Failed computing distance between robot pose and obstructing pose");
      IF_CONDITION_WITH_TIMEOUT_ASSERT(distanceToObstructingPose_mm < 150.0, 10){
        // Put the rubber duck in the way of the robot path.
        SetNodePose(_duckNode, kObstructingPose);
        _driveToPoseSucceeded = false;  // reset var just before we check for it in the next stage just in case
        SET_TEST_STATE(VerifyDriveToPoseCompleted);
      }
      break;
    }

    case TestState::VerifyDriveToPoseCompleted:
    {
      // Takes about 12 seconds for robot to complete the path, set timeout at 20 for some leeway.
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_driveToPoseSucceeded, 20){
        SET_TEST_STATE(VerifyObstacleAvoidance);
      }
      break;
    }

    case TestState::VerifyObstacleAvoidance:
    {
      const Point3f kDistanceThreshold = {5, 5, 5};
      const Radians kAngleThreshold = DEG_TO_RAD(10);

      const Pose3d obstaclePoseActual = GetPose3dOfNode(_duckNode);
      
      CST_ASSERT(obstaclePoseActual.IsSameAs(kObstructingPose,
                                             kDistanceThreshold, kAngleThreshold),
                 "The rubber duck was moved when it should have been avoided by the robot.")

      const Pose3d robotPoseActual = GetRobotPoseActual();
      
      CST_ASSERT(robotPoseActual.IsSameAs(kRobotDestination, kDistanceThreshold, kAngleThreshold),
                 "The robot didn't reach its destination: expected " <<
                 kRobotDestination.GetTranslation().ToString().c_str() <<
                 ", got " << robotPoseActual.GetTranslation().ToString());

      SET_TEST_STATE(Exit);
      break;
    }

    case TestState::Exit:
    {
      CST_EXIT();
      break;
    }
  }

  return _result;
}

void CST_IntroducedObstacleAvoidance::HandleRobotCompletedAction(const ExternalInterface::RobotCompletedAction& msg)
{
  if (msg.actionType == RobotActionType::DRIVE_TO_POSE &&
        msg.result == ActionResult::SUCCESS){
    _driveToPoseSucceeded = true;
  }
}


}  // namespace Vector
}  // namespace Anki
