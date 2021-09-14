/**
 * File: proceduralFaceModifierTypes.h
 *
 * Authors: Jordan Rivas
 * Created: 05/09/2018
 *
 * Description: Define types for procedural face modifiers
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Anki_Cozmo_ProceduralFaceModifierTypes_H__
#define __Anki_Cozmo_ProceduralFaceModifierTypes_H__

#include <vector>

namespace Anki {
namespace Vector {

  // Procedural Eye Blink types
  enum class BlinkState : uint8_t {
    Closing,
    Closed,
    JustOpened,
    Opening
  };
  
  struct BlinkEvent {
    uint32_t time_ms;
    BlinkState state;
    BlinkEvent(uint32_t time_ms, BlinkState state)
    : time_ms(time_ms)
    , state(state) {}
  };
  
  using BlinkEventList = std::vector<BlinkEvent>;

} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_ProceduralFaceModifierTypes_H__ */
