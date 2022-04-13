/**
 * File: memoryMapData.h
 *
 * Author: Raul
 * Date:   08/02/2016
 *
 * Description: Base for data structs that will be held in every node depending on their content type.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_DATA_H
#define ANKI_COZMO_MEMORY_MAP_DATA_H

#include "coretech/common/engine/robotTimeStamp.h"
#include "engine/navMap/memoryMap/memoryMapTypes.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// NavMemoryMapQuadData
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class MemoryMapData
{
protected:
  using MemoryMapDataPtr      = MemoryMapTypes::MemoryMapDataPtr;
  using MemoryMapDataConstPtr = MemoryMapTypes::MemoryMapDataConstPtr;
  using EContentType          = MemoryMapTypes::EContentType;

public:
  const EContentType type;

  // base_of_five_defaults - destructor
  MemoryMapData(EContentType type = EContentType::Unknown, RobotTimeStamp_t time = 0) : MemoryMapData(type, time, false) {}
  MemoryMapData(const MemoryMapData&) = default;
  MemoryMapData(MemoryMapData&&) = default;
  virtual ~MemoryMapData() = default;
  
  // create a copy of self (of appropriate subclass) and return it
  virtual MemoryMapDataPtr Clone() const { return MemoryMapDataPtr(*this); };
  
  virtual ExternalInterface::ENodeContentTypeEnum GetExternalContentType() const;
  
  // return true if this type collides with the robot
  virtual bool IsCollisionType() const { return ((type == EContentType::ObstacleUnrecognized) || (type == EContentType::Cliff)); }
  
  // compare to MemoryMapData and return bool if the data stored is the same
  virtual bool Equals(const MemoryMapData* other) const { return (type == other->type); }
  
  void SetLastObservedTime(RobotTimeStamp_t t) {lastObserved_ms = t;}
  void SetFirstObservedTime(RobotTimeStamp_t t) {firstObserved_ms = t;}
  RobotTimeStamp_t GetLastObservedTime()  const {return lastObserved_ms;}
  RobotTimeStamp_t GetFirstObservedTime() const {return firstObserved_ms;}
  
  // returns true if this node can be replaced by the given content type. Some content type replacement
  // rules depend on whether the quad center is fully contained within the insertion polygon (centerContainedByROI)
  bool CanOverrideSelfWithContent(MemoryMapDataConstPtr newContent) const;
  
  // Wrappers for data casting operations  
  template <class T>
  static MemoryMapDataWrapper<T> MemoryMapDataCast(MemoryMapDataPtr ptr);
  
  template <class T>
  static MemoryMapDataWrapper<const T> MemoryMapDataCast(MemoryMapDataConstPtr ptr);
  
protected:
  MemoryMapData(MemoryMapTypes::EContentType type, RobotTimeStamp_t time, bool calledFromDerived);
  
  static bool HandlesType(MemoryMapTypes::EContentType otherType) {
    return (otherType != MemoryMapTypes::EContentType::ObstacleProx) &&
           (otherType != MemoryMapTypes::EContentType::Cliff) &&
           (otherType != MemoryMapTypes::EContentType::ObstacleObservable);
  }

private:
  RobotTimeStamp_t firstObserved_ms;
  RobotTimeStamp_t lastObserved_ms;
};

inline MemoryMapData::MemoryMapData(MemoryMapTypes::EContentType type, RobotTimeStamp_t time, bool expectsAdditionalData)
  : type(type)
  , firstObserved_ms(time)
  , lastObserved_ms(time) 
{
  // need to make sure we dont ever create a MemoryMapData without providing all information required.
  // This locks us from creating something like MemoryMapData_ObservableObject without the ID, for instance.
  DEV_ASSERT(ExpectsAdditionalData(type) == expectsAdditionalData, "MemoryMapData.ImproperConstructorCalled");
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Helper functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template <class T>
inline MemoryMapDataWrapper<T> MemoryMapData::MemoryMapDataCast(MemoryMapDataPtr ptr)
{
  DEV_ASSERT( ptr.GetSharedPtr(), "MemoryMapDataCast.NullQuadData" );
  DEV_ASSERT( T::HandlesType( ptr->type ), "MemoryMapDataCast.UnexpectedQuadData" );
  DEV_ASSERT( std::dynamic_pointer_cast<T>(ptr.GetSharedPtr()), "MemoryMapDataCast.BadQuadDataDynCast" );
  return MemoryMapDataWrapper<T>(std::static_pointer_cast<T>(ptr.GetSharedPtr()));
}

template <class T>
inline MemoryMapDataWrapper<const T> MemoryMapData::MemoryMapDataCast(MemoryMapDataConstPtr ptr)
{
  assert( T::HandlesType( ptr->type ) );
  assert( std::dynamic_pointer_cast<const T>(ptr.GetSharedPtr()) );
  return MemoryMapDataWrapper<const T>(std::static_pointer_cast<const T>(ptr.GetSharedPtr()));
}

} // namespace
} // namespace

#endif //
