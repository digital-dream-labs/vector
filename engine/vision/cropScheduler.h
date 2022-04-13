/**
 * File: cropScheduler.h
 *
 * Author: Andrew Stein
 * Date:   09/24/2018
 *
 * Description: Helper class to compute, cache, and cycle through cropping rectangles for marker detection.
 *
 * Copyright: Anki, Inc. 2018
 **/


#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/rect_fwd.h"

#include <vector>

namespace Anki {

// Forward declaration
namespace Vision {
  class Camera;
}
  
namespace Vector {
  
// Forward declaration
struct VisionPoseData;

class CropScheduler
{
public:
  
  CropScheduler(const Vision::Camera& camera) : _camera(camera) { }
  
  f32 GetCropWidthFraction() const { return _widthFraction; }
  
  enum class CyclingMode : u8 {
    Middle_Left_Middle_Right,       // Middle, Left, Middle, Right
    MiddleOnly,                     // Just Middle (static)
    Middle2X_MiddlePlusEachSide,    // Middle 2X, Middle+Left, Middle 2X, Middle+Right
  };
  
  void Reset(const f32 cropWidthFraction, const CyclingMode cyclingMode);
  
  // Moves to next crop rectangle each time called.
  // If useHorizontalCycling=true, use "schedule" of horizontal crops for current CyclingMode. Otherwise, use full width.
  // If useVariableHeight=true, uses poseData to vary height of crop.
  // Note: if both useHorizontalCycling=false and useVariableHeight=false, cropRect will be full (nrow x ncols) image.
  // Returns false if the crop would be empty or out of bounds.
  bool GetCropRect(const s32 nrows, const s32 ncols,
                   const bool useHorizontalCycling,
                   const bool useVariableHeight,
                   const VisionPoseData& poseData,                   
                   Rectangle<s32>& cropRect);
  
private:
  
  enum class CropPosition : u8 {
    Middle,
    Left,
    Right,
    MiddlePlusLeft,
    MiddlePlusRight,
    Full,
  };
  
  const Vision::Camera& _camera;
  f32 _widthFraction = 0.f;
  std::vector<CropPosition> _cropSchedule;
  s32 _cropIndex = 0;
  
  f32 GetCurrentWidthFraction(const CropPosition cropPosition) const;
  s32 GetCurrentCropX(const CropPosition cropPosition, const s32 ncols, const s32 cropWidth) const;
  s32 GetCurrentCropY(const CropPosition cropPosition, const VisionPoseData& poseData,
                      const s32 nrows, const s32 ncols) const;
  
}; // class CropScheduler

} // namespace Vector
} // namespace Anki
