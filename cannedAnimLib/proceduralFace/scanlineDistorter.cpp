/**
 * File: scanlineDistorter.cpp
 *
 * Author: Andrew Stein
 * Created: 5/15/2017
 *
 * Description: Responsible for holding distortion parameters for a ProceduralFace
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cannedAnimLib/proceduralFace/scanlineDistorter.h"

#include "cannedAnimLib/baseTypes/keyframe.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"

#include "coretech/vision/engine/image.h"

#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"

#define CONSOLE_GROUP_NAME "Face.ScanlineDistortion"

namespace Anki {
namespace Vector {
  
// Fraction of (nominal) eye area to be off (note: does not consider "Width" parameter below)
CONSOLE_VAR_RANGED(f32, kProcFaceScanline_OffNoiseProb, CONSOLE_GROUP_NAME, 0.1f, 0.f, 1.f);

// Max width of each "off" noise bar
CONSOLE_VAR(s32, kProcFaceScanline_OffNoiseMaxWidth, CONSOLE_GROUP_NAME, 3);

// Max amount to randomly shift control-point distortion shifts left and right, per scanline
CONSOLE_VAR(s32, kProcFaceScanline_MaxShiftNoise, CONSOLE_GROUP_NAME, 3);
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Util::RandomGenerator& ScanlineDistorter::GetRNG()
{
  static const s32 kRandomSeed = 1;
  static Util::RandomGenerator rng(kRandomSeed);
  return rng;
}
 
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ScanlineDistorter::ScanlineDistorter(s32 maxAmount_pix, f32 noiseProb)
{
  // Choose a distortion shape
  _shape = (Shape)GetRNG().RandInt(Util::EnumToUnderlying(Shape::Count));
 
  // Choose a shape direction
  const s32 direction = (GetRNG().RandDbl() < 0.5 ? -1 : 1);
  
  _controlPoints.clear();
  switch(_shape)
  {
    case Shape::Skew:
    {
      // Two control points: top and bottom, moving opposite dirs
      _controlPoints.emplace_back(0.f, -direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      _controlPoints.emplace_back(1.f,  direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      break;
    }
      
    case Shape::Triangle:
    {
      // Three control points: top, bottom, and roughly in the middle (mid moves opposite dir as top/bottom)
      _controlPoints.emplace_back(0.0f, -direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      const f32 midPoint = GetRNG().RandDblInRange(0.35, 0.65);
      _controlPoints.emplace_back(midPoint, direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      _controlPoints.emplace_back(1.0f, -direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      break;
    }
      
    case Shape::S_Curve:
    {
      // Four control points moving in alternating directions
      _controlPoints.emplace_back(0.0f, -direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      const f32 upperMidPoint = GetRNG().RandDblInRange(0.15,0.35);
      _controlPoints.emplace_back(upperMidPoint, direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      const f32 lowerMidPoint = GetRNG().RandDblInRange(0.65,0.85);
      _controlPoints.emplace_back(lowerMidPoint, -direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      _controlPoints.emplace_back(1.0f, direction, GetRNG().RandIntInRange(1, maxAmount_pix));
      break;
    }
      
    case Shape::Count:
    {
      PRINT_NAMED_ERROR("ProceduralFace.InitScanlineDistortion.BadShape", "Count is not valid shape");
      return;
    }
  }
  
  if(Util::IsFltGTZero(noiseProb))
  {
    const f32 eyeArea = (f32)(ProceduralFace::NominalEyeHeight * ProceduralFace::NominalEyeWidth);
    const size_t N = (size_t)(noiseProb * eyeArea);
    
    _offNoisePoints.reserve(N);
    
    for(s32 i=0; i<N; ++i)
    {
      _offNoisePoints.emplace_back((f32)GetRNG().RandDblInRange(-0.5f, 0.5f),
                                   (f32)GetRNG().RandDblInRange(-0.5f, 0.5f));
    }
    
  }
  
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ScanlineDistorter::Update(s32 maxAmount_pix)
{
  // Shift the control points relative to the direction they are already headed (based on direction)
  // (Positive means keep moving in the same direction, Negative means the opposite direciton)
  for(auto & controlPt : _controlPoints)
  {
    const s32 direction = (maxAmount_pix < 0 ? -controlPt.direction  : controlPt.direction);
    const s32 shift = direction * GetRNG().RandIntInRange(1, std::abs(maxAmount_pix));
    controlPt.amount_pix += shift;
  }

}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 ScanlineDistorter::GetEyeDistortionAmount(f32 eyeFrac) const
{
  DEV_ASSERT(_controlPoints.size() > 1, "ScanlineDistorter.GetEyeScanlineDistortion.NotEnoughControlPoints");
  
  s32 distortionAmount_pix = 0;
  for(s32 iControlPt = 0; iControlPt < _controlPoints.size()-1; ++iControlPt)
  {
    // Is this eyeFrac between this control point and next?
    const auto& controlPt1 = _controlPoints[iControlPt];
    const auto& controlPt2 = _controlPoints[iControlPt+1];
    
    if(eyeFrac >= controlPt1.vertical_frac && eyeFrac < controlPt2.vertical_frac)
    {
      // Linearly interpolate distortion amount based on position between y1 and y2
      const f32 w = (eyeFrac - controlPt1.vertical_frac) / (controlPt2.vertical_frac - controlPt1.vertical_frac);
      DEV_ASSERT(Util::InRange(w, 0.f, 1.f), "ProceduralFace.GetScanlineDistortion.BadWeight");
      distortionAmount_pix = std::round((1.f - w)*(f32)controlPt1.amount_pix + w*(f32)controlPt2.amount_pix);
      
      if(kProcFaceScanline_MaxShiftNoise > 0)
      {
        distortionAmount_pix += GetRNG().RandIntInRange(-kProcFaceScanline_MaxShiftNoise,
                                                        kProcFaceScanline_MaxShiftNoise);
      }
    }
  }
  
  return distortionAmount_pix;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool ScanlineDistorter::GetNextDistortionFrame(const f32 degree, ProceduralFace& faceData, TimeStamp_t& timeInc)
{
  struct DistortParams {
    f32 probNoDistortionAfter; // Probability that after this frame we show the undistorted face for a single frame
    s32 amount_pix;
  };
  
  // Note:
  // (1) "amount" is cumulative!
  // (2) if spacing is greater than one keyframe time, the undistorted face will be shown until next distortion
  static const std::vector<DistortParams> distortionAmounts{
    {.probNoDistortionAfter = 0.f,   .amount_pix = 1},
    {.probNoDistortionAfter = 0.f,   .amount_pix = 1},
    {.probNoDistortionAfter = 0.75f, .amount_pix = 2}, // Will flash undistorted for a frame after this
    {.probNoDistortionAfter = 0.f,   .amount_pix = 1},
    {.probNoDistortionAfter = 0.f,   .amount_pix = 4},
    {.probNoDistortionAfter = 0.f,   .amount_pix = 10},
    {.probNoDistortionAfter = 0.f,   .amount_pix = -1},
    {.probNoDistortionAfter = 0.f,   .amount_pix = -9},
    {.probNoDistortionAfter = 0.75f, .amount_pix = -5}, // Will flash undistorted for a frame after this
    {.probNoDistortionAfter = 0.f,   .amount_pix = 2},
    {.probNoDistortionAfter = 0.f,   .amount_pix = -2},
  };
  
  static auto distortionIter = distortionAmounts.begin();
  
  if(distortionIter == distortionAmounts.end())
  {
    // Reset for next time and leave faceData with no distortion. Let caller know there's nothing else coming.
    faceData.RemoveScanlineDistorter();
    distortionIter = distortionAmounts.begin();
    timeInc = 33;
    return false;
  }
  else
  {
    const s32 amount_pix = std::round(degree * (f32)distortionIter->amount_pix);
    
    if(distortionIter == distortionAmounts.begin())
    {
      // TODO: tie the noise probability to degree
      faceData.InitScanlineDistorter(amount_pix, kProcFaceScanline_OffNoiseProb);
    }
    else
    {
      DEV_ASSERT(nullptr != faceData.GetScanlineDistorter(),
                 "ScanlineDistorter.GetNextDistortionFrame.NullScanlineDistorter");
      
      faceData.GetScanlineDistorter()->Update(amount_pix);
    }
    
    if(Util::IsFltGTZero(distortionIter->probNoDistortionAfter) &&
       (GetRNG().RandDbl() < distortionIter->probNoDistortionAfter))
    {
      timeInc = 2*ANIM_TIME_STEP_MS;
    }
    else
    {
      timeInc = ANIM_TIME_STEP_MS;
    }
    
    ++distortionIter;
    return true;
  }
}
  
} // namespace Vector
} // namespace Anki
