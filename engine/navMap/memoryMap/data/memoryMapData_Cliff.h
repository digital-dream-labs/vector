/**
 * File: memoryMapData_Cliff.h
 *
 * Author: Raul
 * Date:   08/02/2016
 *
 * Description: Data for Cliff quads.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_DATA_CLIFF_H
#define ANKI_COZMO_MEMORY_MAP_DATA_CLIFF_H

#include "memoryMapData.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/robotTimeStamp.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// NavMemoryMapQuadData_Cliff
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
struct MemoryMapData_Cliff : public MemoryMapData
{
  // constructor
  MemoryMapData_Cliff(const Pose3d& cliffPose, RobotTimeStamp_t t);
  
  // create a copy of self (of appropriate subclass) and return it
  MemoryMapDataPtr Clone() const override;
  
  // compare to INavMemoryMapQuadData and return bool if the data stored is the same
  bool Equals(const MemoryMapData* other) const override;
  
  virtual ExternalInterface::ENodeContentTypeEnum GetExternalContentType() const override;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // If you add attributes, make sure you add them to ::Equals and ::Clone (if required)
  Pose3d pose; // location and direction we presume for the cliff (from detection)

  // cliff detections from the cliff-sensor
  bool isFromCliffSensor;

  // cliff detections from vision require nearby connected cliff-sensor cliffs
  bool isFromVision;

  static bool HandlesType(EContentType otherType) {
    return otherType == EContentType::Cliff;
  }
};
 
} // namespace
} // namespace

#endif //
