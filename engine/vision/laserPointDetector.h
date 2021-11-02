/**
 * File: laserPointDetector.h
 *
 * Author: Andrew Stein
 * Date:   2017-04-25
 *
 * Description: Vision system component for detecting laser points on the ground.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef __Anki_Cozmo_Basestation_LaserPointDetector_H__
#define __Anki_Cozmo_Basestation_LaserPointDetector_H__

#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/point_fwd.h"

#include "coretech/vision/engine/compressedImage.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/image.h"

#include "clad/externalInterface/messageEngineToGame.h"

#include <list>
#include <string>

namespace Anki {
  
  // Forward declaration:
  namespace Vision {
    class ImageCache;
  }

namespace Vector {
    
// Forward declaration:
struct VisionPoseData;
class VizManager;

class LaserPointDetector
{
public:
  
  LaserPointDetector(VizManager* vizManager);
  
  // If imageInColor is not empty, extra checks are done to verify red/green color saturation.
  // Otherwise, imageInGray is used for detecting potential laser dots.
  // isDarkExposure specifies whether the passed-in images were captured under low-gain, fast-exposure settings.
  Result Detect(Vision::ImageCache&   imageCache,
                const VisionPoseData& poseData,
                const bool isDarkExposure,
                std::list<ExternalInterface::RobotObservedLaserPoint>& points,
                Vision::DebugImageList<Vision::CompressedImage>& debugImages);

  // Same as above, but without the poseData. Searches in the whole image. Used for testing and debug
  Result Detect(Vision::ImageCache&   imageCache,
                const bool isDarkExposure,
                std::list<ExternalInterface::RobotObservedLaserPoint>& points,
                Vision::DebugImageList<Vision::CompressedImage>& debugImages);

private:

  Result FindConnectedComponents(const Vision::ImageRGB& imgColor,
                                 const Vision::Image&    imgGray,
                                 const u8 lowThreshold,
                                 const u8 highThreshold);

  size_t FindLargestRegionCentroid(const Vision::ImageRGB& imgColor,
                                   const Vision::Image&    imgGray,
                                   const Quad2f&           groundQuadInImage,
                                   const bool              isDarkExposure,
                                   Point2f& centroid);

  // TODO: Make static (and pass in vizManager?)
  bool IsOnGroundPlane(const Quad2f& groundQuadInImage, const Vision::Image::ConnectedComponentStats& stat);
  bool IsSurroundedByDark(const Vision::Image& image, const Vision::Image::ConnectedComponentStats& stat,
                          const f32 darkThresholdFraction);
  bool IsSaturated(const Vision::ImageRGB& image, const Vision::Image::ConnectedComponentStats& stat,
                   const f32 redThreshold, const f32 greenThreshold);

  VizManager*   _vizManager    = nullptr;

  std::vector<Vision::Image::ConnectedComponentStats> _connCompStats;

  Vision::ImageRGB _debugImage;


};
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_LaserPointDetector_H__ */

