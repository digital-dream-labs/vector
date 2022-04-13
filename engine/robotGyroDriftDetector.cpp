//
//  robotGyroDriftDetector.cpp
//
//  Created by Matt Michini 2017-05-15
//  Copyright (c) 2017 Anki, Inc. All rights reserved.
//

#include "engine/robotGyroDriftDetector.h"

#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/robot.h"

#include "util/logging/DAS.h"

#include <limits> // std::numeric_limits<>

namespace Anki {
namespace Vector {

namespace {
const float kDriftCheckMaxRate_rad_per_sec = DEG_TO_RAD(10.f);
const float kDriftCheckPeriod_ms = 5000.f;
const float kDriftCheckGyroZMotionThresh_rad_per_sec = DEG_TO_RAD(1.f);
const float kDriftCheckMaxAngleChangeRate_rad_per_sec = DEG_TO_RAD(0.1f);

const float kAccelMovingThresh_mmps2 = 50.f; // highpass-filtered accelerometer magnitude must be below this to be considered stationary
const uint32_t kBiasCheckDuration_ms = 5000; // duration that robot must be stationary before checking for gyro bias
const int kBiasCheckMinReadings = 50; // minimum required number of readings for bias detection
const float kBiasDetectionThresh_rad_per_sec = DEG_TO_RAD(0.1f); // near-constant gyro readings above this value is considered 'bias'
const float kBiasMaxRange_rad_per_sec = DEG_TO_RAD(0.05f); // maximum allowed difference between min and max gyro readings during bias check period.
} // anonymous namespace


RobotGyroDriftDetector::RobotGyroDriftDetector()
: IDependencyManagedComponent(this, RobotComponentID::GyroDriftDetector)
{

}

void RobotGyroDriftDetector::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps)
{
  _robot = robot;
}


// 'Legacy' drift detection based on robot estimated pose angle
void RobotGyroDriftDetector::DetectGyroDrift(const RobotState& msg)
{
  f32 gyroZ = msg.gyro.z;

  if (!_gyroDriftReported) {

    // Reset gyro drift detector if
    // 1) Wheels are moving
    // 2) Raw gyro reading exceeds possible drift value
    // 3) Cliff is detected
    // 4) Head isn't calibrated
    // 5) Drift detector started but the raw gyro reading deviated too much from starting values, indicating motion.
    if (_robot->GetMoveComponent().IsMoving() ||
        (std::fabsf(gyroZ) > kDriftCheckMaxRate_rad_per_sec) ||
        _robot->GetCliffSensorComponent().IsCliffDetected() ||
        !_robot->IsHeadCalibrated() ||

        ((_startTime_ms != 0) &&
         ((std::fabsf(_startGyroZ_rad_per_sec - gyroZ) > kDriftCheckGyroZMotionThresh_rad_per_sec) ||
          (_startPoseFrameId != _robot->GetPoseFrameID())))

        ) {
      _startTime_ms = 0;
    }

    // Robot's not moving. Initialize drift detection.
    else if (_startTime_ms == 0) {
      _startPoseFrameId        = _robot->GetPoseFrameID();
      _startAngle_rad          = _robot->GetPose().GetRotation().GetAngleAroundZaxis();
      _startGyroZ_rad_per_sec  = gyroZ;
      _startTime_ms            = msg.timestamp;
      _cumSumGyroZ_rad_per_sec = gyroZ;
      _minGyroZ_rad_per_sec    = gyroZ;
      _maxGyroZ_rad_per_sec    = gyroZ;
      _numReadings             = 1;
    }

    // If gyro readings have been accumulating for long enough...
    else if (msg.timestamp - _startTime_ms > kDriftCheckPeriod_ms) {

      // ...check if there was a sufficient change in heading angle or pitch. Otherwise, reset detector.
      const f32 headingAngleChange = std::fabsf((_startAngle_rad - _robot->GetPose().GetRotation().GetAngleAroundZaxis()).ToFloat());
      const f32 angleChangeThresh = kDriftCheckMaxAngleChangeRate_rad_per_sec * Util::MilliSecToSec(kDriftCheckPeriod_ms);

      if (headingAngleChange > angleChangeThresh) {
        // Report drift detected just one time during a session
        const int min_mdeg_per_sec = std::round(RAD_TO_DEG(1000.f * _minGyroZ_rad_per_sec));
        const int max_mdeg_per_sec = std::round(RAD_TO_DEG(1000.f * _maxGyroZ_rad_per_sec));
        const int mean_mdeg_per_sec = std::round(RAD_TO_DEG(1000.f * _cumSumGyroZ_rad_per_sec) / _numReadings);
        const int headingAngleChange_mdeg_per_sec = std::round(RAD_TO_DEG(1000.f * headingAngleChange));
        
        DASMSG(gyro_bias_detected, "gyro.drift_detected", "We have detected gyro bias drift ('legacy' detection method)");
        DASMSG_SET(i1, min_mdeg_per_sec, "min gyro z value (millidegrees per sec)");
        DASMSG_SET(i2, max_mdeg_per_sec, "max gyro z value (millidegrees per sec)");
        DASMSG_SET(i3, mean_mdeg_per_sec, "mean gyro z value (millidegrees per sec)");
        DASMSG_SET(i4, headingAngleChange_mdeg_per_sec, "heading angle change (millidegrees per sec)");
        DASMSG_SEND();
        _gyroDriftReported = true;
      }

      _startTime_ms = 0;
    }

    // Record min and max observed gyro readings and cumulative sum for later mean computation
    else {
      if (gyroZ > _maxGyroZ_rad_per_sec) {
        _maxGyroZ_rad_per_sec = gyroZ;
      }
      if (gyroZ < _minGyroZ_rad_per_sec) {
        _minGyroZ_rad_per_sec = gyroZ;
      }
      _cumSumGyroZ_rad_per_sec += gyroZ;
      ++_numReadings;
    }
  }
}

// Detect drift using raw IMU values
void RobotGyroDriftDetector::DetectBias(const RobotState& msg)
{
  // Only report detected gyro bias once per app run
  if (_gyroBiasReported) {
    return;
  }

  bool isMoving = static_cast<bool>(msg.status & Util::EnumToUnderlying(RobotStatusFlag::IS_MOVING));

  // High pass filter the accelerometer readings to make sure
  // the robot is definitely not moving
  const float kFiltAccel = 0.8;
  const float currAccelMag = _robot->GetHeadAccelMagnitude();
  _hpFiltAccelMag = kFiltAccel * (currAccelMag - _accelMagPrev + _hpFiltAccelMag);
  _accelMagPrev = currAccelMag;

  if (std::abs(_hpFiltAccelMag) > kAccelMovingThresh_mmps2) {
    isMoving = true;
  }

  if (!isMoving) {
    if (_biasCheckStartTime_ms == 0) {
      _biasCheckStartTime_ms = msg.timestamp;
    } else if (msg.timestamp - _biasCheckStartTime_ms > kBiasCheckDuration_ms) {
      // Only check for bias if we've accumulated enough readings
      if (_nReadings >= kBiasCheckMinReadings) {
        // Check data min/max data on each axis. If gyro values held
        // steady at some non-zero value, then there is bias.
        for (int i=0 ; i<3 ; i++) {
          const auto minGyro = _minFiltGyroVals[i];
          const auto maxGyro = _maxFiltGyroVals[i];
          const auto range = maxGyro - minGyro;
          const bool sameSign = (signbit(minGyro) == signbit(maxGyro));
          if (sameSign &&
              std::abs(minGyro) > kBiasDetectionThresh_rad_per_sec &&
              std::abs(maxGyro) > kBiasDetectionThresh_rad_per_sec &&
              range < kBiasMaxRange_rad_per_sec) {
            // Log a DAS event and warning
            const std::string axisStr = (i==0 ? "x" : (i==1 ? "y" : "z"));
            DASMSG(gyro_bias_detected, "gyro.bias_detected", "We have detected gyro bias drift");
            DASMSG_SET(i1, std::round(1000.f * RAD_TO_DEG(minGyro)), "min gyro value (millidegrees per sec)");
            DASMSG_SET(i2, std::round(1000.f * RAD_TO_DEG(maxGyro)), "max gyro value (millidegrees per sec)");
            DASMSG_SET(s1, axisStr, "axis of bias");
            DASMSG_SEND();
            PRINT_NAMED_WARNING("RobotGyroDriftDetector.BiasDetected",
                                "Gyro bias detected on %s axis (min=%.2f deg/sec, max=%.2f deg/sec)",
                                axisStr.c_str(),
                                RAD_TO_DEG(minGyro),
                                RAD_TO_DEG(maxGyro));
            _gyroBiasReported = true;
          }
        }
      }

      ResetBiasDetector();
    }

    ++_nReadings;

    // Apply low-pass filter to gyro data
    const auto& gyro = Vec3f{msg.gyro.x, msg.gyro.y, msg.gyro.z};
    const float kFiltGyro = 0.95f;
    for (int i=0 ; i<3 ; i++) {
      _gyroFilt[i] = kFiltGyro * _gyroFilt[i] + (1.f - kFiltGyro) * gyro[i];
      _minFiltGyroVals[i] = std::min(_minFiltGyroVals[i], _gyroFilt[i]);
      _maxFiltGyroVals[i] = std::max(_maxFiltGyroVals[i], _gyroFilt[i]);
    }
  } else if (_biasCheckStartTime_ms != 0) {
    // Reset detector
    ResetBiasDetector();
  }
}

void RobotGyroDriftDetector::ResetBiasDetector()
{
  _biasCheckStartTime_ms = 0;
  _nReadings = 0;

  const auto inf = std::numeric_limits<float>::infinity();
  _minFiltGyroVals = {inf, inf, inf};
  _maxFiltGyroVals = {0.f, 0.f, 0.f};
}


} // namespace Vector
} // namespace Anki
