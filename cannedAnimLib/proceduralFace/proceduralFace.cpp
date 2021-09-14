/**
 * File: ProceduralFace.cpp
 *
 * Author: Lee Crippen
 * Created: 11/17/15
 *
 * Description: Holds and sets the face rig data used by ProceduralFace.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#include "cannedAnimLib/baseTypes/cozmo_anim_generated.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"
#include "cannedAnimLib/proceduralFace/scanlineDistorter.h"
#include "coretech/common/engine/jsonTools.h"
#include "coretech/common/shared/math/matrix.h"
#include "coretech/vision/shared/hueSatWrapper.h"
#include "util/helpers/fullEnumToValueArrayChecker.h"
#include "util/helpers/templateHelpers.h"

namespace Anki {
namespace Vector {

ProceduralFace* ProceduralFace::_resetData = nullptr;
ProceduralFace* ProceduralFace::_blankFaceData = nullptr;
ProceduralFace::Value ProceduralFace::_hue = DefaultHue;

ProceduralFace::Value ProceduralFace::_saturation = DefaultSaturation;
Vision::Image ProceduralFace::_hueImage = Vision::Image(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH, static_cast<u8>(_hue * std::numeric_limits<u8>::max()));
Vision::Image ProceduralFace::_satImage = Vision::Image(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH, static_cast<u8>(_saturation * std::numeric_limits<u8>::max()));

#define CONSOLE_GROUP "Face.ParameterizedFace"

// Set up hue and saturation console functions
std::unique_ptr<Anki::Util::IConsoleFunction> ProceduralFace::_hueConsoleFunc = std::unique_ptr<Anki::Util::IConsoleFunction>(new Anki::Util::IConsoleFunction("ProcFace_Hue", HueConsoleFunction, CONSOLE_GROUP, "float hue") );
std::unique_ptr<Anki::Util::IConsoleFunction> ProceduralFace::_saturationConsoleFunc = std::unique_ptr<Anki::Util::IConsoleFunction>(new Anki::Util::IConsoleFunction("ProcFace_Saturation", SaturationConsoleFunction, CONSOLE_GROUP, "float saturation") );
#undef CONSOLE_GROUP

void ProceduralFace::HueConsoleFunction(ConsoleFunctionContextRef context)
{
  const float hue = ConsoleArg_Get_Float(context, "hue");
  ProceduralFace::SetHue(hue);
}

void ProceduralFace::SaturationConsoleFunction(ConsoleFunctionContextRef context)
{
  const float saturation = ConsoleArg_Get_Float(context, "saturation");
  ProceduralFace::SetSaturation(saturation);
}

#if PROCEDURALFACE_SCANLINE_FEATURE
  CONSOLE_VAR_EXTERN(ProceduralFace::Value, kProcFace_DefaultScanlineOpacity);
#endif

namespace {
# define CONSOLE_GROUP "Face.ParameterizedFace"

  CONSOLE_VAR_RANGED(s32, kProcFace_NominalEyeSpacing, CONSOLE_GROUP, 92, -FACE_DISPLAY_WIDTH, FACE_DISPLAY_WIDTH);  // V1: 64;

# undef CONSOLE_GROUP

  //
  // This is a big lookup table for all the properties of the eye parameters.
  // We use the magic of the FullEnumToValueArrayChecker to compile-time guarantee each property is set for each param.
  // Not using CLAD's enum concept here because it is not generated for the CppLite emitter which is used for the
  //   robot/anim process -- plus we're storing a complex type (a struct) instead of just a single value.
  //

  // Different ways we combine eye parameters
  enum class EyeParamCombineMethod : u8 {
    None,
    Add,
    Multiply,
    Average
  };

  // Each entry in the LUT is one of these
  struct EyeParamInfo {
    bool                  isAngle;
    bool                  canBeUnset; // parameter can be "unset", i.e. -1 (cannot use this if isAngle=true!)
    ProceduralFace::Value defaultValue; // initial value for the parameter
    ProceduralFace::Value defaultValueIfCombiningWithUnset; // value to use as default when combining and both unset, ignored if canBeUnset=false
    EyeParamCombineMethod combineMethod;
    struct { ProceduralFace::Value min; ProceduralFace::Value max; } clipLimits;
  };

  // NOTE: HotSpotCenters are marked as canBeUnset=true, but (a) -1 is a valid value, and (b) we aren't doing anything
  //       special when we combine/interpolate them later despite this setting (VIC-13592)
  constexpr static const Util::FullEnumToValueArrayChecker::FullEnumToValueArray<ProceduralFace::Parameter, EyeParamInfo,
  ProceduralFace::Parameter::NumParameters> kEyeParamInfoLUT {
    {ProceduralFace::Parameter::EyeCenterX,        { false, false,  0.f,  0.f, EyeParamCombineMethod::Add,      {-FACE_DISPLAY_WIDTH/2, FACE_DISPLAY_WIDTH/2 }    }     },
    {ProceduralFace::Parameter::EyeCenterY,        { false, false,  0.f,  0.f, EyeParamCombineMethod::Add,      {-FACE_DISPLAY_HEIGHT/2,FACE_DISPLAY_HEIGHT/2}    }     },
    {ProceduralFace::Parameter::EyeScaleX,         { false, false,  1.f,  0.f, EyeParamCombineMethod::Multiply, {0.f, 10.f}    }     },
    {ProceduralFace::Parameter::EyeScaleY,         { false, false,  1.f,  0.f, EyeParamCombineMethod::Multiply, {0.f, 10.f}    }     },
    {ProceduralFace::Parameter::EyeAngle,          { true,  false,  0.f,  0.f, EyeParamCombineMethod::Add,      {-360, 360}    }     },
    {ProceduralFace::Parameter::LowerInnerRadiusX, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::LowerInnerRadiusY, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperInnerRadiusX, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperInnerRadiusY, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperOuterRadiusX, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperOuterRadiusY, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::LowerOuterRadiusX, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::LowerOuterRadiusY, { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperLidY,         { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::UpperLidAngle,     { true,  false,  0.f,  0.f, EyeParamCombineMethod::Add,      {-45,  45}    }     },
    {ProceduralFace::Parameter::UpperLidBend,      { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {-1.f, 1.f}    }     },
    {ProceduralFace::Parameter::LowerLidY,         { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {0.f, 1.f}    }     },
    {ProceduralFace::Parameter::LowerLidAngle,     { true,  false,  0.f,  0.f, EyeParamCombineMethod::Add,      {-45,  45}    }     },
    {ProceduralFace::Parameter::LowerLidBend,      { false, false,  0.f,  0.f, EyeParamCombineMethod::None,     {-1.f, 1.f}    }     },
    {ProceduralFace::Parameter::Saturation,        { false, true,  -1.f,  1.f, EyeParamCombineMethod::None,     {-1.f, 1.f}   }     },
    {ProceduralFace::Parameter::Lightness,         { false, true,  -1.f,  1.f, EyeParamCombineMethod::None,     {-1.f, 1.f}   }     },
    {ProceduralFace::Parameter::GlowSize,          { false, true,  -1.f, 0.0f, EyeParamCombineMethod::None,     {-1.f, 1.f}   }     },
    {ProceduralFace::Parameter::HotSpotCenterX,    { false, true,   0.f, 0.0f, EyeParamCombineMethod::Average,  {-1.f, 1.f}   }     },
    {ProceduralFace::Parameter::HotSpotCenterY,    { false, true,   0.f, 0.0f, EyeParamCombineMethod::Average,  {-1.f, 1.f}   }     },
    {ProceduralFace::Parameter::GlowLightness,     { false, true,   0.f, 0.f,  EyeParamCombineMethod::None,     {0.f, 1.f}   }     },
  };

  static_assert( Util::FullEnumToValueArrayChecker::IsSequentialArray(kEyeParamInfoLUT),
                "EyeParamInfoLUT array does not define each entry in order, once and only once!");

}

void ProceduralFace::SetResetData(const ProceduralFace& newResetData)
{
  ProceduralFace* oldPointer = _resetData;
  _resetData = new ProceduralFace(newResetData);
  Util::SafeDelete(oldPointer);
}

void ProceduralFace::SetBlankFaceData(const ProceduralFace& blankFace)
{
  ProceduralFace* oldPointer = _blankFaceData;
  _blankFaceData = new ProceduralFace(blankFace);
  Util::SafeDelete(oldPointer);
}

void ProceduralFace::Reset(bool withBlankFace)
{
  if ((!withBlankFace) && (nullptr != _resetData))
  {
    *this = *_resetData;
  }
  else if (withBlankFace && (nullptr != _blankFaceData))
  {
    *this = *_blankFaceData;
  }
  else
  {
    PRINT_NAMED_ERROR("ProceduralFace.Reset.NoFacePtr",
                      "No valid %s face pointer to reset with",
                      withBlankFace ? "blank" : "default");
  }
}


ProceduralFace::ProceduralFace()
{
  for (std::underlying_type<Parameter>::type iParam=0; iParam < Util::EnumToUnderlying(Parameter::NumParameters); ++iParam)
  {
    _eyeParams[WhichEye::Left][iParam] = kEyeParamInfoLUT[iParam].Value().defaultValue;
  }
  _eyeParams[WhichEye::Right] = _eyeParams[WhichEye::Left];

#if PROCEDURALFACE_SCANLINE_FEATURE
  _scanlineOpacity = kProcFace_DefaultScanlineOpacity;
#endif
}

ProceduralFace::ProceduralFace(const ProceduralFace& other)
: _eyeParams(other._eyeParams)
, _faceAngle_deg(other._faceAngle_deg)
, _faceScale(other._faceScale)
, _faceCenter(other._faceCenter)
#if PROCEDURALFACE_SCANLINE_FEATURE
, _scanlineOpacity(other._scanlineOpacity)
#endif
{
  if(nullptr != other._scanlineDistorter)
  {
    _scanlineDistorter.reset(new ScanlineDistorter(*other._scanlineDistorter));
  }
}

bool ProceduralFace::operator==(const ProceduralFace& other) const
{
  if (_eyeParams[ProceduralFace::WhichEye::Left] != other._eyeParams[ProceduralFace::WhichEye::Left]) {
    return false;
  }
  if (_eyeParams[ProceduralFace::WhichEye::Right] != other._eyeParams[ProceduralFace::WhichEye::Right]) {
    return false;
  }
  if (_faceAngle_deg != other._faceAngle_deg) {
    return false;
  }
  if (_faceScale != other._faceScale) {
    return false;
  }
  if (_faceCenter != other._faceCenter) {
    return false;
  }
#if PROCEDURALFACE_SCANLINE_FEATURE
  if (_scanlineOpacity != other._scanlineOpacity) {
    return false;
  }
#endif
  return true;
}

s32 ProceduralFace::GetNominalLeftEyeX()
{
  return (WIDTH - kProcFace_NominalEyeSpacing)/2;
}

s32 ProceduralFace::GetNominalRightEyeX()
{
  return ProceduralFace::GetNominalLeftEyeX() + kProcFace_NominalEyeSpacing;
}

s32 ProceduralFace::GetNominalEyeY()
{
  return HEIGHT/2;
}

ProceduralFace& ProceduralFace::operator=(const ProceduralFace &other)
{
  if(this != &other)
  {
    _eyeParams     = other._eyeParams;
    _faceAngle_deg = other._faceAngle_deg;
    _faceScale     = other._faceScale;
    _faceCenter    = other._faceCenter;
#if PROCEDURALFACE_SCANLINE_FEATURE
    _scanlineOpacity = other._scanlineOpacity;
#endif

    if(nullptr != other._scanlineDistorter)
    {
      // Deep copy other's ScanlineDistorter (since they are unique)
      _scanlineDistorter.reset(new ScanlineDistorter(*other._scanlineDistorter));
    }
    else if(nullptr != _scanlineDistorter)
    {
      // Other does not have a ScanlineDistorter, but this does. Get rid of this's.
      _scanlineDistorter.reset();
    }
  }
  return *this;
}

ProceduralFace::~ProceduralFace() = default;

static const char* kFaceAngleKey = "faceAngle";
static const char* kFaceCenterXKey = "faceCenterX";
static const char* kFaceCenterYKey = "faceCenterY";
static const char* kFaceScaleXKey = "faceScaleX";
static const char* kFaceScaleYKey = "faceScaleY";
static const char* kScanlineOpacityKey = "scanlineOpacity";
static const char* kLeftEyeKey = "leftEye";
static const char* kRightEyeKey = "rightEye";

void ProceduralFace::SetEyeArrayHelper(WhichEye eye, const std::vector<Value>& eyeArray)
{
  const char* eyeStr = (eye == WhichEye::Left) ? kLeftEyeKey : kRightEyeKey;

  // NOTE: don't do this

  // TODO: replace with a single version of assets, same version of code and assets,
  //       that is pushed with atomic releases
  // https://ankiinc.atlassian.net/browse/VIC-1964

  const size_t N = static_cast<size_t>(Parameter::NumParameters);
  const size_t N_without_hotspots = N - 6; // Before Saturation, Lightness, Glow, and HotSpotCenterX/Y were added
  const size_t N_without_glowlightness = N - 1; // Before Eye Glow Lightness
  if(eyeArray.size() != N && eyeArray.size() != N_without_hotspots && eyeArray.size() != N_without_glowlightness)
  {
    PRINT_NAMED_WARNING("ProceduralFace.SetEyeArrayHelper.WrongNumParams",
                        "Unexpected number of parameters for %s array (%lu vs. %lu or %lu or %lu)",
                        eyeStr, (unsigned long)eyeArray.size(), (unsigned long)N, (unsigned long)N_without_hotspots, (unsigned long)N_without_glowlightness);
  }
  for(s32 i=0; i<std::min(eyeArray.size(), N); ++i)
  {
    SetParameter(eye, static_cast<ProceduralFace::Parameter>(i), eyeArray[i]);
  }

  // Upgrade old param arrays to add hotspots/glow as needed
  static_assert(N_without_hotspots < N_without_glowlightness,
                "Expecting hotspot parameters to come before glow");
  if (eyeArray.size() <= N_without_glowlightness)
  {
    // Start updating params beginning with hotspots or glow
    const size_t N_upgrade_start = (eyeArray.size() <= N_without_hotspots ?
                                    N_without_hotspots :
                                    N_without_glowlightness);

    for (std::underlying_type<Parameter>::type iParam=N_upgrade_start; iParam < N; ++iParam)
    {
      const auto& paramInfo = kEyeParamInfoLUT[iParam].Value();
      if(paramInfo.canBeUnset)
      {
        _eyeParams[eye][iParam] = paramInfo.defaultValueIfCombiningWithUnset;
      }
    }
  }

}

void ProceduralFace::SetFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe)
{
  std::vector<Value> eyeParams;

  auto leftEyeData = procFaceKeyframe->leftEye();
  for (int leIdx=0; leIdx < leftEyeData->size(); leIdx++) {
    auto leftEyeVal = leftEyeData->Get(leIdx);
    eyeParams.push_back(leftEyeVal);
  }
  SetEyeArrayHelper(WhichEye::Left, eyeParams);

  eyeParams.clear();

  auto rightEyeData = procFaceKeyframe->rightEye();
  for (int reIdx=0; reIdx < rightEyeData->size(); reIdx++) {
    auto rightEyeVal = rightEyeData->Get(reIdx);
    eyeParams.push_back(rightEyeVal);
  }
  SetEyeArrayHelper(WhichEye::Right, eyeParams);

  const f32 fbFaceAngle = procFaceKeyframe->faceAngle();
  SetFaceAngle(fbFaceAngle);

  const f32 fbFaceCenterX = procFaceKeyframe->faceCenterX();
  const f32 fbFaceCenterY = procFaceKeyframe->faceCenterY();
  SetFacePosition({fbFaceCenterX, fbFaceCenterY});

  const f32 fbFaceScaleX = procFaceKeyframe->faceScaleX();
  const f32 fbFaceScaleY = procFaceKeyframe->faceScaleY();
  SetFaceScale({fbFaceScaleX, fbFaceScaleY});

  const f32 fbScanlineOpacity = procFaceKeyframe->scanlineOpacity();
  SetScanlineOpacity(fbScanlineOpacity);
}

void ProceduralFace::SetFromJson(const Json::Value &jsonRoot)
{
  std::vector<Value> eyeParams;
  if(JsonTools::GetVectorOptional(jsonRoot, kLeftEyeKey, eyeParams)) {
    SetEyeArrayHelper(WhichEye::Left, eyeParams);
  }
  eyeParams.clear();
  if(JsonTools::GetVectorOptional(jsonRoot, kRightEyeKey, eyeParams)) {
    SetEyeArrayHelper(WhichEye::Right, eyeParams);
  }

  f32 jsonFaceAngle=0;
  if(JsonTools::GetValueOptional(jsonRoot, kFaceAngleKey, jsonFaceAngle)) {
    SetFaceAngle(jsonFaceAngle);
  }

  f32 jsonFaceCenX=0, jsonFaceCenY=0;
  if(JsonTools::GetValueOptional(jsonRoot, kFaceCenterXKey, jsonFaceCenX) &&
     JsonTools::GetValueOptional(jsonRoot, kFaceCenterYKey, jsonFaceCenY))
  {
    SetFacePosition({jsonFaceCenX, jsonFaceCenY});
  }

  f32 jsonFaceScaleX=1.f,jsonFaceScaleY=1.f;
  if(JsonTools::GetValueOptional(jsonRoot, kFaceScaleXKey, jsonFaceScaleX) &&
     JsonTools::GetValueOptional(jsonRoot, kFaceScaleYKey, jsonFaceScaleY))
  {
    SetFaceScale({jsonFaceScaleX, jsonFaceScaleY});
  }

  f32 scanlineOpacity = -1.f;
  if(JsonTools::GetValueOptional(jsonRoot, kScanlineOpacityKey, scanlineOpacity))
  {
    SetScanlineOpacity(scanlineOpacity);
  }
}

void ProceduralFace::SetFromValues(const std::vector<f32>& leftEyeData, const std::vector<f32>& rightEyeData,
                                   f32 faceAngle_deg, f32 faceCenterX, f32 faceCenterY, f32 faceScaleX, f32 faceScaleY,
                                   f32 scanlineOpacity)
{
  std::vector<Value> eyeParams;

  for (std::vector<f32>::const_iterator it = leftEyeData.begin(); it != leftEyeData.end(); ++it) {
    eyeParams.push_back(*it);
  }
  SetEyeArrayHelper(WhichEye::Left, eyeParams);

  eyeParams.clear();

  for (std::vector<f32>::const_iterator it = rightEyeData.begin(); it != rightEyeData.end(); ++it) {
    eyeParams.push_back(*it);
  }
  SetEyeArrayHelper(WhichEye::Right, eyeParams);

  SetFaceAngle(faceAngle_deg);
  SetFacePosition({faceCenterX, faceCenterY});
  SetFaceScale({faceScaleX, faceScaleY});
  SetScanlineOpacity(scanlineOpacity);
}

void ProceduralFace::SetFromMessage(const ProceduralFaceParameters& msg)
{
  SetFaceAngle(msg.faceAngle_deg);
  SetFacePosition({msg.faceCenX, msg.faceCenY});
  SetFaceScale({msg.faceScaleX, msg.faceScaleY});
  SetScanlineOpacity(msg.scanlineOpacity);

  for(s32 i=0; i<(size_t)ProceduralEyeParameter::NumParameters; ++i)
  {
    SetParameter(WhichEye::Left,  static_cast<ProceduralFace::Parameter>(i), msg.leftEye[i]);
    SetParameter(WhichEye::Right, static_cast<ProceduralFace::Parameter>(i), msg.rightEye[i]);
  }
}

void ProceduralFace::LookAt(f32 xShift, f32 yShift, f32 xmax, f32 ymax,
                            f32 lookUpMaxScale, f32 lookDownMinScale, f32 outerEyeScaleIncrease)
{
  SetFacePositionAndKeepCentered({xShift, yShift});

  // Amount "outer" eye will increase in scale depending on how far left/right we look
  const f32 yscaleLR = 1.f + outerEyeScaleIncrease * std::min(1.f, std::abs(xShift)/xmax);

  // Amount both eyes will increase/decrease in size depending on how far we look
  // up or down
  const f32 yscaleUD = (lookUpMaxScale-lookDownMinScale)*std::min(1.f, (1.f - (yShift + ymax)/(2.f*ymax))) + lookDownMinScale;

  if(xShift < 0) {
    SetParameter(WhichEye::Left,  ProceduralEyeParameter::EyeScaleY, yscaleLR*yscaleUD);
    SetParameter(WhichEye::Right, ProceduralEyeParameter::EyeScaleY, (2.f-yscaleLR)*yscaleUD);
  } else {
    SetParameter(WhichEye::Left,  ProceduralEyeParameter::EyeScaleY, (2.f-yscaleLR)*yscaleUD);
    SetParameter(WhichEye::Right, ProceduralEyeParameter::EyeScaleY, yscaleLR*yscaleUD);
  }

  DEV_ASSERT_MSG(FLT_GT(GetParameter(WhichEye::Left,  ProceduralEyeParameter::EyeScaleY), 0.f),
                 "ProceduralFace.LookAt.NegativeLeftEyeScaleY",
                 "yShift=%f yscaleLR=%f yscaleUD=%f ymax=%f",
                 yShift, yscaleLR, yscaleUD, ymax);
  DEV_ASSERT_MSG(FLT_GT(GetParameter(WhichEye::Right, ProceduralEyeParameter::EyeScaleY), 0.f),
                 "ProceduralFace.LookAt.NegativeRightEyeScaleY",
                 "yShift=%f yscaleLR=%f yscaleUD=%f ymax=%f",
                 yShift, yscaleLR, yscaleUD, ymax);

  //SetParameterBothEyes(ProceduralEyeParameter::EyeScaleX, xscale);

  // If looking down (positive y), push eyes together (IOD=interocular distance)
  const f32 MaxIOD = 2.f;
  f32 reduceIOD = 0.f;
  if(yShift > 0) {
    reduceIOD = MaxIOD*std::min(1.f, yShift/ymax);
  }
  SetParameter(WhichEye::Left,  ProceduralEyeParameter::EyeCenterX,  reduceIOD);
  SetParameter(WhichEye::Right, ProceduralEyeParameter::EyeCenterX, -reduceIOD);

  //PRINT_NAMED_DEBUG("ProceduralFace.LookAt",
  //                  "shift=(%.1f,%.1f), up/down scale=%.3f, left/right scale=%.3f), reduceIOD=%.3f",
  //                  xShift, yShift, yscaleUD, yscaleLR, reduceIOD);
}

template<typename T>
inline static T LinearBlendHelper(const T value1, const T value2, const float blendFraction)
{
  if(value1 == value2) {
    // Special case, no math needed
    return value1;
  }

  T blendValue = static_cast<T>((1.f - blendFraction)*static_cast<float>(value1) +
                                blendFraction*static_cast<float>(value2));
  return blendValue;
}

inline static ProceduralFace::Value BlendAngleHelper(const ProceduralFace::Value angle1_deg,
                                                     const ProceduralFace::Value angle2_deg,
                                                     const float blendFraction)
{
  if(angle1_deg == angle2_deg) {
    // Special case, no math needed
    return angle1_deg;
  }

  float start_deg = angle1_deg;
  float end_deg = angle2_deg;

  float diff = fabsf(end_deg - start_deg);
  if (diff > 180) {
    if (end_deg > start_deg) {
      start_deg += 360;
    }
    else {
      end_deg += 360;
    }
  }
  const float result = LinearBlendHelper(start_deg, end_deg, blendFraction);
  return static_cast<ProceduralFace::Value>(result);
}

void ProceduralFace::Interpolate(const ProceduralFace& face1, const ProceduralFace& face2,
                                 float blendFraction, bool usePupilSaccades)
{
  assert(blendFraction >= 0.f && blendFraction <= 1.f);

  // Special cases, no blending required:
  if(Util::IsNearZero(blendFraction)) {
    *this = face1;
    return;
  } else if(Util::IsNear(blendFraction, 1.f)) {
    *this = face2;
    return;
  }

  for(auto const whichEye : {WhichEye::Left, WhichEye::Right})
  {
    for(int iParam=0; iParam < static_cast<int>(Parameter::NumParameters); ++iParam)
    {
      Parameter param = static_cast<Parameter>(iParam);

      const auto& paramInfo = kEyeParamInfoLUT[iParam].Value();
      if(paramInfo.isAngle) {
        // Treat this param as an angle
        DEV_ASSERT_MSG(!paramInfo.canBeUnset,
                       "ProceduralFace.Interpolate.AngleParamCannotAlsoBeUnset",
                       "%s", EnumToString(param));
        SetParameter(whichEye, param, BlendAngleHelper(face1.GetParameter(whichEye, param),
                                                       face2.GetParameter(whichEye, param),
                                                       blendFraction));
      }
      else if(paramInfo.canBeUnset) {
        // Special linear blend taking into account whether values are "set"
        // NOTE: Despite preceding comment, this is just a regular linear blend. Is that ok? (VIC-13592)
        SetParameter(whichEye, param, LinearBlendHelper(face1.GetParameter(whichEye, param),
                                                        face2.GetParameter(whichEye, param),
                                                        blendFraction));
      }
      else {
        SetParameter(whichEye, param, LinearBlendHelper(face1.GetParameter(whichEye, param),
                                                        face2.GetParameter(whichEye, param),
                                                        blendFraction));
      }

    } // for each parameter
  } // for each eye
  SetFaceAngle(BlendAngleHelper(face1.GetFaceAngle(), face2.GetFaceAngle(), blendFraction));
  SetFacePosition({LinearBlendHelper(face1.GetFacePosition().x(), face2.GetFacePosition().x(), blendFraction),
                   LinearBlendHelper(face1.GetFacePosition().y(), face2.GetFacePosition().y(), blendFraction)});
  SetFaceScale({LinearBlendHelper(face1.GetFaceScale().x(), face2.GetFaceScale().x(), blendFraction),
                LinearBlendHelper(face1.GetFaceScale().y(), face2.GetFaceScale().y(), blendFraction)});

  SetScanlineOpacity(LinearBlendHelper(face1.GetScanlineOpacity(), face2.GetScanlineOpacity(),
                                       blendFraction));

} // Interpolate()

void ProceduralFace::GetEyeBoundingBox(Value& xmin, Value& xmax, Value& ymin, Value& ymax)
{
  // Left edge of left eye
  const Value leftHalfWidth = GetParameter(WhichEye::Left, Parameter::EyeScaleX) * NominalEyeWidth/2;
  const Value rightHalfWidth = GetParameter(WhichEye::Right, Parameter::EyeScaleX) * NominalEyeWidth/2;
  xmin = (ProceduralFace::GetNominalLeftEyeX() +
          _faceScale.x() * (GetParameter(WhichEye::Left, Parameter::EyeCenterX) - leftHalfWidth));

  // Right edge of right eye
  xmax = (ProceduralFace::GetNominalRightEyeX() +
          _faceScale.x()*(GetParameter(WhichEye::Right, Parameter::EyeCenterX) + rightHalfWidth));


  // Min of the top edges of the two eyes
  const Value leftHalfHeight = GetParameter(WhichEye::Left, Parameter::EyeScaleY) * NominalEyeHeight/2;
  const Value rightHalfHeight = GetParameter(WhichEye::Right, Parameter::EyeScaleY) * NominalEyeHeight/2;
  ymin = (ProceduralFace::GetNominalEyeY() + _faceScale.y() * (std::min(GetParameter(WhichEye::Left, Parameter::EyeCenterY) - leftHalfHeight,
                                                                        GetParameter(WhichEye::Right, Parameter::EyeCenterY) - rightHalfHeight)));

  // Max of the bottom edges of the two eyes
  ymax = (ProceduralFace::GetNominalEyeY() + _faceScale.y() * (std::max(GetParameter(WhichEye::Left, Parameter::EyeCenterY) + leftHalfHeight,
                                                                        GetParameter(WhichEye::Right, Parameter::EyeCenterY) + rightHalfHeight)));

} // GetEyeBoundingBox()

void ProceduralFace::SetFacePosition(Point<2, Value> center)
{
  _faceCenter = center;
}

void ProceduralFace::SetFacePositionAndKeepCentered(Point<2, Value> center)
{
  // Try not to let the eyes drift off the face (ignores outer glow)
  // NOTE: (1) if you set center and *then* change eye centers/scales, you could still go off screen
  //       (2) this also doesn't take lid height into account, so if the top lid is half closed and
  //           you move the eyes way down, it could look like they disappeared, for example

  Value xmin=0, xmax=0, ymin=0, ymax=0;
  GetEyeBoundingBox(xmin, xmax, ymin, ymax);

  // The most we can move left is the distance b/w left edge of left eye and the
  // left edge of the screen. The most we can move right is the distance b/w the
  // right edge of the right eye and the right edge of the screen
  SetFacePosition({CLIP(center.x(), -xmin, ProceduralFace::WIDTH-xmax),
                   CLIP(center.y(), -ymin, ProceduralFace::HEIGHT-ymax)});
}


void ProceduralFace::CombineEyeParams(EyeParamArray& eyeArray0, const EyeParamArray& eyeArray1)
{
  for (std::underlying_type<Parameter>::type iParam=0; iParam < Util::EnumToUnderlying(Parameter::NumParameters); ++iParam)
  {
    const auto& paramInfo = kEyeParamInfoLUT[iParam].Value();
    switch(paramInfo.combineMethod)
    {
      case EyeParamCombineMethod::None:
        // Nothing to do
        continue;

      case EyeParamCombineMethod::Add:
        eyeArray0[iParam] += eyeArray1[iParam];
        break;

      case EyeParamCombineMethod::Multiply:
        eyeArray0[iParam] *= eyeArray1[iParam];
        break;

      case EyeParamCombineMethod::Average:
        eyeArray0[iParam] = LinearBlendHelper(eyeArray0[iParam], eyeArray1[iParam], 0.5f);
        break;
    }
  }
}

ProceduralFace& ProceduralFace::Combine(const ProceduralFace& otherFace)
{
  CombineEyeParams(_eyeParams[(int)WhichEye::Left], otherFace.GetParameters(WhichEye::Left));
  CombineEyeParams(_eyeParams[(int)WhichEye::Right], otherFace.GetParameters(WhichEye::Right));

  _faceAngle_deg += otherFace.GetFaceAngle();
  _faceScale     *= otherFace.GetFaceScale();
  _faceCenter    += otherFace.GetFacePosition();

#if PROCEDURALFACE_SCANLINE_FEATURE
  _scanlineOpacity = LinearBlendHelper(_scanlineOpacity, otherFace._scanlineOpacity, 0.5f);
#endif

  const bool thisHasScanlineDistortion  = (nullptr != _scanlineDistorter);
  const bool otherHasScanlineDistortion = (nullptr != otherFace._scanlineDistorter);

  if(thisHasScanlineDistortion && otherHasScanlineDistortion)
  {
    // Need to pick one. Convention, for whatever reason, will be to choose the one that distorts the
    // midpoint of the eyes the most (in either direction)
    const s32 thisMidPointShift  = std::abs(_scanlineDistorter->GetEyeDistortionAmount(0.5f));
    const s32 otherMidPointShift = std::abs(otherFace._scanlineDistorter->GetEyeDistortionAmount(0.5f));
    if(otherMidPointShift > thisMidPointShift)
    {
      // Other distorts more. Use it.
      _scanlineDistorter.reset(new ScanlineDistorter(*otherFace._scanlineDistorter));
    }
  }
  else if(otherHasScanlineDistortion)
  {
    DEV_ASSERT(!thisHasScanlineDistortion, "ProceduralFace.Combine.LogicError");
    _scanlineDistorter.reset(new ScanlineDistorter(*otherFace._scanlineDistorter));
  }

  return *this;
}


ProceduralFace::Value ProceduralFace::Clip(WhichEye eye, Parameter param, Value newValue) const
{
  auto const& clipLimits = kEyeParamInfoLUT[Util::EnumToUnderlying(param)].Value().clipLimits;
  if(!Util::InRange(newValue, clipLimits.min, clipLimits.max))
  {
    ClipWarnFcn(EnumToString(param), newValue, clipLimits.min, clipLimits.max);
    newValue = Util::Clamp(newValue, clipLimits.min, clipLimits.max);
  }

  if(std::isnan(newValue)) {
    PRINT_NAMED_WARNING("ProceduralFace.Clip.NaN",
                        "Returning original value instead of NaN for %s",
                        EnumToString(param));
    newValue = GetParameter(eye, param);
  }

  return newValue;
}

static void ClipWarning(const char* paramName,
                        ProceduralFace::Value value,
                        ProceduralFace::Value minVal,
                        ProceduralFace::Value maxVal)
{
  PRINT_NAMED_WARNING("ProceduralFace.Clip.OutOfRange",
                      "Value of %f out of range [%f,%f] for parameter %s. Clipping.",
                      value, minVal, maxVal, paramName);
}

static void NoClipWarning(const char* paramName,
                          ProceduralFace::Value value,
                          ProceduralFace::Value minVal,
                          ProceduralFace::Value maxVal)
{
  // Do nothing
}

// Start out with warnings enabled:
std::function<void(const char *,
                   ProceduralFace::Value,
                   ProceduralFace::Value,
                   ProceduralFace::Value)> ProceduralFace::ClipWarnFcn = &ClipWarning;

void ProceduralFace::EnableClippingWarning(bool enable)
{
  if(enable) {
    ClipWarnFcn = ClipWarning;
  } else {
    ClipWarnFcn = NoClipWarning;
  }
}

std::shared_ptr<Vision::HueSatWrapper> ProceduralFace::GetHueSatWrapper()
{
  static std::shared_ptr<Vision::HueSatWrapper> hsImg(
    new Vision::HueSatWrapper(GetHueImagePtr(), GetSaturationImagePtr()));
  return hsImg;
}

void ProceduralFace::InitScanlineDistorter(s32 maxAmount_pix, f32 noiseProb)
{
  _scanlineDistorter.reset(new ScanlineDistorter(maxAmount_pix, noiseProb));
}

void ProceduralFace::RemoveScanlineDistorter()
{
  _scanlineDistorter.reset();
}

template <class T>
void ProceduralFace::AddConsoleVar(T & var, const char * name, const char * group, const T & minVal, const T & maxVal)
{
  // Allocate console var, then add it to list of console vars managed by this object
  ConsoleVarPtr ptr(new Anki::Util::ConsoleVar<T>(var, name, group, minVal, maxVal, true));
  _consoleVars.emplace_back(std::move(ptr));
}

#define CONSOLE_GROUP "Face.ParameterizedFace"

void ProceduralFace::RegisterFaceWithConsoleVars()
{
  AddConsoleVar<float>(_faceCenter[0], "kProcFace_CenterX", CONSOLE_GROUP, -100.f, 100.f);
  AddConsoleVar<float>(_faceCenter[1], "kProcFace_CenterY", CONSOLE_GROUP, -100.f, 100.f);
  AddConsoleVar<float>(_faceAngle_deg, "kProcFace_Angle_deg", CONSOLE_GROUP, -90.f, 90.f);
  AddConsoleVar<float>(_faceScale[0], "kProcFace_ScaleX", CONSOLE_GROUP, 0.f, 4.f);
  AddConsoleVar<float>(_faceScale[1],"kProcFace_ScaleY", CONSOLE_GROUP, 0.f, 4.f);
  AddConsoleVar<float>(_hue, "kProcFace_Hue", CONSOLE_GROUP, 0.f, 1.f);
  AddConsoleVar<float>(_saturation, "kProcFace_Saturation", CONSOLE_GROUP, 0., 1.f);

  for (auto whichEye : {WhichEye::Left, WhichEye::Right}) {
    for (std::underlying_type<Parameter>::type iParam=0; iParam < Util::EnumToUnderlying(Parameter::NumParameters); ++iParam) {
      if (!PROCEDURALFACE_GLOW_FEATURE) {
        if (iParam == (int)Parameter::GlowSize ||
            iParam == (int)Parameter::GlowLightness) {
          continue;
        }
      }
      if (!PROCEDURALFACE_ANIMATED_SATURATION) {
        if (iParam == (int)Parameter::Saturation) {
          continue;
        }
      }

      std::string eyeName = (whichEye == 0 ? "Left" : "Right");
      std::string group = std::string("Face.") + eyeName;
      std::string param = eyeName+"_"+EnumToString(kEyeParamInfoLUT[iParam].EnumValue());
      AddConsoleVar<float>(_eyeParams[whichEye][iParam],
                           param.c_str(),
                           group.c_str(),
                           kEyeParamInfoLUT[iParam].Value().clipLimits.min,
                           kEyeParamInfoLUT[iParam].Value().clipLimits.max
                          );
    }
  }
}
#undef CONSOLE_GROUP

} // namespace Vector
} // namespace Anki
