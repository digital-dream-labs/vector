/**
 * File: detectOverHeadEdges.h
 *
 * Author: Lorenzo Riano
 * Date:   2017-08-31
 *
 * Description: Vision system component for detecting edges in the ground plane.
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef __Anki_Cozmo_Basestation_DetectOverheadEdges_H__
#define __Anki_Cozmo_Basestation_DetectOverheadEdges_H__

#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/image.h"
#include "engine/vision/visionSystem.h"

namespace Anki {

// Forward declaration
namespace Vision {
class Camera;
class ImageCache;
class Profiler;
}

namespace Vector {
// Forward declaration:
struct VisionPoseData;
class VizManager;

class OverheadEdgesDetector {

public:
  OverheadEdgesDetector(const Vision::Camera &camera,
                        VizManager *vizManager,
                        Vision::Profiler &profiler,
                        f32 edgeThreshold = 50.0f,
                        u32 minChainLength = 3);

  Result Detect(Vision::ImageCache &imageCache, const VisionPoseData &crntPoseData,
                VisionProcessingResult &currentResult);

private:
  template<typename ImageTraitType>
  Result DetectHelper(const typename ImageTraitType::ImageType &image,
                      const VisionPoseData &crntPoseData,
                      VisionProcessingResult &currentResult);

  const Vision::Camera&   _camera;
  VizManager*             _vizManager = nullptr;
  Vision::Profiler&       _profiler;
  const f32               _kEdgeThreshold = 50.0f;
  const u32               _kMinChainLength = 3;

};

}

}

#endif
