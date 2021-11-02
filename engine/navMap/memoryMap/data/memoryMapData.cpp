/**
 * File: memoryMapData.cpp
 *
 * Author: rpss
 * Date:   apr 12 2018
 *
 * Description: Base for data structs that will be held in every node depending on their content type.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "engine/navMap/memoryMap/data/memoryMapData.h"
#include "engine/navMap/memoryMap/data/memoryMapData_ProxObstacle.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMapData::CanOverrideSelfWithContent(MemoryMapDataConstPtr newContent) const
{
  EContentType newContentType = newContent->type;
  EContentType dataType = type;
  if ( newContentType == EContentType::Cliff ) 
  {
    // note: new cliffs cannot override old cliffs
    // a special transformation function is needed instead to ensure that
    // from-vision and from-sensor fields are properly handled
    return ( dataType != EContentType::Cliff );
  }
  else if ( dataType == EContentType::Cliff )
  {
    // Cliff can only be overridden by a full ClearOfCliff (the cliff is gone)
    const bool isTotalClear = (newContentType == EContentType::ClearOfCliff);
    return isTotalClear;
  }
  else if ( newContentType == EContentType::ClearOfObstacle )
  {
    // ClearOfObstacle currently comes from vision or prox sensor having a direct line of sight
    // to some object, so it can't clear obsstacles it cant see (cliffs and unrecognized). Additionally,
    // ClearOfCliff is currently a superset of Clear of Obstacle, so trust ClearOfCliff flags.
    const bool isTotalClear = ( dataType != EContentType::Cliff ) &&
                              ( dataType != EContentType::ClearOfCliff ) &&
                              ( dataType != EContentType::ObstacleUnrecognized )&&
                              ( dataType != EContentType::ObstacleObservable );
    return isTotalClear;
  }
  else if ( newContentType == EContentType::InterestingEdge )
  {
    // InterestingEdge can only override basic node types, because it would cause data loss otherwise. For example,
    // we don't want to override a recognized marked cube or a cliff with their own border
    if ( ( dataType == EContentType::ObstacleObservable   ) ||
         ( dataType == EContentType::ObstacleUnrecognized ) ||
         ( dataType == EContentType::Cliff                ) ||
         ( dataType == EContentType::NotInterestingEdge   ) )
    {
      return false;
    }
  }
  else if ( newContentType == EContentType::ObstacleProx )
  {
    if ( ( dataType == EContentType::ObstacleObservable   ) ||
         ( dataType == EContentType::Cliff                ) )
    {
      return false;
    }
    // an unexplored prox obstacle shouldnt replace an explored prox obstacle
    if( dataType == EContentType::ObstacleProx ) {
      DEV_ASSERT( dynamic_cast<const MemoryMapData_ProxObstacle*>(this) != nullptr, "MemoryMapData.CanOverride.InvalidCast" );
      auto castPtr = static_cast<const MemoryMapData_ProxObstacle*>(this);
      return !castPtr->IsExplored();
    }
  }
  else if ( newContentType == EContentType::NotInterestingEdge )
  {
    // NotInterestingEdge can only override interesting edges
    if ( dataType != EContentType::InterestingEdge ) {
      return false;
    }
  }
  
  return true;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ExternalInterface::ENodeContentTypeEnum MemoryMapData::GetExternalContentType() const
{
  using namespace MemoryMapTypes;
  using namespace ExternalInterface;
  
  ENodeContentTypeEnum externalContentType = ENodeContentTypeEnum::Unknown;
  switch (type) {
    case EContentType::Unknown:               { externalContentType = ENodeContentTypeEnum::Unknown;              break; }
    case EContentType::ClearOfObstacle:       { externalContentType = ENodeContentTypeEnum::ClearOfObstacle;      break; }
    case EContentType::ClearOfCliff:          { externalContentType = ENodeContentTypeEnum::ClearOfCliff;         break; }
    case EContentType::ObstacleUnrecognized:  { externalContentType = ENodeContentTypeEnum::ObstacleUnrecognized; break; }
    case EContentType::InterestingEdge:       { externalContentType = ENodeContentTypeEnum::InterestingEdge;      break; }
    case EContentType::NotInterestingEdge:    { externalContentType = ENodeContentTypeEnum::NotInterestingEdge;   break; }
    // handled elsewhere
    case EContentType::ObstacleObservable:
    case EContentType::Cliff:
    case EContentType::ObstacleProx:
    case EContentType::_Count:
    {
      DEV_ASSERT(false, "NavMeshQuadTreeNode._Count");
    }
      break;
  }
  return externalContentType;
}


} // namespace Vector
} // namespace Anki
