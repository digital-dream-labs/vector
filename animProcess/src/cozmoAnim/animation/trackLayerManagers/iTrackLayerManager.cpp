/**
 * File: ITrackLayerManager.cpp
 *
 * Authors: Al Chaussee
 * Created: 06/28/2017
 *
 * Description:
 *
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cozmoAnim/animation/trackLayerManagers/iTrackLayerManager.h"

#define LOG_CHANNEL    "TrackLayerManager"

#define DEBUG_FACE_LAYERING 0

namespace Anki {
namespace Vector {
namespace Anim {

template<class FRAME_TYPE>
ITrackLayerManager<FRAME_TYPE>::ITrackLayerManager(const Util::RandomGenerator& rng)
: _rng(rng)
{
  
}

template<class FRAME_TYPE>
bool ITrackLayerManager<FRAME_TYPE>::ApplyLayersToFrame(FRAME_TYPE& frame,
                                                        const TimeStamp_t timeSinceAnimStart_ms,
                                                        ApplyLayerFunc applyLayerFunc) const
{
  if (DEBUG_FACE_LAYERING)
  {
    if (!_layers.empty())
    {
      LOG_DEBUG("AnimationStreamer.UpdateFace.ApplyingFaceLayers",
                        "NumLayers=%lu", (unsigned long)_layers.size());
    }
  }

  bool frameUpdated = false;
    
  for (auto layerIter = _layers.begin(); layerIter != _layers.end(); ++layerIter)
  {
    auto& layer = layerIter->second;
    
    // Apply the layer's track with frame
    frameUpdated |= applyLayerFunc(layer.track, timeSinceAnimStart_ms, frame);
  }
  
  return frameUpdated;
}

template<class FRAME_TYPE>
Result ITrackLayerManager<FRAME_TYPE>::AddLayer(const std::string& name,
                                                const Animations::Track<FRAME_TYPE>& track)
{
  if (_layers.find(name) != _layers.end()) {
    PRINT_NAMED_WARNING("TrackLayerManager.AddLayer.LayerAlreadyExists", "");
  }
  
  Result lastResult = RESULT_OK;
  
  Layer newLayer;
  newLayer.track = track; // COPY the track in
  newLayer.track.MoveToStart();
  newLayer.isPersistent = false;
  newLayer.sentOnce = false;
  
  _layers[name] = std::move(newLayer);
  
  return lastResult;
}

template<class FRAME_TYPE>
void ITrackLayerManager<FRAME_TYPE>::AddPersistentLayer(const std::string& name,
                                                        const Animations::Track<FRAME_TYPE>& track)
{
  if (_layers.find(name) != _layers.end()) {
    PRINT_NAMED_WARNING("TrackLayerManager.AddPersistentLayer.LayerAlreadyExists", "");
  }
  
  if(ANKI_DEV_CHEATS){
    ValidateTrack(track);
  }
  
  Layer newLayer;
  newLayer.track = track;
  newLayer.track.MoveToStart();
  newLayer.isPersistent = true;
  newLayer.sentOnce = false;
  
  _layers[name] = std::move(newLayer);
}

template<class FRAME_TYPE>
void ITrackLayerManager<FRAME_TYPE>::AddToPersistentLayer(const std::string& layerName, FRAME_TYPE& keyframe)
{
  auto layerIter = _layers.find(layerName);
  if (layerIter != _layers.end())
  {
    auto& track = layerIter->second.track;
    assert(nullptr != track.GetLastKeyFrame());
    auto* lastKeyframe = track.GetLastKeyFrame();
    
    // Make keyframe trigger one sample length (plus any internal delay) past
    // the last keyframe's trigger time
    keyframe.SetTriggerTime_ms(lastKeyframe->GetTimestampActionComplete_ms());
    track.AddKeyFrameToBack(keyframe);
    layerIter->second.sentOnce = false;
    
    if(ANKI_DEV_CHEATS){
      ValidateTrack(track);
    }
  }
}

template<class FRAME_TYPE>
void ITrackLayerManager<FRAME_TYPE>::RemovePersistentLayer(const std::string& layerName,
                                                           TimeStamp_t streamTime_ms,
                                                           TimeStamp_t duration_ms)
{
  auto layerIter = _layers.find(layerName);
  if (layerIter != _layers.end())
  {
    auto& layerName = layerIter->first;
    LOG_INFO("ITrackLayerManager.RemovePersistentLayer",
             "%s, (Layers remaining=%lu)",
             layerName.c_str(), (unsigned long)_layers.size()-1);
    
    
    // Add a layer that takes us back from where this persistent frame leaves
    // off to no adjustment at all.
    Animations::Track<FRAME_TYPE> track;
    if (duration_ms > 0)
    {
      FRAME_TYPE firstFrame(layerIter->second.track.GetCurrentKeyFrame());
      firstFrame.SetTriggerTime_ms(streamTime_ms);
      track.AddKeyFrameToBack(std::move(firstFrame));
    }
    FRAME_TYPE lastFrame;
    lastFrame.SetTriggerTime_ms(streamTime_ms + duration_ms);
    track.AddKeyFrameToBack(std::move(lastFrame));
    
    AddLayer("Remove" + layerName, track);
    
    _layers.erase(layerIter);
  }
}

template<class FRAME_TYPE>
bool ITrackLayerManager<FRAME_TYPE>::HaveLayersToSend() const
{
  if (_layers.empty())
  {
    return false;
  }
  else
  {
    // There are layers, but we want to ignore any that are persistent that
    // have already been sent once
    for (const auto & layer : _layers)
    {
      if (!layer.second.isPersistent || !layer.second.sentOnce)
      {
        // There's at least one non-persistent layer, or a persistent layer
        // that has not been sent in its entirety at least once: return that there
        // are still layers to send
        return true;
      }
    }
    // All layers are persistent ones that have been sent, so no need to keep sending them
    // by themselves. They only need to be re-applied while there's something
    // else being sent
    return false;
  }
}

template<class FRAME_TYPE>
bool ITrackLayerManager<FRAME_TYPE>::HasLayer(const std::string& layerName) const
{
  return _layers.find(layerName) != _layers.end();
}

template<class FRAME_TYPE>
void ITrackLayerManager<FRAME_TYPE>::AdvanceTracks(const TimeStamp_t toTime_ms)
{
  std::list<std::string> layersToErase;

  for(auto& pair: _layers){
    auto& layerName = pair.first;
    auto& layer = pair.second;
    layer.track.AdvanceTrack(toTime_ms);
    if(!layer.isPersistent){
      layer.track.ClearUpToCurrent();
    }

    if (!layer.track.HasFramesLeft()){
      // This layer is done...
      if(layer.isPersistent)
      {
        if (layer.track.IsEmpty())
        {
          LOG_WARNING("AnimationStreamer.UpdateFace.EmptyPersistentLayer",
                      "Persistent face layer is empty - perhaps live frames were "
                      "used? (layer=%s)", layerName.c_str());
          layer.isPersistent = false;
        }
        else
        {
          //...but is marked persistent, so keep applying last frame
          layer.track.MoveToPrevKeyFrame(); // so we're not at end() anymore
          layer.track.GetCurrentKeyFrame().SetTriggerTime_ms(toTime_ms);
          
          if (DEBUG_FACE_LAYERING)
          {
            LOG_DEBUG("AnimationStreamer.UpdateFace.HoldingLayer",
                      "Holding last frame of face layer %s",
                      layerName.c_str());
          }
          
          layer.sentOnce = true; // mark that it has been sent at least once
          
          // We no longer need anything but the last frame (which should now be
          // "current")
          layer.track.ClearUpToCurrent();
        }
      }
      else
      {
        //...and is not persistent, so delete it
        if (DEBUG_FACE_LAYERING)
        {
          LOG_DEBUG("AnimationStreamer.UpdateFace.RemovingFaceLayer",
                    "%s (Layers remaining=%lu)",
                    layerName.c_str(), (unsigned long)_layers.size()-1);
        }
        
        layersToErase.push_back(layerName);
      }
    }
  }
  
  // Actually erase elements from the map
  for (const auto& layerName : layersToErase)
  {
    _layers.erase(layerName);
  }

}

  
template<class FRAME_TYPE>
void ITrackLayerManager<FRAME_TYPE>::ValidateTrack(const Animations::Track<FRAME_TYPE>& track)
{
  if(ANKI_DEV_CHEATS){
    // Ensure tracks don't overlap
    for(const auto& keyframe: track.GetCopyOfKeyframes()){
      ANKI_VERIFY(keyframe.GetTriggerTime_ms() != keyframe.GetTimestampActionComplete_ms(),
                  "ITrackLayerManager.ValidateTrack.KeyframeWithNoLength",
                  "All keyframes must have a duration");
    }
  }
}
  
template<>
void ITrackLayerManager<ProceduralFaceKeyFrame>::ValidateTrack(const Animations::Track<ProceduralFaceKeyFrame>& track)
{
  if(ANKI_DEV_CHEATS){
    // Ensure tracks don't overlap
    auto keyframes = track.GetCopyOfKeyframes();
    for(auto keyframeIter = keyframes.begin(); keyframeIter != keyframes.end(); keyframeIter++){
      ANKI_VERIFY(keyframeIter->GetTriggerTime_ms() != keyframeIter->GetTimestampActionComplete_ms(),
                  "ITrackLayerManager.ValidateTrack.KeyframeWithNoLength",
                  "All keyframes must have a duration");
      auto nextIter = keyframeIter;
      nextIter++;
      if(nextIter != keyframes.end()){
        ANKI_VERIFY(keyframeIter->GetTimestampActionComplete_ms() == nextIter->GetTriggerTime_ms(),
                    "ITrackLayerManager.ValidateTrack.ProceduralKeyframeTimeMismatch",
                    "Previous keyframe ends at %u, but next frame does not trigger until %u, interpolation will break",
                    keyframeIter->GetTimestampActionComplete_ms(), nextIter->GetTriggerTime_ms());
      }
    }
  }
}

  



// Explicit instantiation of allowed templated classes
template class ITrackLayerManager<RobotAudioKeyFrame>;        // AudioLayerManager
template class ITrackLayerManager<BackpackLightsKeyFrame>;    // BackpackLayerManager
template class ITrackLayerManager<ProceduralFaceKeyFrame>;    // FaceLayerManager

}
}
}
