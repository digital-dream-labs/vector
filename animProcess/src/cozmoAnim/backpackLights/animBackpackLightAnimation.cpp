/**
* File: backpackLightAnimation.cpp
*
* Authors: Kevin M. Karol
* Created: 6/21/18
*
* Description: Definitions for building/parsing backpack lights
*
* Copyright: Anki, Inc. 2018
*
**/

#include "cozmoAnim/backpackLights/animBackpackLightAnimation.h"
#include "coretech/common/engine/jsonTools.h"

namespace Anki {
namespace Vector {
namespace Anim {
namespace BackpackLightAnimation {

bool DefineFromJSON(const Json::Value& jsonDef, BackpackAnimation& outAnim)
{
#define ARRAY_TO_LIGHTSTATE(arr, lightState, lightStateField) \
  {                                                           \
    for(int i = 0; i < arr.size(); ++i) {                     \
      lightState[i].lightStateField = arr[i];                 \
    }                                                         \
  }

  // Json definitions have an individual array for each field but
  // we actually store it as a single array of a struct containing all fields
  // so we need to convert between the two
  bool res = true;
  std::array<u32, (int)LEDId::NUM_BACKPACK_LEDS> arr;
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "onColors", arr, true);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, onColor);
  
  res &= JsonTools::GetColorValuesToArrayOptional(jsonDef, "offColors", arr, true);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offColor);

  res &= JsonTools::GetArrayOptional(jsonDef, "onPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, onPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "offPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOnPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, transitionOnPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "transitionOffPeriod_ms", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, transitionOffPeriod_ms);

  res &= JsonTools::GetArrayOptional(jsonDef, "offset", arr);
  ARRAY_TO_LIGHTSTATE(arr, outAnim.lights.lights, offset_ms);

  return res;

  #undef ARRAY_TO_LIGHTSTATE
}

} // namespace BackpackLightAnimation
} // namespace Anim
} // namespace Vector
} // namespace Anki
