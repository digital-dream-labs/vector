/**
 * File: dubbinsPathPlanner.h
 *
 * Author: Andrew / Kevin (reorg by Brad)
 * Created: 2015-09-16
 *
 * Description: Dubbins path planner
 *
 * Copyright: Anki, Inc. 2015
 *
 **/

#ifndef __DUBBINSPATHPLANNER_H__
#define __DUBBINSPATHPLANNER_H__

#include "pathPlanner.h"

namespace Anki {
namespace Vector {

class DubbinsPlanner : public IPathPlanner
{
public:
  DubbinsPlanner();

protected:
  virtual EComputePathStatus ComputePath(const Pose3d& startPose,
                                         const Pose3d& targetPose) override;
      
};

}
}

#endif
