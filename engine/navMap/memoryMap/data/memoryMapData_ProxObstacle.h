/**
 * File: memoryMapData_ProxObstacle.h
 *
 * Author: Michael Willett
 * Date:   2017-07-31
 *
 * Description: Data for obstacle quads (explored and unexplored).
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_DATA_PROX_OBSTACLE_H
#define ANKI_COZMO_MEMORY_MAP_DATA_PROX_OBSTACLE_H

#include "memoryMapData.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/robotTimeStamp.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// MemoryMapData_ProxObstacle
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class MemoryMapData_ProxObstacle : public MemoryMapData
{
public:
  enum ExploredType {
    NOT_EXPLORED=0,
    EXPLORED
  };
  
  // constructor
  MemoryMapData_ProxObstacle(ExploredType explored, const Pose2d& pose, RobotTimeStamp_t t);
  
  // create a copy of self (of appropriate subclass) and return it
  virtual MemoryMapDataPtr Clone() const override;
  
  // return true if this type collides with the robot
  virtual bool IsCollisionType() const override { return IsConfirmedObstacle(); }

  // disable collisions with this prox obstacle (eg, if in the habitat)
  void SetCollidable(bool enable) { _collidable = enable; }

  // compare to IMemoryMapData and return bool if the data stored is the same
  virtual bool Equals(const MemoryMapData* other) const override;
  
  static bool HandlesType(EContentType otherType) {
    return (otherType == MemoryMapTypes::EContentType::ObstacleProx);
  }
  
  virtual ExternalInterface::ENodeContentTypeEnum GetExternalContentType() const override;
  
  void MarkExplored() { _explored = ExploredType::EXPLORED; }

  // NOTE: at the time of adding the `_belief` value, there was still pending tuning as to performance for 
  //       removing objects. Once weights and thresholds have been verified, we should probably a more formal
  //       coded relationship to the paramters if possible.
  void MarkObserved() { _belief = (_belief >= 96) ? 100 : _belief + 4; }
  void MarkClear()    { _belief = (_belief <= 6 ) ? 0   : _belief - 6; }

  bool IsExplored()          const { return (_explored == ExploredType::EXPLORED); }
  bool IsConfirmedObstacle() const { return (_belief > 40); }
  bool IsConfirmedClear()    const { return (_belief == 0); }

  const Pose2d& GetObservationPose() const    { return _pose; }
  u8            GetObstacleConfidence() const { return _belief; }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // If you add attributes, make sure you add them to ::Equals and ::Clone (if required)
  
  // Important: this is available in all NOT_EXPLORED obstacles and only some EXPLORED. We lose
  // these params when flood filling from EXPLORED to NOT_EXPLORED, although that's not ideal. todo: fix this (FillBorder)
private:

  Pose2d       _pose;       // assumed obstacle pose (based off robot pose when detected)
  ExploredType _explored;   // has Victor visited this node?
  u8           _belief;     // our confidence that there really is an obstacle here
  bool         _collidable; // if the robot should consider this object as a collision type
};
 
} // namespace
} // namespace

#endif //
