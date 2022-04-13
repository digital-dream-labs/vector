/**
* File: vizObjectBaseId
*
* Author: damjan stulic
* Created: 9/16/15
*
* Description:
*
* Copyright: Anki, inc. 2015
*
*/

#include "engine/viz/vizObjectBaseId.h"
#include <stdint.h>

namespace Anki {
namespace Vector {

const uint32_t kLastValidObjectID = std::numeric_limits<uint32_t>::max() - 100;

// Base IDs for each VizObject type
constexpr const uint32_t VizObjectBaseID[(int)VizObjectType::NUM_VIZ_OBJECT_TYPES+1] = {
  0,         // VIZ_OBJECT_ROBOT
  10000000,  // VIZ_OBJECT_CUBOID
  30000000,  // VIZ_OBJECT_CHARGER
  40000000,  // VIZ_OBJECT_PREDOCKPOSE
  70000000,  // VIZ_OBJECT_HUMAN_HEAD
  15000000,  // VIZ_OBJECT_TEXT
  kLastValidObjectID // Last valid object ID allowed
};

// If this static_assert is hit, and additions have been made to VizObjectType enum
// then there are missing entries in the initializer list written above here
static_assert(VizObjectBaseID[(int)VizObjectType::NUM_VIZ_OBJECT_TYPES] == kLastValidObjectID,  "The last valid object ID does not match expected");

} // end namespace Vector
} // end namespace Anki

