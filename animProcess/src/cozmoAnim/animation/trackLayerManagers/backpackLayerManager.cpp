/**
 * File: backpackLayerManager.cpp
 *
 * Authors: Al Chaussee
 * Created: 06/26/2017
 *
 * Description: Specific track layer manager for BackpackLightsKeyFrames
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cozmoAnim/animation/trackLayerManagers/backpackLayerManager.h"

#include "util/console/consoleInterface.h"
#include "util/helpers/boundedWhile.h"

namespace Anki {
namespace Vector {
namespace Anim {

// How long to wait before the lights should start glitching
CONSOLE_VAR(u32, kGlitchLightDelay_ms,    "GlitchLights", 200);

// Duration of each glitchy backpack light keyframe
CONSOLE_VAR(u32, kGlitchLightDuration_ms, "GlitchLights", 60);
  
BackpackLayerManager::BackpackLayerManager(const Util::RandomGenerator& rng)
: ITrackLayerManager<BackpackLightsKeyFrame>(rng)
{

}

void BackpackLayerManager::GenerateGlitchLights(Animations::Track<BackpackLightsKeyFrame>& track) const
{
  
  track.Clear();
  
  /*
  // Off lights until delay_ms is reached
  AnimKeyFrame::BackpackLights lights;
  lights.colors.fill(0);
  BackpackLightsKeyFrame frame;
  frame.SetLights(lights);
  frame.SetDuration(kGlitchLightDelay_ms);
  frame.SetTriggerTime_ms(0);
  track.AddKeyFrameToBack(frame);
  
  // Turn middle light on
  lights.colors[(int)LEDId::LED_BACKPACK_MIDDLE] = NamedColors::RED;
  frame.SetLights(lights);
  frame.SetTriggerTime_ms(kGlitchLightDelay_ms);
  frame.SetDuration(kGlitchLightDuration_ms);
  track.AddKeyFrameToBack(frame);
  
  // Can pick random lights from everything except for middle lights
  std::vector<int> backpackLightsToPickFrom = {{
    (int)LEDId::LED_BACKPACK_BACK,
    (int)LEDId::LED_BACKPACK_FRONT,
    (int)LEDId::LED_BACKPACK_LEFT,
    (int)LEDId::LED_BACKPACK_RIGHT
  }};
  
  // Middle light plus a random light
  int rand = GetRNG().RandInt((int)backpackLightsToPickFrom.size());
  lights.colors[backpackLightsToPickFrom[rand]] = NamedColors::RED;
  frame.SetLights(lights);
  frame.SetTriggerTime_ms(kGlitchLightDuration_ms + kGlitchLightDelay_ms);
  track.AddKeyFrameToBack(frame);
  
  // Remove the random light from the set of lights we can pick so we don't pick it again
  backpackLightsToPickFrom.erase(backpackLightsToPickFrom.begin() + rand);
  
  // Clear middle light
  lights.colors[(int)LEDId::LED_BACKPACK_MIDDLE] = 0;
  
  // Previous random light plus another random light
  rand = GetRNG().RandInt((int)backpackLightsToPickFrom.size());
  lights.colors[backpackLightsToPickFrom[rand]] = NamedColors::RED;
  frame.SetLights(lights);
  frame.SetTriggerTime_ms((kGlitchLightDuration_ms*2) + kGlitchLightDelay_ms);
  track.AddKeyFrameToBack(frame);
  
  // Off lights
  lights.colors.fill(0);
  frame.SetLights(lights);
  frame.SetTriggerTime_ms((kGlitchLightDuration_ms*3) + kGlitchLightDelay_ms);
  track.AddKeyFrameToBack(frame);
   */
}

}  
}
}
