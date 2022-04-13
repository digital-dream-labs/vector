/**
 * File: robotPointSamplerHelper.cpp
 *
 * Author: ross
 * Created: 2018 Jun 29
 *
 * Description: Helper class for rejection sampling 2D positions that abide by some constraints related to the robot
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "engine/utils/robotPointSamplerHelper.h"

#include "coretech/common/engine/math/pose.h"
#include "engine/charger.h"
#include "engine/navMap/iNavMap.h"
#include "engine/navMap/memoryMap/data/memoryMapData_Cliff.h"
#include "util/random/randomGenerator.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "RobotPointSampler"

namespace Anki {
namespace Vector {
  
namespace {
  const float kMaxCliffIntersectionDist_mm = 10000.0f;
}

namespace RobotPointSamplerHelper {
  
using RandomGenerator = Util::RandomGenerator;
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Point2f SamplePointInCircle( Util::RandomGenerator& rng, f32 radius , f32 minTheta, f32 maxTheta )
{
  // (there's another way to do this without the sqrt, but it requires three
  // uniform r.v.'s, and some quick tests show that that ends up being slower)
  DEV_ASSERT((radius > 0.f) && (minTheta < maxTheta) && (minTheta + M_TWO_PI_F >= maxTheta),
             "RobotPointSamplerHelper.SamplePointInCircle.InvalidArgs");
  Point2f ret;
  const float theta = static_cast<float>( rng.RandDblInRange(minTheta, maxTheta) );
  const float u = static_cast<float>( rng.RandDbl() );
  const float r = radius * std::sqrtf( u );
  ret.x() = r * cosf(theta);
  ret.y() = r * sinf(theta);
  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Point2f SamplePointInAnnulus( Util::RandomGenerator& rng, f32 minRadius, f32 maxRadius , f32 minTheta, f32 maxTheta)
{
  DEV_ASSERT((minRadius >= 0.f) && (minRadius < maxRadius) && (minTheta < maxTheta) && (minTheta + M_TWO_PI_F >= maxTheta),
             "RobotPointSamplerHelper.SamplePointInAnnulus.InvalidArgs");
  Point2f ret;
  const f32 minRadiusSq = minRadius * minRadius;
  const float theta = static_cast<float>( rng.RandDblInRange(minTheta, maxTheta) );
  const float u = static_cast<float>( rng.RandDbl() );
  const float r = std::sqrtf( minRadiusSq + (maxRadius*maxRadius - minRadiusSq)*u );
  ret.x() = r * cosf(theta);
  ret.y() = r * sinf(theta);
  return ret;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
RejectIfWouldCrossCliff::RejectIfWouldCrossCliff( float minCliffDist_mm )
  : _minCliffDistSq( minCliffDist_mm * minCliffDist_mm )
{}
 
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfWouldCrossCliff::SetAcceptanceInterpolant( float maxCliffDist_mm, RandomGenerator& rng )
{
  _rng = &rng;
  _maxCliffDistSq = maxCliffDist_mm*maxCliffDist_mm;
  DEV_ASSERT( _maxCliffDistSq > _minCliffDistSq, "RejectIfWouldCrossCliff.SetAcceptanceInterpolant.DistanceError" );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfWouldCrossCliff::SetRobotPosition(const Point2f& pos)
{
  _robotPos = pos;
  _setRobotPos = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfWouldCrossCliff::UpdateCliffs( std::shared_ptr<const INavMap> memoryMap )
{
  _cliffs.clear();
  if( memoryMap == nullptr ) {
    return;
  }
  MemoryMapTypes::MemoryMapDataConstList wasteList;
  memoryMap->FindContentIf([this](MemoryMapTypes::MemoryMapDataConstPtr data){
    if( data->type == MemoryMapTypes::EContentType::Cliff ) {
      const auto& cliffData = MemoryMapData::MemoryMapDataCast<MemoryMapData_Cliff>(data);
      _cliffs.insert( &cliffData->pose );
    }
    return false; // don't actually gather any data
  }, wasteList);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool RejectIfWouldCrossCliff::operator()( const Point2f& sampledPos )
{
  DEV_ASSERT( _setRobotPos, "RejectIfWouldCrossCliff.CallOperator.UninitializedRobotPos" );
  LineSegment lineRobotToSample{ sampledPos, _robotPos };
  float pAccept = 1.0f; // this may be decremented for multiple cliffs
  for( const auto* cliffPose : _cliffs ) {
    const Vec3f cliffDirection = (cliffPose->GetRotation() * X_AXIS_3D());
    // do this in 2d
    const Vec2f cliffEdgeDirection = CrossProduct( Z_AXIS_3D(), cliffDirection ); // sign doesn't matter
    const Point2f cliffPos = cliffPose->GetTranslation();
    // find intersection of lineRobotToSample with cliffEdgeDirection
    LineSegment cliffLine{ cliffPos + cliffEdgeDirection * kMaxCliffIntersectionDist_mm,
                           cliffPos - cliffEdgeDirection * kMaxCliffIntersectionDist_mm };
    Point2f intersectionPoint;
    const bool intersects = lineRobotToSample.IntersectsAt( cliffLine, intersectionPoint );
    if( intersects ) {
      // confirm intersection point lies on cliff edge
      if (!AreVectorsAligned( (intersectionPoint - cliffPos), cliffEdgeDirection, 0.001f )) {
        LOG_WARNING("RejectIfWouldCrossCliff.CallOperator.BadIntersection", "vectors not aligned" );
      }
      // if the intersection pos is close to the cliff pos, reject. If it's far, accept. interpolate in between.
      const float distFromCliffSq = (intersectionPoint - cliffPos).LengthSq();
      if( distFromCliffSq < _minCliffDistSq ) {
        return false;
      }
      if( _rng != nullptr ) {
        const float p = (distFromCliffSq > _maxCliffDistSq)
                        ? 0.0f
                        : 1.0f - (distFromCliffSq - _minCliffDistSq) / (_maxCliffDistSq - _minCliffDistSq);
        // multiple cliffs can contribute to the acceptance probability
        pAccept -= p;
        if( pAccept <= 0.0f ) {
          return false;
        }
      }
    }
  }
  if( (_rng != nullptr) && ((pAccept <= 0.0f) || (pAccept < _rng->RandDbl())) ) {
    // reject
    return false;
  } else {
    return true;
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
RejectIfInRange::RejectIfInRange( float minDist_mm, float maxDist_mm )
: _minDistSq( minDist_mm * minDist_mm )
, _maxDistSq( maxDist_mm * maxDist_mm )
{
  DEV_ASSERT(!std::signbit(minDist_mm) &&
             !std::signbit(maxDist_mm) &&
             (maxDist_mm > minDist_mm),
             "RejectIfInRange.Constructor.InvalidArgs");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfInRange::SetOtherPosition(const Point2f& pos)
{
  SetOtherPositions({{pos}});
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfInRange::SetOtherPositions(const std::vector<Point2f>& pos)
{
  _otherPos.clear();
  _otherPos = pos;
  _setOtherPos = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool RejectIfInRange::operator()( const Point2f& sampledPos )
{
  DEV_ASSERT(_setOtherPos, "RejectIfInRange.CallOperator.OtherPosUninitialized" );
  auto inRangePred = [this, &sampledPos](const Point2f& otherPos) {
    const float distSq = (otherPos - sampledPos).LengthSq();
    return (distSq >= _minDistSq) && (distSq <= _maxDistSq);
  };
  const bool reject = std::any_of(_otherPos.begin(),
                                  _otherPos.end(),
                                  inRangePred);
  return !reject;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
RejectIfNotInRange::RejectIfNotInRange( float minDist_mm, float maxDist_mm )
  : _minDistSq( minDist_mm * minDist_mm )
  , _maxDistSq( maxDist_mm * maxDist_mm )
{ }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfNotInRange::SetOtherPosition(const Point2f& pos)
{
  _otherPos = pos;
  _setOtherPos = true;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool RejectIfNotInRange::operator()( const Point2f& sampledPos )
{
  DEV_ASSERT( _setOtherPos, "RejectIfNotInRange.CallOperator.OtherPosUninitialized" );
  const float distSq = (_otherPos - sampledPos).LengthSq();
  return (distSq >= _minDistSq) && (distSq <= _maxDistSq);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
RejectIfChargerOutOfView::RejectIfChargerOutOfView()
{ }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool RejectIfChargerOutOfView::operator()( const Point2f& sampledPos )
{
  DEV_ASSERT(_setChargerPose, "RejectIfChargerOutOfView.CallOperator.ChargerPoseUninitialized" );
  if (_pAccept == 1.f) {
    return true;
  }
  
  Pose3d samplePose;
  samplePose.SetParent(_chargerPose.GetParent());
  samplePose.SetTranslation({sampledPos.x(), sampledPos.y(), _chargerPose.GetTranslation().z()});
  Pose3d sampleWrtCharger;
  if (!samplePose.GetWithRespectTo(_chargerPose, sampleWrtCharger)) {
    LOG_ERROR("RejectIfChargerOutOfView.FailedGetWithRespectToCharger",
              "Could not get samplePose w.r.t. charger pose");
    return false;
  }
  
  // The charger's origin is at the front of the 'lip' of the charger, and the x axis points inward toward the marker.
  // Therefore if the relative x position of the sample point is negative, we should be able to see the marker.
  const bool chargerInView = sampleWrtCharger.GetTranslation().x() < 0.f;
  
  if (!chargerInView && (_rng != nullptr)) {
    return _rng->RandDbl() <= _pAccept;
  }
  return chargerInView;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfChargerOutOfView::SetChargerPose(const Pose3d& pose)
{
  _chargerPose = pose;
  _setChargerPose = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfChargerOutOfView::SetAcceptanceProbability( float p, RandomGenerator& rng )
{
  _rng = &rng;
  _pAccept = p;
  DEV_ASSERT( _pAccept >= 0.0f && _pAccept <= 1.0f, "RejectIfChargerOutOfView.SetAcceptanceProbability.InvalidP" );
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfChargerOutOfView::ClearAcceptanceProbability()
{
  _rng = nullptr;
  _pAccept = 0.f;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
RejectIfCollidesWithMemoryMap::RejectIfCollidesWithMemoryMap( const MemoryMapTypes::FullContentArray& collisionTypes )
  : _collisionTypes( ConvertContentArrayToFlags(collisionTypes) )
{}
    
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void RejectIfCollidesWithMemoryMap::SetAcceptanceProbability( float p, RandomGenerator& rng )
{
  _rng = &rng;
  _pAccept = p;
  DEV_ASSERT( _pAccept >= 0.0f && _pAccept <= 1.0f, "RejectIfCollidesWithMemoryMap.SetAcceptanceProbability.InvalidP" );
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool RejectIfCollidesWithMemoryMap::operator()( const Poly2f& sampledPoly )
{
  if( _memoryMap == nullptr ) {
    return true;
  }
  const bool collides = _memoryMap->AnyOf( FastPolygon(sampledPoly), [&](const auto& data) { return IsInEContentTypePackedType(data->type, _collisionTypes); } );
  if( collides && (_rng != nullptr) ) {
    const float rv = _rng->RandDbl();
    if( rv <= _pAccept ) {
      return true;
    } else {
      return false;
    }
  }
  return !collides;
}
  
} // namespace
  
} // namespace
} // namespace
