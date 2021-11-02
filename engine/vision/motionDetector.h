/**
 * File: motionDetector.h
 *
 * Author: Andrew Stein
 * Date:   2017-04-25
 *
 * Description: Vision system component for detecting motion in images and/or on the ground plane.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef __Anki_Cozmo_Basestation_MotionDetector_H__
#define __Anki_Cozmo_Basestation_MotionDetector_H__

#include "coretech/common/engine/robotTimeStamp.h"

#include "coretech/vision/engine/compressedImage.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/image.h"

#include "clad/externalInterface/messageEngineToGame.h"

#include <list>
#include <string>

namespace Anki {

// Forward declaration
namespace Vision {
class Camera;

class ImageCache;
}

namespace Vector {

// Forward declaration:
struct VisionPoseData;

class VizManager;

namespace {
  // Declared static here and wrapped as an extern console
  // inorder to allow motionDetector_neon.h to have access
  static u8  kMotionDetection_MinBrightness  = 10;
  static f32 kMotionDetection_RatioThreshold = 1.25f;

  static inline f32 RatioTestHelper(u8 value1, u8 value2)
  {
    // NOTE: not checking for divide-by-zero here because kMotionDetection_MinBrightness (DEV_ASSERTed to be > 0 in
    //  the constructor) prevents values of zero from getting to this helper
    if(value1 > value2) {
      return static_cast<f32>(value1) / std::max(1.f, static_cast<f32>(value2));
    } else {
      return static_cast<f32>(value2) / std::max(1.f, static_cast<f32>(value1));
    }
  }
}

// Class for detecting motion in various areas of the image.
// There's two main components: one that detects motion on the ground plane, and one that detects motion in the
// peripheral areas (top, left and right).
class MotionDetector
{
public:
  
  MotionDetector(const Vision::Camera &camera, VizManager *vizManager, const Json::Value &config);

  // Will use Color data if available in ImageCache, otherwise grayscale only
  Result Detect(Vision::ImageCache& imageCache,
                const VisionPoseData& crntPoseData,
                const VisionPoseData& prevPoseData,
                std::list<ExternalInterface::RobotObservedMotion>& observedMotions,
                Vision::DebugImageList<Vision::CompressedImage>& debugImages);

  ~MotionDetector();

private:

  template<class ImageType>
  Result DetectHelper(const ImageType &resizedImage,
                      s32 origNumRows, s32 origNumCols, f32 scaleMultiplier,
                      const VisionPoseData &crntPoseData,
                      const VisionPoseData &prevPoseData,
                      std::list<ExternalInterface::RobotObservedMotion> &observedMotions,
                      Vision::DebugImageList<Vision::CompressedImage> &debugImages);

  // To detect peripheral motion, a simple impulse-decay model is used. The longer motion is detected in a
  // specific area, the higher its activation will be. When it reaches a max value motion is activated in
  // that specific area.
  bool DetectPeripheralMotionHelper(Vision::Image &ratioImage,
                                    Vision::DebugImageList<Vision::CompressedImage> &debugImages,
                                    ExternalInterface::RobotObservedMotion &msg, f32 scaleMultiplier);

  bool DetectGroundAndImageHelper(Vision::Image &foregroundMotion, int numAboveThresh, s32 origNumRows,
                                  s32 origNumCols, f32 scaleMultiplier,
                                  const VisionPoseData &crntPoseData,
                                  const VisionPoseData &prevPoseData,
                                  std::list<ExternalInterface::RobotObservedMotion> &observedMotions,
                                  Vision::DebugImageList<Vision::CompressedImage> &debugImages,
                                  ExternalInterface::RobotObservedMotion &msg);

  template <class ImageType>
  void FilterImageAndPrevImages(const ImageType& image, ImageType& blurredImage);

  void ExtractGroundPlaneMotion(s32 origNumRows, s32 origNumCols, f32 scaleMultiplier,
                                const VisionPoseData &crntPoseData,
                                const Vision::Image &foregroundMotion,
                                const Point2f &centroid,
                                Point2f &groundPlaneCentroid,
                                f32 &groundRegionArea) const;

  template<class ImageType>
  s32 RatioTestNeon(const ImageType& image, Vision::Image& ratioImg);

  // Returns the number of times the ratio between the pixels in image and the pixels
  // in the previous image is above a threshold. The corresponding pixels in ratio12
  // will be set to 255
  s32 RatioTest(const Vision::Image& image,    Vision::Image& ratio12);
  s32 RatioTest(const Vision::ImageRGB& image, Vision::Image& ratio12);
  
  void SetPrevImage(const Vision::Image &image, bool wasBlurred);
  void SetPrevImage(const Vision::ImageRGB &image, bool wasBlurred);
  
  template<class ImageType>
  bool HavePrevImage() const;
  
  template<class ImageType>
  ImageType& GetPrevImage();
  
  template<class ImageType>
  bool WasPrevImageBlurred() const;
  
  // Computes "centroid" at specified percentiles in X and Y
  static size_t GetCentroid(const Vision::Image& motionImg,
                            Anki::Point2f& centroid,
                            f32 xPercentile, f32 yPercentile);

  template<class ImageType>
  inline s32 RatioTestNeonHelper(const u8*& imagePtr,
                                 const u8*& prevImagePtr,
                                 u8*& ratioImgPtr,
                                 u32 numElementsToProcess);

  // The joy of pimpl :)
  class ImageRegionSelector;
  std::unique_ptr<ImageRegionSelector> _regionSelector;

  const Vision::Camera& _camera;

  Vision::ImageRGB _prevImageRGB;
  Vision::Image    _prevImageGray;
  bool _wasPrevImageRGBBlurred = false;
  bool _wasPrevImageGrayBlurred = false;
  
  RobotTimeStamp_t   _lastMotionTime = 0;
  
  VizManager*   _vizManager = nullptr;

  const Json::Value& _config;
};

} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_MotionDetector_H__ */

