/**
 * File: cropScheduler.cpp
 *
 * Author: Andrew Stein
 * Date:   09/24/2018
 *
 * Description: Helper class to compute, cache, and cycle through cropping rectangles for marker detection.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "anki/cozmo/shared/cozmoConfig.h"

#include "coretech/common/shared/math/matrix.h"
#include "coretech/vision/engine/camera.h"
#include "coretech/vision/engine/undistorter.h"

#include "engine/charger.h"
#include "engine/vision/cropScheduler.h"
#include "engine/vision/visionPoseData.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/math/math.h"

namespace Anki {
namespace Vector {

#define LOG_CHANNEL "VisionSystem"
#define VERBOSE_DEBUG 0
  
namespace {
  CONSOLE_VAR_RANGED(f32,  kCropScheduler_MaxMarkerDetectionDist_mm, "Vision.CropScheduler", 500.f, 1.f, 1000.f);
  
  // These named constants don't really seem worth exposing as console vars
  const f32 kChargerHeightSlop_mm = 10.f; // Extra height to add to top of charger for vertical crop computation
  const f32 kHeadAngleDownThresh_deg = -10.f; // Head is "down" if angle below this
  const f32 kLiftHeightDownThresh_mm = LIFT_HEIGHT_LOWDOCK+10; // Lift is "down" if height is below this
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CropScheduler::Reset(const f32 cropWidthFraction, const CyclingMode cyclingMode)
{
  _widthFraction = cropWidthFraction;
  
  switch(cyclingMode)
  {
    case CyclingMode::Middle_Left_Middle_Right:
      _cropSchedule = {
        CropPosition::Middle, CropPosition::Left, CropPosition::Middle, CropPosition::Right
      };
      break;
      
    case CyclingMode::MiddleOnly:
      _cropSchedule = {CropPosition::Middle};
      break;
      
    case CyclingMode::Middle2X_MiddlePlusEachSide:
      _cropSchedule = {
        CropPosition::Middle, CropPosition::Middle,
        CropPosition::MiddlePlusLeft,
        CropPosition::Middle, CropPosition::Middle,
        CropPosition::MiddlePlusRight,
      };
      break;
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
f32 CropScheduler::GetCurrentWidthFraction(const CropPosition cropPosition) const
{
  switch(cropPosition)
  {
    case CropPosition::Full:
      return 1.f;
      
    case CropPosition::Middle:
    case CropPosition::Left:
    case CropPosition::Right:
      return _widthFraction;
      
    case CropPosition::MiddlePlusLeft:
    case CropPosition::MiddlePlusRight:
      return 0.5f*(1.f + _widthFraction);
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 CropScheduler::GetCurrentCropX(const CropPosition cropPosition, const s32 ncols, const s32 cropWidth) const
{
  switch(cropPosition)
  {
    case CropPosition::Full:
    case CropPosition::Left:
    case CropPosition::MiddlePlusLeft:
      return 0;
      
    case CropPosition::Middle:
      return (ncols - cropWidth)/2;
      
    case CropPosition::Right:
    case CropPosition::MiddlePlusRight:
      return (ncols - cropWidth);
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
s32 CropScheduler::GetCurrentCropY(const CropPosition cropPosition, const VisionPoseData& poseData,
                                   const s32 nrows, const s32 ncols) const
{
  // Undistort a point at the bottom of the image
  Point2f undistortedPoint;
  const Result undistortResult = Vision::Undistorter::UndistortPoint(_camera.GetCalibration(), nrows, ncols,
                                                                     Point2f(ncols/2, nrows-1), undistortedPoint);
  if(RESULT_OK != undistortResult)
  {
    LOG_ERROR("CropScheduler.GetCurrentCropY.UndistortFailed", "");
    return -1;
  }
  
  // Only process as much of the image as is needed to see the top of the charger placed at the closest
  // visible point on the ground plane.
  
  // Find the closest point on the ground plane at which we could feasibly see the
  // charger marker at this head angle. This involves finding the distance from the
  // robot x_M, where the bottom of the charger marker projects to the bottom of the
  // camera image. We can use the ground plane homography to find x_G, the closest
  // point on the ground plane that projects into the image. Then we use triangle
  // similarity to figure out x_M, based on the height of the bottom of the charger
  // marker off the ground, z_M, and the current height of the camera off the ground,
  // z_C.
  //
  //  |<- - - - - -  x_M - - - - - - >|<- - - - - x_B - - - - - ->|
  //
  //         (Camera) O......
  //                  |      ......    (Bottom of Charger Marker)
  //                  |            ...O...
  //                  |               |   ......
  //  |< - - x_C - - >|               |         ...... (Projection line from camera to ground)
  //                  | z_C           | z_M           ......
  //   (Robot)        |               |                     ......
  //  O---------------+---------------+---------------------------O (Ground plane intersection)
  //
  //  |<- - - - - - - - - - - - - x_G - - - - - - - - - - - - - ->|
  //
  //  Similar triangles: (x_B / z_M) = (x_G - x_C) / z_C
  //  Solve for x_B:            x_B  = (x_G - x_C) * (z_M / z_C)
  //  Relationship to x_M:      x_B  = x_G - x_M
  //  Solve for x_M:            x_M  = x_G - (x_G - x_C) * (z_M / z_C)  <-- What is coded below
  
  Matrix_3x3f invH = poseData.groundPlaneHomography.GetInverse();
  
  const Point3f temp = invH * Point3f(undistortedPoint.x(), undistortedPoint.y(), 1.f);
  const f32 x_G = temp.x() / temp.z();
  
  if(Util::IsFltLEZero(x_G))
  {
    if(VERBOSE_DEBUG) {
      LOG_DEBUG("CropScheduler.GetCurrentCropY.GroundPlaneNotVisible",
                "x_G: %.2fmm", x_G);
    }
    return -1;
  }
  
  Pose3d cameraPoseWrtRobot;
  bool success = poseData.cameraPose.GetWithRespectTo(poseData.histState.GetPose(), cameraPoseWrtRobot);
  DEV_ASSERT(success, "CropScheduler.GetCurrentCropY.BadCameraPose");
  const f32 x_C = cameraPoseWrtRobot.GetTranslation().x();
  const f32 z_C = cameraPoseWrtRobot.GetTranslation().z();
  const f32 z_M = Charger::kMarkerZPosition - (0.5f*Charger::kMarkerHeight);
  const f32 x_M = x_G - (x_G - x_C) * (z_M/z_C);
  
  if(Util::IsFltGT(x_M, kCropScheduler_MaxMarkerDetectionDist_mm))
  {
    if(VERBOSE_DEBUG) {
      LOG_DEBUG("CropScheduler.GetCurrentCropY.MarkerTooFar",
                "x_M:%.2fmm > max dist (%.1f)", x_M, kCropScheduler_MaxMarkerDetectionDist_mm);
    }
    return -1;
  }
  
  // At the closest point we could see the charger, found above, project the top of the charger
  // (plus a little slop) into the image and see where we can crop the image and still hope
  // to detect the charger's marker
  const Pose3d topOfCharger(0, Z_AXIS_3D(), {x_M, 0.f, Charger::kHeight + kChargerHeightSlop_mm},
                            poseData.histState.GetPose());
  Pose3d poseWrtCamera;
  success = topOfCharger.GetWithRespectTo(poseData.cameraPose, poseWrtCamera);
  DEV_ASSERT(success, "CropScheduler.GetCurrentCropY.TopBadHistCameraPoseTree");
  Point2f projPoint;
  success = _camera.Project3dPoint(poseWrtCamera.GetTranslation(), projPoint);
  if(!success)
  {
    return 0;
  }
  else
  {
    return std::max(0, (s32)std::round(projPoint.y()));
  }

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CropScheduler::GetCropRect(const s32 nrows, const s32 ncols,
                                const bool useHorizontalCycling,
                                const bool useVariableHeight,
                                const VisionPoseData& poseData, Rectangle<s32>& cropRect)
{
  CropPosition cropPosition = CropPosition::Full;
  if(useHorizontalCycling)
  {
    const bool isHeadDown = Util::IsFltLT(RAD_TO_DEG(poseData.histState.GetHeadAngle_rad()), kHeadAngleDownThresh_deg);
    const bool isLiftDown = Util::IsFltLT(poseData.histState.GetLiftHeight_mm(), kLiftHeightDownThresh_mm);
    if(isHeadDown && isLiftDown)
    {
      // If head and lift are considered "down", ignore current cropPosition and just use a "Middle" crop
      cropPosition = CropPosition::Middle;
    }
    else
    {
      cropPosition = _cropSchedule[_cropIndex];
    }
    
    if(_cropSchedule.size() > 1)
    {
      ++_cropIndex;
      if(_cropIndex >= _cropSchedule.size())
      {
        _cropIndex = 0;
      }
    }
  }
  
  const s32 cropY = (useVariableHeight ? GetCurrentCropY(cropPosition, poseData, nrows, ncols) : 0);
  if(cropY < 0)
  {
    return false;
  }
  
  const s32 cropHeight = nrows - cropY;
  
  if(cropHeight <= 0)
  {
    if(VERBOSE_DEBUG) {
      LOG_DEBUG("CropScheduler.GetCropRect.EmptyCrop", "CropY:%d CropHeight:%d", cropY, cropHeight);
    }
    return false;
  }
  
  const f32 currentWidthFraction = GetCurrentWidthFraction(cropPosition);
  const s32 cropWidth = std::round(currentWidthFraction * (f32)ncols);
  const s32 cropX = GetCurrentCropX(cropPosition, ncols, cropWidth);
  
  cropRect = Rectangle<s32>(cropX, cropY, cropWidth, cropHeight);
  
  if(VERBOSE_DEBUG)
  {
    LOG_DEBUG("CropScheduler.GetCropRect.FinalRect",
              "Rect:[%d %d %d %d] Frac:%.2f",
              cropRect.GetX(), cropRect.GetY(), cropRect.GetWidth(), cropRect.GetHeight(),
              (f32)cropRect.Area() / (f32)(nrows*ncols));
  }
  
  return true;
}

} // namespace Vector
} // namespace Anki
