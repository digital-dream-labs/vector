/**
 * File: memoryMapData_ProxObstacle.cpp
 *
 * Author: Michael Willett
 * Date:   2017-07-31
 *
 * Description: Data for obstacle quads (explored and unexplored).
 *
 * Copyright: Anki, Inc. 2017
 **/
 
#include "memoryMapData_ProxObstacle.h"
#include "clad/types/memoryMap.h"

namespace Anki {
namespace Vector {
  

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MemoryMapData_ProxObstacle::MemoryMapData_ProxObstacle(ExploredType explored, const Pose2d& pose, RobotTimeStamp_t t)
: MemoryMapData( MemoryMapTypes::EContentType::ObstacleProx,  t, true)
, _pose(pose)
, _explored(explored)
, _belief(40)
, _collidable(true)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MemoryMapTypes::MemoryMapDataPtr MemoryMapData_ProxObstacle::Clone() const
{
  return MemoryMapDataWrapper<MemoryMapData_ProxObstacle>(*this);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMapData_ProxObstacle::Equals(const MemoryMapData* other) const
{
  if ( other == nullptr || other->type != type ) {
    return false;
  }

  const MemoryMapData_ProxObstacle* castPtr = static_cast<const MemoryMapData_ProxObstacle*>( other );
  const bool isNearLocation = IsNearlyEqual( _pose.GetTranslation(), castPtr->_pose.GetTranslation(), 20.f ); // close enough to initial observed pose
  const bool exploredSame = _explored == castPtr->_explored;
  return isNearLocation && exploredSame;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ExternalInterface::ENodeContentTypeEnum MemoryMapData_ProxObstacle::GetExternalContentType() const
{
  return _explored ? ExternalInterface::ENodeContentTypeEnum::ObstacleProxExplored : ExternalInterface::ENodeContentTypeEnum::ObstacleProx;
}

} // namespace Vector
} // namespace Anki
