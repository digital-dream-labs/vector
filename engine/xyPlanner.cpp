/**
 * File: xyPlanner.cpp
 *
 * Author: Michael Willett
 * Created: 2018-05-11
 *
 * Description: Simple 2d grid uniform planner with path smoothing step
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "engine/xyPlanner.h"
#include "engine/xyPlannerConfig.h"
#include "engine/cozmoContext.h"
#include "engine/robot.h"

#include "coretech/planning/engine/geometryHelpers.h"

#include "util/console/consoleInterface.h"
#include "util/threading/threadPriority.h"

#include <chrono>

#define LOG_CHANNEL "Planner"

namespace Anki {
namespace Vector {

namespace {  
  // priority list for turn radius
  const std::vector<float> arcRadii = { 100., 70., 30., 10. };

  // minimum precision for joining path segments
  const float kPathPrecisionTolerance = .1f;
}

CONSOLE_VAR_RANGED( int, kArtificialPlanningDelay_ms, "XYPlanner", 0, 0, 3900 );

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//  XYPlanner
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
XYPlanner::XYPlanner(Robot* robot, bool runSync)
: IPathPlanner("XYPlanner")
, _map(robot->GetMapComponent())
, _start()
, _targets() 
, _status(EPlannerStatus::CompleteNoPlan)
, _collisionPenalty(0.f)
, _isSynchronous(runSync)
{
  static_assert(std::is_const<std::remove_reference_t<decltype(_map)>>::value, 
                "Map component must be const to guarantee thread safety!");

  _stopThread = runSync;
  _plannerThread = new std::thread( &XYPlanner::Worker, this );
}

XYPlanner::~XYPlanner()
{
  // stop the thread
  if( _plannerThread != nullptr ) {
    _stopThread = true;
    _threadRequest.notify_all(); // noexcept
    LOG_DEBUG("XYPlanner.DestroyThread.Join", "");
    try {
      _plannerThread->join();
      LOG_DEBUG("XYPlanner.DestroyThread.Joined", "");
    }
    catch (std::runtime_error& err) {
      LOG_ERROR("XYPlanner.Destroy.Exception", "locking the context mutex threw: %s", err.what());
      // this will probably crash when we call SafeDelete below
    }
    Util::SafeDelete(_plannerThread);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void XYPlanner::Worker()
{
  Anki::Util::SetThreadName(pthread_self(), "XYPlanner");

  while(!_stopThread) {
    std::unique_lock<std::recursive_mutex> lock(_contextMutex);
    if( _startPlanner ) {
      StartPlanner();
    } else {
      _threadRequest.wait(lock);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EComputePathStatus XYPlanner::ComputePath(const Pose3d& startPose, const std::vector<Pose3d>& targetPoses)
{
  std::vector<Pose2d> goalCopy;  
  for(const auto& t : targetPoses) { goalCopy.push_back(t); }
  return InitializePlanner(startPose, goalCopy, true, true);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EComputePathStatus XYPlanner::ComputeNewPathIfNeeded(const Pose3d& startPose, bool forceReplanFromScratch, bool allowGoalChange) 
{
  std::vector<Pose2d> goalCopy = _targets;
  return InitializePlanner(startPose, goalCopy, forceReplanFromScratch, allowGoalChange);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EComputePathStatus XYPlanner::InitializePlanner(const Pose2d& start, const std::vector<Pose2d>& targets, bool forceReplan, bool allowGoalChange)
{
  // planner will start on next thread cycle
  if (!forceReplan && _startPlanner) { return EComputePathStatus::Running; }
  
  // if the planner is running, flag an abort on the current instance so we can restart ASAP
  if ( !_contextMutex.try_lock() && forceReplan ) {
    _stopPlanner = true; 
    _contextMutex.lock();
  }

  // we are going to generate a new path, so reset all control variables
  std::lock_guard<std::recursive_mutex> lg(_contextMutex, std::adopt_lock); 


  // make sure the collision cost is monotonically decreasing
  if (!forceReplan) {
    const float currentPenalty = GetPathCollisionPenalty( _path );
    if ( FLT_LE(currentPenalty, _collisionPenalty) ) {
      _collisionPenalty = currentPenalty;
      return EComputePathStatus::NoPlanNeeded;
    }
    LOG_INFO("XYPlanner.InitializePlanner.CollisionCostIncreased",
             "Replanning. Old=%6.6f New=%6.6f",
             _collisionPenalty, currentPenalty);
  }

  _path.Clear();
  _start = start;
  _targets = targets;
  _allowGoalChange = allowGoalChange;
  _collisionPenalty = 0.f;
  _stopPlanner  = false;
  _startPlanner = true;
  _status = EPlannerStatus::Running;

  if( _isSynchronous ) { 
    StartPlanner();
  } else {
    _threadRequest.notify_all();
  }

  return EComputePathStatus::Running;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void XYPlanner::StartPlanner()
{
  // grab the lock
  if( ! _contextMutex.try_lock() ) {
    LOG_ERROR("XYPlanner.StartPlanner.InternalThreadingError", "Somehow failed to get mutex inside StartPlanning, but we should already have it at this point");
    return;
  }

  // clean up planner states
  std::lock_guard<std::recursive_mutex> lg(_contextMutex, std::adopt_lock);
  _startPlanner = false;

  // convert targets to planner states
  std::vector<Point2f> plannerGoals;
  std::map<Point2i, Point2f> goalLookup; // we need to map grid-aligned planner goals to true targets
  if (_allowGoalChange || (_path.GetNumSegments() == 0)) {
    for (const auto& g : _targets) {
      Point2f grid_g = GetNearestGridPoint(g.GetTranslation(), kPlanningResolution_mm);
      plannerGoals.push_back( grid_g );
      // grid_g should be a whole number, so cast to int here to prevent weird floating point precision issues
      goalLookup[grid_g.CastTo<int>()] = g.GetTranslation();
    }
  } else {
    // no goal change, so use the end point of the last computed path
    float x, y, t;
    _path[_path.GetNumSegments()-1].GetEndPose(x,y,t);
    plannerGoals.emplace_back(x,y);
  }

  // expand out of collision state if necessary
  // NOTE:  if no safe point exists, the A* search will timeout, but we probably have bigger problems to deal with.
  //        Why can we not find a single safe point anywhere within the searchable range of EscapeObstaclePlanner?
  //
  // NOTE2: there seems to be a bug in the planner where using Point::IsNear is not a sufficient check for determining
  //        that the goal is safe, even if we use a known safe point for the goal. The work around, for now, is
  //        to find the nearest safe -grid- point, and then insert the true goal state after a plan has been made.
  const Point2f plannerStart = FindNearestSafePoint( GetNearestGridPoint(_start.GetTranslation(), kPlanningResolution_mm) );

#if !defined(NDEBUG)
  for (const auto& s : plannerGoals) {
    LOG_DEBUG("XYPlanner.StartPlanner", "Plan from %s to %s (%.1f mm)",
      plannerStart.ToString().c_str(), s.ToString().c_str(), (plannerStart - s).Length() );
  }
#endif

  // profile time it takes to find a plan
  using namespace std::chrono;
  high_resolution_clock::time_point startTime = high_resolution_clock::now();

  PlannerConfig config(plannerStart, plannerGoals, _map, _stopPlanner);
  BidirectionalAStar<PlannerConfig> planner( config );
  auto planS = planner.Search();
  std::vector<Point2f> plan(planS.begin(), planS.end());

  if(!plan.empty()) {
    // planner will only go to the nearest safe grid point, so add the real start and goal points
    plan.insert(plan.begin(), _start.GetTranslation()); 
    const Point2i& plannerGoal = plan.back().CastTo<int>();
    if (goalLookup.find(plannerGoal) != goalLookup.end()) {
      plan.insert(plan.end(), goalLookup[plannerGoal]);
    } else {
      LOG_WARNING("XYPlanner.StartPlanner", "Could not match planner goal point to requested goal point, planning to nearest Planner Point" );
    }

    _path = BuildPath( plan );
    _collisionPenalty = GetPathCollisionPenalty( _path );
    // Update the selected goal target index, by checking the end pose
    //  and finding the nearest goal index matching that.
    float x, y, t;
    _path.GetSegmentConstRef(_path.GetNumSegments()-1).GetEndPose(x,y,t);
    _selectedTargetIdx = FindGoalIndex({x,y});
    ANKI_VERIFY(_selectedTargetIdx < _targets.size(), 
                "XYPlanner.StartPlanner.InvalidGoalIndexSelected",
                "totalGoals=%zd nearestIdx=%u", _targets.size(), _selectedTargetIdx);
    
    _hasValidPath = true;
    _status = EPlannerStatus::CompleteWithPlan;
  } else {
    LOG_WARNING("XYPlanner.StartPlanner", "No path found!" );
    _status = EPlannerStatus::CompleteNoPlan;
  }

  // grab performance metrics
  auto planTime_ms = duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - startTime);
  LOG_INFO("XYPlanner.StartPlanner", "planning took %s ms (%zu expansions at %.2f exp/sec)",
           std::to_string(planTime_ms.count()).c_str(),
           config.GetNumExpansions(),
           ((float) config.GetNumExpansions() * 1000) / (planTime_ms.count()) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline Planning::GoalID XYPlanner::FindGoalIndex(const Point2f& p) const
{
  const auto isPose = [&p](const Pose2d& t) { return IsNearlyEqual(p, t.GetTranslation()); };
  // Will be _targets.size() if no match was found
  return std::distance(_targets.begin(), std::find_if(_targets.begin(), _targets.end(), isPose));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//  Collision Detection
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace {
  // Point turns are just collisions with the spherical robot
  inline Ball2f GetPointCollisionSet(const Point2f& a, float padding) {
    return Ball2f(a, kRobotRadius_mm + padding);
  }

  // straight lines are rectangles the length of the line segment, with robot width
  inline FastPolygon GetLineCollisionSet(const LineSegment& l, float padding) {
    Point2f normal(l.GetFrom().y()-l.GetTo().y(), l.GetTo().x()-l.GetFrom().x());
    normal.MakeUnitLength();
    float width = kRobotRadius_mm + padding;

    return FastPolygon({ l.GetFrom() + normal * width, 
                         l.GetTo() + normal * width,
                         l.GetTo() - normal * width,
                         l.GetFrom() - normal * width });
  }

  // for simplicity, check if arcs are safe using multiple disk checks
  inline std::vector<Ball2f> GetArcCollisionSet(const Arc& a, float padding) {
    // convert to Ball2f to get center and radius
    Ball2f b = ArcToBall(a);

    // calculate start and sweep angles
    Vec2f startVec   = a.start - b.GetCentroid();
    Vec2f endVec     = a.end - b.GetCentroid();
    Radians startAngle( std::atan2(startVec.y(), startVec.x()) );
    Radians sweepAngle( std::atan2(endVec.y(), endVec.x()) - startAngle );

    const int nChecks = std::ceil(ABS(sweepAngle.ToFloat() * b.GetRadius()) / kRobotRadius_mm);
    const float checkLen = sweepAngle.ToFloat() / nChecks;

    std::vector<Ball2f> retv;
    for (int i = 0; i <= nChecks; ++i) {
      f32 rad = startAngle.ToFloat() + i*checkLen;
      retv.emplace_back(b.GetCentroid() + Point2f(std::cos(rad), std::sin(rad))*(kRobotRadius_mm + padding), kRobotRadius_mm + padding);
    }

    return retv;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline float XYPlanner::GetArcPenalty(const Arc& arc, float padding) const
{
  const auto disks = GetArcCollisionSet(arc, padding);
  return std::accumulate(disks.begin(), disks.end(), 0.f, 
    [this] (float cost, const auto& disk) { return cost + _map.GetCollisionArea(disk); }
  );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline bool XYPlanner::IsArcSafe(const Arc& arc, float padding) const
{
  const auto disks = GetArcCollisionSet(arc, padding);
  return std::none_of(disks.begin(), disks.end(), [this] (const auto& disk) { return _map.CheckForCollisions(disk); });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline float XYPlanner::GetLinePenalty(const LineSegment& seg, float padding) const
{
  return _map.GetCollisionArea( GetLineCollisionSet(seg, padding) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline bool XYPlanner::IsLineSafe(const LineSegment& seg, float padding) const
{
  return !_map.CheckForCollisions( GetLineCollisionSet(seg, padding) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline float XYPlanner::GetPointPenalty(const Point2f& p, float padding) const
{
  return _map.GetCollisionArea( GetPointCollisionSet(p, padding) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline bool XYPlanner::IsPointSafe(const Point2f& p, float padding) const
{
  return !_map.CheckForCollisions( GetPointCollisionSet(p, padding) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool XYPlanner::CheckIsPathSafe(const Planning::Path& path, float startAngle) const 
{
  Planning::Path ignore;
  return CheckIsPathSafe(path, startAngle, ignore);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool XYPlanner::CheckIsPathSafe(const Planning::Path& path, float startAngle, Planning::Path& validPath) const
{
  const auto isSafe = [this] (const Planning::PathSegment& seg ) -> bool {
    switch (seg.GetType()) {
      case Planning::PST_POINT_TURN: { return IsPointSafe( CreatePoint2f(seg), 0. ); }
      case Planning::PST_LINE:       { return IsLineSafe( CreateLineSegment(seg), 0.); }
      case Planning::PST_ARC:        { return IsArcSafe( CreateArc(seg), 0.f ); }
      default:                       { return true; }
    }
  };

  validPath.Clear();
  for (int i = 0; i < path.GetNumSegments(); ++i) {
    // TODO VIC-4315 return the actual safe subpath. It might need splitting into smaller components if the
    // current segment is long and only part of it is unsafe.
    if ( !isSafe(path.GetSegmentConstRef(i)) ) { return false; }
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float XYPlanner::GetPathCollisionPenalty(const Planning::Path& path) const 
{
  const auto getCost = [this] (const Planning::PathSegment& seg ) -> float {
    switch (seg.GetType()) {
      case Planning::PST_POINT_TURN: { return GetPointPenalty( CreatePoint2f(seg), 0. ); }
      case Planning::PST_LINE:       { return GetLinePenalty( CreateLineSegment(seg), 0.); }
      case Planning::PST_ARC:        { return GetArcPenalty( CreateArc(seg), 0.f ); }
      default:                       { return 0.f; }
    }
  };

  float retv = 0.f;
  for (int i = 0; i < path.GetNumSegments(); ++i) {
    const float segmentCost = getCost(path.GetSegmentConstRef(i));
    retv += segmentCost;
  } 
  return retv;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Point2f XYPlanner::FindNearestSafePoint(const Point2f& p) const
{
  EscapeObstaclePlanner config(_map, _stopPlanner);
  AStar<Point2f, EscapeObstaclePlanner> planner( config );
  std::vector<Point2f> plan = planner.Search({p});

  if (plan.empty()) {
    LOG_WARNING("XYPlanner.FindNearestSafePoint", "Could not find any collision free point near %s", p.ToString().c_str());
  } else if (plan.size() > 1) {
    LOG_INFO("XYPlanner.FindNearestSafePoint", "had to move start state to %s", plan.back().ToString().c_str());
  }

  return plan.empty() ? p : plan.back();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//  Path Smoothing Methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

Planning::Path XYPlanner::BuildPath(const std::vector<Point2f>& plan) const
{  
  // return empty path if there are no waypoints
  if (plan.size() == 0) {
    return Planning::Path();
  }

  using namespace Planning;
  Planning::Path path;

  std::vector<PathSegment> turns = SmoothCorners( GenerateWayPoints(plan) );

  // start turn is always a point turn, don't add if it is a small turn
  if (!NEAR(turns.front().GetDef().turn.targetAngle, _start.GetAngle().ToFloat(), kPathPrecisionTolerance)) {
    path.AppendSegment(turns[0]);
  }

  // connect all turns via straight lines and add to path
  float x0, y0, x1, y1, theta;
  for (int i = 1; i < turns.size(); ++i) 
  {
    turns[i-1].GetEndPose(x0,y0,theta);
    turns[i].GetStartPoint(x1,y1);

    // don't add trivial Straight Segements
    if ( (Point2f(x0,y0) - Point2f(x1,y1)).Length() > kPathPrecisionTolerance ) {
      path.AppendSegment( CreateLinePath({{x0,y0}, {x1,y1}}) );
    }

    path.AppendSegment( turns[i] );
  }

  if ( !path.CheckContinuity(.0001) ) {
    LOG_WARNING("XYPlanner.BuildPath", "Path smoother gnereated a non-continuous plan");
  }

  return path;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<Point2f> XYPlanner::GenerateWayPoints(const std::vector<Point2f>& plan) const
{
  std::vector<Point2f> out;

  out.push_back(_start.GetTranslation());
  
  const Point2f* iter1 = &out.front();
  const Point2f* iter2 = iter1;

  for (int i = 0; i < plan.size(); ++i) {
    const FastPolygon p = GetLineCollisionSet({*iter1, plan[i]}, kPlanningPadding_mm);
    const bool collision = _map.CheckForCollisions(p);

    if (collision) {
      out.push_back(*iter2);
      iter1 = iter2;
    }
    iter2 = &plan[i];
  }

  // always push the last point
  out.push_back(plan.back());

  return out;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<Planning::PathSegment> XYPlanner::SmoothCorners(const std::vector<Point2f>& pts) const
{
  std::vector<Planning::PathSegment> turns;

  // exit if no corners in defined by the input list
  if (pts.size() < 2) { return turns; }

  // for now, always start and end with a point turn the correct heading. Generating the first/last arc
  // uses different logic since heading angles are constrained, while all intermediate headings are not.

  turns.emplace_back( CreatePointTurnPath(_start, pts[1]) );

  // middle turns
  for (int i = 1; i < pts.size() - 1; ++i) 
  {
    Arc corner;
    bool safeArc = false;
    float x_tail, y_tail, t;
    turns.back().GetEndPose(x_tail,y_tail,t);

    // add the first safe arc that can be constructed for the current waypoints, prioritizing inscribed arcs
    // over circumscribed arcs
    for (const float r : arcRadii) {
      // try inscribed arc first since it is faster, otherwise try circumscibed arc
      if ( GetInscribedArc(pts[i-1], pts[i], pts[i+1], r, corner) ) {
        safeArc = IsArcSafe(corner, kPlanningPadding_mm);
      } 
      if ( !safeArc && GetCircumscribedArc(pts[i-1], pts[i], pts[i+1], r, corner) ) {
        safeArc = IsArcSafe(corner, kPlanningPadding_mm) &&
                  IsLineSafe({Point2f(x_tail,y_tail), corner.start}, kPlanningPadding_mm) && 
                  IsLineSafe({corner.end, pts[i+1]}, kPlanningPadding_mm);
      }
      if (safeArc) { break; }
    }

    turns.emplace_back( (safeArc) ? CreateArcPath(corner) : CreatePointTurnPath( {pts[i-1], pts[i], pts[i+1]} ) );
  }

  // add last point turn when at goal pose
  size_t idx = FindGoalIndex(pts.back());
  if (idx < _targets.size()) {
    turns.emplace_back( CreatePointTurnPath(*(pts.end()-2), _targets[idx]) );
  }

  return turns;
}


} // Cozmo
} // Anki
