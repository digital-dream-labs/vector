/**
 * File: cameraCalibrator.h
 *
 * Author: Al Chaussee
 * Created: 08/16/17
 *
 * Description:
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Cozmo_Basestation_Vision_CameraCalibrator_H__
#define __Cozmo_Basestation_Vision_CameraCalibrator_H__

#include "coretech/common/shared/math/rect_fwd.h"

#include "coretech/vision/engine/cameraCalibration.h"
#include "coretech/vision/engine/compressedImage.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/image.h"
#include "coretech/vision/engine/visionMarker.h"
#include "coretech/vision/shared/MarkerCodeDefinitions.h"

#include "coretech/common/shared/types.h"

#include <set>

namespace Anki {
namespace Vector {
 
class CameraCalibrator
{
public:
  
  CameraCalibrator();
  ~CameraCalibrator();
  
  // Enum of various supported calibration targets
  enum CalibTargetType {
    CHECKERBOARD, // Dot checkerboard
    INVERTED_BOX, // 3-sided inverted box target with markers
    QBERT,        // Target that looks like a QBert level
  };
  
  // Computes camera calibration using stored images of checkerboard target
  // Outputs calibrations and debugImages via reference and returns whether or not calibration succeeded
  Result ComputeCalibrationFromCheckerboard(std::list<Vision::CameraCalibration>& calibration_out,
                                            Vision::DebugImageList<Vision::CompressedImage>& debugImages_out);
  
  // Computes camera calibration using observed markers on either the INVERTED_BOX or QBERT target
  // Outputs calibrations and debugImages via reference and returns whether or not calibration succeeded
  Result ComputeCalibrationFromSingleTarget(CalibTargetType targetType,
                                            const std::list<Vision::ObservedMarker>& observedMarkers,
                                            std::list<Vision::CameraCalibration>& calibration_out,
                                            Vision::DebugImageList<Vision::CompressedImage>& debugImages_out);
  
  // Add an image to be stored for calibration along with a region of interest
  Result AddCalibrationImage(const Vision::Image& calibImg, const Anki::Rectangle<s32>& targetROI);
  
  // Clears all stored calibration images
  Result ClearCalibrationImages();
  
  // Returns the number of stored calibration images
  size_t GetNumStoredCalibrationImages() const { return _calibImages.size(); }
  
  // Structure to hold information about each calibration image
  struct CalibImage {
    // Input provided by AddCalibrationImage
    Vision::Image  img;
    Rectangle<s32> roiRect;
    
    // Output
    // Whether of not dots were found in the image (dot checkerboard calibration)
    bool           dotsFound;
  };
  
  // Returns a vector of all stored calibration images (may or may not have already been used for calibration)
  const std::vector<CalibImage>& GetCalibrationImages() const {return _calibImages;}
  
  // Returns a vector of Camera poses based on where the camera was when taking each CalibImage
  // Each index matches the corresponding images in _calibImages
  const std::vector<Pose3d>& GetCalibrationPoses() const { return _calibPoses;}

private:

  // Calculates expected corner positions of the CHECKERBOARD target with the given board and square
  // sizes
  void CalcBoardCornerPositions(cv::Size boardSize,
                                float squareSize,
                                std::vector<cv::Point3f>& corners);
  
  // Populates markersTo3dCoords with the 3d world coordinates of each corner of each marker on
  // the respective target
  void GetCalibTargetMarkersTo3dCoords_Qbert(std::map<Vision::MarkerType, Quad3f>& markersTo3dCoords,
                                             std::set<Vision::MarkerType>& markersNeededToBeSeen);
  void GetCalibTargetMarkersTo3dCoords_InvertedBox(std::map<Vision::MarkerType, Quad3f>& markersTo3dCoords,
                                                   std::set<Vision::MarkerType>& markersNeededToBeSeen);

  std::vector<CalibImage> _calibImages;
  std::vector<Pose3d>     _calibPoses;
  bool                    _isCalibrating = false;
};
  
}
}

#endif // __Cozmo_Basestation_Vision_CameraCalibrator_H__
