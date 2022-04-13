/**
 * File: mirrorModeManager.h
 *
 * Author: Andrew Stein
 * Date:   09/28/2018
 *
 * Description: Handles creating a "MirrorMode" image for displaying the camera feed on the robot's face,
 *              along with various detections in a VisionProcessingResult.
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef __Anki_Vector_MirrorModeManager_H__
#define __Anki_Vector_MirrorModeManager_H__

#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/image.h"
#include "engine/engineTimeStamp.h"

#include <list>

namespace Anki {

// Forward declarations
namespace Vision {
  class ObservedMarker;
  class TrackedFace;
  struct SalientPoint;
}

namespace Vector {

struct VisionProcessingResult;
  
class MirrorModeManager
{
public:
  
  MirrorModeManager();
  
  // Populates the mirrorModeImg field of the VisionProcessingResult with the given image and
  // any detections it can from the same result.
  Result CreateMirrorModeImage(const Vision::ImageRGB& cameraImg, VisionProcessingResult& procResult);
  
private:
  
  Vision::ImageRGB _screenImg;
  std::list<std::pair<EngineTimeStamp_t, Vision::SalientPoint>> _salientPointsToDraw;
  std::array<u8,256> _gammaLUT;
  f32 _currentGamma = 0.f;
  
  void DrawVisionMarkers(const std::list<Vision::ObservedMarker>& visionMarkers);
  void DrawFaces(const std::list<Vision::TrackedFace>& faceDetections);
  void DrawSalientPoints(const VisionProcessingResult& procResult);
  void DrawAutoExposure(const VisionProcessingResult& procResult);
  
};
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Vector_MirrorModeManager_H__ */
