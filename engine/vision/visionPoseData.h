/**
 * File: visionPoseData.h
 *
 * Author: Andrew Stein
 * Date:   (various)
 *
 * Description: Container for passing around pose/state information from a certain
 *              timestamp, useful for vision processing.
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef __Anki_Cozmo_Basestation_VisionPoseData_H__
#define __Anki_Cozmo_Basestation_VisionPoseData_H__

#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/matrix_fwd.h"
#include "coretech/common/engine/robotTimeStamp.h"

#include "engine/components/sensors/imuComponent.h"
#include "engine/robotStateHistory.h"
#include "engine/rollingShutterCorrector.h"
#include "engine/vision/groundPlaneROI.h"

namespace Anki {
namespace Vector {

struct VisionPoseData
{
  using ImuDataHistory = ImuHistory; // TODO: Remove
  
  // TODO: Add getters for these and make them private, prefixed with underscore (COZMO-14998)
  RobotTimeStamp_t      timeStamp;
  HistRobotState        histState;  // contains historical head/lift/pose info
  Pose3d                cameraPose; // w.r.t. pose in poseStamp
  bool                  groundPlaneVisible;
  Matrix_3x3f           groundPlaneHomography;
  GroundPlaneROI        groundPlaneROI;
  ImuDataHistory        imuDataHistory;
  
  VisionPoseData() = default;
  
  void Set(const RobotTimeStamp_t histTimeStamp_in,
           const HistRobotState&  histState_in,
           const Pose3d&          cameraPose_in,
           const bool             groundPlaneVisible_in,
           const Matrix_3x3f&     groundPlaneHomography_in,
           const ImuDataHistory&  imuHistory_in);
  
  // Helpers to check whether the body/head have changed more than a given amount
  // as compared to another VisionPoseData
  bool IsBodyPoseSame(const VisionPoseData& other, const Radians& bodyAngleThresh, const f32 bodyPoseThresh_mm) const;
  bool IsHeadAngleSame(const VisionPoseData& other, const Radians& headAngleThresh) const;
  
  // ---------- Begin Custom copy implementation ------- //
  template<typename T1, typename T2>
  friend void swap(T1&& first, T2&& second);
  
  template<typename T>
  VisionPoseData(T&& other);
  
  VisionPoseData& operator=(VisionPoseData other);
  
}; // struct VisionPoseData
  
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<typename T>
VisionPoseData::VisionPoseData(T&& other)
{
  swap(*this, std::forward<T>(other));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline VisionPoseData& VisionPoseData::operator=(VisionPoseData other)
{
  swap(*this, other);
  return *this;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<typename T1, typename T2>
void swap(T1&& first, T2&& second)
{
  // This enables ADL
  using std::swap;
  
  swap(first.timeStamp, second.timeStamp);
  swap(first.histState, second.histState);
  swap(first.cameraPose, second.cameraPose);
  swap(first.groundPlaneVisible, second.groundPlaneVisible);
  swap(first.groundPlaneHomography, second.groundPlaneHomography);
  swap(first.groundPlaneROI, second.groundPlaneROI);
  swap(first.imuDataHistory, second.imuDataHistory);
  
  // Because the cameraPose is wrt the pose contained in poseStamp, set it explicitly
  first.cameraPose.SetParent(first.histState.GetPose());
  second.cameraPose.SetParent(second.histState.GetPose());
}


} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_VisionPoseData_H__ */
