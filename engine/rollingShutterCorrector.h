/**
 * File: rollingShutterCorrector.h
 *
 * Author: Al Chaussee
 * Created: 4-1-2016
 *
 * Description:
 *    Class for handling rolling shutter correction and imu data history
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#ifndef ANKI_COZMO_ROLLING_SHUTTER_CORRECTOR_H
#define ANKI_COZMO_ROLLING_SHUTTER_CORRECTOR_H

#include "coretech/vision/engine/image.h"
#include "coretech/common/shared/math/rotation.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include <deque>
#include <vector>

namespace Anki {
  
  namespace Vision {
    class Camera;
  }

  namespace Vector {
  
    class Robot;
    struct VisionPoseData;
    
    class RollingShutterCorrector
    {
    public:
      RollingShutterCorrector() { };
    
      // Calculates the amount of pixel shift to account for rolling shutter
      void ComputePixelShifts(const VisionPoseData& poseData,
                              const VisionPoseData& prevPoseData,
                              const u32 numRows);
      
      // Shifts the image by the calculated pixel shifts
      Vision::Image WarpImage(const Vision::Image& img);
      
      const std::vector<Vec2f>& GetPixelShifts() const { return _pixelShifts; }
      int GetNumDivisions() const { return _rsNumDivisions; }
      
      static constexpr f32 timeBetweenFrames_ms = 65.0;
      
    private:
      // Calculates pixel shifts based on gyro rates from ImageIMUData messages
      // Returns false if unable to calculate shifts due to not having relevant gyro data
      bool ComputePixelShiftsWithImageIMU(RobotTimeStamp_t t,
                                          Vec2f& shift,
                                          const VisionPoseData& poseData,
                                          const VisionPoseData& prevPoseData,
                                          const f32 frac);
    
      // Vector of vectors of varying pixel shift amounts based on gyro rates and vertical position in the image
      std::vector<Vec2f> _pixelShifts;
      
      // Whether or not to do vertical rolling shutter correction
      // TODO: Do we want to be doing vertical correction?
      bool doVerticalCorrection = false;
      
      // The number of rows to divide the image into and calculate warps for
      static constexpr f32 _rsNumDivisions = 180.f;
      
      // Proportionality constant that relates gyro rates to pixel shift
      static constexpr f32 rateToPixelProportionalityConst = 22.0;
      
    };
  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_ROLLING_SHUTTER_CORRECTOR
