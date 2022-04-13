/**
 * File: rollingShutterCorrector.cpp
 *
 * Author: Al Chaussee
 * Created: 4-1-2016
 *
 * Description:
 *    Class for handling rolling shutter correction
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/rollingShutterCorrector.h"

#include "engine/robot.h"
#include "engine/vision/visionSystem.h"


namespace Anki {
  namespace Vector {

    namespace {
      const TimeStamp_t kMaxAllowedDelay_ms = 100; 
    }
 
    void RollingShutterCorrector::ComputePixelShifts(const VisionPoseData& poseData,
                                                     const VisionPoseData& prevPoseData,
                                                     const u32 numRows)
    {
      _pixelShifts.clear();
      _pixelShifts.reserve(_rsNumDivisions);

      // Time difference between subdivided rows in the image
      const f32 timeDif = timeBetweenFrames_ms/_rsNumDivisions;
      
      // Whether or not a call to computePixelShiftsWithImageIMU returned false meaning it
      // was unable to compute the pixelShifts from imageIMU data
      bool didComputePixelShiftsFail = false;
      
      // Compounded pixelShifts across the image
      Vec2f shiftOffset = 0;
      
      // The fraction each subdivided row in the image will contribute to the total shifts for this image
      const f32 frac = 1.f / _rsNumDivisions;
      
      for(int i=1;i<=_rsNumDivisions;i++)
      {
        Vec2f pixelShifts;
        const RobotTimeStamp_t time = poseData.timeStamp - Anki::Util::numeric_cast<TimeStamp_t>(std::round(i*timeDif));
        didComputePixelShiftsFail |= !ComputePixelShiftsWithImageIMU(time,
                                                                     pixelShifts,
                                                                     poseData,
                                                                     prevPoseData,
                                                                     frac);
        
        _pixelShifts.insert(_pixelShifts.end(),
                            Vec2f(pixelShifts.x() + shiftOffset.x(), pixelShifts.y() + shiftOffset.y()));
        
        shiftOffset.x() += pixelShifts.x();
        shiftOffset.y() += pixelShifts.y();
      }
      
      if(didComputePixelShiftsFail)
      {
        if(!poseData.imuDataHistory.empty())
        {
          PRINT_NAMED_WARNING("RollingShutterCorrector.ComputePixelShifts.NoImageIMUData",
                              "No ImageIMU data from timestamp %i have data from time %i:%i",
                              (TimeStamp_t)poseData.timeStamp,
                              (TimeStamp_t)poseData.imuDataHistory.front().timestamp,
                              (TimeStamp_t)poseData.imuDataHistory.back().timestamp);
        }
        else
        {
          PRINT_NAMED_WARNING("RollingShutterCorrector.ComputePixelShifts.EmptyHistory",
                              "No ImageIMU data from timestamp %i, imuDataHistory is empty",
                              (TimeStamp_t)poseData.timeStamp);
        }
      }
    }
    
    Vision::Image RollingShutterCorrector::WarpImage(const Vision::Image& imgOrig)
    {
      Vision::Image img(imgOrig.GetNumRows(), imgOrig.GetNumCols());
      img.SetTimestamp(imgOrig.GetTimestamp());
      
      const int numRows = img.GetNumRows() - 1;
      
      const f32 rowsPerDivision = ((f32)numRows)/_rsNumDivisions;
      
      for(int i=1;i<=_rsNumDivisions;i++)
      {
        const Vec2f& pixelShifts = _pixelShifts[i-1];
        
        const int firstRow = numRows - (i * rowsPerDivision);
        const int lastRow  = numRows - ((i-1) * rowsPerDivision);
        
        for(int y = firstRow; y < lastRow; y++)
        {
          const unsigned char* row = imgOrig.GetRow(y);
          unsigned char* shiftRow = img.GetRow(y);
          for(int x=0;x<img.GetNumCols();x++)
          {
            int xIdx = x - pixelShifts.x();
            if(xIdx < 0 || xIdx >= img.GetNumCols())
            {
              shiftRow[x] = 0;
            }
            else
            {
              shiftRow[x] = row[xIdx];
            }
          }
        }
      }
      return img;
    }
    
    bool RollingShutterCorrector::ComputePixelShiftsWithImageIMU(RobotTimeStamp_t t,
                                                                 Vec2f& shift,
                                                                 const VisionPoseData& poseData,
                                                                 const VisionPoseData& prevPoseData,
                                                                 const f32 frac)
    {
      if(poseData.imuDataHistory.empty())
      {
        shift = Vec2f(0,0);
        return false;
      }
      
      ImuHistory::const_iterator ImuBeforeT;
      ImuHistory::const_iterator ImuAfterT;

      float rateY = 0;
      float rateZ = 0;

      bool beforeAfterSet = false;
      // Find the ImuData before and after the timestamp if it exists
      for(auto iter = poseData.imuDataHistory.begin(); iter != poseData.imuDataHistory.end(); ++iter)
      {
        if(iter->timestamp >= t)
        {
          // No imageIMU data before time t
          if(iter == poseData.imuDataHistory.begin())
          {
            shift = Vec2f(0, 0);
            return false;
          }
          else
          {
            ImuBeforeT = iter - 1;
            ImuAfterT = iter;
            beforeAfterSet = true;
          }
          
          const TimeStamp_t tMinusBeforeTime     = (TimeStamp_t)(t - ImuBeforeT->timestamp);
          const TimeStamp_t afterMinusBeforeTime = (TimeStamp_t)(ImuAfterT->timestamp - ImuBeforeT->timestamp);
          
          // Linearly interpolate the imu data using the timestamps before and after imu data was captured
          rateY = (((tMinusBeforeTime)*(ImuAfterT->gyroRobotFrame.y - ImuBeforeT->gyroRobotFrame.y)) / (afterMinusBeforeTime)) + ImuBeforeT->gyroRobotFrame.y;
          rateZ = (((tMinusBeforeTime)*(ImuAfterT->gyroRobotFrame.z - ImuBeforeT->gyroRobotFrame.z)) / (afterMinusBeforeTime)) + ImuBeforeT->gyroRobotFrame.z;
          
          break;
        } 
      }
      
      // If we don't have imu data for the timestamps before and after the timestamp we are looking for just
      // use the latest data
      if(!beforeAfterSet && !poseData.imuDataHistory.empty())
      {
        // if the IMU data is recent enough, just assume gyro data hasn't changed too much
        if ( t - poseData.imuDataHistory.back().timestamp <= kMaxAllowedDelay_ms ) {
          rateY = poseData.imuDataHistory.back().gyroRobotFrame.y;
          rateZ = poseData.imuDataHistory.back().gyroRobotFrame.z;
        } else {          
          shift = Vec2f(0, 0);
          return false;
        }
      }
      
      // If we aren't doing vertical correction then setting rateY to zero will ensure no Y shift
      if(!doVerticalCorrection)
      {
        rateY = 0;
      }
      
      // The rates are in world coordinate frame but we want them in camera frame which is why Z and X are switched
      shift = Vec2f(rateZ * rateToPixelProportionalityConst * frac,
                    rateY * rateToPixelProportionalityConst * frac);
      
      return true;
    }
  }
}
