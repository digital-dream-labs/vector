/**
 * File: faceAndApproachPlanner.cpp
 *
 * Author: Brad Neuman
 * Created: 2014-06-18
 *
 * Description: Simple planner that does a point turn and straight to
 * get to a goal. Supports replanning
 *
 * Copyright: Anki, Inc. 2014
 *
 **/


#include "coretech/common/engine/math/pose.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "coretech/planning/shared/path.h"
#include "faceAndApproachPlanner.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "Planner"

// amount of radians to be off from the desired angle in order to
// introduce a turn in place action
#define FACE_AND_APPROACH_THETA_THRESHOLD 0.0872664625997

// distance (in mm) away at which to introduce a straight action
#define FACE_AND_APPROACH_LENGTH_THRESHOLD DEFAULT_POSE_EQUAL_DIST_THRESOLD_MM

#define FACE_AND_APPROACH_LENGTH_SQUARED_THRESHOLD FACE_AND_APPROACH_LENGTH_THRESHOLD * FACE_AND_APPROACH_LENGTH_THRESHOLD

#define FACE_AND_APPROACH_PLANNER_ACCEL 200.0f
#define FACE_AND_APPROACH_PLANNER_DECEL 200.0f
#define FACE_AND_APPROACH_TARGET_SPEED 30.0f

#define FACE_AND_APPROACH_PLANNER_ROT_ACCEL 10.0f
#define FACE_AND_APPROACH_PLANNER_ROT_DECEL 10.0f
#define FACE_AND_APPROACH_TARGET_ROT_SPEED 1.5f

#define FACE_AND_APPRACH_DELTA_THETA_FOR_BACKUP 1.0471975512


namespace Anki {
namespace Vector {

EComputePathStatus FaceAndApproachPlanner::ComputePath(const Pose3d& startPose,
                                                       const Pose3d& targetPose)
{
  _targetVec = targetPose.GetTranslation();
  _finalTargetAngle = targetPose.GetRotationAngle<'Z'>().ToFloat();

  return ComputeNewPathIfNeeded(startPose, true);
}

EComputePathStatus FaceAndApproachPlanner::ComputeNewPathIfNeeded(const Pose3d& startPose,
                                                                  bool forceReplanFromScratch,
                                                                  bool allowGoalChange)
{

  _hasValidPath = false;

  // for now, don't try to replan
  if(!forceReplanFromScratch) {
    // just use the existing path
    _hasValidPath = true;
    return EComputePathStatus::NoPlanNeeded;
  }

  // TODO:(bn) this logic is incorrect for planning because it will
  // just constantly send a new plan. Instead if needs to detect if it
  // has veered off the plan somehow

  bool doTurn0 = false;
  bool doTurn1 = false;
  bool doStraight = false;

  Vec3f startVec(startPose.GetTranslation());

  // check if we need to do each segment
  Radians currAngle = startPose.GetRotationAngle<'Z'>();
  
  // The intermediate angle the robot should be in before doing the final turn.
  // If a straight segment ends up being unnecessary then this is just the start angle.
  Radians intermediateTargetAngle = currAngle;

  Point2f start2d(startVec.x(), startVec.y());
  Point2f target2d(_targetVec.x(), _targetVec.y());
  float distSquared = pow(target2d.x() - start2d.x(), 2) + pow(target2d.y() - start2d.y(), 2);
  if(distSquared > FACE_AND_APPROACH_LENGTH_SQUARED_THRESHOLD) {
    LOG_INFO("FaceAndApproachPlanner.Straight", "doing straight because distance^2 of %f > %f",
                     distSquared,
                     FACE_AND_APPROACH_LENGTH_SQUARED_THRESHOLD);
    doStraight = true;
    
    // If doing a straight then, the target angle is the approach angle to the target point.
    intermediateTargetAngle = atan2(_targetVec.y() - startVec.y(), _targetVec.x() - startVec.x());
  }

  float deltaTheta1 = -(intermediateTargetAngle - _finalTargetAngle).ToFloat();
  if(std::abs(deltaTheta1) > FACE_AND_APPROACH_THETA_THRESHOLD) {
    LOG_INFO("FaceAndApproachPlanner.FinalTurn", "doing final turn because delta theta of %f > %f",
                     deltaTheta1,
                     FACE_AND_APPROACH_THETA_THRESHOLD);
    doTurn1 = true;
  }

  float deltaTheta = (intermediateTargetAngle - currAngle).ToFloat();
  if(doStraight && std::abs(deltaTheta) > FACE_AND_APPROACH_THETA_THRESHOLD) {
    LOG_INFO("FaceAndApproachPlanner.InitialTurn", "doing initial turn because delta theta of %f > %f",
                     deltaTheta,
                     FACE_AND_APPROACH_THETA_THRESHOLD);
    doTurn0 = true;
  }
  
  if(!doTurn0 && !doStraight && !doTurn1) {
    _hasValidPath = true;
    return EComputePathStatus::Running;
  }

  _path.Clear();

  bool backup = false;
  if(doTurn0) {
    if(std::abs(deltaTheta) > FACE_AND_APPRACH_DELTA_THETA_FOR_BACKUP) {
      printf("FaceAndApproachPlanner: deltaTheta of %f above threshold, doing backup!\n", deltaTheta);
      deltaTheta = (Radians(deltaTheta) + M_PI_F).ToFloat();
      deltaTheta1 = (Radians(deltaTheta1) + M_PI_F).ToFloat();
      intermediateTargetAngle = intermediateTargetAngle + M_PI_F;
      backup = true;
    }

    _path.AppendPointTurn(startVec.x(), startVec.y(), currAngle.ToFloat(), intermediateTargetAngle.ToFloat(),
                          deltaTheta < 0 ? -FACE_AND_APPROACH_TARGET_ROT_SPEED : FACE_AND_APPROACH_TARGET_ROT_SPEED,
                          FACE_AND_APPROACH_PLANNER_ROT_ACCEL,
                          FACE_AND_APPROACH_PLANNER_ROT_DECEL,
                          POINT_TURN_ANGLE_TOL,
                          true);
  }

  if(doStraight) {
    _path.AppendLine(startVec.x(), startVec.y(),
                     _targetVec.x(), _targetVec.y(),
                     backup ? -FACE_AND_APPROACH_TARGET_SPEED : FACE_AND_APPROACH_TARGET_SPEED,
                     FACE_AND_APPROACH_PLANNER_ACCEL,
                     FACE_AND_APPROACH_PLANNER_DECEL);
  }

  if(doTurn1) {
    _path.AppendPointTurn(_targetVec.x(), _targetVec.y(), intermediateTargetAngle.ToFloat(), _finalTargetAngle,
                          deltaTheta1 < 0 ? -FACE_AND_APPROACH_TARGET_ROT_SPEED : FACE_AND_APPROACH_TARGET_ROT_SPEED,
                          FACE_AND_APPROACH_PLANNER_ROT_ACCEL,
                          FACE_AND_APPROACH_PLANNER_ROT_DECEL,
                          POINT_TURN_ANGLE_TOL,
                          true);
  }

  _hasValidPath = true;

  return EComputePathStatus::Running;
}

    
 
} // namespace Vector
} // namespace Anki
