/**
 * File: xyPlannerConfig.h
 *
 * Author: Michael Willett
 * Created: 2018-06-22
 *
 * Description: Configurations for A* planners used in xyPlanner
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Victor_Engine_XYPlannerConfig_H__
#define __Victor_Engine_XYPlannerConfig_H__

#include "engine/robot.h"
#include "engine/navMap/mapComponent.h"

#include "coretech/planning/engine/aStar.h"
#include "coretech/planning/engine/bidirectionalAStar.h"
#include "coretech/common/engine/math/ball.h"

#include <numeric>


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace Anki {
namespace Vector {

class PlannerPoint;

}
}

namespace std {
  template<>
  struct std::hash<Anki::Vector::PlannerPoint> : public std::hash<Anki::Point2f> {};
}

namespace Anki {
namespace Vector {

namespace {  
  const size_t kPlanningResolution_mm        = 32;
  const size_t kMaxSubsampleDepth            = 2;  // this currently corresponds to a minimum step size of 8mm == navMap resolution
  const float  kPlanningPadding_mm           = 3.f;
  const float  kRobotRadius_mm               = ROBOT_BOUNDING_Y / 2.f;
 
  const size_t kEscapeObstacleMaxExpansions  = 10000;
  const size_t kPlanPathMaxExpansions        = 100000;

  template <typename T>
  constexpr std::array<Point2<T>,4> FourConnectedGrid(const T& res) {
    return {{ { res, T(0)},
              {-res, T(0)},
              { T(0), -res},
              { T(0),  res} }};
  }

  // NOTE: the escape grid resolution needs to be the same as the planner Resolution, otherwise it will
  //       generate invalid goal positions 
  constexpr std::array<Point2f, 8> EightConnectedGrid(const float res) {
    return {{
      { res, 0.f},
      {-res, 0.f},
      { 0.f, -res},
      { 0.f,  res},
      { res,  res},
      {-res,  res},
      { res, -res},
      {-res, -res}
    }};
  }

  const std::array<Point2f, 8> escapeGrid = EightConnectedGrid(kPlanningResolution_mm);
  const std::array<Point2f, 4> plannerFullGrid = FourConnectedGrid(1.f);
  const std::array<Point2f, 4> plannerHalfGrid = FourConnectedGrid(.5f);

  static_assert(plannerFullGrid.size() == plannerHalfGrid.size(), "PlannerPoint Half Steps and Full Steps must have the same number of successors" );
  
  // snap to planning grid via (float->int->float) cast
  inline Point2f GetNearestGridPoint(const Point2f& p, const float gridSize) {
    return Point2f(roundf(p.x() / gridSize), roundf(p.y() / gridSize)) * gridSize;
  }

  float ManhattanDistance(const Point2f& p, const Point2f& q) {
    Point2f d = (p - q).Abs();
    return d.x() + d.y(); 
  }

}

class PlannerPoint : public Point2f {
public:
    PlannerPoint() {}

    PlannerPoint(const Point2f& p, size_t depth = 0) 
    : Point2f( p )
    , _depth(depth)
    , _stepSize(kPlanningResolution_mm >> depth) {}

    ~PlannerPoint() {}

    inline float GetStepSize() { return _stepSize; } 

    PlannerPoint FullStep(size_t dir) const {
      // since we are only do half and full steps, if we aren't at depth 0, a "full step"
      // will put us one level higher, decreasing depth.
      return { (*this) + plannerFullGrid[dir] * _stepSize, (_depth > 0) ? _depth - 1 : 0 };
    }

    PlannerPoint HalfStep(size_t dir) const {
      return (_depth <= kMaxSubsampleDepth) ? PlannerPoint( (*this) + (plannerHalfGrid[dir] * _stepSize), _depth+1 )
                                            : FullStep(dir);
    }

    static size_t MaxSuccessors() { return plannerFullGrid.size(); }
private:
    size_t _depth;
    float  _stepSize;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//  Bidirectional A* Configuration through collision free space
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class PlannerConfig : public BidirectionalAStarConfig<PlannerPoint, PlannerConfig> {
public:
  // define a custom iterator class to avoid dynamic memory allocation
  class SuccessorIter : private std::iterator<std::input_iterator_tag, Successor>{
  public:
    SuccessorIter(const PlannerPoint& p, const MapComponent& m, int idx = 0 ) 
    : _idx(idx), _parent(p), _map(m) { UpdateState(); }

    bool          operator!=(const SuccessorIter& rhs) { return this->_idx != rhs._idx; }
    Successor     operator*()                          { return {_state, _state.GetStepSize()}; }
    SuccessorIter operator++()                         { ++_idx; UpdateState(); return *this; }
    SuccessorIter begin()                              { return SuccessorIter(_parent, _map); }
    SuccessorIter end()                                { return SuccessorIter(_parent, _map, -1); }

  private:
    inline void UpdateState() {
      if (_idx >= PlannerPoint::MaxSuccessors()) { 
        if (_collisionFree || _substepping) {
          _idx = -1; 
          return; 
        } else {
          _idx = 0;
          _substepping = true;
          _collisionFree = true;
        }
      }

      _state = _substepping ? _parent.HalfStep(_idx) : _parent.FullStep(_idx);
      if ( _map.CheckForCollisions( Ball2f(_state, kRobotRadius_mm + kPlanningPadding_mm) ) ) { 
        _collisionFree = false;
        ++(*this); 
      }
    }

    int                 _idx;
    PlannerPoint        _state;
    const PlannerPoint  _parent;
    const MapComponent& _map;
    bool                _collisionFree = true;
    bool                _substepping = false;
  };

  PlannerConfig(const Point2f& start, const std::vector<Point2f>& goals, const MapComponent& map, const volatile bool& stopPlanning) 
  : _start(start)
  , _goals(goals.begin(), goals.end())
  , _map(map)
  , _abort(stopPlanning) {}

  inline bool   StopPlanning()                                    { return _abort || (++_numExpansions > kPlanPathMaxExpansions); }
  inline size_t GetNumExpansions() const                          { return _numExpansions; }
  inline SuccessorIter GetSuccessors(const PlannerPoint& p) const { return SuccessorIter(p, _map); };
  
  inline float ReverseHeuristic(const Point2f& p) const { return ManhattanDistance(p, _start); };
  inline float ForwardHeuristic(const Point2f& p) const { 
    float minDist = std::numeric_limits<float>::max();
    for (const auto& g : _goals) {
      minDist = fmin(minDist, (ManhattanDistance(p, g)));
    }
    return minDist;
  };

  const auto& GetStart() const { return _start; }
  const auto& GetGoals() const { return _goals; }

private:
  const PlannerPoint               _start;
  const std::vector<PlannerPoint>  _goals;

  const MapComponent&         _map;
  const volatile bool&        _abort;
  size_t                      _numExpansions = 0;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//  Dijkstra Configuration that finds the nearest collision free state with uniform action cost
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class EscapeObstaclePlanner : public IAStarConfig<Point2f, EscapeObstaclePlanner> {
public:
  EscapeObstaclePlanner(const MapComponent& map, const volatile bool& stopPlanning) 
  : _map(map)
  , _abort(stopPlanning) {}

  inline float  Heuristic(const Point2f& p) const { return 0.f; };
  inline bool   StopPlanning()                    { return _abort || (++_numExpansions > kEscapeObstacleMaxExpansions); }
  inline bool   IsGoal(const Point2f& p)    const { return !_map.CheckForCollisions( Ball2f(p, kRobotRadius_mm + kPlanningPadding_mm) ); }

  inline std::array<Successor, 8> GetSuccessors(const Point2f& p) const { 
    std::array<Successor, 8> retv;
    const Point2f gridP = GetNearestGridPoint(p, kPlanningResolution_mm);
    std::transform(escapeGrid.begin(), escapeGrid.end(), retv.begin(), 
      [&gridP](const auto& dir) { return Successor{gridP + dir, dir.Length()}; });
    return retv;
  };

private:

  const MapComponent&  _map;
  const volatile bool& _abort;
  size_t               _numExpansions = 0;
};

} // namespace Vector
} // namespace Anki

#endif // __Victor_Engine_XYPlannerConfig_H__