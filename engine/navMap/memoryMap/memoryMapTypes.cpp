/**
 * File: navMemoryMapTypes.cpp
 *
 * Author: Raul
 * Date:   01/11/2016
 *
 * Description: Type definitions for the navMemoryMap.
 *
 * Copyright: Anki, Inc. 2015
 **/
#include "memoryMapTypes.h"

#include "util/logging/logging.h"
#include "util/math/numericCast.h"
#include "util/helpers/fullEnumToValueArrayChecker.h"

namespace Anki {
namespace Vector {
namespace MemoryMapTypes {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool ExpectsAdditionalData(EContentType type)
{
  DEV_ASSERT(type != EContentType::_Count, "MemoryMapTypes.ExpectsAdditionalData.UsingControlTypeIsNotAllowed");
  
  // using switch to force at compilation type to decide on new types
  switch(type)
  {
    case EContentType::Unknown:
    case EContentType::ClearOfObstacle:
    case EContentType::ClearOfCliff:
    case EContentType::ObstacleUnrecognized:
    case EContentType::InterestingEdge:
    case EContentType::NotInterestingEdge:
    {
      return false;
    }
    case EContentType::ObstacleObservable:
    case EContentType::Cliff:
    case EContentType::ObstacleProx:
    {
      return true;
    }
    
    case EContentType::_Count:
    {
      return false;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* EContentTypeToString(EContentType contentType)
{
  switch (contentType) {
    case EContentType::Unknown: return "Unknown";
    case EContentType::ClearOfObstacle: return "ClearOfObstacle";
    case EContentType::ClearOfCliff: return "ClearOfCliff";
    case EContentType::ObstacleObservable: return "ObstacleObservable";
    case EContentType::ObstacleProx: return "ObstacleProx";
    case EContentType::ObstacleUnrecognized: return "ObstacleUnrecognized";
    case EContentType::Cliff: return "Cliff";
    case EContentType::InterestingEdge: return "InterestingEdge";
    case EContentType::NotInterestingEdge: return "NotInterestingEdge";
    case EContentType::_Count: return "ERROR_COUNT_SHOULD_NOT_BE_USED";
  }
  return "ERROR";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EContentTypePackedType EContentTypeToFlag(EContentType contentType)
{
  const int contentTypeValue = Util::numeric_cast<int>( contentType );
  DEV_ASSERT(contentTypeValue < sizeof(EContentTypePackedType)*8, "ENodeContentTypeToFlag.InvalidContentType");
  const EContentTypePackedType flag = (1 << contentTypeValue);
  return flag;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool IsInEContentTypePackedType(EContentType contentType, EContentTypePackedType contentPackedTypes)
{
  const EContentTypePackedType packedType = EContentTypeToFlag(contentType);
  const bool isIn = (packedType & contentPackedTypes) != 0;
  return isIn;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EContentTypePackedType ConvertContentArrayToFlags(const MemoryMapTypes::FullContentArray& array)
{
  using namespace MemoryMapTypes;
  using namespace QuadTreeTypes;
  
  DEV_ASSERT(IsSequentialArray(array), "MemoryMapTreeTypes.ConvertContentArrayToFlags.InvalidArray");

  EContentTypePackedType contentTypeFlags = 0;
  for( const auto& entry : array )
  {
    if ( entry.Value() ) {
      const EContentTypePackedType contentTypeFlag = EContentTypeToFlag(entry.EnumValue());
      contentTypeFlags = contentTypeFlags | contentTypeFlag;
    }
  }

  return contentTypeFlags;
}


} // namespace
} // namespace
} // namespace
