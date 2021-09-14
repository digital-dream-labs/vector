/**
 * File: ProceduralFace.h
 *
 * Author: Lee Crippen
 * Created: 11/17/15
 *
 * Description: Holds and sets the face rig data used by ProceduralFace.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#ifndef __Anki_Cozmo_ProceduralFace_H__
#define __Anki_Cozmo_ProceduralFace_H__

#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/point.h"
#include "coretech/vision/engine/image.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "clad/types/proceduralFaceTypes.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include <array>
#include <list>
#include <vector>

#define PROCEDURALFACE_NOISE_FEATURE         1 // feature capable and enabled as num frames = 5
#define PROCEDURALFACE_ANIMATED_SATURATION   0 // disable saturation in canned animations
#define PROCEDURALFACE_PROCEDURAL_SATURATION 1 // only take saturation from the C++ API
#define PROCEDURALFACE_GLOW_FEATURE          0
#define PROCEDURALFACE_SCANLINE_FEATURE      0 // feature capable but disabled by default as kProcFace_Scanlines = false

namespace Json {
  class Value;
}

namespace CozmoAnim {
  struct ProceduralFace;
}

namespace Anki {

namespace Util {
  class IConsoleFunction;
  class IConsoleVariable;
}

namespace Vision{
  class HueSatWrapper;
}

namespace Vector {

// Forward declarations
namespace ExternalInterface {
  struct DisplayProceduralFace;
}

class ScanlineDistorter;

class ProceduralFace
{
public:

  static constexpr int WIDTH  = FACE_DISPLAY_WIDTH;
  static constexpr int HEIGHT = FACE_DISPLAY_HEIGHT;

  // Nominal positions/sizes for everything (these are things that aren't
  // parameterized at dynamically, but could be if we want)

  // These values are based off of V1 parameters but scaled up by a ratio of V2 dimensions : V1 dimensions (roughly 1.43x)
  // V1 width: 128   New: 184  => 1.43x increase
  // V1 height: 64   New:  96  => 1.5x  increase
  static constexpr s32   NominalEyeHeight       = 57;  // V1: 40;
  static constexpr s32   NominalEyeWidth        = 43;  // V1: 30;

  static constexpr f32 DefaultHue = 0.45f;
  static constexpr f32 DefaultSaturation = 1.0f;

  using Value = f32;
  using Parameter = ProceduralEyeParameter;

  // Container for the parameters for both eyes
  using EyeParamArray = std::array<Value, static_cast<size_t>(Parameter::NumParameters)>;

  // Note: SCREEN Left and Right, not Cozmo's left and right!!!!
  enum WhichEye {
    Left,
    Right
  };

  ProceduralFace();
  ProceduralFace(const ProceduralFace& other);
  ProceduralFace& operator=(const ProceduralFace& other);

  ~ProceduralFace();

  bool operator==(const ProceduralFace& other) const;

  // Allows setting an instance of ProceduralFace to be used as reset values
  static void SetResetData(const ProceduralFace& newResetData);
  static void SetBlankFaceData(const ProceduralFace& blankFace);

  // Reset parameters to their nominal values. If !withBlankFace, uses the face passed to SetResetData.
  // If withBlankFace, uses the face passed to SetBlankFaceData
  void Reset(bool withBlankFace = false);

  // Read in available parameters from Json, FlatBuffers or input values
  void SetFromFlatBuf(const CozmoAnim::ProceduralFace* procFaceKeyframe);
  void SetFromJson(const Json::Value &jsonRoot);
  void SetFromValues(const std::vector<f32>& leftEyeData, const std::vector<f32>& rightEyeData,
                     f32 faceAngle_deg, f32 faceCenterX, f32 faceCenterY, f32 faceScaleX, f32 faceScaleY,
                     f32 scanlineOpacity);
  void SetFromMessage(const ProceduralFaceParameters& msg);

  static s32 GetNominalLeftEyeX();
  static s32 GetNominalRightEyeX();
  static s32 GetNominalEyeY();

  // Get/Set each of the above procedural parameters, for each eye
  void  SetParameter(WhichEye whichEye, Parameter param, Value value);
  Value GetParameter(WhichEye whichEye, Parameter param) const;
  const EyeParamArray& GetParameters(WhichEye whichEye) const;
  void SetParameters(WhichEye whichEye, const EyeParamArray& params);

  // Set the same value to a parameter for both eyes:
  void SetParameterBothEyes(Parameter param, Value value);

  // Get/Set the overall angle of the whole face (still using parameter on interval [-1,1]
  void SetFaceAngle(Value value);
  Value GetFaceAngle() const;

  // Get/Set the overall face position
  void SetFacePosition(Point<2,Value> center);
  void SetFacePositionAndKeepCentered(Point<2, Value> center);
  Point<2,Value> const& GetFacePosition() const;

  // Get/Set the overall face scale
  void SetFaceScale(Point<2,Value> scale);
  Point<2,Value> const& GetFaceScale() const;

  // Get/Set the scanline opacity
  void SetScanlineOpacity(Value opacity);
  Value GetScanlineOpacity() const;

  // Set the global hue of all faces
  static void  SetHue(Value hue);
  static Value GetHue();
  static void ResetHueToDefault();

  // Set the global saturation of all faces
  static void  SetSaturation(Value saturation);
  static Value GetSaturation();

  // Get an image filled with the current hue value
  static Vision::Image& GetHueImage();
  static Vision::Image* GetHueImagePtr();

  // Get an image filled with a saturation value suitable for
  // creating an HSV face image
  static Vision::Image& GetSaturationImage();
  static Vision::Image* GetSaturationImagePtr();

  // Get a pointer that encapsulates the procedural face's hue and saturation images
  static std::shared_ptr<Vision::HueSatWrapper> GetHueSatWrapper();

  // Initialize scanline distortion
  void InitScanlineDistorter(s32 maxAmount_pix, f32 noiseProb);

  // Get rid of any scanline distortion
  void RemoveScanlineDistorter();

  // Get ScanlineDistortion component. Returns nullptr if there is no scanline distortion
  // (i.e. if InitScanlineDistortion has not been called)
  ScanlineDistorter* GetScanlineDistorter()             { return _scanlineDistorter.get(); }
  const ScanlineDistorter* GetScanlineDistorter() const { return _scanlineDistorter.get(); }

  // Set this face's parameters to values interpolated from two other faces.
  //   When BlendFraction == 0.0, the parameters will be equal to face1's.
  //   When BlendFraction == 1.0, the parameters will be equal to face2's.
  //   TODO: Support other types of interpolation besides simple linear
  //   Note: 0.0 <= BlendFraction <= 1.0!
  // If usePupilSaccades==true, pupil positions don't interpolate smoothly but
  //   instead jump when fraction crossed 0.5.
  void Interpolate(const ProceduralFace& face1,
                   const ProceduralFace& face2,
                   float fraction,
                   bool usePupilSaccades = false);

  // Adjust settings to make the robot look at a give place. You specify the
  // (x,y) position of the face center and the normalize factor which is the
  // maximum distance in x or y this LookAt is relative to. The eyes are then
  // shifted, scaled, and squeezed together as needed to create the effect of
  // the robot looking there.
  //  - lookUpMaxScale controls how big the eyes get when looking up (negative y)
  //  - lookDownMinScale controls how small the eyes get when looking down (positive y)
  //  - outerEyeScaleIncrease controls the differentiation between inner/outer eye height
  //    when looking left or right
  void LookAt(f32 x, f32 y, f32 xmax, f32 ymax,
              f32 lookUpMaxScale = 1.1f, f32 lookDownMinScale=0.85f, f32 outerEyeScaleIncrease=0.1f);

  // Combine the input params with those from our instance
  ProceduralFace& Combine(const ProceduralFace& otherFace);

  // E.g. for unit tests
  static void EnableClippingWarning(bool enable);

  // Get the bounding edge of the current eyes in screen pixel space, at their current
  // size and position, without taking into account the current FacePosition (a.k.a.
  // face center) or face angle.
  void GetEyeBoundingBox(Value& xmin, Value& xmax, Value& ymin, Value& ymax);

  // Initialize console variables for this object
  void RegisterFaceWithConsoleVars();

private:
  static Vision::Image _hueImage;
  static Vision::Image _satImage;

  std::array<EyeParamArray, 2> _eyeParams{{}};

  std::unique_ptr<ScanlineDistorter> _scanlineDistorter;

  Value           _faceAngle_deg   = 0.0f;
  Point<2,Value>  _faceScale       = 1.0f;
  Point<2,Value>  _faceCenter      = 0.0f;
#if PROCEDURALFACE_SCANLINE_FEATURE
  Value           _scanlineOpacity; // set to default from console var in constructor
#endif

  static Value    _hue;
  static Value    _saturation;

  static void HueConsoleFunction(ConsoleFunctionContextRef context);
  static void SaturationConsoleFunction(ConsoleFunctionContextRef context);

  static std::unique_ptr<Anki::Util::IConsoleFunction> _hueConsoleFunc;
  static std::unique_ptr<Anki::Util::IConsoleFunction> _saturationConsoleFunc;

  void SetEyeArrayHelper(WhichEye eye, const std::vector<Value>& eyeArray);
  void CombineEyeParams(EyeParamArray& eyeArray0, const EyeParamArray& eyeArray1);

  Value Clip(WhichEye eye, Parameter whichParam, Value value) const;

  static ProceduralFace* _resetData;
  static ProceduralFace* _blankFaceData;
  static std::function<void(const char*,Value,Value,Value)> ClipWarnFcn;

  // Console variables managed by this object
  using ConsoleVarPtr = std::unique_ptr<Anki::Util::IConsoleVariable>;
  using ConsoleVarPtrList = std::list<ConsoleVarPtr>;
  ConsoleVarPtrList _consoleVars;

  // Add a console variable to be managed by this object
  template<class T>
  void AddConsoleVar(T & var, const char * name, const char * group, const T & minVal, const T & maxVal);

}; // class ProceduralFace

#pragma mark Inlined Methods

inline void ProceduralFace::SetParameter(WhichEye whichEye, Parameter param, Value value)
{
  _eyeParams[whichEye][static_cast<size_t>(param)] = Clip(whichEye, param, value);
}

inline ProceduralFace::Value ProceduralFace::GetParameter(WhichEye whichEye, Parameter param) const
{
  return _eyeParams[whichEye][static_cast<size_t>(param)];
}

inline const ProceduralFace::EyeParamArray& ProceduralFace::GetParameters(WhichEye whichEye) const
{
  return _eyeParams[whichEye];
}

inline void ProceduralFace::SetParameters(WhichEye eye, const EyeParamArray& params) {
  _eyeParams[eye] = params;
}

inline void ProceduralFace::SetParameterBothEyes(Parameter param, Value value)
{
  SetParameter(WhichEye::Left,  param, value);
  SetParameter(WhichEye::Right, param, value);
}

inline ProceduralFace::Value ProceduralFace::GetFaceAngle() const {
  return _faceAngle_deg;
}

inline void ProceduralFace::SetFaceAngle(Value angle_deg) {
  // TODO: Define face angle limits?
  _faceAngle_deg = angle_deg;
}

inline Point<2,ProceduralFace::Value> const& ProceduralFace::GetFacePosition() const {
  return _faceCenter;
}

inline void ProceduralFace::SetFaceScale(Point<2,Value> scale) {
  if(scale.x() < 0) {
    ClipWarnFcn("FaceScaleX", scale.x(), 0, std::numeric_limits<Value>::max());
    scale.x() = 0;
  }
  if(scale.y() < 0) {
    ClipWarnFcn("FaceScaleY", scale.y(), 0, std::numeric_limits<Value>::max());
    scale.y() = 0;
  }
  _faceScale = scale;
}

inline Point<2,ProceduralFace::Value> const& ProceduralFace::GetFaceScale() const {
  return _faceScale;
}

inline void ProceduralFace::SetScanlineOpacity(Value opacity)
{
#if PROCEDURALFACE_SCANLINE_FEATURE
  _scanlineOpacity = opacity;
  if(!Util::InRange(_scanlineOpacity, Value(0), Value(1)))
  {
    ClipWarnFcn("ScanlineOpacity", _scanlineOpacity, Value(0), Value(1));
    _scanlineOpacity = Util::Clamp(_scanlineOpacity, Value(0), Value(1));
  }
#endif
}

inline ProceduralFace::Value ProceduralFace::GetScanlineOpacity() const {
#if PROCEDURALFACE_SCANLINE_FEATURE
  return _scanlineOpacity;
#else
  return 1.0f;
#endif
}

inline void ProceduralFace::SetHue(Value hue) {
  _hue = hue;
  if(!Util::InRange(_hue, Value(0), Value(1)))
  {
    ClipWarnFcn("Hue", _hue, Value(0), Value(1));
    _hue = Util::Clamp(_hue, Value(0), Value(1));
  }
  // Update the hue image (used for displaying FaceAnimations):
  GetHueImage().FillWith(static_cast<u8>(_hue * std::numeric_limits<u8>::max()));
}

inline ProceduralFace::Value ProceduralFace::GetHue() {
  return _hue;
}

inline void ProceduralFace::ResetHueToDefault() {
  _hue = DefaultHue;
}

inline void ProceduralFace::SetSaturation(Value saturation) {
  _saturation = saturation;
  if(!Util::InRange(_saturation, Value(0), Value(1)))
  {
    ClipWarnFcn("Saturation", _saturation, Value(0), Value(1));
    _saturation = Util::Clamp(_saturation, Value(0), Value(1));
  }
  // Update the saturation image (used for displaying FaceAnimations):
  GetSaturationImage().FillWith(static_cast<u8>(_saturation * std::numeric_limits<u8>::max()));
}

inline ProceduralFace::Value ProceduralFace::GetSaturation() {
  return _saturation;
}

inline Vision::Image& ProceduralFace::GetHueImage() {
  return _hueImage;
}

inline Vision::Image* ProceduralFace::GetHueImagePtr() {
  return &_hueImage;
}

inline Vision::Image& ProceduralFace::GetSaturationImage() {
  return _satImage;
}

inline Vision::Image* ProceduralFace::GetSaturationImagePtr() {
  return &_satImage;
}


} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_ProceduralFace_H__
