/**
 * File: faceLayerManager.cpp
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

#include "cozmoAnim/animation/trackLayerManagers/faceLayerManager.h"

#include "cannedAnimLib/proceduralFace/proceduralFaceDrawer.h"
#include "cannedAnimLib/proceduralFace/scanlineDistorter.h"
#include "anki/cozmo/shared/cozmoConfig.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"

#define DEBUG_FACE_LAYERING 0

#define CONSOLE_GROUP_NAME "Face.KeepAlive"

namespace Anki {
namespace Vector {
namespace Anim {

namespace {

// Eye dart params

#if REMOTE_CONSOLE_ENABLED
// Only used for setting console var ranges below, so not used in shipping builds (thus ANKI_DEV_CHEATS)
constexpr int kMaxDartDist = std::max(FACE_DISPLAY_WIDTH,FACE_DISPLAY_HEIGHT)/2;
#endif

// Global keep-alive eye dart params (spacing, distance, scaling)
CONSOLE_VAR_RANGED(s32, kKeepAliveEyeDart_SpacingMinTime_ms,             CONSOLE_GROUP_NAME, 1000,  0, 10000);
CONSOLE_VAR_RANGED(s32, kKeepAliveEyeDart_SpacingMaxTime_ms,             CONSOLE_GROUP_NAME, 2250, 0, 10000);
CONSOLE_VAR_RANGED(s32, kKeepAliveEyeDart_MaxDistFromCenter_pix,         CONSOLE_GROUP_NAME, 15, 0, kMaxDartDist);
CONSOLE_VAR_RANGED(s32, kKeepAliveEyeDart_MaxDistFromCenterFocused_pix,  CONSOLE_GROUP_NAME, 1, 0, kMaxDartDist);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_UpMaxScale,                    CONSOLE_GROUP_NAME, 1.05f, 1.f,  1.2f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_DownMinScale,                  CONSOLE_GROUP_NAME, 0.9f,  0.5f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_OuterEyeScaleIncrease,         CONSOLE_GROUP_NAME, 0.03f, 0.f, 0.2f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_ShiftLagFraction,              CONSOLE_GROUP_NAME, 0.4f,  0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_HotSpotPositionMultiplier,     CONSOLE_GROUP_NAME, 1.5f,  0.5f, 10.f);
  
// Medium distance eye dart params (when dart's length is larger than threshold)
// These darts have a single interpolation frame (with associated dart distance and squash fractions)
constexpr int kMediumDartDefaultThresh_pix = 5;
CONSOLE_VAR_RANGED(int, kKeepAliveEyeDart_MediumDistanceThresh_pix,      CONSOLE_GROUP_NAME, kMediumDartDefaultThresh_pix, 0, kMaxDartDist);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_MediumShiftFraction,           CONSOLE_GROUP_NAME, 0.2f, 0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_MediumSquashFraction,          CONSOLE_GROUP_NAME, 0.85f, 0.5f, 1.f);

// Long distance eye dart params (when dart's length is larger than threshold)
// These darts have two interpolation frames (with associated dart distance and squash fractions)
constexpr int kLongDartDefaultThresh_pix = 10;
CONSOLE_VAR_RANGED(int, kKeepAliveEyeDart_LongDistanceThresh_pix,        CONSOLE_GROUP_NAME, kLongDartDefaultThresh_pix, 0, kMaxDartDist);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_LongShiftFraction1,            CONSOLE_GROUP_NAME, 0.2f, 0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_LongShiftFraction2,            CONSOLE_GROUP_NAME, 0.4f, 0.f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_LongSquashFraction1,           CONSOLE_GROUP_NAME, 0.7f, 0.5f, 1.f);
CONSOLE_VAR_RANGED(f32, kKeepAliveEyeDart_LongSquashFraction2,           CONSOLE_GROUP_NAME, 0.85f, 0.5f, 1.f);

static_assert(kMediumDartDefaultThresh_pix < kLongDartDefaultThresh_pix,
              "Medium dart threshold should be less than long dart threshold");
  
// Blink params:
CONSOLE_VAR(f32, kMaxBlinkSpacingTimeForScreenProtection_ms, CONSOLE_GROUP_NAME, 30000);
CONSOLE_VAR_RANGED(s32, kKeepAliveBlink_SpacingMinTime_ms, CONSOLE_GROUP_NAME, 3000, 0, 30000);
CONSOLE_VAR_RANGED(s32, kKeepAliveBlink_SpacingMaxTime_ms, CONSOLE_GROUP_NAME, 10000, 0, 30000);
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FaceLayerManager::FaceLayerManager(const Util::RandomGenerator& rng)
: ITrackLayerManager<ProceduralFaceKeyFrame>(rng)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FaceLayerManager::GetFaceHelper(const Animations::Track<ProceduralFaceKeyFrame>& track,
                                     TimeStamp_t timeSinceAnimStart_ms,
                                     ProceduralFaceKeyFrame& procFace,
                                     bool shouldReplace) const
{
  bool paramsSet = false;
  
  if(track.HasFramesLeft()) {
    ProceduralFaceKeyFrame& currentKeyFrame = track.GetCurrentKeyFrame();
    if(currentKeyFrame.IsTimeToPlay(timeSinceAnimStart_ms))
    {
      ProceduralFace interpolatedFace;
      
      const ProceduralFaceKeyFrame* nextFrame = track.GetNextKeyFrame();
      if (nextFrame != nullptr) {
        if (nextFrame->IsTimeToPlay(timeSinceAnimStart_ms)) {
          // If it's time to play the next frame and the current frame at the same time, something's wrong!
          PRINT_NAMED_WARNING("FaceLayerManager.GetFaceHelper.FramesTooClose",
                              "currentFrameTriggerTime: %d ms, nextFrameTriggerTime: %d, StreamTime: %d",
                              currentKeyFrame.GetTriggerTime_ms(), nextFrame->GetTriggerTime_ms(), timeSinceAnimStart_ms);
        } else {
          /*
           // If we're within one sample period following the currFrame, just play the current frame
           if (currStreamTime - currentKeyFrame.GetTriggerTime_ms() < ANIM_TIME_STEP_MS) {
           interpolatedParams = currentKeyFrame.GetFace().GetParams();
           paramsSet = true;
           }
           // We're on the way to the next frame, but not too close to it: interpolate.
           else if (nextFrame->GetTriggerTime_ms() - currStreamTime >= ANIM_TIME_STEP_MS) {
           */
          interpolatedFace = currentKeyFrame.GetInterpolatedFace(*nextFrame, timeSinceAnimStart_ms);
          paramsSet = true;
          //}
        }
      } else {
        // There's no next frame to interpolate towards: just send this keyframe
        interpolatedFace = currentKeyFrame.GetFace();
        paramsSet = true;
      }
      
      if(paramsSet) {
        if(DEBUG_FACE_LAYERING) {
          PRINT_NAMED_DEBUG("AnimationStreamer.GetFaceHelper.EyeShift",
                            "Applying eye shift from face layer of (%.1f,%.1f)",
                            interpolatedFace.GetFacePosition().x(),
                            interpolatedFace.GetFacePosition().y());
        }
        
        if (shouldReplace)
        {
          procFace = interpolatedFace;
        }
        else
        {
          const_cast<ProceduralFace&>(procFace.GetFace()).Combine(interpolatedFace);
        }
      }
    } // if(nextFrame != nullptr
  } // if(track.HasFramesLeft())
  
  return paramsSet;
} // GetFaceHelper()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FaceLayerManager::GenerateEyeShift(f32 xPix, f32 yPix,
                                         f32 xMax, f32 yMax,
                                         f32 lookUpMaxScale,
                                         f32 lookDownMinScale,
                                         f32 outerEyeScaleIncrease,
                                         TimeStamp_t duration_ms,
                                         ProceduralFaceKeyFrame& frame) const
{
  ProceduralFace procFace;
  ProceduralFace::Value xMin=0, yMin=0;
  procFace.GetEyeBoundingBox(xMin, xMax, yMin, yMax);
  procFace.LookAt(xPix, yPix,
                  std::max(xMin, ProceduralFace::WIDTH-xMax),
                  std::max(yMin, ProceduralFace::HEIGHT-yMax),
                  lookUpMaxScale, lookDownMinScale, outerEyeScaleIncrease);
  
  ProceduralFaceKeyFrame keyframe(procFace, duration_ms);
  frame = std::move(keyframe);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int GetNumEyeDartInterpFrames(const Point2f& dartVector)
{
  const f32 dartDistanceSq = dartVector.LengthSq();
  
  if(ANKI_DEV_CHEATS && (kKeepAliveEyeDart_MediumDistanceThresh_pix >= kKeepAliveEyeDart_LongDistanceThresh_pix))
  {
    LOG_WARNING("FaceLayerManager.GetNumEyeDartInterpFrames.BadThresholds",
                "Medium threshold (%d) >= Long threshold (%d), using 0 interp frames",
                kKeepAliveEyeDart_MediumDistanceThresh_pix, kKeepAliveEyeDart_LongDistanceThresh_pix);
    return 0;
  }
  
  int numInterpFrames = 0;
  if(dartDistanceSq > (kKeepAliveEyeDart_LongDistanceThresh_pix*kKeepAliveEyeDart_LongDistanceThresh_pix))
  {
    numInterpFrames = 2;
  }
  else if(dartDistanceSq > (kKeepAliveEyeDart_MediumDistanceThresh_pix*kKeepAliveEyeDart_MediumDistanceThresh_pix))
  {
    numInterpFrames = 1;
  }
  
  return numInterpFrames;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
struct DartParam
{
  Point2f position;
  Point2f hotspotPosition;
  f32     verticalSquash;  // Vertical squash adds a sort of mini-blink to the dart (per animators), so no horizontal
};

static inline DartParam InterpDartParam(const Point2f& lastDartPosition,
                                        const Vec2f& dartVector,
                                        const Point2f& dartFinalHotspot,
                                        f32 xshiftFraction, const f32 squashFraction)
{
  // Compute interpolated dart position. To produce less linear motion, we move the X and Y coordinates
  // by differing amounts, depending on direction, and not in uniform steps to the final position.
  // This gives the feel of both an ease-in and an arc shape to the path.
  // - If eyes are darting downward, X shift lags Y shift
  // - If eyes are darting upward, Y shift lags X shift
  f32 yshiftFraction = xshiftFraction;
  const bool isLookingDown = Util::IsFltGTZero(dartVector.y());
  if(isLookingDown)
  {
    xshiftFraction *= kKeepAliveEyeDart_ShiftLagFraction;
  }
  else
  {
    yshiftFraction *= kKeepAliveEyeDart_ShiftLagFraction;
  }

  Point2f interpPosition(lastDartPosition);
  const Point2f interpDart(dartVector.x() * xshiftFraction,
                           dartVector.y() * yshiftFraction);
  interpPosition += interpDart;
  
  // Hot spot moves in the same direction as the dart vector but is simply relative amounts (not absolute positions)
  const Point2f interpHotspot(dartFinalHotspot.x() * xshiftFraction,
                              dartFinalHotspot.y() * yshiftFraction);
  
  DartParam dartParam{interpPosition, interpHotspot, squashFraction};
  return dartParam;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FaceLayerManager::GenerateKeepAliveEyeDart(const std::string& layerName,
                                                bool hasDartLayer,
                                                const f32 maxDist_pix,
                                                const TimeStamp_t timeSinceKeepAliveStart_ms)
{
  if(timeSinceKeepAliveStart_ms < ANIM_TIME_STEP_MS)
  {
    // Reset last position if we just started keep alive again
    _lastDartPosition = 0.f;
  }
  
  DEV_ASSERT(Util::IsFltGTZero(maxDist_pix), "FaceLayerManager.GenerateKeepAliveEyDart.ZeroDistance");
  
  const Point2f dartFinalPosition(GetRNG().RandIntInRange(-maxDist_pix, maxDist_pix),
                                  GetRNG().RandIntInRange(-maxDist_pix, maxDist_pix));
  
  // bucket the number of frames depending on the dart distance
  const Vec2f dartVector = dartFinalPosition - _lastDartPosition;
  const int numInterpFrames = GetNumEyeDartInterpFrames(dartVector);
  
  const f32 hotSpotScale = kKeepAliveEyeDart_HotSpotPositionMultiplier / maxDist_pix;
  const Point2f dartFinalHotspot(Util::Clamp(dartFinalPosition.x()*hotSpotScale, -1.f, 1.f),
                                 Util::Clamp(dartFinalPosition.y()*hotSpotScale, -1.f, 1.f));
  
  std::vector<DartParam> dartParams;
  ProceduralFace dartFace;
  
  switch(numInterpFrames)
  {
    case 0:
      // No interpolation: just dart straight to final position
      break;
      
    case 1:
    {
      // 1 frame to interpolate: dart part of the way and then to final
      dartParams.emplace_back(InterpDartParam(_lastDartPosition, dartVector, dartFinalHotspot,
                                              kKeepAliveEyeDart_MediumShiftFraction,
                                              kKeepAliveEyeDart_MediumSquashFraction));
      break;
    }
      
    case 2:
    {
      // 2 frames of interpolation: dart with two intermediate positions
      dartParams.emplace_back(InterpDartParam(_lastDartPosition, dartVector, dartFinalHotspot,
                                              kKeepAliveEyeDart_LongShiftFraction1,
                                              kKeepAliveEyeDart_LongSquashFraction1));
      
      dartParams.emplace_back(InterpDartParam(dartParams.back().position, dartVector, dartFinalHotspot,
                                              kKeepAliveEyeDart_LongShiftFraction2,
                                              kKeepAliveEyeDart_LongSquashFraction2));
      break;
    }
      
    default:
      DEV_ASSERT_MSG(false, "FaceLayerManager.GenerateKeepAliveEyeDart.InvalidNumInterpFrames",
                     "%d not in {0,1,2}", numInterpFrames);
  }
  
  // Always finish with final position at full scale
  dartParams.emplace_back(DartParam{dartFinalPosition, dartFinalHotspot, 1.f});
  
  for(const auto& dartParam : dartParams)
  {
    dartFace.LookAt(dartParam.position.x(), dartParam.position.y(),
                    kKeepAliveEyeDart_LongDistanceThresh_pix, kKeepAliveEyeDart_LongDistanceThresh_pix,
                    kKeepAliveEyeDart_UpMaxScale,
                    kKeepAliveEyeDart_DownMinScale,
                    kKeepAliveEyeDart_OuterEyeScaleIncrease);
    
    dartFace.SetFaceScale({1.f, dartParam.verticalSquash});
    dartFace.SetParameterBothEyes(ProceduralFace::Parameter::HotSpotCenterX, dartParam.hotspotPosition.x());
    dartFace.SetParameterBothEyes(ProceduralFace::Parameter::HotSpotCenterY, dartParam.hotspotPosition.y());
    
    ProceduralFaceKeyFrame frame(dartFace);
    
    if(!hasDartLayer)
    {
      // No existing persistent dart layer, so create one to use now
      FaceTrack faceTrack;
      frame.SetTriggerTime_ms(timeSinceKeepAliveStart_ms);
      faceTrack.AddKeyFrameToBack(frame);
      AddPersistentLayer(layerName, faceTrack);
      hasDartLayer = true;
    }
    else {
      // This automagically handles the trigger time
      AddToPersistentLayer(layerName, frame);
    }
  }
  
  // Store where we ended up for next dart
  _lastDartPosition = dartFinalPosition;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FaceLayerManager::GenerateBlink(Animations::Track<ProceduralFaceKeyFrame>& track,
                                     const TimeStamp_t timeSinceKeepAliveStart_ms,
                                     BlinkEventList& out_eventList) const
{
  ProceduralFace blinkFace;
  TimeStamp_t totalOffset = timeSinceKeepAliveStart_ms;
  BlinkState blinkState;
  TimeStamp_t timeInc;
  bool moreBlinkFrames = false;
  out_eventList.clear();
  do {
    moreBlinkFrames = ProceduralFaceDrawer::GetNextBlinkFrame(blinkFace, blinkState, timeInc);
    track.AddKeyFrameToBack(ProceduralFaceKeyFrame(blinkFace, totalOffset, timeInc));
    out_eventList.emplace_back(totalOffset, blinkState);
    totalOffset += timeInc;
  } while(moreBlinkFrames);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result FaceLayerManager::AddBlinkToFaceTrack(const std::string& layerName,
                                             const TimeStamp_t timeSinceKeepAliveStart_ms,
                                             BlinkEventList& out_eventList)
{
  if (HasLayer(layerName)) {
    out_eventList.clear();
    return RESULT_FAIL;
  }
  Animations::Track<ProceduralFaceKeyFrame> faceTrack;
  GenerateBlink(faceTrack, timeSinceKeepAliveStart_ms, out_eventList);
  Result result = AddLayer(layerName, faceTrack);
  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 FaceLayerManager::GetNextBlinkTime_ms() const
{
  s32 blinkSpaceMin_ms = kKeepAliveBlink_SpacingMinTime_ms;
  s32 blinkSpaceMax_ms = kKeepAliveBlink_SpacingMaxTime_ms;
  if(blinkSpaceMax_ms <= blinkSpaceMin_ms)
  {
    PRINT_NAMED_WARNING("AnimationStreamer.KeepFaceAlive.BadBlinkSpacingParams",
                        "Max (%d) must be greater than min (%d)",
                        blinkSpaceMax_ms, blinkSpaceMin_ms);
    blinkSpaceMin_ms = kMaxBlinkSpacingTimeForScreenProtection_ms * .25f;
    blinkSpaceMax_ms = kMaxBlinkSpacingTimeForScreenProtection_ms;
  }
  return GetRNG().RandIntInRange(blinkSpaceMin_ms, blinkSpaceMax_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result FaceLayerManager::AddEyeDartToFaceTrack(const std::string& layerName,
                                               const bool isFocused,
                                               const TimeStamp_t timeSinceKeepAliveStart_ms,
                                               TimeStamp_t& out_interpolationTime_ms)
{
  const f32 maxDist = (isFocused ? kKeepAliveEyeDart_MaxDistFromCenterFocused_pix : kKeepAliveEyeDart_MaxDistFromCenter_pix);
  out_interpolationTime_ms = 0;
  if (Util::IsFltGTZero(maxDist) )
  {
    const size_t numLayers = GetNumLayers();
    const bool hasDartLayer = HasLayer(layerName);
    const bool noOtherFaceLayers = (numLayers == 0 ||
                                    (numLayers == 1 && hasDartLayer));

    // If there's no other face layer active right now, do the dart. Otherwise,
    // skip it
    if(noOtherFaceLayers)
    {
      GenerateKeepAliveEyeDart(layerName, hasDartLayer, maxDist, timeSinceKeepAliveStart_ms);
    }
  }

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 FaceLayerManager::GetNextEyeDartTime_ms() const
{
  return GetRNG().RandIntInRange(kKeepAliveEyeDart_SpacingMinTime_ms, kKeepAliveEyeDart_SpacingMaxTime_ms);
}
 
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FaceLayerManager::AddKeepFaceAliveTrack(const std::string& layerName)
{
  ProceduralFaceKeyFrame frame;
  FaceTrack faceTrack;
  faceTrack.AddKeyFrameToBack(frame);
  AddLayer(layerName, faceTrack);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
u32 FaceLayerManager::GenerateFaceDistortion(float distortionDegree,
                                               Animations::Track<ProceduralFaceKeyFrame>& track) const
{
  u32 numFrames = 0;
  ProceduralFace repairFace;
  
  TimeStamp_t totalOffset = 0;
  bool moreDistortionFrames = false;
  do {
    TimeStamp_t timeInc;
    moreDistortionFrames = ScanlineDistorter::GetNextDistortionFrame(distortionDegree, repairFace, timeInc);
    totalOffset += timeInc;
    track.AddKeyFrameToBack(ProceduralFaceKeyFrame(repairFace, totalOffset));
    ++numFrames;
  } while(moreDistortionFrames);
  return numFrames;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FaceLayerManager::GenerateSquint(f32 squintScaleX,
                                      f32 squintScaleY,
                                      f32 upperLidAngle,
                                      Animations::Track<ProceduralFaceKeyFrame>& track,
                                      const TimeStamp_t timeSinceKeepAliveStart_ms) const
{
  ProceduralFace squintFace;
  const f32 DockSquintScaleY = 0.35f;
  const f32 DockSquintScaleX = 1.05f;
  const TimeStamp_t interpolationTime_ms = 250;
  squintFace.SetParameterBothEyes(ProceduralFace::Parameter::EyeScaleY, DockSquintScaleY);
  squintFace.SetParameterBothEyes(ProceduralFace::Parameter::EyeScaleX, DockSquintScaleX);
  squintFace.SetParameterBothEyes(ProceduralFace::Parameter::UpperLidAngle, -10.0f);
  // need start at t=0 (a.k.a. timeSinceKeepAliveStart_ms) to get interpolation
  track.AddKeyFrameToBack(ProceduralFaceKeyFrame(timeSinceKeepAliveStart_ms, interpolationTime_ms));
  track.AddKeyFrameToBack(ProceduralFaceKeyFrame(squintFace, (timeSinceKeepAliveStart_ms + interpolationTime_ms)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
u32 FaceLayerManager::GetMaxBlinkSpacingTimeForScreenProtection_ms() const
{
  return kMaxBlinkSpacingTimeForScreenProtection_ms;
}

} // namespace Anim
} // namespace Vector
} // namespace Anki

