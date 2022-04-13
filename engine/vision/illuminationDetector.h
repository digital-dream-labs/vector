/**
 * File: illuminationDetector.h
 * 
 * Author: Humphrey Hu
 * Date:   2018-05-25
 * 
 * Description: Vision system component for detecting scene illumination state/changes.
 * 
 * Copyright: Anki, Inc. 2018
 **/

#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/matrix_fwd.h"
#include "coretech/vision/engine/image.h"
#include "clad/externalInterface/messageEngineToGame.h"

#include "json/json-forwards.h"

#include <deque>

#ifndef __Anki_Victor_IlluminationDetector_H__
#define __Anki_Victor_IlluminationDetector_H__

namespace Anki {

// Forward declaration
class LinearClassifier;

// Forward declarations
namespace Vision {

class ImageCache;
class VisionPoseData;

}

namespace Vector {

// Forward declarations
class CozmoContext;
struct VisionPoseData;
class CozmoFeatureGate;

/** 
 * Class for detecting the scene illumination state
 * 
 * Manages a linear classifier and image feature computation. Features are currently
 * multiple intensity percentiles from consecutive images, concatenated together.
 * 
 * Note that detection does not run if the robot is moving or picked up. In these cases
 * the detector will output 'Unknown' illumination state.
 */ 
class IlluminationDetector
{
public:

  // Create an uninitialized detector
  IlluminationDetector();

  // Initialize from JSON config
  Result Init( const Json::Value& config, const CozmoContext* context );

  // Perform illumination detection if the robot is not moving
  Result Detect( Vision::ImageCache& cache, 
                 const VisionPoseData& poseData,
                 ExternalInterface::RobotObservedIllumination& illumination );

private:

  s32 _featPercSubsample;            // Subsample rate for percentile computation
  std::set<f32> _featPercentiles;    // Percentiles to compute for features
  u32 _featWindowLength;             // Number of sequential timepoints to use for features
  
  CozmoFeatureGate* _featureGate;
  std::unique_ptr<LinearClassifier> _classifier;
  std::deque<f32> _featureBuffer;
  f32 _illumMinProb;
  f32 _darkMaxProb;
  bool _allowMovement;

  // Checks for movement, returns whether detection can happen or not
  bool CanRunDetection( const VisionPoseData& poseData ) const;

  // Computes image features and pushes them to the head of the feature buffer
  void GenerateFeatures( Vision::ImageCache& cache );
};

} // end namespace Vector
} // end namespace Anki

#endif //__Anki_Victor_IlluminationDetector_H__
