/**
 * File: pathMotionProfileHelpers.cpp
 *
 * Author: Brad Neuman
 * Created: 2016-02-22
 *
 * Description: Helpers for motion profile clad struct
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/pathMotionProfileHelpers.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "clad/types/pathMotionProfile.h"
#include "json/json.h"

namespace Anki {
namespace Vector {

void LoadPathMotionProfileFromJson(PathMotionProfile& profile, const Json::Value& config)
{
  profile.speed_mmps = config.get("speed_mmps", DEFAULT_PATH_MOTION_PROFILE.speed_mmps).asFloat();
  profile.accel_mmps2 = config.get("accel_mmps2", DEFAULT_PATH_MOTION_PROFILE.accel_mmps2).asFloat();
  profile.decel_mmps2 = config.get("decel_mmps2", DEFAULT_PATH_MOTION_PROFILE.decel_mmps2).asFloat();
  profile.pointTurnSpeed_rad_per_sec = config.get("pointTurnSpeed_rad_per_sec",
                                                  DEFAULT_PATH_MOTION_PROFILE.pointTurnSpeed_rad_per_sec).asFloat();
  profile.pointTurnAccel_rad_per_sec2 = config.get("pointTurnAccel_rad_per_sec2",
                                                   DEFAULT_PATH_MOTION_PROFILE.pointTurnAccel_rad_per_sec2).asFloat();
  profile.pointTurnDecel_rad_per_sec2 = config.get("pointTurnDecel_rad_per_sec2",
                                                   DEFAULT_PATH_MOTION_PROFILE.pointTurnDecel_rad_per_sec2).asFloat();
  profile.dockSpeed_mmps = config.get("dockSpeed_mmps", DEFAULT_PATH_MOTION_PROFILE.dockSpeed_mmps).asFloat();
  profile.dockAccel_mmps2 = config.get("dockAccel_mmps2", DEFAULT_PATH_MOTION_PROFILE.dockAccel_mmps2).asFloat();
  profile.dockDecel_mmps2 = config.get("dockDecel_mmps2", DEFAULT_PATH_MOTION_PROFILE.dockDecel_mmps2).asFloat();
  profile.reverseSpeed_mmps = config.get("reverseSpeed_mmps", DEFAULT_PATH_MOTION_PROFILE.reverseSpeed_mmps).asFloat();
}

}
}
