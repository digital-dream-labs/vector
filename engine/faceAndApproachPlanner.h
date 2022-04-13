/**
 * File: faceAndApproachPlanner.h
 *
 * Author: Brad Neuman
 * Created: 2015-09-16
 *
 * Description: A simple "planner" which will do a turn in place, followed by a straight action, followed by a
 * final turn in point.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#ifndef __FACEANDAPPROACHPLANNER_H__
#define __FACEANDAPPROACHPLANNER_H__

#include "pathPlanner.h"

namespace Anki {
namespace Vector {

class FaceAndApproachPlanner : public IPathPlanner
{
public:

  FaceAndApproachPlanner() : IPathPlanner("FaceAndApproach") {}

  virtual EComputePathStatus ComputeNewPathIfNeeded(const Pose3d& startPose,
                                                    bool forceReplanFromScratch = false,
                                                    bool allowGoalChange = true) override;
protected:

  virtual EComputePathStatus ComputePath(const Pose3d& startPose,
                                         const Pose3d& targetPose) override;

  Vec3f _targetVec;
  float _finalTargetAngle = 0.f;
};

}
}


#endif
