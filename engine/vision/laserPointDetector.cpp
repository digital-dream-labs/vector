/**
 * File: laserPointDetector.cpp
 *
 * Author: Andrew Stein
 * Date:   2017-04-25
 *
 * Description: Vision system component for detecting laser points on the ground.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "laserPointDetector.h"

#include "coretech/common/engine/math/quad.h"
#include "coretech/common/shared/math/rect.h"
#include "coretech/common/engine/math/linearAlgebra.h"

#include "coretech/vision/engine/image.h"
#include "coretech/vision/engine/imageCache.h"

#include "engine/vision/visionSystem.h"
#include "engine/viz/vizManager.h"

#include "util/console/consoleInterface.h"

static const char * const kLogChannelName = "VisionSystem";

namespace Anki {
namespace Vector {

namespace Params
{
# define CONSOLE_GROUP_NAME "Vision.LaserPointDetector"

  // Set > 1 to process at lower resolution for speed
  CONSOLE_VAR_RANGED(s32, kLaser_scaleMultiplier, CONSOLE_GROUP_NAME, 2, 1, 8);

  // NOTE: these are tuned for 320x240 resolution:
  static const Point2f kRadiusAtResolution{320.f, 240.f};
  CONSOLE_VAR(f32, kLaser_minRadius_pix, CONSOLE_GROUP_NAME, 2.f);
  CONSOLE_VAR(f32, kLaser_maxRadius_pix, CONSOLE_GROUP_NAME, 25.f);

  CONSOLE_VAR_RANGED(f32, kLaser_darkThresholdFraction_darkExposure, CONSOLE_GROUP_NAME,  0.7f, 0.f, 1.f);
  CONSOLE_VAR_RANGED(f32, kLaser_darkThresholdFraction_normalExposure, CONSOLE_GROUP_NAME,  0.9f, 0.f, 1.f);

  CONSOLE_VAR(f32, kLaser_darkSurroundRadiusFraction, CONSOLE_GROUP_NAME, 2.5f);

  CONSOLE_VAR(s32, kLaser_MaxSurroundStdDev, CONSOLE_GROUP_NAME, 25);

  CONSOLE_VAR(u8, kLaser_lowThreshold_normalExposure,  CONSOLE_GROUP_NAME, 235);
  CONSOLE_VAR(u8, kLaser_highThreshold_normalExposure, CONSOLE_GROUP_NAME, 240);

  CONSOLE_VAR(u8, kLaser_lowThreshold_darkExposure,    CONSOLE_GROUP_NAME, 128);
  CONSOLE_VAR(u8, kLaser_highThreshold_darkExposure,   CONSOLE_GROUP_NAME, 160);

  // For determining when a laser point is saturated enough in either red or green, when color
  // data is available. Bounding box fraction should be >= 1.0
  CONSOLE_VAR(f32, kLaser_saturationThreshold_red,       CONSOLE_GROUP_NAME, 30.f);
  CONSOLE_VAR(f32, kLaser_saturationThreshold_green,     CONSOLE_GROUP_NAME, 15.f);
  CONSOLE_VAR(f32, kLaser_saturationBoundingBoxFraction, CONSOLE_GROUP_NAME, 1.25f);

  CONSOLE_VAR(bool, kLaser_DrawDetectionsInCameraView, CONSOLE_GROUP_NAME, false);

  // Set to 0 to disable
  // Set to 1 to draw laser point(s) in the camera image
  // Set to 2 to also draw separate debug images showing laser saliency (in image and on ground)
  CONSOLE_VAR(s32, kLaserDetectionDebug, CONSOLE_GROUP_NAME, 0 )

# undef CONSOLE_GROUP_NAME
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
LaserPointDetector::LaserPointDetector(VizManager* vizManager)
: _vizManager(vizManager)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline static u8 GetValue(const u8 p) {
  return p;
}

inline static u8 GetValue(const Vision::PixelRGB& p) {
  return p.max();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class ImageType>
static void ConnCompValidityHelper(const Array2d<s32>& labelImage,
                                   const std::vector<Vision::Image::ConnectedComponentStats>& ccStats,
                                   const ImageType& img,
                                   const u8 highThreshold,
                                   std::vector<bool> &isConnCompValid)
{
  DEV_ASSERT(!img.IsEmpty(), "LaserPointDetector.ConnCompValidityHelper.EmptyImage");

  DEV_ASSERT(labelImage.GetNumRows() == img.GetNumRows() &&
             labelImage.GetNumCols() == img.GetNumCols(),
             "LaserPointDetector.ConnCompValidityHelper.LabelImageSizeMismatch");

  DEV_ASSERT(!isConnCompValid.empty(), "LaserPointDetector.ConnCompValidityHelper.EmptyValidityVector");

  // Skip background label by starting iStat=1
  for(s32 iStat=1; iStat<ccStats.size(); ++iStat)
  {
    const Vision::Image::ConnectedComponentStats& stat = ccStats[iStat];

    // Get ROI around this connected component in the label image and the color/gray image
    Rectangle<s32> bbox(stat.boundingBox);
    Array2d<s32> labelROI  = labelImage.GetROI(bbox);
    const ImageType imgROI = img.GetROI(bbox);

    // Check if any pixel in the connected component is above high threshold
    bool markedValid = false;
    for(s32 i=0; i<labelROI.GetNumRows(); ++i)
    {
      const s32* labelROI_i = labelROI.GetRow(i);
      const auto* imgROI_i = imgROI.GetRow(i);

      for(s32 j=0; j<labelROI.GetNumCols(); ++j)
      {
        const s32 label = labelROI_i[j];

        if((label == iStat) && (GetValue(imgROI_i[j]) > highThreshold) )
        {
          markedValid = true;

          // As soon as any pixel is above high threshold, we can quit
          // looking at this connected component
          break;
        }
      }

      if(markedValid)
      {
        break;
      }
    }

    if(markedValid)
    {
      isConnCompValid[iStat] = true;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result LaserPointDetector::FindConnectedComponents(const Vision::ImageRGB& imgColor,
                                                   const Vision::Image& imgGray,
                                                   const u8 lowThreshold,
                                                   const u8 highThreshold)
{
  DEV_ASSERT(!imgGray.IsEmpty(), "LaserPointDetector.FindConnectedComponents.EmptyGrayImage");

  // Find pixels above the low threshold
  Vision::Image aboveLowThreshImg;

  const bool isColorAvailable = !imgColor.IsEmpty();
  if(isColorAvailable)
  {
    // Make use of color if we have it
    const bool kAnyChannel = true;
    aboveLowThreshImg = imgColor.Threshold(lowThreshold, kAnyChannel);
  }
  else
  {
    // Simple grayscale threshold
    aboveLowThreshImg = imgGray.Threshold(lowThreshold);
  }

  DEV_ASSERT(aboveLowThreshImg.GetNumRows() == imgGray.GetNumRows() &&
             aboveLowThreshImg.GetNumCols() == imgGray.GetNumCols(),
             "LaserPointDetector.FindConnectedComponents.LowThreshImageSizeMismatch");

  // Get connected components of the regions above the low threshold
  Array2d<s32> labelImage;
  std::vector<Vision::Image::ConnectedComponentStats> allConnCompStats;
  size_t numRegions = aboveLowThreshImg.GetConnectedComponents(labelImage, allConnCompStats);

  // Make the min/max area threshold resolution-independent
  const f32 tuningArea = Params::kRadiusAtResolution.x() * Params::kRadiusAtResolution.y();
  const f32 minAreaFraction = (f32)(Params::kLaser_minRadius_pix * Params::kLaser_minRadius_pix * M_PI_F)/tuningArea;
  const f32 maxAreaFraction = (f32)(Params::kLaser_maxRadius_pix * Params::kLaser_maxRadius_pix * M_PI_F)/tuningArea;

  const size_t minArea = minAreaFraction * (f32)imgGray.GetNumElements();
  const size_t maxArea = maxAreaFraction * (f32)imgGray.GetNumElements();

  // If any pixel within a connected component is above the high threshold,
  // mark that connected component as one we want to keep
  std::vector<bool> isConnCompValid(numRegions,false);
  if(isColorAvailable)
  {
    ConnCompValidityHelper(labelImage, allConnCompStats, imgColor, highThreshold, isConnCompValid);
  }
  else
  {
    ConnCompValidityHelper(labelImage, allConnCompStats, imgGray, highThreshold, isConnCompValid);
  }

  // Keep only connected components we selected above that are also within area limits
  // Note: start at iStat=1 because we don't care about the 0th connected component, which is "background"
  _connCompStats.clear();
  for(s32 iStat=1; iStat < allConnCompStats.size(); ++iStat)
  {
    if(isConnCompValid[iStat])
    {
      auto const& stat = allConnCompStats[iStat];
      if(stat.area >= minArea && stat.area <= maxArea)
      {
        _connCompStats.emplace_back(stat);
      }
    }
  }

  if(Params::kLaserDetectionDebug > 1)
  {
    _debugImage.Allocate(aboveLowThreshImg.GetNumRows(), aboveLowThreshImg.GetNumCols());

    // Record only those connected components that we're keeping in the debug iamge
    for(s32 i=0; i<labelImage.GetNumRows(); ++i)
    {
      const s32* labelImage_i = labelImage.GetRow(i);
      Vision::PixelRGB* debugImg_i = _debugImage.GetRow(i);

      for(s32 j=0; j<labelImage.GetNumCols(); ++j)
      {
        const s32 label = labelImage_i[j];
        debugImg_i[j] = (isConnCompValid[label] ? 255 : 0);
      }
    }
  }

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result LaserPointDetector::Detect(Vision::ImageCache&   imageCache,
                                  const VisionPoseData& poseData,
                                  const bool isDarkExposure,
                                  std::list<ExternalInterface::RobotObservedLaserPoint>& points,
                                  Vision::DebugImageList<Vision::CompressedImage>& debugImages)
{
  if(!poseData.groundPlaneVisible)
  {
    // Can't look for laser points unless we can see the ground
    return RESULT_OK;
  }

  Point2f centroid(0.f,0.f);
  Point2f groundPlaneCentroid(0.f,0.f);
  Point2f groundCentroidInImage(0.f, 0.f);

  const Vision::ImageCacheSize scaleSize = Vision::ImageCache::GetSize(Params::kLaser_scaleMultiplier);

  Vision::ImageRGB imageColor;
  if(imageCache.HasColor())
  {
    imageColor = imageCache.GetRGB(scaleSize);
  }

  const Vision::Image& imageGray = imageCache.GetGray(scaleSize);

  // Choose the thresholds based on the exposure
  const u8 lowThreshold  = (isDarkExposure ? Params::kLaser_lowThreshold_darkExposure  : Params::kLaser_lowThreshold_normalExposure );
  const u8 highThreshold = (isDarkExposure ? Params::kLaser_highThreshold_darkExposure : Params::kLaser_highThreshold_normalExposure);

  Result result = FindConnectedComponents(imageColor, imageGray, lowThreshold, highThreshold);

  if(RESULT_OK != result)
  {
    PRINT_NAMED_WARNING("LaserPointDetector.Detect.FindConnectedComponentsFailed", "");
    return result;
  }

  // Get centroid of all the motion within the ground plane, if we have one to reason about
  Quad2f imgQuad;
  poseData.groundPlaneROI.GetImageQuad(poseData.groundPlaneHomography,
                                       imageCache.GetNumCols(Vision::ImageCacheSize::Half),
                                       imageCache.GetNumRows(Vision::ImageCacheSize::Half),
                                       imgQuad);

  imgQuad *= 1.f/(f32)Params::kLaser_scaleMultiplier;

  // Find centroid(s) of saliency inside the ground plane
  const f32 imgQuadArea = imgQuad.ComputeArea();
  f32 groundRegionArea = FindLargestRegionCentroid(imageColor, imageGray, imgQuad, isDarkExposure,
                                                   groundCentroidInImage);

  if(Util::IsNearZero(groundRegionArea))
  {
    // No laser point
    return RESULT_OK;
  }

  // Switch centroid back to original resolution, since that's where the
  // homography information is valid
  groundCentroidInImage *= (f32)Params::kLaser_scaleMultiplier;

  // Map the centroid onto the ground plane, by doing inv(H) * centroid
  Point3f temp;
  Result solveResult = LeastSquares(poseData.groundPlaneHomography,
                                    Point3f{groundCentroidInImage.x(), groundCentroidInImage.y(), 1.f},
                                    temp);
  if(RESULT_OK != solveResult) {
    PRINT_NAMED_WARNING("LaserPointDetector.Detect.LeastSquaresFailed",
                        "Failed to project laser centroid (%.1f,%.1f) to ground plane",
                        groundCentroidInImage.x(), groundCentroidInImage.y());
    // Don't report this centroid
    groundRegionArea = 0.f;
    groundCentroidInImage = 0.f;
  } else if(temp.z() <= 0.f) {
    PRINT_NAMED_WARNING("LaserPointDetector.Detect.BadProjectedZ",
                        "z<=0 (%f) when projecting laser centroid to ground. Bad homography at head angle %.3f deg?",
                        temp.z(), RAD_TO_DEG(poseData.histState.GetHeadAngle_rad()));

    // Don't report this centroid
    groundRegionArea = 0.f;
    groundCentroidInImage = 0.f;
  } else {
    const f32 divisor = 1.f/temp.z();
    groundPlaneCentroid.x() = temp.x() * divisor;
    groundPlaneCentroid.y() = temp.y() * divisor;

    // This is just a sanity check that the centroid is reasonable
    if(ANKI_DEVELOPER_CODE)
    {
      // Scale ground quad slightly to account for numerical inaccuracy.
      // Centroid just needs to be very nearly inside the ground quad.
      Quad2f testQuad(poseData.groundPlaneROI.GetGroundQuad());
      testQuad.Scale(1.01f); // Allow for 1% error
      if(!testQuad.Contains(groundPlaneCentroid)) {
        PRINT_NAMED_WARNING("LaserPointDetector.Detect.BadGroundPlaneCentroid",
                            "Laser Centroid=%s", centroid.ToString().c_str());
      }
    }
  }

  if(Params::kLaserDetectionDebug)
  {
    PRINT_CH_INFO(kLogChannelName, "LaserPointDetector.Detect.FoundCentroid",
                  "Found %.1f-pixel laser point centered at (%.1f,%.1f)",
                  groundRegionArea, groundPlaneCentroid.x(), groundPlaneCentroid.y());
  }

  {
    // Note that we convert area to fraction of image area (to be resolution-independent)
    ExternalInterface::RobotObservedLaserPoint laserPoint(imageGray.GetTimestamp(),
                                                          groundRegionArea / imgQuadArea,
                                                          std::round(groundPlaneCentroid.x()),
                                                          std::round(groundPlaneCentroid.y()));

    points.emplace_back(std::move(laserPoint));
  }

  if(Params::kLaser_DrawDetectionsInCameraView && (nullptr != _vizManager))
  {
    const f32 groundOvalSize = std::max(0.5f, Params::kLaser_scaleMultiplier * std::sqrtf(groundRegionArea / M_PI_F));
    _vizManager->DrawCameraOval(groundCentroidInImage, groundOvalSize, groundOvalSize, NamedColors::GREEN);
  }

  if(Params::kLaserDetectionDebug > 1)
  {
    Vision::ImageRGB saliencyImageFullSize;
    if(Params::kLaser_scaleMultiplier > 1)
    {
      saliencyImageFullSize.Allocate(imageCache.GetNumRows(Vision::ImageCacheSize::Half),
                                     imageCache.GetNumCols(Vision::ImageCacheSize::Half));
      _debugImage.Resize(saliencyImageFullSize, Vision::ResizeMethod::NearestNeighbor);
    }

    _debugImage.DrawCircle(groundCentroidInImage * (1.f / (f32)Params::kLaser_scaleMultiplier), NamedColors::RED, 4);
    char tempText[128];
    //snprintf(tempText, 127, "Area:%.2f X:%d Y:%d", imgRegionArea, salientPoint.img_x, salientPoint.img_y);
    //cv::putText(saliencyImgDisp.get_CvMat_(), std::string(tempText),
    //            cv::Point(0,saliencyImgDisp.GetNumRows()), CV_FONT_NORMAL, .4f, CV_RGB(0,255,0));
    debugImages.emplace_back("LaserSaliencyImage", _debugImage);

    Vision::ImageRGB saliencyImageDispGround(poseData.groundPlaneROI.GetOverheadImage(saliencyImageFullSize,
                                                                                      poseData.groundPlaneHomography));
    if(groundRegionArea > 0.f) {
      Point2f dispCentroid(groundPlaneCentroid.x(), -groundPlaneCentroid.y()); // Negate Y for display
      saliencyImageDispGround.DrawCircle(dispCentroid - poseData.groundPlaneROI.GetOverheadImageOrigin(), NamedColors::RED, 3);
      Quad2f groundQuad(poseData.groundPlaneROI.GetGroundQuad());
      groundQuad -= poseData.groundPlaneROI.GetOverheadImageOrigin();
      saliencyImageDispGround.DrawQuad(groundQuad, NamedColors::YELLOW, 1.f);
      snprintf(tempText, 127, "Area:%.2f%% X:%d Y:%d", groundRegionArea*100.f,
               (s32)std::round(groundPlaneCentroid.x()), (s32)std::round(groundPlaneCentroid.y()));
      saliencyImageDispGround.DrawText({0.f,poseData.groundPlaneROI.GetWidthFar()}, tempText, NamedColors::GREEN, .4f);
    }
    debugImages.emplace_back("LaserSaliencyImageGround", saliencyImageDispGround);
  }

  return RESULT_OK;

} // Detect (with pose)

Result LaserPointDetector::Detect(Vision::ImageCache& imageCache,
                                  const bool isDarkExposure,
                                  std::list<ExternalInterface::RobotObservedLaserPoint>& points,
                                  Vision::DebugImageList<Vision::CompressedImage>& debugImages)
{

  Point2f centroidInImage(0.f, 0.f);

  const Vision::ImageCacheSize scaleSize = Vision::ImageCache::GetSize(Params::kLaser_scaleMultiplier);

  Vision::ImageRGB imageColor;
  if(imageCache.HasColor())
  {
    imageColor = imageCache.GetRGB(scaleSize);
  }

  const Vision::Image& imageGray = imageCache.GetGray(scaleSize);

  // Choose the thresholds based on the exposure
  const u8 lowThreshold  = (isDarkExposure ? Params::kLaser_lowThreshold_darkExposure  : Params::kLaser_lowThreshold_normalExposure );
  const u8 highThreshold = (isDarkExposure ? Params::kLaser_highThreshold_darkExposure : Params::kLaser_highThreshold_normalExposure);

  Result result = FindConnectedComponents(imageColor, imageGray, lowThreshold, highThreshold);

  if(RESULT_OK != result)
  {
    PRINT_NAMED_WARNING("LaserPointDetector.Detect.FindConnectedComponentsFailed", "");
    return result;
  }

  // Find centroid(s) of saliency inside the image
  // Use a whole image quad to search everywhere
  const Quad2f wholeImageQuad(Point2f(0, 0),
                              Point2f(0, imageGray.GetNumRows()),
                              Point2f(imageGray.GetNumCols(), 0),
                              Point2f(imageGray.GetNumCols(), imageGray.GetNumRows()));

  f32 regionArea = FindLargestRegionCentroid(imageColor, imageGray,
                                             wholeImageQuad,
                                             isDarkExposure,
                                             centroidInImage);

  if(Util::IsNearZero(regionArea))
  {
    // No laser point
    return RESULT_OK;
  }

  // Switch centroid back to original resolution, since that's where the
  // homography information is valid
  centroidInImage *= (f32)Params::kLaser_scaleMultiplier;


  if(Params::kLaserDetectionDebug)
  {
    PRINT_CH_INFO(kLogChannelName, "LaserPointDetector.Detect.FoundCentroid",
                  "Found %.1f-pixel laser point centered at (%.1f,%.1f)",
                  regionArea, centroidInImage.x(), centroidInImage.y());
  }

  {
    // Note that we convert area to fraction of image area (to be resolution-independent)
    ExternalInterface::RobotObservedLaserPoint laserPoint(imageGray.GetTimestamp(),
                                                          regionArea / imageGray.GetNumElements(),
                                                          std::round(centroidInImage.x()),
                                                          std::round(centroidInImage.y()));

    points.emplace_back(std::move(laserPoint));
  }

  if(Params::kLaser_DrawDetectionsInCameraView && (nullptr != _vizManager))
  {
    const f32 groundOvalSize = std::max(0.5f, Params::kLaser_scaleMultiplier * std::sqrtf(regionArea / M_PI_F));
    _vizManager->DrawCameraOval(centroidInImage, groundOvalSize, groundOvalSize, NamedColors::GREEN);
  }

  if(Params::kLaserDetectionDebug > 1)
  {
    Vision::ImageRGB saliencyImageFullSize;
    if(Params::kLaser_scaleMultiplier > 1)
    {
      saliencyImageFullSize.Allocate(imageCache.GetNumRows(Vision::ImageCacheSize::Half),
                                     imageCache.GetNumCols(Vision::ImageCacheSize::Half));
      _debugImage.Resize(saliencyImageFullSize, Vision::ResizeMethod::NearestNeighbor);
    }

    _debugImage.DrawCircle(centroidInImage * (1.f / (f32)Params::kLaser_scaleMultiplier), NamedColors::RED, 4);
    debugImages.emplace_back("LaserSaliencyImage", _debugImage);
  }

  return RESULT_OK;

} // Detect (without pose)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
size_t LaserPointDetector::FindLargestRegionCentroid(const Vision::ImageRGB& imgColor,
                                                     const Vision::Image&    imgGray,
                                                     const Quad2f&           groundQuadInImage,
                                                     const bool              isDarkExposure,
                                                     Point2f& centroid)
{
  const bool isColorAvailable = !imgColor.IsEmpty();

  const f32 darkThresholdFraction = (isDarkExposure ?
                                     Params::kLaser_darkThresholdFraction_darkExposure :
                                     Params::kLaser_darkThresholdFraction_normalExposure);

  // Find largest connected component that passes the filter
  size_t largestArea = 0;
  for(auto const& stat : _connCompStats)
  {
    if(stat.area > largestArea &&
       IsOnGroundPlane(groundQuadInImage, stat) &&
       IsSurroundedByDark(imgGray, stat, darkThresholdFraction) &&
       (!isColorAvailable || IsSaturated(imgColor, stat, Params::kLaser_saturationThreshold_red, Params::kLaser_saturationThreshold_green)))
    {
      // All checks passed: keep this as largest
      largestArea = stat.area;
      centroid = stat.centroid;
    }
  }

  return largestArea;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline bool LaserPointDetector::IsOnGroundPlane(const Quad2f& groundQuadInImage,
                                                const Vision::Image::ConnectedComponentStats& stat)
{
  const bool isCentroidContained = groundQuadInImage.Contains(stat.centroid);
  return isCentroidContained;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool LaserPointDetector::IsSurroundedByDark(const Vision::Image& image,
                                            const Vision::Image::ConnectedComponentStats& stat,
                                            const f32 darkThresholdFraction)
{
  const u8 centerPixel = std::round(darkThresholdFraction * (f32)image(std::round(stat.centroid.y()),
                                                                       std::round(stat.centroid.x())));

  const f32 radius = Params::kLaser_darkSurroundRadiusFraction*std::sqrtf((f32)stat.area / M_PI_F);

  // sin/cos of [0 45 90 135 180 225 270 315] degrees. (cos is first, sin is second)
  const s32 kNumSurroundPoints = 8;
  const std::array<std::pair<f32,f32>,kNumSurroundPoints> sinCosPairs{{
    { 1.f, 0.f}, { 0.7071f,  0.7071f}, {0.f, 1.f}, {-0.7071f,  0.7071f},
    {-1.f, 0.f}, {-0.7071f, -0.7071f}, {0.f,-1.f}, { 0.7071f, -0.7071f}
  }};

  s32 surroundSum   = 0;
  s32 surroundSumSq = 0;

  for(auto &sinCosPair : sinCosPairs)
  {
    const s32 x = std::round(stat.centroid.x() + radius*sinCosPair.first);
    const s32 y = std::round(stat.centroid.y() + radius*sinCosPair.second);

    if(x >= 0 && x < image.GetNumCols() && y >= 0 && y < image.GetNumRows())
    {
      if(Params::kLaser_DrawDetectionsInCameraView && (nullptr != _vizManager))
      {
        _vizManager->DrawCameraOval({(f32)x*(f32)Params::kLaser_scaleMultiplier, (f32)y*Params::kLaser_scaleMultiplier},
                                    0.5f, 0.5f, NamedColors::RED);
      }

      // If any surrounding point in the saliency image is _on_ ignore this region (not dot shaped!)
      const u8 pixVal = image(y,x);
      if( pixVal > centerPixel )
      {
        if(Params::kLaserDetectionDebug > 1)
        {
          PRINT_NAMED_WARNING("LaserPointDetector.IsSurroundedByDark", "Not surrounded by dark ring: %d > %d",
                              pixVal, centerPixel);
        }
        return false; // once a single point is off, no reason to continue
      }

      surroundSum += pixVal;
      surroundSumSq += pixVal*pixVal;
    }
  }

  // Are surround points sufficiently similar?
  const s32 surroundMean = surroundSum / kNumSurroundPoints;
  const s32 surroundVar = (surroundSumSq / kNumSurroundPoints) - (surroundMean*surroundMean);
  if(surroundVar > (Params::kLaser_MaxSurroundStdDev*Params::kLaser_MaxSurroundStdDev))
  {
    if(Params::kLaserDetectionDebug > 1)
    {
      PRINT_NAMED_WARNING("LaserPointDetector.IsSurroundedByDark.VarianceTooHigh",
                          "Variance=%d", surroundVar);
    }
    return false;
  }

  // All points passed
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool LaserPointDetector::IsSaturated(const Vision::ImageRGB& image,
                                     const Vision::Image::ConnectedComponentStats& stat,
                                     const f32 redThreshold, const f32 greenThreshold)
{
  DEV_ASSERT(!image.IsEmpty(), "LaserPointDetector.IsSaturated.EmptyColorImage");

  // check if the region is somewhat saturated (i.e. mostly red, green, or blue), to help reduce
  // false positives for bright spots which are uncolored
  Anki::Rectangle<s32> boundingBoxScaled(stat.boundingBox);
  boundingBoxScaled.Scale( Params::kLaser_saturationBoundingBoxFraction );
  Vision::ImageRGB roi = image.GetROI(boundingBoxScaled);

  s32 sumSaturation_red   = 0;
  s32 sumSaturation_green = 0;
  for(s32 i=0; i<roi.GetNumRows(); ++i)
  {
    const Vision::PixelRGB* roi_i = roi.GetRow(i);
    for(s32 j=0; j<roi.GetNumCols(); ++j)
    {
      const Vision::PixelRGB& pixel = roi_i[j];
      sumSaturation_red   += std::max(0, (s32)pixel.r() - std::max((s32)pixel.g(), (s32)pixel.b()));
      sumSaturation_green += std::max(0, (s32)pixel.g() - std::max((s32)pixel.r(), (s32)pixel.b()));
    }
  }

  const f32 avgSaturation_red   = (f32)sumSaturation_red   / (f32)roi.GetNumElements();
  const f32 avgSaturation_green = (f32)sumSaturation_green / (f32)roi.GetNumElements();

  // Debug display
  if(Params::kLaserDetectionDebug && _vizManager)
  {
    _vizManager->DrawCameraText(stat.centroid * Params::kLaser_scaleMultiplier,
                                std::to_string((s32)std::round(avgSaturation_red)) + ":" +
                                std::to_string((s32)std::round(avgSaturation_green)), NamedColors::RED);
  }

  if(avgSaturation_red > redThreshold || avgSaturation_green > greenThreshold)
  {
    return true;
  }
  else
  {
    // Not saturated enough
    if(Params::kLaserDetectionDebug > 1)
    {
      PRINT_NAMED_WARNING("LaserPointDetector.IsSaturated", "Not saturated: R=%1.f G=%.1f",
                          avgSaturation_red, avgSaturation_green);
    }
    return false;
  }
}

} // namespace Vector
} // namespace Anki
