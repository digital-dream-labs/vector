/**
 * File: memoryMapData_ObservableObject.h
 *
 * Author: Michael Willett
 * Date:   2017-07-31
 *
 * Description: Data for obstacle quads.
 *
 * Copyright: Anki, Inc. 2017
 **/
 
 #ifndef __Anki_Cozmo_MemoryMapDataObservableObject_H__
 #define __Anki_Cozmo_MemoryMapDataObservableObject_H__

#include "memoryMapData.h"

#include "coretech/common/engine/math/polygon_fwd.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "engine/cozmoObservableObject.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// MemoryMapData_ObservableObject
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class MemoryMapData_ObservableObject : public MemoryMapData
{
public:
  // constructor
  MemoryMapData_ObservableObject(const ObservableObject& o, const Poly2f& p, RobotTimeStamp_t t);
  
  // create a copy of self (of appropriate subclass) and return it
  MemoryMapDataPtr Clone() const override;
  
  // return true if this type collides with the robot
  virtual bool IsCollisionType() const override { return _poseIsVerified; }

  // if we should have seen the object with the camera, but did not
  void MarkUnobserved() { _poseIsVerified = false; }
  
  // compare to IMemoryMapData and return bool if the data stored is the same
  bool Equals(const MemoryMapData* other) const override;
  
  virtual ExternalInterface::ENodeContentTypeEnum GetExternalContentType() const override;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // If you add attributes, make sure you add them to ::Equals and ::Clone (if required)
  const ObjectID id;
  const Poly2f boundingPoly; 
  
  static bool HandlesType(EContentType otherType) {
    return otherType == EContentType::ObstacleObservable;
  }

private: 
  bool _poseIsVerified;
};
 
} // namespace
} // namespace

#endif //
