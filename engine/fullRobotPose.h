/**
 * File: fullRobotPose.h
 *
 * Author: Kevin M. Karol
 * Created: 3/16/18
 *
 * Description: Class which wraps all descriptors of the physical state of
 * a robot
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Cozmo_Basestation_FullRobotPose_H__
#define __Cozmo_Basestation_FullRobotPose_H__

#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/common/engine/math/pose.h"
#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/image.h"
#include "engine/robotComponents_fwd.h"
#include "util/entityComponent/iDependencyManagedComponent.h"


namespace Anki {
namespace Vector {


class FullRobotPose : public IDependencyManagedComponent<RobotComponentID>
{
public:
  FullRobotPose();
  virtual ~FullRobotPose();

  Pose3d&         GetPose()               { return _pose;         }
  const Pose3d&   GetPose()         const { return _pose;         }
  void  SetPose(const Pose3d& pose)       { _pose = pose;         }

  f32   GetHeadAngle()          const { return _headAngle;    }
  void  SetHeadAngle(f32 angle)       { _headAngle = angle;   }

  f32  GetLiftAngle()    const { return _liftAngle;    }
  void SetLiftAngle(f32 angle) { _liftAngle = angle;   }

  const Radians&  GetPitchAngle()   const { return _pitchAngle;   }
  void  SetPitchAngle(const Radians& rad) { _pitchAngle = rad;    }
  
  const Radians&  GetRollAngle()   const { return _rollAngle;   }
  void  SetRollAngle(const Radians& rad) { _rollAngle = rad;    }

  const Pose3d&   GetNeckPose()     const { return _neckPose;     }
  const Pose3d&   GetHeadCamPose()  const { return _headCamPose;  }

  const Pose3d&   GetLiftBasePose() const { return _liftBasePose; }
  const Pose3d&   GetLiftPose()     const { return _liftPose;     }
  Pose3d&         GetLiftPose()           { return _liftPose;     }

  #if SHOULD_SEND_DISPLAYED_FACE_TO_ENGINE
  const Vision::ImageRGB& GetDisplayImg()   const   { return _displayImg;   }
  void SetDisplayImg(Vision::ImageRGB& displayImg)  { _displayImg = displayImg;}
  #endif

private:
  Pose3d           _pose;
  const Pose3d     _neckPose;     // joint around which head rotates
  Pose3d           _headCamPose;  // in canonical (untilted) position w.r.t. neck joint
  const Pose3d     _liftBasePose; // around which the base rotates/lifts
  Pose3d           _liftPose;     // current, w.r.t. liftBasePose

  f32              _headAngle;
  f32              _liftAngle;
  Radians          _pitchAngle;
  Radians          _rollAngle;

  #if SHOULD_SEND_DISPLAYED_FACE_TO_ENGINE
  Vision::ImageRGB         _displayImg;
  #endif
};

} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Basestation_FullRobotPose_H__
