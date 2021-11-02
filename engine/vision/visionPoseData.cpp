/**
 * File: visionPoseData.cpp
 *
 * Author: Andrew Stein
 * Date:   (various)
 *
 * Description: Container for passing around pose/state information from a certain
 *              timestamp, useful for vision processing.
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "visionPoseData.h"

#include "coretech/common/engine/math/poseOriginList.h"

namespace Anki {
namespace Vector {

void VisionPoseData::Set(const RobotTimeStamp_t histTimeStamp_in,
                         const HistRobotState&  histState_in,
                         const Pose3d&          cameraPose_in,
                         const bool             groundPlaneVisible_in,
                         const Matrix_3x3f&     groundPlaneHomography_in,
                         const ImuDataHistory&  imuHistory_in)
{
  DEV_ASSERT(histState_in.GetPose().GetRootID() != PoseOriginList::UnknownOriginID, "VisionPoseData.Set.UnknownOriginID");
  
  // The camera pose's parent is expected to be the histState's pose
  DEV_ASSERT(cameraPose_in.IsChildOf(histState_in.GetPose()), "VisionPoseData.Set.BadCameraPoseParent");
  
  timeStamp = histTimeStamp_in;
  histState = histState_in;
  cameraPose = cameraPose_in;
  cameraPose.SetParent(histState.GetPose()); // Not histState_in.GetPose()!!
  groundPlaneVisible = groundPlaneVisible_in;
  groundPlaneHomography = groundPlaneHomography_in;
  imuDataHistory = imuHistory_in;
  
  // Pose data is *assumed* to be w.r.t. a root on the vision thread.
  // Check that that is true and then clear the parent so that the vision
  // thread can hook it up to its own origin, since we don't want calls
  // that walk pose chains (e.g. GetWithRespectTo, FindRoot, etc) to
  // ever see or refer to poses on a different thread that may get modified.
  DEV_ASSERT(histState.GetPose().GetParent().IsRoot(), "VisionPoseData.Set.HistoricalPoseParentNotRoot");
  histState.ClearPoseParent();
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionPoseData::IsBodyPoseSame(const VisionPoseData& other, const Radians& bodyAngleThresh,
                                    const f32 bodyPoseThresh_mm) const
{
  const bool isXPositionSame = NEAR(histState.GetPose().GetTranslation().x(),
                                    other.histState.GetPose().GetTranslation().x(),
                                    bodyPoseThresh_mm);
  
  const bool isYPositionSame = NEAR(histState.GetPose().GetTranslation().y(),
                                    other.histState.GetPose().GetTranslation().y(),
                                    bodyPoseThresh_mm);
  
  const bool isAngleSame     = NEAR(histState.GetPose().GetRotation().GetAngleAroundZaxis().ToFloat(),
                                    other.histState.GetPose().GetRotation().GetAngleAroundZaxis().ToFloat(),
                                    bodyAngleThresh.ToFloat());
  
  const bool isPoseSame = isXPositionSame && isYPositionSame && isAngleSame;
  
  return isPoseSame;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool VisionPoseData::IsHeadAngleSame(const VisionPoseData& other, const Radians& headAngleThresh) const
{
  const bool headSame =  NEAR(histState.GetHeadAngle_rad(),
                              other.histState.GetHeadAngle_rad(),
                              headAngleThresh.ToFloat());
  
  return headSame;
}

} // namespace Vector
} // namespace Anki
