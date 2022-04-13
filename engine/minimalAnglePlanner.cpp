/**
 * File: minimalAnglePlanner.cpp
 *
 * Author: Brad Neuman
 * Created: 2015-11-09
 *
 * Description: A simple "planner" which tries to minimize the amount it turns away from the angle it is
 * currently facing. It will back straight up some distance, then turn in place to face the goal, drive to the
 * goal, then turn in place again. Very similar to FaceAndApproachPlanner, but will look better in some cases,
 * e.g. when docking
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "coretech/common/engine/math/pose.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "coretech/planning/shared/path.h"
#include "minimalAnglePlanner.h"
#include "util/logging/logging.h"
#include <cmath>

#define LOG_CHANNEL "Planner"

// minimum amount of radians for which to try to execute a point turn
#define MINIMAL_ANGLE_PLANNER_THETA_THRESHOLD 0.01

// distance (in mm) away at which to introduce a straight action
#define MINIMAL_ANGLE_PLANNER_LENGTH_THRESHOLD ( 0.25f * DEFAULT_POSE_EQUAL_DIST_THRESOLD_MM )

#define MINIMAL_ANGLE_PLANNER_ACCEL 200.0f
#define MINIMAL_ANGLE_PLANNER_DECEL 200.0f
#define MINIMAL_ANGLE_PLANNER_TARGET_SPEED 45.0f

#define MINIMAL_ANGLE_PLANNER_ROT_ACCEL 10.0f
#define MINIMAL_ANGLE_PLANNER_ROT_DECEL 10.0f
#define MINIMAL_ANGLE_PLANNER_TARGET_ROT_SPEED 1.0f


namespace Anki {
namespace Vector {

EComputePathStatus MinimalAnglePlanner::ComputePath(const Pose3d& startPose,
                                                    const Pose3d& targetPose)
{
  _targetVec = targetPose.GetTranslation();
  _finalTargetAngle = targetPose.GetRotationAngle<'Z'>();

  return ComputeNewPathIfNeeded(startPose, true);
}

EComputePathStatus MinimalAnglePlanner::ComputeNewPathIfNeeded(const Pose3d& startPose,
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

  // This planner has (up to) 4 actions:
  // 1. Back straight up by backupDistance
  // 2. Turn in place to face the target (turn0)
  // 3. Drive straight to target (x,y) position
  // 4. Turn in place to align with the goal angle (turn1)

  const float maxTurnAngle = PLANNER_MAINTAIN_ANGLE_THRESHOLD;
  const float minBackupDistance = 0.0f;
  const float maxBackupDistance = 75.0f;

  // backupDistance is automatically computed so that turn0 turns at most maxTurnAngle away from the robots
  // starting angle. The are caps on how long that backup distance can be.

  // Here's some math on how to calculate the backup distance. feel free to ignore
  // The goal is to minimize backupDistance such that we turn at most maxTurnAngle

  // This can equivalently be though of as turning exactly maxTurnAngle, and solving for backupDistance. If we
  // get a negative backupDistance, that means we don't need to back up at all

  // let the robot start at (x_r, y_r, theta_r), and the goal is (x_g, y_g, theta_g)
  const float x_r = startPose.GetTranslation().x();
  const float y_r = startPose.GetTranslation().y();
  const float x_g = _targetVec.x();
  const float y_g = _targetVec.y();
  // We first back up to an intermediate point (x_i, y_i, theta_r), which matches theta_r since it is a straight back up
  // then we turn exactly maxTurnAngle (in the correct direction).
  // now we have the following:

  // maxTurnAngle = atan2( y_i - y_g, x_i - x_g)
  // y_i = y_r - backupDistance * sin( theta_r )
  // x_i = x_r - backupDistance * cos( theta_r )

  // to make life easier, lets solve this problem assuming theta_r = 0. We are solving for backupDistance, and
  // backupDistance won't depend on theta_r, so we can ignore it. Imagine this as rotating the entire problem
  // by -theta_r

  // with theta_r = 0, and substituting from above, we get
  // maxTurnAngle = atan2( y_r - y_g, x_r - backupDistance - x_g)
  // solving for backupDistance gives:
  // backupDistance = x_r - x_g + (y_r - y_g) / tan( maxTurnAngle )
  // since maxTurnAngle can be positive or negative:
  // backupDistance = x_r - x_g +/- (y_r - y_g) / tan( maxTurnAngle )
  // and we want the minimum positive answer
  const float lhs = x_r - x_g;
  const float rhs_denom = std::tan( maxTurnAngle );

  DEV_ASSERT(maxTurnAngle > 0.0f && maxTurnAngle < M_PI/2.0f, "MinimalAnglePlanner.InvalidMaxAngle");
  
  // now we know tan should be >= 0

  float backupDistance = 0.0f;

  if( rhs_denom > 1e-6 ) {
    const float rhs = (y_r - y_g) / std::tan( maxTurnAngle );
  
    if( rhs > lhs ) {
      backupDistance = lhs + rhs;
    }
    else {
      backupDistance = lhs - rhs;
    }

    backupDistance = CLIP( backupDistance, minBackupDistance, maxBackupDistance );
  }
  // else, don't back up

  // OK, now we have everything we need to know, so start building the plan!
  _path.Clear();

  Pose2d curr( startPose );

  // first, check if we need to do the initial backup
  if( backupDistance > MINIMAL_ANGLE_PLANNER_LENGTH_THRESHOLD ) {
    Pose2d backupIntermediatePose( curr );
    backupIntermediatePose.TranslateForward(-backupDistance);

    _path.AppendLine(curr.GetX(), curr.GetY(),
                     backupIntermediatePose.GetX(), backupIntermediatePose.GetY(),
                     -MINIMAL_ANGLE_PLANNER_TARGET_SPEED,
                     MINIMAL_ANGLE_PLANNER_ACCEL,
                     MINIMAL_ANGLE_PLANNER_DECEL);

    LOG_INFO("MinimalAnglePlanner.Plan.Backup", "%f", -backupDistance);

    curr = backupIntermediatePose;
  }

  // next, do a point turn to the new angle
  Radians turn0Angle = atan2( _targetVec.y() - curr.GetY(), _targetVec.x() - curr.GetX() );
  Radians deltaTheta = turn0Angle - curr.GetAngle();
  // wait to apply the turn until we see if we need to drive straight first

  float straightDist = std::sqrt( std::pow( _targetVec.x() - curr.GetX(), 2 ) +
                                  std::pow( _targetVec.y() - curr.GetY(), 2 ) );

  if( straightDist > MINIMAL_ANGLE_PLANNER_LENGTH_THRESHOLD ) {

    // if we need to drive straight, then apply the previous turn (if there was one) first
    if( deltaTheta.getAbsoluteVal().ToFloat() > MINIMAL_ANGLE_PLANNER_THETA_THRESHOLD ) {
  
      _path.AppendPointTurn(curr.GetX(), curr.GetY(), curr.GetAngle().ToFloat(), turn0Angle.ToFloat(),
                            deltaTheta < 0 ?
                            -MINIMAL_ANGLE_PLANNER_TARGET_ROT_SPEED
                            : MINIMAL_ANGLE_PLANNER_TARGET_ROT_SPEED,
                            MINIMAL_ANGLE_PLANNER_ROT_ACCEL,
                            MINIMAL_ANGLE_PLANNER_ROT_DECEL,
                            POINT_TURN_ANGLE_TOL,
                            true);

      LOG_INFO("MinimalAnglePlanner.Plan.Turn0", "%f", deltaTheta.ToFloat());

      curr.SetRotation( turn0Angle );
    }

    
    Pose2d nextPose(curr);
    nextPose.TranslateForward(straightDist);

    _path.AppendLine(curr.GetX(), curr.GetY(),
                     nextPose.GetX(), nextPose.GetY(),
                     MINIMAL_ANGLE_PLANNER_TARGET_SPEED,
                     MINIMAL_ANGLE_PLANNER_ACCEL,
                     MINIMAL_ANGLE_PLANNER_DECEL);

    LOG_INFO("MinimalAnglePlanner.Plan.Straight", "%f", straightDist);

    curr = nextPose;
  }

  // last but not least, face the correct goal angle
  deltaTheta = _finalTargetAngle - curr.GetAngle();
  if( deltaTheta.getAbsoluteVal().ToFloat() > MINIMAL_ANGLE_PLANNER_THETA_THRESHOLD ) {
    _path.AppendPointTurn(curr.GetX(), curr.GetY(), curr.GetAngle().ToFloat(), _finalTargetAngle.ToFloat(),
                          deltaTheta < 0 ?
                          -MINIMAL_ANGLE_PLANNER_TARGET_ROT_SPEED
                          : MINIMAL_ANGLE_PLANNER_TARGET_ROT_SPEED,
                          MINIMAL_ANGLE_PLANNER_ROT_ACCEL,
                          MINIMAL_ANGLE_PLANNER_ROT_DECEL,
                          POINT_TURN_ANGLE_TOL,
                          true);

    LOG_INFO("MinimalAnglePlanner.Plan.Turn1", "%f", deltaTheta.ToFloat());

    curr.SetRotation( _finalTargetAngle );
  }

  LOG_INFO("MinimalAnglePlanner.FinalPosition", "(%f, %f, %fdeg)",
                   curr.GetX(), curr.GetY(), curr.GetAngle().getDegrees() );

  _hasValidPath = true;

  return EComputePathStatus::Running;
}

}
}
