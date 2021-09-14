/**
 * File: backpackLayerManager.h
 *
 * Authors: Al Chaussee
 * Created: 06/26/2017
 *
 * Description: Specific track layer manager for BackpackLightsKeyFrames
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_BackpackLayerManager_H__
#define __Anki_Cozmo_BackpackLayerManager_H__

#include "cozmoAnim/animation/trackLayerManagers/iTrackLayerManager.h"

#include "coretech/common/shared/types.h"
#include "cannedAnimLib/cannedAnims/animation.h"
#include "cannedAnimLib/baseTypes/track.h"

namespace Anki {
namespace Vector {
namespace Anim {

class BackpackLayerManager : public ITrackLayerManager<BackpackLightsKeyFrame>
{
public:
  
  BackpackLayerManager(const Util::RandomGenerator& rng);
  
  // TODO: VIC-447: Restore glitching
  // Generates a track of all keyframes necessary for glitchy backpack lights
  void GenerateGlitchLights(Animations::Track<BackpackLightsKeyFrame>& track) const;
  
};
  
}
}
}

#endif
