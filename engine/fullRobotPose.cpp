/**
 * File: fullRobotPose.cpp
 *
 * Author: Kevin M. Karol
 * Created: 3/16/18
 *
 * Description: Class which wraps all descriptors of the physical state of
 * a robot
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "engine/fullRobotPose.h"
#include "anki/cozmo/shared/cozmoConfig.h"




namespace Anki {
namespace Vector {

namespace{
// For tool code reading
// Camera looking straight:
//const RotationMatrix3d Robot::_kDefaultHeadCamRotation = RotationMatrix3d({
//   0,     0,   1.f,
//  -1.f,   0,   0,
//   0,    -1.f, 0,
//});
// 4-degree look down:
const RotationMatrix3d kDefaultHeadCamRotation = RotationMatrix3d({
  0,      -0.0698f,  0.9976f,
-1.0000f,  0,        0,
  0,      -0.9976f, -0.0698f,
});
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FullRobotPose::FullRobotPose()
: IDependencyManagedComponent<RobotComponentID>(this, RobotComponentID::FullRobotPose)
, _neckPose(0.f,Y_AXIS_3D(),
            {NECK_JOINT_POSITION[0], NECK_JOINT_POSITION[1], NECK_JOINT_POSITION[2]}, _pose, "RobotNeck")
, _headCamPose(kDefaultHeadCamRotation,
                {HEAD_CAM_POSITION[0], HEAD_CAM_POSITION[1], HEAD_CAM_POSITION[2]}, _neckPose, "RobotHeadCam")
, _liftBasePose(0.f, Y_AXIS_3D(),
                {LIFT_BASE_POSITION[0], LIFT_BASE_POSITION[1], LIFT_BASE_POSITION[2]}, _pose, "RobotLiftBase")
, _liftPose(0.f, Y_AXIS_3D(), {LIFT_ARM_LENGTH, 0.f, 0.f}, _liftBasePose, "RobotLift")
, _headAngle(MIN_HEAD_ANGLE)
, _liftAngle(0)
{}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FullRobotPose::~FullRobotPose()
{

}


} // namespace Vector
} // namespace Anki
