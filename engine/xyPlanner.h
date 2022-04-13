/**
 * File: xyPlanner.h
 *
 * Author: Michael Willett
 * Created: 2018-05-11
 *
 * Description: Simple 2d grid uniform planner with path smoothing step
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Victor_Engine_XYPlanner_H__
#define __Victor_Engine_XYPlanner_H__

#include "engine/pathPlanner.h"
#include "coretech/planning/shared/goalDefs.h"
#include "coretech/common/shared/math/point_fwd.h"
#include "coretech/common/engine/math/pose.h"

#include "util/helpers/noncopyable.h"

#include <thread>
#include <condition_variable>

namespace Anki {

struct Arc;
class LineSegment;

namespace Vector {

class Path;
class Robot;
class MapComponent;

class XYPlanner : public IPathPlanner, private Util::noncopyable
{
public:
  // if runSync TRUE, run the planner in the main engine thread
  explicit XYPlanner(Robot* robot, bool runSync = false);
  virtual ~XYPlanner() override;

  // ComputePath functions start computation of a path.
  // Return value of Error indicates that there was a problem starting the plan and it isn't running. Running
  // means it is (or may have already finished)
  virtual EComputePathStatus ComputePath(const Pose3d& startPose, const std::vector<Pose3d>& targetPoses) override;

  // While we are following a path, we can do a more efficient check to see if we need to update that path
  // based on new obstacles or other information. 
  virtual EComputePathStatus ComputeNewPathIfNeeded(const Pose3d& startPose,
                                                    bool forceReplanFromScratch = false,
                                                    bool allowGoalChange = true) override;

  // exit the current planning routine
  virtual void StopPlanning() override { _stopPlanner = true; }

  virtual EPlannerStatus CheckPlanningStatus() const override { return _status; }
  
  // Returns true if this planner checks for fatal obstacle collisions
  bool ChecksForCollisions() const override { return true; }
  
  // Returns true if the path avoids obstacles. Some planners don't know about obstacles, so the default is always true.
  // If provided, clears and fills validPath to be that portion of path that is below the max obstacle penalty.
  virtual bool CheckIsPathSafe(const Planning::Path& path, float startAngle) const override;
  virtual bool CheckIsPathSafe(const Planning::Path& path, float startAngle, Planning::Path& validPath) const override;

  // return a test path
  virtual void GetTestPath(const Pose3d& startPose, Planning::Path &path, const PathMotionProfile* motionProfile = nullptr) override {}

protected:
  virtual EComputePathStatus ComputePath(const Pose3d& startPose, const Pose3d& targetPose) override { return ComputePath(startPose, std::vector<Pose3d>({targetPose})); }

private:
  // start the planner after thread has been checked and locked
  void StartPlanner();

  // initialize all control states and notify the planner thread to get a plan
  EComputePathStatus InitializePlanner(const Pose2d& start, const std::vector<Pose2d>& targets, bool forceReplan, bool allowGoalChange);

  // convert a set of way points to a smooth path
  Planning::Path BuildPath(const std::vector<Point2f>& plan) const;

  // builds a simplified list of waypoints from closed set
  std::vector<Point2f> GenerateWayPoints(const std::vector<Point2f>& plan) const;

  // given a set of points, generate the largest safe circumscibed arc for each turn
  std::vector<Planning::PathSegment> SmoothCorners(const std::vector<Point2f>& pts) const;

  // if p corresponds to a pose in _targets, return the correspondance index. if it is not, returns _targets.size()
  Planning::GoalID FindGoalIndex(const Point2f& p) const;

  // Finds the nearest safe point to p. If no safe point exists, default to p
  Point2f FindNearestSafePoint(const Point2f& p) const;

  // get total cost of traversing the path in the current map
  float GetPathCollisionPenalty(const Planning::Path& path) const;

  // returns true if the provided region has no collisions with supported types in the map
  bool IsArcSafe(const Arc& a, float padding) const;
  bool IsLineSafe(const LineSegment& l, float padding) const;
  bool IsPointSafe(const Point2f& p, float padding) const;
  
  // get area of any collision with supported types from the map 
  float GetArcPenalty(const Arc& a, float padding) const;
  float GetLinePenalty(const LineSegment& l, float padding) const;
  float GetPointPenalty(const Point2f& p, float padding) const;

  // member vars
  const MapComponent&  _map;
  Pose2d               _start;
  std::vector<Pose2d>  _targets;
  EPlannerStatus       _status;
  float                _collisionPenalty;
  bool                 _allowGoalChange;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Thread Handling
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  void Worker();                                           // thread loop

  std::thread*                _plannerThread;              // for safe spinup/shutdown
  std::recursive_mutex        _contextMutex;               // mutex for locking
  std::condition_variable_any _threadRequest;              // for syncing with other threads

  const bool                  _isSynchronous  = false;     // if TRUE, do not start a thread on construction
  volatile bool               _stopThread     = false;     // clean up and stop the thread entirely
  volatile bool               _startPlanner   = false;     // start planning now if it isn't running
  volatile bool               _stopPlanner    = false;     // if the planner is currently running, force it to stop
};
    
    
} // namespace Vector
} // namespace Anki


#endif // __Victor_Engine_XYPlanner_H__

