/**
 * File: trackLayerComponent.h
 *
 * Authors: Al Chaussee
 * Created: 06/28/2017
 *
 * Description: Component which manages creating various procedural animations by
 *              using the trackLayerManagers to generate keyframes and add them to
 *              track layers
 *              Currently there are only three trackLayerManagers face, backpack, and audio
 *
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_TrackLayerComponent_H__
#define __Anki_Cozmo_TrackLayerComponent_H__

#include "cannedAnimLib/cannedAnims/animation.h"
#include "coretech/common/shared/types.h"
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>


namespace Anki {
namespace Vector {
namespace Anim {
class AnimContext;
class AnimationStreamer;
class AudioLayerManager;
class BackpackLayerManager;
class FaceLayerManager;

class TrackLayerComponent
{
public:

  // Output struct that contains the final keyframes to
  // stream to the robot
  struct LayeredKeyFrames {
    bool haveAudioKeyFrame = false;
    RobotAudioKeyFrame audioKeyFrame;
    
    bool haveBackpackKeyFrame = false;
    BackpackLightsKeyFrame backpackKeyFrame;
    
    bool haveFaceKeyFrame = false;
    ProceduralFaceKeyFrame faceKeyFrame;
  };
  
  
  TrackLayerComponent(const Anim::AnimContext* context);
  ~TrackLayerComponent();
  
  void Init(AnimationStreamer& animStreamer);
  
  void Update();
  void AdvanceTracks(const TimeStamp_t toTime_ms);
  
  void EnableProceduralAudio(bool enabled);
  
  // Pulls the current keyframe from various tracks of the anim
  // and combines it with any track layers that may exist
  // Outputs layeredKeyframes struct which contains the final combined
  // keyframes from the anim and the various track layers
  void ApplyLayersToAnim(Animation* anim,
                         const TimeStamp_t timeSinceAnimStart_ms,
                         LayeredKeyFrames& layeredKeyFrames,
                         bool storeFace) const;
  
  // Keep Victor's face alive using the params specified
  // (call each tick while the face should be kept alive)
  void KeepFaceAlive(const TimeStamp_t timeSinceKeepAliveStart_ms);
  
  // Put keep face alive into "focused" mode, which reduces the jumpiness and eye darts
  // to make the robot appear more focused and looking straight ahead, but without "dead" eyes.
  void SetKeepFaceAliveFocus(bool enable) { _isKeepFaceAliveFocused = enable; }
  
  // Removes the live face after duration_ms has passed
  // Note: Will not cancel/remove a blink that is in progress
  void RemoveKeepFaceAlive(TimeStamp_t streamTime_ms, TimeStamp_t duration_ms);
  
  // Resets timers for keeping face alive.
  // Call this when KeepFaceAlive timing parameters have changed.
  void ResetKeepFaceAliveTimers();
  
  // Keep Victor's face alive, but the same, by posting empty new frames
  // so that noise keeps working
  void KeepFaceTheSame();
  
  void SetLastProceduralFaceAsBlank();

  // Make Victor squint (will continue to squint until removed)
  void AddSquint(const std::string& name,
                 f32 squintScaleX,
                 f32 squintScaleY,
                 f32 upperLidAngle,
                 TimeStamp_t streamTime_ms);

  // Removes specified squint after duration_ms has passed
  void RemoveSquint(const std::string& name, TimeStamp_t streamTime_ms, TimeStamp_t duration_ms = 0);
  
  // Either start an eye shift or update an already existing eye shift with new params
  // Note: Eye shift will continue until removed so if eye shift with the same name
  // was already added without being removed, this will just update it
  void AddOrUpdateEyeShift(const std::string& name,
                           f32 xPix,
                           f32 yPix,
                           TimeStamp_t duration_ms,
                           TimeStamp_t streamTime_ms,
                           f32 xMax = ProceduralFace::HEIGHT,
                           f32 yMax = ProceduralFace::WIDTH,
                           f32 lookUpMaxScale = 1.1f,
                           f32 lookDownMinScale = 0.85f,
                           f32 outerEyeScaleIncrease = 0.1f);
  
  // Removes the specified eye shift after duration_ms has passed
  void RemoveEyeShift(const std::string& name, TimeStamp_t streamTime_ms, TimeStamp_t duration_ms = 0);
  
  // Make Victor glitch
  void AddGlitch(f32 glitchDegree);
  
  // Returns true if any of the layerManagers have layers to send
  bool HaveLayersToSend() const;
  
  u32 GetMaxBlinkSpacingTimeForScreenProtection_ms() const;
  
private:
  
  // The KeepFaceAlive system consists of multiple Modifiers applied to the face by the
  // AnimationStreamer when no animation is controlling the face -- to keep the robot
  // looking "alive". For example, blinks are one Modifier, and eye darts are another. Multiple
  // Modifiers exist on separate layers and can be run at the same time. A priority flag
  // could be added in the future to handle more complicated cases.
  
  struct KeepAliveModifier {
    using ActivityPerformFunc = std::function<bool(const TimeStamp_t streamTime_ms)>;
    using ActivityGetNextPerformanceTimeFunc = std::function<s32()>;
    std::string name;
    ActivityPerformFunc performFunc = nullptr;
    ActivityGetNextPerformanceTimeFunc getNextPerformanceTimeFunc = nullptr;
    bool hasFaceLayers = false; // If false, we need to use idle face layers
    s32 nextPerformanceTime_ms = 0;
    KeepAliveModifier(const std::string& name,
                      ActivityPerformFunc performFunc,
                      ActivityGetNextPerformanceTimeFunc getNextPerformanceTimeFunc,
                      bool hasFaceLayers = false)
    : name(name)
    , performFunc(performFunc)
    , getNextPerformanceTimeFunc(getNextPerformanceTimeFunc)
    , hasFaceLayers(hasFaceLayers) {}
    
    void UpdateNextPerformanceTime() { nextPerformanceTime_ms = getNextPerformanceTimeFunc(); }
  };


  std::unique_ptr<AudioLayerManager>            _audioLayerManager;
  std::unique_ptr<BackpackLayerManager>         _backpackLayerManager;
  std::unique_ptr<FaceLayerManager>             _faceLayerManager;
  std::unique_ptr<ProceduralFace> _lastProceduralFace;
  std::vector<KeepAliveModifier>                _keepAliveModifiers;

  // Audio letancy offset tracking vars
  mutable bool _validAudioKeyframeIt = false;
  mutable std::list<RobotAudioKeyFrame>::iterator _audioKeyframeIt;
  
  // Setup and add keep face alive activites to _keepAliveActivities vector
  void SetupKeepFaceAliveActivities();
  bool _isKeepFaceAliveFocused = false;
  
  // Handle track layer
  void ApplyAudioLayersToAnim(Animation* anim,
                              const TimeStamp_t timeSinceAnimStart_ms,
                              LayeredKeyFrames& layeredKeyFrames) const;
  
  void ApplyBackpackLayersToAnim(Animation* anim,
                                 const TimeStamp_t timeSinceAnimStart_ms,
                                 LayeredKeyFrames& layeredKeyFrames) const;
  
  void ApplyFaceLayersToAnim(Animation* anim,
                             const TimeStamp_t timeSinceAnimStart_ms,
                             LayeredKeyFrames& layeredKeyFrames,
                             bool storeFace) const;

};
  
}
}
}

#endif
