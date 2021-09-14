/**
 * File: iTrackLayerManager.h
 *
 * Authors: Al Chaussee
 * Created: 06/28/2017
 *
 * Description: Templated class for managing animation track layers
 *              of a specific keyframe type
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_ITrackLayerManager_H__
#define __Anki_Cozmo_ITrackLayerManager_H__

#include "coretech/common/shared/types.h"
#include "cannedAnimLib/cannedAnims/animation.h"
#include "cannedAnimLib/baseTypes/track.h"

#include <map>

namespace Anki {
namespace Vector {
namespace Anim {

template<class FRAME_TYPE>
class ITrackLayerManager
{
public:

  ITrackLayerManager(const Util::RandomGenerator& rng);
  
  using ApplyLayerFunc = std::function<bool(const Animations::Track<FRAME_TYPE>&,
                                            const TimeStamp_t,
                                            FRAME_TYPE&)>;
  
  // Updates frame by applying all layers to it using applyLayerFunc which should define
  // how to combine the current keyframe in the layer's track with another keyframe
  // Both applyFunc and this function should return whether or not the frame was updated
  // Note: applyLayerFunc is responsible for moving to the next keyframe of a layer's track
  bool ApplyLayersToFrame(FRAME_TYPE& frame,
                          const TimeStamp_t timeSinceAnimStart_ms,
                          ApplyLayerFunc applyLayerFunc) const;
  
  // Adds the given track as a new layer
  Result AddLayer(const std::string& name,
                  const Animations::Track<FRAME_TYPE>& track);
  
  // Adds the given track as a persitent layer
  void AddPersistentLayer(const std::string& layerName,
                          const Animations::Track<FRAME_TYPE>& track);
  
  // Adds a keyframe onto an existing persistent layer
  void AddToPersistentLayer(const std::string& layerName, FRAME_TYPE& keyFrame);
  
  // Removes a persitent layer after duration_ms has passed
  void RemovePersistentLayer(const std::string& layerName,
                             TimeStamp_t streamTime_ms,
                             TimeStamp_t duration_ms);
  
  // Returns true if there are any layers
  bool HaveLayersToSend() const;
  
  // Returns the number of layers
  size_t GetNumLayers() const { return _layers.size(); }
  
  // Returns true if there is a layer with the name 'layerName'
  bool HasLayer(const std::string& layerName) const;

  // Advance all tracks to the keyframe that should play in ms
  // NOTE: This function only moves tracks forwards
  void AdvanceTracks(const TimeStamp_t toTime_ms);
  
protected:
  
  const Util::RandomGenerator& GetRNG() const { return _rng; }
  
private:

  const Util::RandomGenerator& _rng;

  // Structure defining an individual layer
  struct Layer {
    Animations::Track<FRAME_TYPE> track;
    bool         sentOnce;
    bool         isPersistent;
  };

  std::map<std::string, Layer> _layers;
  
  // Ensures that expected playback parameters are met - this is not a long term
  // fix, it's a hack to try and catch animation streamer issues more quickly
  // while the system's in flux
  void ValidateTrack(const Animations::Track<FRAME_TYPE>& track);
};

}
}
}

#endif
