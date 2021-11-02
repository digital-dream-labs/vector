/**
 * File: memoryMapData_ObservableObject.cpp
 *
 * Author: Michael Willett
 * Date:   2017-07-31
 *
 * Description: Data for obstacle quads.
 *
 * Copyright: Anki, Inc. 2017
 **/
 
#include "memoryMapData_ObservableObject.h"
#include "clad/types/memoryMap.h"
#include "coretech/common/engine/math/polygon.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MemoryMapData_ObservableObject::MemoryMapData_ObservableObject(const ObservableObject& o, 
                                                               const Poly2f& p, 
                                                               RobotTimeStamp_t t)
: MemoryMapData(MemoryMapTypes::EContentType::ObstacleObservable, t, true)
, id(o.GetID())
, boundingPoly(p)
, _poseIsVerified(true)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MemoryMapTypes::MemoryMapDataPtr MemoryMapData_ObservableObject::Clone() const
{
  return MemoryMapDataWrapper<MemoryMapData_ObservableObject>(*this);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMapData_ObservableObject::Equals(const MemoryMapData* other) const
{
  if ( other == nullptr || other->type != type ) {
    return false;
  }

  const MemoryMapData_ObservableObject* castPtr = static_cast<const MemoryMapData_ObservableObject*>( other );
  const bool retv = (id == castPtr->id);
  return retv;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ExternalInterface::ENodeContentTypeEnum MemoryMapData_ObservableObject::GetExternalContentType() const
{
  return ExternalInterface::ENodeContentTypeEnum::ObstacleCube;
}

} // namespace Vector
} // namespace Anki
