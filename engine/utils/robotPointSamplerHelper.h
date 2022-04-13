/**
 * File: robotPointSamplerHelper.h
 *
 * Author: ross
 * Created: 2018 Jun 29
 *
 * Description: Helper class for rejection sampling 2D positions and polygons that abide by some constraints related to the robot
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Engine_Utils_RobotPointSamplerHelper_H__
#define __Engine_Utils_RobotPointSamplerHelper_H__

#include "coretech/common/engine/math/polygon_fwd.h"
#include "coretech/common/engine/math/pose.h"
#include "engine/navMap/memoryMap/memoryMapTypes.h"
#include "util/random/rejectionSamplerHelper.h"

#include <set>

namespace Anki{
  
class Pose3d;
  
namespace Util {
  class RandomGenerator;
}
  
namespace Vector{
  
class INavMap;
  
namespace RobotPointSamplerHelper {
  
// uniformly sample a point on circle of radius (0, radius). Optionally supply minTheta and maxTheta to only sample
// points in a semi-circle where theta is in [minTheta, maxTheta)
Point2f SamplePointInCircle( Util::RandomGenerator& rng,
                             f32 radius,
                             f32 minTheta = -M_PI_F, f32 maxTheta = M_PI_F );

// uniformly sample a point on an annulus between radii (minRadius, maxRadius). Optionally supply minTheta and maxTheta
// to only sample points in a semi-annulus where theta is in [minTheta, maxTheta)
Point2f SamplePointInAnnulus( Util::RandomGenerator& rng,
                              f32 minRadius, f32 maxRadius,
                              f32 minTheta = -M_PI_F, f32 maxTheta = M_PI_F );

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RejectIfWouldCrossCliff : public Anki::Util::RejectionSamplingCondition<Point2f>
{
public:
  explicit RejectIfWouldCrossCliff( float minCliffDist_mm );
  
  void SetRobotPosition(const Point2f& pos);
  
  // If not set, any sample that is within minCliffDist_mm is accepted and any outside is rejected.
  // If set, then additionally, any sample between minCliffDist_mm and maxCliffDist_mm is accepted
  // with probability linearly increasing from 0 to 1 over that range
  void SetAcceptanceInterpolant( float maxCliffDist_mm, Util::RandomGenerator& rng );
  
  // This method caches cliff pointers, so must be called every time you want to use this condition
  // with the latest memory map data
  void UpdateCliffs( std::shared_ptr<const INavMap> memoryMap );
  
  virtual bool operator()( const Point2f& sampledPos ) override;
  
private:
  std::set<const Pose3d*> _cliffs;
  Point2f _robotPos;
  bool _setRobotPos = false;
  float _minCliffDistSq;
  Util::RandomGenerator* _rng = nullptr;
  float _maxCliffDistSq = 0.0f;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RejectIfInRange : public Anki::Util::RejectionSamplingCondition<Point2f>
{
public:
  RejectIfInRange( float minDist_mm, float maxDist_mm );

  // Note: Calling either of these functions will overwrite any existing
  // "other positions"
  void SetOtherPosition(const Point2f& pos);
  void SetOtherPositions(const std::vector<Point2f>& pos);

  // Will reject the sampledPos (i.e. return false) if samplePos is in range
  // of _any_ of the "other positions".
  //
  // For example, say you want to reject any sampled point that is near a cube.
  // Call SetOtherPositions() with a vector of all the known cube positions. Then
  // call operator() with your sample position, and it will return false if it
  // is too close to any cube.
  //
  // Note: Requires SetOtherPosition(s) to be set before calling this.
  virtual bool operator()( const Point2f& sampledPos ) override;
private:
  std::vector<Point2f> _otherPos;
  const float _minDistSq;
  const float _maxDistSq;
  bool _setOtherPos = false;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RejectIfNotInRange : public Anki::Util::RejectionSamplingCondition<Point2f>
{
  public:
    RejectIfNotInRange( float minDist_mm, float maxDist_mm );
    void SetOtherPosition(const Point2f& pos);
    
    // Requires SetOtherPosition to be set
    virtual bool operator()( const Point2f& sampledPos ) override;
  private:
    Point2f _otherPos;
    const float _minDistSq;
    const float _maxDistSq;
    bool _setOtherPos = false;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RejectIfChargerOutOfView : public Anki::Util::RejectionSamplingCondition<Point2f>
{
public:
  RejectIfChargerOutOfView();
  
  void SetChargerPose(const Pose3d& pose);
  
  // If not set, any sample from which the charger is out of view is rejected. If set, it is accepted with probability p
  void SetAcceptanceProbability( float p, Util::RandomGenerator& rng );
  void ClearAcceptanceProbability();
  
  // Will reject any position from which the charger would not be visible. Note that this does not check distance to
  // charger, just that the marker would be visible from the given position.
  virtual bool operator()( const Point2f& sampledPos ) override;
  
private:
  Pose3d _chargerPose;
  bool _setChargerPose = false;
  Util::RandomGenerator* _rng = nullptr;
  float _pAccept = 0.0f;
};
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RejectIfCollidesWithMemoryMap : public Anki::Util::RejectionSamplingCondition<Poly2f>
{
public:
  explicit RejectIfCollidesWithMemoryMap( const MemoryMapTypes::FullContentArray& collisionTypes );
  
  void SetMemoryMap( std::shared_ptr<const INavMap> memoryMap ) { _memoryMap = memoryMap; }
  
  // If not set, any sample that collides is rejected. If set, it is accepted with probability p
  void SetAcceptanceProbability( float p, Util::RandomGenerator& rng );
  
  virtual bool operator()( const Poly2f& sampledPoly ) override;
  
private:
  std::shared_ptr<const INavMap> _memoryMap = nullptr; // BAD! Nothing guarantees that this is current
  const MemoryMapTypes::EContentTypePackedType _collisionTypes;
  Util::RandomGenerator* _rng = nullptr;
  float _pAccept = 0.0f;
};
  
}
  
} // namespace
} // namespace

#endif
