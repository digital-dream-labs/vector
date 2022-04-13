/**
 * File: pathPlanner.h
 *
 * Author: Kevin Yoon
 * Date:   2/24/2014
 *
 * Description: Interface for path planners
 *
 * Copyright: Anki, Inc. 2014-2015
 **/

#ifndef __PATH_PLANNER_H__
#define __PATH_PLANNER_H__

#include "coretech/common/engine/objectIDs.h"
#include "coretech/planning/shared/path.h"
#include "coretech/planning/shared/goalDefs.h"
#include "clad/types/objectTypes.h"
#include "clad/types/pathMotionProfile.h"
#include <set>


namespace Anki {

class Pose3d;

namespace Vector {

enum class EComputePathStatus {
  Error, // Could not create path as requested
  Running, // Signifies that planning has successfully begun (may also be finished already)
  NoPlanNeeded, // Signifies that planning is not necessary, useful in the replanning case
};

enum class EPlannerStatus {
  Error, // Planner encountered an error while running
  Running, // Planner is still thinking
  CompleteWithPlan, // Planner has finished, and has a valid plan
  CompleteNoPlan, // Planner has finished, but has no plan (either encountered an error, or stopped early)
};
  
enum class EPlannerErrorType {
  None=0,
  PlannerFailed,
  TooFarFromPlan,
  InvalidAppendant
};

class IPathPlanner
{
public:

  IPathPlanner(const std::string& name);
  virtual ~IPathPlanner() {}

  // return a planner name for debugging and analytics purposes
  const std::string& GetName() const { return _name; }

  // ComputePath functions start computation of a path. The underlying planner may run in a thread, or it may
  // compute immediately in the calling thread.
  // 
  // There are two versions of the function. They both take a single start pose, one takes multiple possible
  // target poses, and one takes a single target pose. The one with multiple poses allows the planner to choose
  // which pose to drive to on its own. If the multiple goal poses version is not implemented, the single goal
  // will be selected which is the closest according to ComputeClosestGoalPose. The single pose function is
  // protected to simplify the external interface (simply pass a vector of one element if that is desired)
  // 
  // Return value of Error indicates that there was a problem starting the plan and it isn't running. Running
  // means it is (or may have already finished)

  virtual EComputePathStatus ComputePath(const Pose3d& startPose,
                                         const std::vector<Pose3d>& targetPoses);

  // Utility function to simply select the closest target pose from a vector
  static Planning::GoalID ComputeClosestGoalPose(const Pose3d& startPose, const std::vector<Pose3d>& targetPoses);

  // While we are following a path, we can do a more efficient check to see if we need to update that path
  // based on new obstacles or other information. This function assumes that the robot is following the last
  // path that was computed by ComputePath and returned by GetCompletePath. If a new path is needed, it will
  // compute it just like ComputePath. If not, it may do something more efficient to return the existing path
  // (or a portion of it). In either case, GetCompletePath should be called to return the path that should be
  // used
  // Default implementation never plans (just gives you the same path as last time)
  virtual EComputePathStatus ComputeNewPathIfNeeded(const Pose3d& startPose,
                                                    bool forceReplanFromScratch = false,
                                                    bool allowGoalChange = true);

  virtual void StopPlanning() {}

  virtual EPlannerStatus CheckPlanningStatus() const;
  
  virtual EPlannerErrorType GetErrorType() const;
  
  // Returns true if the path avoids obstacles. Some planners don't know about obstacles, so the default is always true.
  // If provided, clears and fills validPath to be that portion of path that is below the max obstacle penalty.
  virtual bool CheckIsPathSafe(const Planning::Path& path, float startAngle) const;
  virtual bool CheckIsPathSafe(const Planning::Path& path, float startAngle, Planning::Path& validPath) const;
  
  // Returns true if this planner checks for fatal obstacle collisions
  virtual bool ChecksForCollisions() const { return false; }

  // returns true if it was able to copy a complete path into the passed in argument. If specified,
  // selectedTargetIndex will be set to the index of the targetPoses that was selected in the original
  // targetPoses vector passed in to ComputePath, or 0 if only one target pose was passed in to the most
  // recent ComputePlan call. If the robot has moved significantly between computing the path and getting the
  // complete path, the path may trim the first few actions removed to account for the robots motion, based on
  // the passed in position
  bool HasCompletePath() const { return _hasValidPath; }
  const Planning::Path& GetCompletePath() const { return _path; }
  const Planning::GoalID& GetPathSelectedTargetIndex() const { return _selectedTargetIdx; }

  // return a test path
  virtual void GetTestPath(const Pose3d& startPose, Planning::Path &path, const PathMotionProfile* motionProfile = nullptr) {}

  // Modifies in path according to `motionProfile` to produce output path.
  // Takes deceleration into account in order to produce a path with smooth deceleration
  // over multiple path segments
  // TODO: This is where Cozmo mood/skill-based path wonkification would occur,
  //       but currently it just changes speeds and accel on each segment.
  static Planning::Path ApplyMotionProfile(const Planning::Path &in, const PathMotionProfile& motionProfile);
  
protected:

  bool _hasValidPath;
  bool _planningError;
  Planning::GoalID _selectedTargetIdx;
  Planning::Path _path;
  static const int finalPathSegmentSpeed_mmps = 20;
  static const int distToDecelSegLenTolerance_mm = 5;
  static const int diffInDecel = 1;

  // see docs in the public section above
  virtual EComputePathStatus ComputePath(const Pose3d& startPose,
                                         const Pose3d& targetPose) = 0;

private:

  std::string _name;

  
}; // Interface IPathPlanner
  
constexpr const char* EPlannerStatusToString(EPlannerStatus status)
{
#define HANDLE_EPS_CASE(v) case EPlannerStatus::v: return #v
  switch(status) {
    HANDLE_EPS_CASE(Error);
    HANDLE_EPS_CASE(Running);
    HANDLE_EPS_CASE(CompleteWithPlan);
    HANDLE_EPS_CASE(CompleteNoPlan);
  }
#undef HANDLE_ETDTPS_CASE
}
    
    
} // namespace Vector
} // namespace Anki


#endif // PATH_PLANNER_H
