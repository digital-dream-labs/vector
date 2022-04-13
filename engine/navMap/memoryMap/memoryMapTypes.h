/**
 * File: memoryMapTypes.h
 *
 * Author: Raul
 * Date:   01/11/2016
 *
 * Description: Type definitions for the MemoryMap.
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_TYPES_H
#define ANKI_COZMO_MEMORY_MAP_TYPES_H

#include "engine/navMap/quadTree/quadTreeTypes.h"

#include "util/helpers/fullEnumToValueArrayChecker.h"
#include "util/helpers/templateHelpers.h"
#include "clad/types/memoryMap.h"

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace Anki {
namespace Vector {

class MemoryMapData;

namespace MemoryMapTypes {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// structs
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// content detected in the map
enum class EContentType : uint8_t {
  Unknown,               // not discovered
  ClearOfObstacle,       // an area without obstacles
  ClearOfCliff,          // an area without obstacles or cliffs
  ObstacleObservable,    // an area with obstacles we recognize as observable
  ObstacleProx,          // an area with an obstacle found with the prox sensor
  ObstacleUnrecognized,  // an area with obstacles we do not recognize
  Cliff,                 // an area with cliffs or holes
  InterestingEdge,       // a border/edge detected by the camera
  NotInterestingEdge,    // a border/edge detected by the camera that we have already explored and it's not interesting anymore
  _Count // Flag, not a type
};

struct MapBroadcastData {
  MapBroadcastData() : mapInfo(), quadInfo() {}
  ExternalInterface::MemoryMapInfo                  mapInfo;
  std::vector<ExternalInterface::MemoryMapQuadInfo> quadInfo;
  std::vector<ExternalInterface::MemoryMapQuadInfoFull> quadInfoFull;
};

// Provide a custom hasher for unordered sets
template<class T>
struct MemoryMapDataHasher
{
  size_t operator()(const MemoryMapDataWrapper<T> & obj) const {
    return std::hash<std::shared_ptr<T>>()(obj.GetSharedPtr());
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Common Aliases
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

using MemoryMapRegion        = QuadTreeTypes::FoldableRegion;

using MemoryMapDataPtr       = MemoryMapDataWrapper<MemoryMapData>;
using MemoryMapDataConstPtr  = MemoryMapDataWrapper<const MemoryMapData>;

using MemoryMapDataList      = std::unordered_set<MemoryMapDataPtr, MemoryMapDataHasher<MemoryMapData>>;
using MemoryMapDataConstList = std::unordered_set<MemoryMapDataConstPtr, MemoryMapDataHasher<const MemoryMapData>>;

using NodeTransformFunction  = QuadTreeTypes::NodeTransformFunction;
using NodePredicate          = std::function<bool (MemoryMapDataConstPtr)>;

using QuadInfoVector         = std::vector<ExternalInterface::MemoryMapQuadInfo>;
using QuadInfoFullVector     = std::vector<ExternalInterface::MemoryMapQuadInfoFull>;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Helper Functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// returns false if the base constructor for MemoryMapData can be used with content type, and true if a derived 
// class constructor must be called, forcing additional data to be provided on instantiation
bool ExpectsAdditionalData(EContentType type);

// String representing ENodeContentType for debugging purposes
const char* EContentTypeToString(EContentType contentType);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Array of content that provides an API with compilation checks for algorithms that require combinations
// of content types. It's for example used to make sure that you define a value for all content types, rather
// than including only those you want to be true.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

using FullContentArray = Util::FullEnumToValueArrayChecker::FullEnumToValueArray<EContentType, bool>;
using Util::FullEnumToValueArrayChecker::IsSequentialArray; // import IsSequentialArray to this namespace

// variable type in which we can pack EContentType as flags. Check ENodeContentTypeToFlag
using EContentTypePackedType = uint32_t;

// Converts EContentType values into flag bits. This is handy because I want to store EContentType in
// the smallest type possible since we have a lot of quad nodes, but I want to pass groups as bit flags in one
// packed variable
EContentTypePackedType EContentTypeToFlag(EContentType nodeContentType);

// Converts and array of EContentType values into flag bits
EContentTypePackedType ConvertContentArrayToFlags(const MemoryMapTypes::FullContentArray& array);

// returns true if contentType is in PackedTypes
bool IsInEContentTypePackedType(EContentType contentType, EContentTypePackedType contentPackedTypes);

} // namespace MemoryMapTypes
} // namespace Vector
} // namespace Anki

#endif // 
