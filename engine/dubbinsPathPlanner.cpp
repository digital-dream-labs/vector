/**
 * File: dubbinsPathPlanner.h
 *
 * Author: Andrew / Kevin (reorg by Brad)
 * Created: 2015-09-16
 *
 * Description: Dubbins path planner
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "coretech/common/engine/math/pose.h"
#include "coretech/planning/shared/path.h"
#include "dubbinsPathPlanner.h"
#include "util/logging/logging.h"

#define DUBINS_TARGET_SPEED_MMPS 50
#define DUBINS_ACCEL_MMPS2 200
#define DUBINS_DECEL_MMPS2 200

#define DUBINS_START_RADIUS_MM 50.f
#define DUBINS_END_RADIUS_MM 50.f

namespace Anki {
namespace Vector {

DubbinsPlanner::DubbinsPlanner() : IPathPlanner("Dubbins") {}

EComputePathStatus DubbinsPlanner::ComputePath(const Pose3d& startPose,
                                               const Pose3d& targetPose)
{
  _hasValidPath = false;
  _path.Clear();

  Vec3f startPt = startPose.GetTranslation();
  f32 startAngle = startPose.GetRotationAngle<'Z'>().ToFloat(); // Assuming robot is not tilted
      
  // Currently, we can only deal with rotations around (0,0,1) or (0,0,-1).
  // If it's something else, then quit.
  // TODO: Something smarter?
  Vec3f rotAxis;
  Radians rotAngle;
  startPose.GetRotationVector().GetAngleAndAxis(rotAngle, rotAxis);
  float dotProduct = DotProduct(rotAxis, Z_AXIS_3D());
  const float dotProductThreshold = 0.0152f; // 1.f - std::cos(DEG_TO_RAD(10)); // within 10 degrees
  if(!rotAngle.IsNear(0.f, DEG_TO_RAD(10.f)) && !NEAR(std::abs(dotProduct), 1.f, dotProductThreshold)) {
    PRINT_NAMED_ERROR("PathPlanner.GetPlan.NonZAxisRot_start",
                      "GetPlan() does not support rotations around anything other than z-axis (%f %f %f)",
                      rotAxis.x(), rotAxis.y(), rotAxis.z());
    return EComputePathStatus::Error;
  }
  if (FLT_NEAR(rotAxis.z(), -1.f)) {
    startAngle *= -1;
  }
      
      
  Vec3f targetPt = targetPose.GetTranslation();
  f32 targetAngle = targetPose.GetRotationAngle<'Z'>().ToFloat(); // Assuming robot is not tilted

  // Currently, we can only deal with rotations around (0,0,1) or (0,0,-1).
  // If it's something else, then quit.
  // TODO: Something smarter?
  targetPose.GetRotationVector().GetAngleAndAxis(rotAngle, rotAxis);
  dotProduct = DotProduct(rotAxis, Z_AXIS_3D());
  if(!rotAngle.IsNear(0.f, DEG_TO_RAD(10.f)) && !NEAR(std::abs(dotProduct), 1.f, dotProductThreshold)) {
    PRINT_NAMED_ERROR("PathPlanner.GetPlan.NonZAxisRot_target",
                      "GetPlan() does not support rotations around anything other than z-axis (%f %f %f)",
                      rotAxis.x(), rotAxis.y(), rotAxis.z());
    return EComputePathStatus::Error;
  }

  if (FLT_NEAR(rotAxis.z(), -1.f)) {
    targetAngle *= -1;
  }

  const f32 dubinsRadius = (targetPt - startPt).Length() * 0.25f;
      
  if (Planning::GenerateDubinsPath(_path,
                                   startPt.x(), startPt.y(), startAngle,
                                   targetPt.x(), targetPt.y(), targetAngle,
                                   std::min(DUBINS_START_RADIUS_MM, dubinsRadius),
                                   std::min(DUBINS_END_RADIUS_MM, dubinsRadius),
                                   DUBINS_TARGET_SPEED_MMPS, DUBINS_ACCEL_MMPS2, DUBINS_DECEL_MMPS2) == 0) {
    PRINT_CH_INFO("Planner", "GetPlan.NoPathFound",
                  "Could not generate Dubins path (startPose %f %f %f, targetPose %f %f %f)",
                  startPt.x(), startPt.y(), startAngle,
                  targetPt.x(), targetPt.y(), targetAngle);
    return EComputePathStatus::Error;
  }
  else {
    _hasValidPath = true;
    return EComputePathStatus::Running;
  }
}

}
}

