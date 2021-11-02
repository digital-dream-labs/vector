/**
 * File: overHeadMap.h
 *
 * Author: Lorenzo Riano
 * Created: 10/9/17
 *
 * Description: This class maintains an overhead map of the robot's surrounding. The map has images taken
 * from the ground plane in front of the robot.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_OverheadMap_H__
#define __Anki_Cozmo_OverheadMap_H__

#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/compressedImage.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/image.h"
#include "engine/vision/visionPoseData.h"

#include <unordered_set>

namespace Anki {
namespace Vector {

class CozmoContext;

/*
 * This class maintains an overhead map of the robot's surrounding. The map has images taken
 * from the ground plane in front of the robot. Every time the robot covers a distance the map
 * is updated/overwritten. The area underneath the robot can be extracted using GetImageCenteredOnRobot()
 */
class OverheadMap
{
public:
  OverheadMap(int numrows, int numcols, const CozmoContext *context);
  OverheadMap(const Json::Value& config, const CozmoContext *context);

  Result Update(const Vision::ImageRGB& image, const VisionPoseData& poseData,
                Vision::DebugImageList<Vision::CompressedImage>& debugImages);

  const Vision::ImageRGB& GetOverheadMap() const;
  const Vision::Image& GetFootprintMask() const;

  // Save the generated overhead map plus a list of drivable and non drivable pixels.
  // Used for offline training and testing
  void SaveMaskedOverheadPixels(const std::string& positiveExamplesFilename,
                                const std::string& negativeExamplesFileName,
                                const std::string& overheadMapFileName) const;

  // helper structs
  struct pixelHash {
    constexpr s32 operator()(const Vision::PixelRGB& p) const {
      return s32(p.r() << 16) | s32(p.g() << 8) | s32(p.b());
    };
  };
  using PixelSet = std::unordered_set<Vision::PixelRGB, pixelHash>;
  // Returns a pair of:
  //  * all the pixels in the overhead map that are non-black in the footprint mask, i.e. areas that the robot traversed
  //  * all the pixels in the overhead map that the robot mapped but didn't traverse, making them potential obstacles
  // Since many pixels might be duplicates, sets are used here
  void GetDrivableNonDrivablePixels(PixelSet& drivablePixels,
                                    PixelSet& nonDrivablePixels) const;
  // This function is like above, but uses generic containers rather than sets.
  // It's less efficient since data is copied. Useful for pre-allocated vectors and array.
  // Duplicate pixels are still removed
  template<class InputArray>
  void GetDrivableNonDrivablePixels(InputArray& drivablePixels,
                                    InputArray& nonDrivablePixels) const {
    PixelSet drivableSet;
    PixelSet nonDrivableSet;

    GetDrivableNonDrivablePixels(drivableSet, nonDrivableSet);
    std::copy(drivableSet.begin(), drivableSet.end(), std::begin(drivablePixels));
    std::copy(nonDrivableSet.begin(), nonDrivableSet.end(), std::begin(nonDrivablePixels));
  }

private:

  // Set the overhead map to be all black
  void ResetMaps();
  // Paint all the _footprintMask pixels underneath the robot footprint with white
  void UpdateFootprintMask(const Pose3d& robotPose, Vision::DebugImageList<Vision::CompressedImage>& debugImages);

  // TODO in the future these could be sparse matrices for optimized iteration over non-black pixels
  Vision::ImageRGB _overheadMap;
  Vision::Image _footprintMask; // this image has 0 (black) if the robot has never been to that position, 1 otherwise

  const CozmoContext*  _context;

  // Calculate the footprint as a cv::RotatedRect. To get the correct size the footprint is aligned
  // with the axis
  cv::RotatedRect GetFootprintRotatedRect(const Pose3d& robotPose) const;

  Vision::ImageRGB GetImageCenteredOnRobot(const Pose3d& robotPose,
                                           Vision::DebugImageList<Vision::CompressedImage>& debugImages) const;
};

} // namespace Vector
} // namespace Anki


#endif //__Anki_Cozmo_OverheadMap_H__
