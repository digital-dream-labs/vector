/**
 * File: speedChooser.h
 *
 * Author: Al Chaussee
 * Date:   4/20/2016
 *
 * Description: Creates a path motion profile based on robot state
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_SPEED_CHOOSER_H
#define ANKI_COZMO_SPEED_CHOOSER_H

#include "coretech/common/engine/math/pose.h"
#include "clad/types/pathMotionProfile.h"
#include "util/random/randomGenerator.h"
#include "util/signals/simpleSignal_fwd.h"

namespace Anki {
  namespace Vector {
    
    class Robot;
    
    class SpeedChooser
    {
      public:
        SpeedChooser(Robot& robot);
      
        // Generates a path motion profile based on the distance to the goal pose
        PathMotionProfile GetPathMotionProfile(const Pose3d& goal);
      
        // Generates a path motion profile based on the distance to the closest goal
        PathMotionProfile GetPathMotionProfile(const std::vector<Pose3d>& goals);

        // Handle various message types
        template<typename T>
        void HandleMessage(const T& msg);
      
      private:
        Robot& _robot;
      
        bool _enabled = true;

        // Max speed a generated motion profile can have
        const float maxSpeed_mmps = MAX_SAFE_WHEEL_SPEED_MMPS;
      
        // Min speed a generated motion profile can have
        const float minSpeed_mmps = MAX_SAFE_WHILE_CARRYING_WHEEL_SPEED_MMPS;

        const float minAccel_mmps2 = 80.0f;
        const float maxAccel_mmps2 = 100.0f;
      
        const float distToObjectForMaxSpeed_mm = 300;
        
        std::vector<Signal::SmartHandle> _signalHandles;
    };
  }
}

#endif // ANKI_COZMO_SPEED_CHOOSER_H
