/**
 * File: faceLayerManager.h
 *
 * Authors: Andrew Stein
 * Created: 05/16/2017
 *
 * Description: Specific track layer manager for ProceduralFaceKeyFrames
 *              Handles procedural face layering, which includes things like KeepAlive,
 *              look-ats while turning, blinks, and repair glitches.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_FaceLayerManager_H__
#define __Anki_Cozmo_FaceLayerManager_H__

#include "cozmoAnim/animation/trackLayerManagers/iTrackLayerManager.h"
#include "cannedAnimLib/proceduralFace/proceduralFaceModifierTypes.h"
#include <map>
#include <string>


namespace Anki {
namespace Vector {

// Forward declaration
class ProceduralFace;

namespace Anim {
class FaceLayerManager : public ITrackLayerManager<ProceduralFaceKeyFrame>
{
public:
  
  using FaceTrack = Animations::Track<ProceduralFaceKeyFrame>;
  
  FaceLayerManager(const Util::RandomGenerator& rng);
  
  // Helper to fold the next procedural face from the given track (if one is
  // ready to play) into the passed-in procedural face params.
  bool GetFaceHelper(const Animations::Track<ProceduralFaceKeyFrame>& track,
                     TimeStamp_t timeSinceAnimStart_ms,
                     ProceduralFaceKeyFrame& procFace,
                     bool shouldReplace) const;
  
  // Generates a single keyframe with shifted eyes according to the arguments
  // Eye shifts keyframes are generated with a relative start time - they should then
  // be updated to reflect their true playback time within a track
  void GenerateEyeShift(f32 xPix, f32 yPix,
                        f32 xMax, f32 yMax,
                        f32 lookUpMaxScale,
                        f32 lookDownMinScale,
                        f32 outerEyeScaleIncrease,
                        TimeStamp_t duration_ms,
                        ProceduralFaceKeyFrame& frame) const;
  
  // Generates short, persistent eye dart track
  void GenerateKeepAliveEyeDart(const std::string& layerName, bool hasDartLayer,
                                const f32 maxDist_pix,
                                const TimeStamp_t timeSinceKeepAliveStart_ms);
  
  // Generates a track of all keyframes necessary to make the eyes blink
  void GenerateBlink(Animations::Track<ProceduralFaceKeyFrame>& track,
                     const TimeStamp_t timeSinceKeepAliveStart_ms,
                     BlinkEventList& out_eventList) const;
  
  // Generate eye blink sequence
  // Add BlinkStateEvnets to eventList for other layers to sync with
  // Return RESULT_FAIL if there is already a blink layer
  Result AddBlinkToFaceTrack(const std::string& layerName,
                             const TimeStamp_t timeSinceKeepAliveStart_ms,
                             BlinkEventList& out_eventList);
  
  // Get the next eye blink time
  s32 GetNextBlinkTime_ms() const;
  
  // Generate eye dart
  // Set eye dart interpolationTime_ms for other layers to sync with
  // When isFocused=true, eye darts will be much smaller in order to keep the eyes moving but still looking forward
  Result AddEyeDartToFaceTrack(const std::string& layerName,
                               const bool isFocused,
                               const TimeStamp_t timeSinceKeepAliveStart_ms,
                               TimeStamp_t& out_interpolationTime_ms);
  
  // Get the next eye dart time
  s32 GetNextEyeDartTime_ms() const;
  
  // Add "alive" frames to Face Track
  void AddKeepFaceAliveTrack(const std::string& layerName);
  
  // Generates a track of all keyframes necessary to make the eyes squint
  void GenerateSquint(f32 squintScaleX,
                      f32 squintScaleY,
                      f32 upperLidAngle,
                      Animations::Track<ProceduralFaceKeyFrame>& track,
                      const TimeStamp_t timeSinceKeepAliveStart_ms) const;
  
  // Generates a track of all keyframes necessary to make the face have distortion
  // Returns how many keyframes were generated
  u32 GenerateFaceDistortion(float distortionDegree, Animations::Track<ProceduralFaceKeyFrame>& track) const;
  
  u32 GetMaxBlinkSpacingTimeForScreenProtection_ms() const;
  
private:
  
  Point2f _lastDartPosition;
  
};

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_FaceLayerManager_H__ */
