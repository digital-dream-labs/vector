/**
 * File: motionDetector.cpp
 *
 * Author: Andrew Stein
 * Date:   2017-04-25
 *
 * Description: Vision system component for detecting motion in images and/or on the ground plane.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "engine/vision/motionDetector.h"
#include "engine/vision/motionDetector_neon.h"

#include "coretech/common/engine/math/linearAlgebra.h"
#include "coretech/common/engine/math/quad.h"
#include "coretech/vision/engine/camera.h"
#include "coretech/vision/engine/imageCache.h"
#include "coretech/common/engine/jsonTools.h"
#include "engine/vision/visionPoseData.h"
#include "engine/viz/vizManager.h"
#include "util/console/consoleInterface.h"

#include <opencv2/highgui/highgui.hpp>
#include <iomanip>


namespace Anki {
namespace Vector {

namespace {
# define CONSOLE_GROUP_NAME "Vision.MotionDetection"
  
  // For speed, compute motion detection at lower resolution (1 for full resolution, 2 for half, etc)
  CONSOLE_VAR_RANGED(s32, kMotionDetection_ScaleMultiplier,          CONSOLE_GROUP_NAME, 4, 1, 8);
  
  // How long we have to wait between motion detections. This may be reduce-able, but can't get
  // too small or we'll hallucinate image change (i.e. "motion") due to the robot moving.
  CONSOLE_VAR(u32,  kMotionDetection_LastMotionDelay_ms,  CONSOLE_GROUP_NAME, 500);
  
  // Affects sensitivity (darker pixels are inherently noisier and should be ignored for
  // change detection). Range is [0,255]
  WRAP_EXTERN_CONSOLE_VAR(u8,   kMotionDetection_MinBrightness,       CONSOLE_GROUP_NAME);
  
  // This is the main sensitivity parameter: higher means more image difference is required
  // to register a change and thus report motion.
  WRAP_EXTERN_CONSOLE_VAR(f32,  kMotionDetection_RatioThreshold,      CONSOLE_GROUP_NAME);
  CONSOLE_VAR(f32,  kMotionDetection_MinAreaFraction,     CONSOLE_GROUP_NAME, 1.f/225.f); // 1/15 of each image dimension
  
  // For computing robust "centroid" of motion
  CONSOLE_VAR(f32,  kMotionDetection_CentroidPercentileX,       CONSOLE_GROUP_NAME, 0.5f);  // In image coordinates
  CONSOLE_VAR(f32,  kMotionDetection_CentroidPercentileY,       CONSOLE_GROUP_NAME, 0.5f);  // In image coordinates
  CONSOLE_VAR(f32,  kMotionDetection_GroundCentroidPercentileX, CONSOLE_GROUP_NAME, 0.05f); // In robot coordinates (Most important for pounce: distance from robot)
  CONSOLE_VAR(f32,  kMotionDetection_GroundCentroidPercentileY, CONSOLE_GROUP_NAME, 0.50f); // In robot coordinates
  
  // Tight constraints on max movement allowed to attempt frame differencing for "motion detection"
  CONSOLE_VAR(f32,  kMotionDetection_MaxHeadAngleChange_deg,    CONSOLE_GROUP_NAME, 0.1f);
  CONSOLE_VAR(f32,  kMotionDetection_MaxBodyAngleChange_deg,    CONSOLE_GROUP_NAME, 0.1f);
  CONSOLE_VAR(f32,  kMotionDetection_MaxPoseChange_mm,          CONSOLE_GROUP_NAME, 0.5f);
  
  CONSOLE_VAR(bool, kMotionDetection_DrawGroundDetectionsInCameraView, CONSOLE_GROUP_NAME, false);

  // The smaller this value the more broken up will be the motion areas, leading to fragmented ones.
  // If too big artificially big motion areas can be created.
  CONSOLE_VAR(u32,  kMotionDetection_MorphologicalSize_pix, CONSOLE_GROUP_NAME, 20);

  // The higher this value the less susceptible to noise motion detection will be. A too high value
  // will lead to discarding some motion areas.
  CONSOLE_VAR(u32,  kMotionDetection_MinAreaForMotion_pix, CONSOLE_GROUP_NAME, 500);

  // How much blurring to apply to the camera image before doing motion detection.
  CONSOLE_VAR(u32,  kMotionDetection_BlurFilterSize_pix, CONSOLE_GROUP_NAME, 21);
  
  CONSOLE_VAR(bool, kMotionDetectionDebug, CONSOLE_GROUP_NAME, false);
  
# undef CONSOLE_GROUP_NAME
}
  

// This class is used to accumulate data for peripheral motion detection. The image area is divided in three sections:
// top, right and left. If the centroid of a motion patch falls inside one these areas, it's increased, otherwise it's
// decreased. This follows a very simple impulse/decay model. The parameters *horizontalSize* and *verticalSize* control
// how much of the image is for the left/right sectors and the top/bottom sector. The parameters *increaseFactor* and
// *decreaseFactor* control the impulse response.
//
// The centroid of the motion in the different sectors are also stored here. Every time one of the areas is activated,
// the centroid "moves" towards the new activation following an exponential moving average method.
class MotionDetector::ImageRegionSelector
{
public:
  ImageRegionSelector(int imageWidth, int imageHeight, float horizontalSize, float verticalSize,
                      float increaseFactor, float decreaseFactor, float maxValue,
                      float alpha)
  : _alpha(alpha)
  , _maxValue(maxValue)
  , _leftMargin(imageWidth * horizontalSize)
  , _rightMargin(imageWidth - _leftMargin)
  , _topMargin(imageHeight * verticalSize)
  , _bottomMargin(imageHeight - _topMargin)
  , _topArea(verticalSize * imageHeight * imageWidth)
  , _bottomArea(verticalSize * imageHeight * imageWidth)
  , _leftArea(horizontalSize * imageHeight * imageWidth)
  , _rightArea(horizontalSize * imageHeight * imageWidth)
  , _topID(increaseFactor, decreaseFactor, maxValue)
  , _bottomID(increaseFactor, decreaseFactor, maxValue)
  , _leftID(increaseFactor, decreaseFactor, maxValue)
  , _rightID(increaseFactor, decreaseFactor, maxValue)
  {
    DEV_ASSERT(horizontalSize <= 0.5, "MotionDetector::ImageRegionSelector: horizontal size has to be less then half"
                                      "of the image");
    DEV_ASSERT(verticalSize <= 0.5, "MotionDetector::ImageRegionSelector: vertical size has to be less then half"
                                    "of the image");
  }

  float GetTopResponse() const
  {
    return _topID.Value();
  }
  float GetBottomResponse() const
  {
    return _bottomID.Value();
  }
  float GetLeftResponse() const
  {
    return _leftID.Value();
  }
  float GetRightResponse() const
  {
    return _rightID.Value();
  }

  float GetTopActivationArea() const
  {
    return _topActivatedArea;
  }
  float GetBottomActivationArea() const
  {
    return _bottomActivatedArea;
  }
  float GetLeftActivationArea() const
  {
    return _leftActivatedArea;
  }
  float GetRightActivationArea() const
  {
    return _rightActivatedArea;
  }

  bool IsTopActivated() const {
    return GetTopResponse() >= _maxValue;
  }
  bool IsBottomActivated() const {
    return GetBottomResponse() >= _maxValue;
  }
  bool IsRightActivated() const {
    return GetRightResponse() >= _maxValue;
  }
  bool IsLeftActivated() const {
    return GetLeftResponse() >= _maxValue;
  }

  const Point2f& GetTopResponseCentroid() const {
    return _topCentroid;
  }
  const Point2f& GetBottomResponseCentroid() const {
    return _bottomCentroid;
  }
  const Point2f& GetLeftResponseCentroid() const {
    return _leftCentroid;
  }
  const Point2f& GetRightResponseCentroid() const {
    return _rightCentroid;
  }

  float GetTopMargin() const {
    return _topMargin;
  }
  float GetBottomMargin() const {
    return _bottomMargin;
  }
  float GetLeftMargin() const {
    return _leftMargin;
  }
  float GetRightMargin() const {
    return _rightMargin;
  }

  void Update(const Point2f &point,
              float value)
  {

    const float x = point.x();
    const float y = point.y();

    // Where does the point lie?
    // Either top or bottom
    //top
    if (y <= _topMargin) {
      // The real activation value is a fraction of the relative image area
      const float realValue = value / _topArea;
      _topID.Update(realValue);
      _topCentroid = UpdatePoint(_topCentroid, point);
      _topActivatedArea = std::min(realValue, 1.0f);

      _bottomID.Decay();
      _bottomActivatedArea = 0.0;
      _bottomCentroid = Point2f(-1, -1);
    }
    // bottom
    else if (y >= _bottomMargin) {
      // The real activation value is a fraction of the relative image area
      const float realValue = value / _bottomArea;
      _bottomID.Update(realValue);
      _bottomCentroid = UpdatePoint(_bottomCentroid, point);
      _bottomActivatedArea = std::min(realValue, 1.0f);

      _topID.Decay();
      _topActivatedArea = 0.0;
      _topCentroid = Point2f(-1, -1);
    }
    // they both go down
    else {
      _topID.Decay();
      _bottomID.Decay();
      _topActivatedArea = 0.0;
      _bottomActivatedArea = 0.0;
      _topCentroid = Point2f(-1, -1);
      _bottomCentroid = Point2f(-1, -1);
    }

    //left and right are not in exclusion with top or bottom
    //left
    if (x <= _leftMargin) {
      // The real activation value is a fraction of the relative image area
      const float realValue = value / _leftArea;
      _leftID.Update(realValue);
      _leftCentroid = UpdatePoint(_leftCentroid, point);
      _leftActivatedArea = std::min(realValue, 1.0f);

      _rightID.Decay();
      _rightActivatedArea = 0.0;
      _rightCentroid = Point2f(-1, -1);
    }
    //right
    else if (x >= _rightMargin) {
      // The real activation value is a fraction of the relative image area
      const float realValue = value / _rightArea;
      _rightID.Update(realValue);
      _rightCentroid = UpdatePoint(_rightCentroid, point);
      _rightActivatedArea = std::min(realValue, 1.0f);

      _leftID.Decay();
      _leftActivatedArea = 0.0;
      _leftCentroid = Point2f(-1, -1);
    }
    //they both go down
    else {
      _rightID.Decay();
      _leftID.Decay();
      _rightActivatedArea = 0.0;
      _leftActivatedArea = 0.0;
      _rightCentroid = Point2f(-1, -1);
      _leftCentroid = Point2f(-1, -1);
    }
  }

  void Decay() {
    _topID.Decay();
    _bottomID.Decay();
    _rightID.Decay();
    _leftID.Decay();
    _topActivatedArea = 0.0;
    _bottomActivatedArea = 0.0;
    _rightActivatedArea = 0.0;
    _leftActivatedArea = 0.0;
    _topCentroid = Point2f(-1, -1);
    _bottomCentroid = Point2f(-1, -1);
    _rightCentroid = Point2f(-1, -1);
    _leftCentroid = Point2f(-1, -1);
  }


private:

  // Implements an exponential moving average, a.k.a. low pass filter
  Point2f UpdatePoint(const Point2f &oldPoint, const Point2f &newPoint) const
  {
    if (oldPoint.x() < 0) {
      return newPoint;
    }
    else {
      return newPoint * _alpha  + oldPoint * (1.0 - _alpha);
    }
  }

  class ImpulseDecay
  {
  public:
    ImpulseDecay(float increaseFactor, float decreaseFactor, float maxValue) :
        _increaseFactor(increaseFactor),
        _decreaseFactor(decreaseFactor),
        _maxValue(maxValue)
    {

    }

    float Update(float value = 0)
    {
      _value = _value + _increaseFactor * value - _decreaseFactor;
      _value = Util::Clamp(_value, 0.0f, _maxValue);

      return _value;
    }

    float Decay()
    {
      return Update(0);
    }

    float Value() const {
      return _value;
    }

  private:
    float _increaseFactor;
    float _decreaseFactor;
    float _maxValue;
    float _value = 0;

  };

  const float _alpha = 0.6;
  const float _maxValue;
  const float _leftMargin;
  const float _rightMargin;
  const float _topMargin;
  const float _bottomMargin;
  const float _topArea;     // area of top of the image (up until _topMargin)
  const float _bottomArea;  // area of bottom of the image (down below _topMargin)
  const float _leftArea;    // area of right of the image (up until _rightMargin)
  const float _rightArea;   // area of left of the image (up until _leftMargin)

  ImpulseDecay _topID;
  ImpulseDecay _bottomID;
  ImpulseDecay _leftID;
  ImpulseDecay _rightID;

  Point2f _topCentroid       = Point2f(-1, -1);
  Point2f _bottomCentroid    = Point2f(-1, -1);
  Point2f _leftCentroid      = Point2f(-1, -1);
  Point2f _rightCentroid     = Point2f(-1, -1);
  float _topActivatedArea    = 0;
  float _bottomActivatedArea = 0;
  float _leftActivatedArea   = 0;
  float _rightActivatedArea  = 0;
};

static const char * const kLogChannelName = "VisionSystem";

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MotionDetector::MotionDetector(const Vision::Camera &camera, VizManager *vizManager, const Json::Value &config)
:
  _regionSelector(nullptr) //need image size information before we can build this
, _camera(camera)
, _vizManager(vizManager)
, _config(config)
{
  DEV_ASSERT(kMotionDetection_MinBrightness > 0, "MotionDetector.Constructor.MinBrightnessIsZero");

}

// Need to do this for the pimpl implementation of ImageRegionSelector
MotionDetector::~MotionDetector() = default;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
bool MotionDetector::HavePrevImage<Vision::ImageRGB>() const
{
  return !_prevImageRGB.IsEmpty();
}

template<>
bool MotionDetector::HavePrevImage<Vision::Image>() const
{
  return !_prevImageGray.IsEmpty();
}

void MotionDetector::SetPrevImage(const Vision::Image &image, bool wasBlurred)
{
  image.CopyTo(_prevImageGray);
  _wasPrevImageGrayBlurred = wasBlurred;
  _wasPrevImageRGBBlurred = false;
  _prevImageRGB = Vision::ImageRGB();
}

void MotionDetector::SetPrevImage(const Vision::ImageRGB &image, bool wasBlurred)
{
  image.CopyTo(_prevImageRGB);
  _wasPrevImageRGBBlurred = wasBlurred;
  _wasPrevImageGrayBlurred = false;
  _prevImageGray = Vision::Image();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 MotionDetector::RatioTest(const Vision::ImageRGB& image, Vision::Image& ratioImg)
{
  DEV_ASSERT(ratioImg.GetNumRows() == image.GetNumRows() && ratioImg.GetNumCols() == image.GetNumCols(),
             "MotionDetector.RatioTestColor.MismatchedSize");
  
  u32 numAboveThresh = 0;
  
#ifdef __ARM_NEON__

  return RatioTestNeon(image, ratioImg);

#else

  // somehow auto doens't work here... the right type cannot be deduced. Bug in clang?
  std::function<u8(const Vision::PixelRGB& thisElem, const Vision::PixelRGB& otherElem)> ratioTest =
      [&numAboveThresh](const Vision::PixelRGB& p1, const Vision::PixelRGB& p2)
  {
    u8 retVal = 0;
    if(p1.IsBrighterThan(kMotionDetection_MinBrightness) &&
       p2.IsBrighterThan(kMotionDetection_MinBrightness))
    {
      const f32 ratioR = RatioTestHelper(p1.r(), p2.r());
      const f32 ratioG = RatioTestHelper(p1.g(), p2.g());
      const f32 ratioB = RatioTestHelper(p1.b(), p2.b());
      if(ratioR > kMotionDetection_RatioThreshold || ratioG > kMotionDetection_RatioThreshold || ratioB > kMotionDetection_RatioThreshold) {
        ++numAboveThresh;
        retVal = 255; // use 255 because it will actually display
      }
    } // if both pixels are bright enough

    return retVal;
  };
  
  image.ApplyScalarFunction(ratioTest, _prevImageRGB, ratioImg);

#endif
  
  return numAboveThresh;
}

s32 MotionDetector::RatioTest(const Vision::Image& image, Vision::Image& ratioImg)
{
  DEV_ASSERT(ratioImg.GetNumRows() == image.GetNumRows() && ratioImg.GetNumCols() == image.GetNumCols(),
             "MotionDetector.RatioTestGray.MismatchedSize");
  
  s32 numAboveThresh = 0;

#ifdef __ARM_NEON__

  return RatioTestNeon(image, ratioImg);

#else
  
  std::function<u8(const u8& thisElem, const u8& otherElem)> ratioTest = [&numAboveThresh](const u8& p1, const u8& p2)
  {
    u8 retVal = 0;
    if((p1 > kMotionDetection_MinBrightness) && (p2 > kMotionDetection_MinBrightness))
    {
      const f32 ratio = RatioTestHelper(p1, p2);
      if(ratio > kMotionDetection_RatioThreshold)
      {
        ++numAboveThresh;
        retVal = 255; // use 255 because it will actually display
      }
    } // if both pixels are bright enough
    
    return retVal;
  };
  
  image.ApplyScalarFunction(ratioTest, _prevImageGray, ratioImg);

#endif
  
  return numAboveThresh;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result MotionDetector::Detect(Vision::ImageCache&     imageCache,
                              const VisionPoseData&   crntPoseData,
                              const VisionPoseData&   prevPoseData,
                              std::list<ExternalInterface::RobotObservedMotion>& observedMotions,
                              Vision::DebugImageList<Vision::CompressedImage>& debugImages)
{
  const Vision::ImageCacheSize imageSize = Vision::ImageCache::GetSize(kMotionDetection_ScaleMultiplier);

  // Call the right helper based on image's color
  if(imageCache.HasColor())
  {
    const Vision::ImageRGB& imageColor = imageCache.GetRGB(imageSize);
    return DetectHelper(imageColor,
                        imageCache.GetNumRows(Vision::ImageCacheSize::Half),
                        imageCache.GetNumCols(Vision::ImageCacheSize::Half),
                        kMotionDetection_ScaleMultiplier,
                        crntPoseData, prevPoseData, observedMotions, debugImages);
  }
  else
  {
    const Vision::Image& imageGray = imageCache.GetGray(imageSize);
    return DetectHelper(imageGray,
                        imageCache.GetNumRows(Vision::ImageCacheSize::Half),
                        imageCache.GetNumCols(Vision::ImageCacheSize::Half),
                        kMotionDetection_ScaleMultiplier,
                        crntPoseData, prevPoseData, observedMotions, debugImages);
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class ImageType>
Result MotionDetector::DetectHelper(const ImageType &image,
                                    s32 origNumRows, s32 origNumCols, f32 scaleMultiplier,
                                    const VisionPoseData& crntPoseData,
                                    const VisionPoseData& prevPoseData,
                                    std::list<ExternalInterface::RobotObservedMotion> &observedMotions,
                                    Vision::DebugImageList<Vision::CompressedImage>& debugImages)
{

  // Create the ImageRegionSelector. It has to be done here since the image size is not known before
  if (_regionSelector == nullptr) {

    // Helper macro to try to get the specified field and store it in the given variable
    // and return RESULT_FAIL if that doesn't work
#   define GET_JSON_PARAMETER(__json__, __fieldName__, __variable__) \
    do { \
    if(!JsonTools::GetValueOptional(__json__, __fieldName__, __variable__)) { \
      PRINT_NAMED_ERROR("MotionDetection.DetectHelper.MissingJsonParameter", "%s", __fieldName__); \
      return RESULT_FAIL; \
    }} while(0)

    const Json::Value& detectionConfig = _config["MotionDetector"];
    float kHorizontalSize;
    float kVerticalSize;
    float kIncreaseFactor;
    float kDecreaseFactor;
    float kMaxValue;
    float kCentroidStability;

    GET_JSON_PARAMETER(detectionConfig, "HorizontalSize", kHorizontalSize);
    GET_JSON_PARAMETER(detectionConfig, "VerticalSize", kVerticalSize);
    GET_JSON_PARAMETER(detectionConfig, "IncreaseFactor", kIncreaseFactor);
    GET_JSON_PARAMETER(detectionConfig, "DecreaseFactor", kDecreaseFactor);
    GET_JSON_PARAMETER(detectionConfig, "MaxValue", kMaxValue);
    GET_JSON_PARAMETER(detectionConfig, "CentroidStability", kCentroidStability);

    _regionSelector.reset( new ImageRegionSelector(image.GetNumCols(), image.GetNumRows(),
                                                   kHorizontalSize, kVerticalSize, kIncreaseFactor,
                                                   kDecreaseFactor, kMaxValue, kCentroidStability));
  }


  const bool headSame = crntPoseData.IsHeadAngleSame(prevPoseData, DEG_TO_RAD(kMotionDetection_MaxHeadAngleChange_deg));
  
  const bool poseSame = crntPoseData.IsBodyPoseSame(prevPoseData,
                                                    DEG_TO_RAD(kMotionDetection_MaxBodyAngleChange_deg),
                                                    kMotionDetection_MaxPoseChange_mm);
  
  //PRINT_STREAM_INFO("pose_angle diff = %.1f\n", RAD_TO_DEG(std::abs(_robotState.pose_angle - _prevRobotState.pose_angle)));

  //Often this will be false
  const bool longEnoughSinceLastMotion = ((image.GetTimestamp() - _lastMotionTime) > kMotionDetection_LastMotionDelay_ms);

  if(headSame && poseSame &&
     HavePrevImage<ImageType>() &&
     !crntPoseData.histState.WasCameraMoving() &&
     !crntPoseData.histState.WasPickedUp() &&
     longEnoughSinceLastMotion)
  {
    // Save timestamp and prepare the msg
    _lastMotionTime = image.GetTimestamp();
    ExternalInterface::RobotObservedMotion msg;
    msg.timestamp = image.GetTimestamp();

    // Remove noise here before motion detection
    ImageType blurredImage(image.GetNumRows(), image.GetNumCols());
    FilterImageAndPrevImages<ImageType>(image, blurredImage);

    // Create the ratio test image
    Vision::Image foregroundMotion(blurredImage.GetNumRows(), blurredImage.GetNumCols());
    s32 numAboveThresh = RatioTest(blurredImage, foregroundMotion);

    // Run the peripheral motion detection
    const bool peripheralMotionDetected = DetectPeripheralMotionHelper(foregroundMotion, debugImages, msg,
                                                                       scaleMultiplier);

    const bool groundMotionDetected = DetectGroundAndImageHelper(foregroundMotion, numAboveThresh, origNumRows,
                                                                 origNumCols,
                                                                 scaleMultiplier, crntPoseData, prevPoseData,
                                                                 observedMotions,
                                                                 debugImages, msg);

    if (peripheralMotionDetected || groundMotionDetected) {
      if (kMotionDetectionDebug) {
        PRINT_CH_INFO(kLogChannelName, "MotionDetector.DetectMotion.DetectHelper",
                      "Motion found, sending message");
      }
      observedMotions.emplace_back(std::move(msg));
    }
    
    // Store a blurred copy of the current image for next time (at correct resolution!)
    const bool kBlurHappened = true;
    SetPrevImage(blurredImage, kBlurHappened);
    
  } // if(headSame && poseSame)
  else
  {
    // Store a copy of the current image for next time (at correct resolution!)
    const bool kBlurHappened = false;
    SetPrevImage(image, kBlurHappened);
  }
  
  return RESULT_OK;
  
}

bool MotionDetector::DetectGroundAndImageHelper(Vision::Image &foregroundMotion, int numAboveThresh, s32 origNumRows,
                                                s32 origNumCols, f32 scaleMultiplier,
                                                const VisionPoseData &crntPoseData,
                                                const VisionPoseData &prevPoseData,
                                                std::list<ExternalInterface::RobotObservedMotion> &observedMotions,
                                                Vision::DebugImageList<Vision::CompressedImage> &debugImages,
                                                ExternalInterface::RobotObservedMotion &msg)
{

  Point2f centroid(0.f,0.f);
  Point2f groundPlaneCentroid(0.f,0.f);
  bool motionFound = false;

  // Get overall image centroid
  const size_t minArea = std::round((f32)foregroundMotion.GetNumElements() * kMotionDetection_MinAreaFraction);
  f32 imgRegionArea    = 0.f;
  f32 groundRegionArea = 0.f;
  if(numAboveThresh > minArea) {
    imgRegionArea = GetCentroid(foregroundMotion, centroid, kMotionDetection_CentroidPercentileX,
                                kMotionDetection_CentroidPercentileY);
  }

  // Get centroid of all the motion within the ground plane, if we have one to reason about
  if(crntPoseData.groundPlaneVisible && prevPoseData.groundPlaneVisible)
  {
    ExtractGroundPlaneMotion(origNumRows, origNumCols, scaleMultiplier, crntPoseData,
                             foregroundMotion, centroid, groundPlaneCentroid, groundRegionArea);
  }

  // If there's motion either in the image or in the ground area
  if(imgRegionArea > 0 || groundRegionArea > 0.f)
  {
    motionFound = true;
    if(kMotionDetectionDebug)
    {
      PRINT_CH_INFO(kLogChannelName, "MotionDetector.DetectGroundAndImageHelper.FoundCentroid",
                    "Found motion centroid for %.1f-pixel area region at (%.1f,%.1f) "
                        "-- %.1f%% of ground area at (%.1f,%.1f)",
                    imgRegionArea, centroid.x(), centroid.y(),
                    groundRegionArea*100.f, groundPlaneCentroid.x(), groundPlaneCentroid.y());
    }

    if(kMotionDetection_DrawGroundDetectionsInCameraView && (nullptr != _vizManager))
    {
      const f32 radius = std::max(1.f, sqrtf(groundRegionArea * (f32)foregroundMotion.GetNumElements() / M_PI_F));
      _vizManager->DrawCameraOval(centroid * scaleMultiplier, radius, radius, NamedColors::YELLOW);
    }


    if(imgRegionArea > 0)
    {
      DEV_ASSERT(centroid.x() >= 0.f && centroid.x() <= foregroundMotion.GetNumCols() &&
                 centroid.y() >= 0.f && centroid.y() <= foregroundMotion.GetNumRows(),
                 "MotionDetector.DetectGroundAndImageHelper.CentroidOOB");

      // Using scale multiplier to return the coordinates in original image coordinates
      msg.img_x = int16_t(std::round(centroid.x() * scaleMultiplier));
      msg.img_y = int16_t(std::round(centroid.y() * scaleMultiplier));
      msg.img_area = imgRegionArea / static_cast<f32>(foregroundMotion.GetNumElements());
    } else {
      msg.img_area = 0;
      msg.img_x = 0;
      msg.img_y = 0;
    }

    if(groundRegionArea > 0.f)
    {
      // groundPlaneCentroid had already been scaled by scaleMultiplier before
      msg.ground_x = int16_t(std::round(groundPlaneCentroid.x()));
      msg.ground_y = int16_t(std::round(groundPlaneCentroid.y()));
      msg.ground_area = groundRegionArea;
    } else {
      msg.ground_area = 0;
      msg.ground_x = 0;
      msg.ground_y = 0;
    }

    observedMotions.emplace_back(std::move(msg));

    if(kMotionDetectionDebug)
    {
      char tempText[128];
      Vision::ImageRGB ratioImgDisp(foregroundMotion);
      ratioImgDisp.DrawCircle(centroid + (_camera.GetCalibration()->GetCenter() * (1.f / scaleMultiplier)),
                              NamedColors::RED, 4);
      snprintf(tempText, 127, "Area:%.2f X:%d Y:%d", imgRegionArea, msg.img_x, msg.img_y);
      putText(ratioImgDisp.get_CvMat_(), std::string(tempText),
              cv::Point(0, ratioImgDisp.GetNumRows()), CV_FONT_NORMAL, .4f, CV_RGB(0, 255, 0));
      debugImages.emplace_back("RatioImg", ratioImgDisp);

      //_currentResult.debugImages.push_back({"PrevRatioImg", _prevRatioImg});
      //_currentResult.debugImages.push_back({"ForegroundMotion", foregroundMotion});
      //_currentResult.debugImages.push_back({"AND", cvAND});

      Vision::Image foregroundMotionFullSize(origNumRows, origNumCols);
      foregroundMotion.Resize(foregroundMotionFullSize, Vision::ResizeMethod::NearestNeighbor);
      Vision::ImageRGB ratioImgDispGround(crntPoseData.groundPlaneROI.GetOverheadImage(foregroundMotionFullSize,
                                                                                       crntPoseData.groundPlaneHomography));
      if(groundRegionArea > 0.f) {
        Point2f dispCentroid(groundPlaneCentroid.x(), -groundPlaneCentroid.y()); // Negate Y for display
        ratioImgDispGround.DrawCircle(dispCentroid - crntPoseData.groundPlaneROI.GetOverheadImageOrigin(),
                                      NamedColors::RED, 2);
        snprintf(tempText, 127, "Area:%.2f X:%d Y:%d", groundRegionArea, msg.ground_x, msg.ground_y);
        putText(ratioImgDispGround.get_CvMat_(), std::string(tempText),
                cv::Point(0, crntPoseData.groundPlaneROI.GetWidthFar()), CV_FONT_NORMAL, .4f,
                CV_RGB(0,255,0));
      }
      debugImages.emplace_back("RatioImgGround", ratioImgDispGround);

    }
  }

  return motionFound;
}

void MotionDetector::ExtractGroundPlaneMotion(s32 origNumRows, s32 origNumCols, f32 scaleMultiplier,
                                              const VisionPoseData &crntPoseData,
                                              const Vision::Image &foregroundMotion,
                                              const Point2f &centroid,
                                              Point2f &groundPlaneCentroid, f32 &groundRegionArea) const
{
  Quad2f imgQuad;
  crntPoseData.groundPlaneROI.GetImageQuad(crntPoseData.groundPlaneHomography, origNumCols, origNumRows, imgQuad);

  imgQuad *= 1.f / scaleMultiplier;

  Rectangle<s32> boundingRect(imgQuad);
  Vision::Image groundPlaneForegroundMotion;
  foregroundMotion.GetROI(boundingRect).CopyTo(groundPlaneForegroundMotion);

  // Zero out everything in the ratio image that's not inside the ground plane quad
  imgQuad -= boundingRect.GetTopLeft().CastTo<float>();

  Vision::Image mask(groundPlaneForegroundMotion.GetNumRows(),
                     groundPlaneForegroundMotion.GetNumCols());
  mask.FillWith(0);
  fillConvexPoly(mask.get_CvMat_(), std::vector<cv::Point>{
        imgQuad[Quad::TopLeft].get_CvPoint_(),
        imgQuad[Quad::TopRight].get_CvPoint_(),
        imgQuad[Quad::BottomRight].get_CvPoint_(),
        imgQuad[Quad::BottomLeft].get_CvPoint_(),
      }, 255);

  for(s32 i=0; i<mask.GetNumRows(); ++i) {
    const u8* maskData_i = mask.GetRow(i);
    u8* fgMotionData_i = groundPlaneForegroundMotion.GetRow(i);
    for(s32 j=0; j<mask.GetNumCols(); ++j) {
      if(maskData_i[j] == 0) {
        fgMotionData_i[j] = 0;
      }
    }
  }

  // Find centroid of motion inside the ground plane
  // NOTE!! We swap X and Y for the percentiles because the ground centroid
  //        gets mapped to the ground plane in robot coordinates later, but
  //        small x on the ground corresponds to large y in the *image*, where
  //        the centroid is actually being computed here.
  groundRegionArea = GetCentroid(groundPlaneForegroundMotion,
                                     groundPlaneCentroid,
                                     kMotionDetection_GroundCentroidPercentileY,
                                     (1.f - kMotionDetection_GroundCentroidPercentileX));

  // Move back to image coordinates from ROI coordinates
  groundPlaneCentroid += boundingRect.GetTopLeft().CastTo<float>(); // casting is explicit

  /* Experimental: Try computing moments in an overhead warped view of the ratio image
   groundPlaneRatioImg = _poseData.groundPlaneROI.GetOverheadImage(ratioImg, _poseData.groundPlaneHomography);

   cv::Moments moments = cv::moments(groundPlaneRatioImg.get_CvMat_(), true);
   if(moments.m00 > 0) {
   groundMotionAreaFraction = moments.m00 / static_cast<f32>(groundPlaneRatioImg.GetNumElements());
   groundPlaneCentroid.x() = moments.m10 / moments.m00;
   groundPlaneCentroid.y() = moments.m01 / moments.m00;
   groundPlaneCentroid += _poseData.groundPlaneROI.GetOverheadImageOrigin();

   // TODO: return other moments?
   }
   */

  if(groundRegionArea > 0.f)
  {
    // Switch centroid back to original resolution, since that's where the
    // homography information is valid
    groundPlaneCentroid *= scaleMultiplier;

    // Make ground region area into a fraction of the ground ROI area
    const f32 imgQuadArea = imgQuad.ComputeArea();
    DEV_ASSERT(Util::IsFltGTZero(imgQuadArea), "MotionDetector.Detect.QuadWithZeroArea");
    groundRegionArea /= imgQuadArea;

    // Map the centroid onto the ground plane, by doing inv(H) * centroid
    Point3f homographyMappedPoint; // In homogenous coordinates
    Result solveResult = LeastSquares(crntPoseData.groundPlaneHomography,
                                      Point3f{groundPlaneCentroid.x(), groundPlaneCentroid.y(), 1.f},
                                      homographyMappedPoint);
    if(RESULT_OK != solveResult) {
      PRINT_NAMED_WARNING("MotionDetector.DetectMotion.LeastSquaresFailed",
                          "Failed to project centroid (%.1f,%.1f) to ground plane",
                          groundPlaneCentroid.x(), groundPlaneCentroid.y());
      // Don't report this centroid
      groundRegionArea = 0.f;
      groundPlaneCentroid = 0.f;
    } else if(homographyMappedPoint.z() <= 0.f) {
      PRINT_NAMED_WARNING("MotionDetector.DetectMotion.BadProjectedZ",
                          "z<=0 (%f) when projecting motion centroid to ground. Bad homography at head angle %.3fdeg?",
                          homographyMappedPoint.z(), RAD_TO_DEG(crntPoseData.histState.GetHeadAngle_rad()));
      // Don't report this centroid
      groundRegionArea = 0.f;
      groundPlaneCentroid = 0.f;
    } else {
      const f32 divisor = 1.f/homographyMappedPoint.z();
      groundPlaneCentroid.x() = homographyMappedPoint.x() * divisor;
      groundPlaneCentroid.y() = homographyMappedPoint.y() * divisor;

      // This is just a sanity check that the centroid is reasonable
      if(ANKI_DEVELOPER_CODE)
      {
        // Scale ground quad slightly to account for numerical inaccuracy.
        // Centroid just needs to be very nearly inside the ground quad.
        Quad2f testQuad(crntPoseData.groundPlaneROI.GetGroundQuad());
        testQuad.Scale(1.01f); // Allow for 1% error
        if(!testQuad.Contains(groundPlaneCentroid)) {
          PRINT_NAMED_WARNING("MotionDetector.DetectMotion.BadGroundPlaneCentroid",
                              "Centroid=%s", centroid.ToString().c_str());
        }
      }
    }
  }
}

template<>
inline Vision::Image& MotionDetector::GetPrevImage()
{
  return _prevImageGray;
}

template<>
inline Vision::ImageRGB& MotionDetector::GetPrevImage()
{
  return _prevImageRGB;
}
 
template<>
inline bool MotionDetector::WasPrevImageBlurred<Vision::Image>() const {
  return _wasPrevImageGrayBlurred;
}

template<>
inline bool MotionDetector::WasPrevImageBlurred<Vision::ImageRGB>() const {
  return _wasPrevImageRGBBlurred;
}
  
template <class ImageType>
void MotionDetector::FilterImageAndPrevImages(const ImageType& image, ImageType& blurredImage)
{
  image.BoxFilter(blurredImage, kMotionDetection_BlurFilterSize_pix);

  // If the previous image hadn't been blurred before, do it now
  if(!WasPrevImageBlurred<ImageType>())
  {
    ImageType& prevImage = GetPrevImage<ImageType>();
    prevImage.BoxFilter(prevImage, kMotionDetection_BlurFilterSize_pix);
  }
}

bool MotionDetector::DetectPeripheralMotionHelper(Vision::Image &ratioImage,
                                                  Vision::DebugImageList<Vision::CompressedImage> &debugImages,
                                                  ExternalInterface::RobotObservedMotion &msg, f32 scaleMultiplier)
{

  bool motionDetected = false;
  // The image has several disjoint components, try to join them
  {
    const int kernelSize = int(kMotionDetection_MorphologicalSize_pix / scaleMultiplier);
    cv::Mat structuringElement = cv::getStructuringElement(cv::MORPH_RECT,
                                                           cv::Size(kernelSize, kernelSize));
    cv::Mat &cvRatioImage = ratioImage.get_CvMat_();
    // TODO morphologyEx might be slow. See VIC-1026
    cv::morphologyEx(cvRatioImage, cvRatioImage, cv::MORPH_CLOSE, structuringElement);
  }

  // Get the connected components with stats
  Array2d<s32> labelImage;
  std::vector<Vision::Image::ConnectedComponentStats> stats;
  ratioImage.GetConnectedComponents(labelImage, stats);

  // Update the impulse/decay model
  // The update is done per connected component, which means that several areas might activate at once
  bool updated = false;
  for (uint i = 1; i < stats.size(); ++i) { //skip stats[0] since that's background
    const auto& stat = stats[i];
    const f32 scaledArea = stat.area * scaleMultiplier;
    if (scaledArea < kMotionDetection_MinAreaForMotion_pix) { //too small
      continue;
    }
    updated = true;
    _regionSelector->Update(stat.centroid, stat.area);
    PRINT_CH_DEBUG(kLogChannelName, "MotionDetector.DetectPeripheralMotionHelper.MotionDetected",
                   "Motion detected with an area of %d (scaled %f)", int(stat.area), scaledArea);
  }

  // No movement = global decay
  if (! updated) {
    _regionSelector->Decay();
    PRINT_CH_DEBUG(kLogChannelName, "MotionDetector.DetectPeripheralMotionHelper.NoMotionDetected","");
  }

  // Filling the message
  {
    // top
    if (_regionSelector->IsTopActivated())
    {
      const float value = _regionSelector->GetTopActivationArea();
      DEV_ASSERT_MSG(value > 0, "MotionDetector::DetectPeripheralMotionHelper.WrongTopValue",
                     "Error: top is activated but the activation area is: %f", value);
      const Point2f& centroid = _regionSelector->GetTopResponseCentroid();
      msg.top_img_area = value;
      msg.top_img_x = int16_t(std::round(centroid.x() * scaleMultiplier));
      msg.top_img_y = int16_t(std::round(centroid.y() * scaleMultiplier));
      motionDetected = true;
    }
    else
    {
      msg.top_img_area = 0;
      msg.top_img_x = 0;
      msg.top_img_y = 0;
    }

    // Either area could be activated here, even all 4 of them at the same time

    // bottom
    if (_regionSelector->IsBottomActivated())
    {
      const float value = _regionSelector->GetBottomActivationArea();
      DEV_ASSERT_MSG(value > 0, "MotionDetector::DetectPeripheralMotionHelper.WrongBottomValue",
                     "Error: bottom is activated but the activation area is: %f", value);
      const Point2f& centroid = _regionSelector->GetBottomResponseCentroid();
      msg.bottom_img_area = value;
      msg.bottom_img_x = int16_t(std::round(centroid.x() * scaleMultiplier));
      msg.bottom_img_y = int16_t(std::round(centroid.y() * scaleMultiplier));
      motionDetected = true;
    }
    else
    {
      msg.bottom_img_area = 0;
      msg.bottom_img_x = 0;
      msg.bottom_img_y = 0;
    }

    // left
    if (_regionSelector->IsLeftActivated())
    {
      const float value = _regionSelector->GetLeftActivationArea();
      DEV_ASSERT_MSG(value > 0, "MotionDetector::DetectPeripheralMotionHelper.WrongLeftValue",
                     "Error: left is activated but the activation area is: %f", value);
      const Point2f& centroid = _regionSelector->GetLeftResponseCentroid();
      msg.left_img_area = value;
      msg.left_img_x = int16_t(std::round(centroid.x() * scaleMultiplier));
      msg.left_img_y = int16_t(std::round(centroid.y() * scaleMultiplier));
      motionDetected = true;
    }
    else
    {
      msg.left_img_area = 0;
      msg.left_img_x = 0;
      msg.left_img_y = 0;
    }

    // right
    if (_regionSelector->IsRightActivated())
    {
      const float value = _regionSelector->GetRightActivationArea();
      DEV_ASSERT_MSG(value > 0, "MotionDetector::DetectPeripheralMotionHelper.WrongRightValue",
                     "Error: right is activated but the activation area is: %f", value);
      const Point2f& centroid = _regionSelector->GetRightResponseCentroid();
      msg.right_img_area = value;
      msg.right_img_x = int16_t(std::round(centroid.x() * scaleMultiplier));
      msg.right_img_y = int16_t(std::round(centroid.y() * scaleMultiplier));
      motionDetected = true;
    }
    else
    {
      msg.right_img_area = 0;
      msg.right_img_x = 0;
      msg.right_img_y = 0;
    }
  }

  if (kMotionDetectionDebug) {
    Vision::ImageRGB imageToDisplay(ratioImage);
    // Draw the text
    {
      auto to_string_with_precision = [] (const float a_value, const int n = 6) {
        std::ostringstream out;
        out << std::setprecision(n) << a_value;
        return out.str();
      };

      const float scale = 0.5;
      if (_regionSelector->IsTopActivated())
      {
        const float value = msg.top_img_area;
        const std::string& text = to_string_with_precision(value, 3);
        const Point2f origin(imageToDisplay.GetNumCols()/2 - 10, 30);
        imageToDisplay.DrawText(origin, text, Anki::NamedColors::RED, scale);
      }
      if (_regionSelector->IsBottomActivated())
      {
        const float value = msg.bottom_img_area;
        const std::string& text = to_string_with_precision(value, 3);
        const Point2f origin(imageToDisplay.GetNumCols()/2 - 10, imageToDisplay.GetNumRows() - 30);
        imageToDisplay.DrawText(origin, text, Anki::NamedColors::BLUE, scale);
      }
      if (_regionSelector->IsLeftActivated())
      {
        const float value = msg.left_img_area;
        const std::string& text = to_string_with_precision(value, 3);
        const Point2f origin(10, imageToDisplay.GetNumRows()/2);
        imageToDisplay.DrawText(origin, text, Anki::NamedColors::YELLOW, scale);
      }
      if (_regionSelector->IsRightActivated())
      {
        const float value = msg.right_img_area;
        const std::string& text = to_string_with_precision(value, 3);
        const Point2f origin(imageToDisplay.GetNumCols()-50, imageToDisplay.GetNumRows()/2 );
        imageToDisplay.DrawText(origin, text, Anki::NamedColors::GREEN, scale);
      }
    }

    // Draw the bounding lines
    { // Top line
      const int thickness = 1;
      const Point2f topLeft(0, _regionSelector->GetTopMargin());
      const Point2f topRight(imageToDisplay.GetNumCols(), _regionSelector->GetTopMargin());
      imageToDisplay.DrawLine(topLeft, topRight, Anki::NamedColors::RED, thickness);
    }
    { // Bottom line
      const int thickness = 1;
      const Point2f topLeft(0, _regionSelector->GetBottomMargin());
      const Point2f topRight(imageToDisplay.GetNumCols(), _regionSelector->GetBottomMargin());
      imageToDisplay.DrawLine(topLeft, topRight, Anki::NamedColors::RED, thickness);
    }
    { // Left line
      const int thickness = 1;
      const Point2f topLeft(_regionSelector->GetLeftMargin(), 0);
      const Point2f bottomLeft(_regionSelector->GetLeftMargin(), imageToDisplay.GetNumRows());
      imageToDisplay.DrawLine(topLeft, bottomLeft, Anki::NamedColors::RED, thickness);
    }
    { // Right line
      const int thickness = 1;
      const Point2f topRight(_regionSelector->GetRightMargin(), 0);
      const Point2f BottomRight(_regionSelector->GetRightMargin(), imageToDisplay.GetNumCols());
      imageToDisplay.DrawLine(topRight, BottomRight, Anki::NamedColors::RED, thickness);
    }

    //Draw the motion centroids -- scaled back to the working resolution
    {
      if (_regionSelector->IsTopActivated())
      {
        const Point2f centroid(msg.top_img_x / scaleMultiplier, msg.top_img_y / scaleMultiplier);
        imageToDisplay.DrawFilledCircle(centroid, Anki::NamedColors::RED, 10);
      }
      if (_regionSelector->IsBottomActivated())
      {
        const Point2f centroid(msg.bottom_img_x / scaleMultiplier, msg.bottom_img_y / scaleMultiplier);
        imageToDisplay.DrawFilledCircle(centroid, Anki::NamedColors::BLUE, 10);
      }
      if (_regionSelector->IsLeftActivated())
      {
        const Point2f centroid(msg.left_img_x / scaleMultiplier, msg.left_img_y / scaleMultiplier);
        imageToDisplay.DrawFilledCircle(centroid, Anki::NamedColors::YELLOW, 10);
      }
      if (_regionSelector->IsRightActivated())
      {
        const Point2f centroid(msg.right_img_x / scaleMultiplier, msg.right_img_y / scaleMultiplier);
        imageToDisplay.DrawFilledCircle(centroid, Anki::NamedColors::GREEN, 10);
      }
    }
    debugImages.emplace_back("PeripheralMotion", imageToDisplay);
  }

  return motionDetected;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Explicit instantiation of Detect() method for Gray and RGB images
template Result MotionDetector::DetectHelper(const Vision::Image&, s32, s32, f32,
                                             const VisionPoseData&,
                                             const VisionPoseData&,
                                             std::list<ExternalInterface::RobotObservedMotion>&,
                                             Vision::DebugImageList<Vision::CompressedImage>&);

template Result MotionDetector::DetectHelper(const Vision::ImageRGB&, s32, s32, f32,
                                             const VisionPoseData&,
                                             const VisionPoseData&,
                                             std::list<ExternalInterface::RobotObservedMotion>&,
                                             Vision::DebugImageList<Vision::CompressedImage>&);
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Computes "centroid" at specified percentiles in X and Y
size_t MotionDetector::GetCentroid(const Vision::Image& motionImg, Point2f& centroid, f32 xPercentile, f32 yPercentile)
{
  std::vector<s32> xValues, yValues;
  
  for(s32 y=0; y<motionImg.GetNumRows(); ++y)
  {
    const u8* motionData_y = motionImg.GetRow(y);
    for(s32 x=0; x<motionImg.GetNumCols(); ++x) {
      if(motionData_y[x] != 0) {
        xValues.push_back(x);
        yValues.push_back(y);
      }
    }
  }
  
  DEV_ASSERT(xValues.size() == yValues.size(), "MotionDetector.GetCentroid.xyValuesSizeMismatch");
  
  if(xValues.empty()) {
    centroid = 0.f;
    return 0;
  } else {
    DEV_ASSERT(xPercentile >= 0.f && xPercentile <= 1.f, "MotionDetector.GetCentroid.xPercentileOOR");
    DEV_ASSERT(yPercentile >= 0.f && yPercentile <= 1.f, "MotionDetector.GetCentroid.yPercentileOOR");
    const size_t area = xValues.size(); // NOTE: area > 0 if we get here
    auto xcen = xValues.begin() + std::round(xPercentile * (f32)(area-1));
    auto ycen = yValues.begin() + std::round(yPercentile * (f32)(area-1));
    std::nth_element(xValues.begin(), xcen, xValues.end());
    std::nth_element(yValues.begin(), ycen, yValues.end());
    centroid.x() = *xcen;
    centroid.y() = *ycen;
    DEV_ASSERT_MSG(centroid.x() >= 0.f && centroid.x() < motionImg.GetNumCols(),
                   "MotionDetector.GetCentroid.xCenOOR",
                   "xcen=%f, not in [0,%d)", centroid.x(), motionImg.GetNumCols());
    DEV_ASSERT_MSG(centroid.y() >= 0.f && centroid.y() < motionImg.GetNumRows(),
                   "MotionDetector.GetCentroid.yCenOOR",
                   "ycen=%f, not in [0,%d)", centroid.y(), motionImg.GetNumRows());
    return area;
  }
  
}
// GetCentroid()

} // namespace Vector
} // namespace Anki

