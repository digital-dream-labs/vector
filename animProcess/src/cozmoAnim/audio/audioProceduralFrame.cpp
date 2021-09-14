/**
 * File: audioProceduralFrame.cpp
 *
 * Author: Jordan Rivas
 * Created: 03/15/18
 *
 * Description: Store robot’s movement data of a single frame. By tracking multiple frames it is possible to track the
 *              robot’s changes in movement. The class also provides helper methods to calculate useful values for
 *              procedural audio.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "clad/types/robotStatusAndActions.h"
#include "cozmoAnim/audio/audioProceduralFrame.h"
#include "util/logging/logging.h"
#include <iostream>
#include <string>

namespace Anki {
namespace Vector {
namespace Audio {
namespace {
#define CONSOLE_PATH "Audio.Procedural"
CONSOLE_VAR_RANGED(float, kMaxTreadSpeed_mmps, CONSOLE_PATH, MAX_WHEEL_SPEED_MMPS,
                   MAX_WHEEL_SPEED_MMPS - 100, MAX_WHEEL_SPEED_MMPS + 100);
CONSOLE_VAR_RANGED(float, kMaxTurnSpeed_mmps, CONSOLE_PATH, MAX_WHEEL_SPEED_MMPS,
                   MAX_WHEEL_SPEED_MMPS - 100, MAX_WHEEL_SPEED_MMPS + 100);
CONSOLE_VAR_RANGED(float, kMaxHeadSpeed_rpms, CONSOLE_PATH, 0.005f, 0.0f, 0.025f);
CONSOLE_VAR_RANGED(float, kMaxLiftSpeed_rpms, CONSOLE_PATH, 0.0025f, 0.0f, 0.05f);
CONSOLE_VAR_RANGED(float, kMaxTreadAccel_mmpms2, CONSOLE_PATH, 5.0f, 0.0f, 10.0f);
CONSOLE_VAR_RANGED(float, kMaxHeadAccel_rpms2, CONSOLE_PATH, 0.0001f, 0.0f, 0.001f);
CONSOLE_VAR_RANGED(float, kMaxLiftAccel_rpms2, CONSOLE_PATH, 0.0001f, 0.0f, 0.001f);
CONSOLE_VAR_RANGED(float, kTreadMovementThreshold_mmps, CONSOLE_PATH, 0.0f, 0.0f, 0.01f);
CONSOLE_VAR_RANGED(float, kHeadMovementThreshold_rpms, CONSOLE_PATH, 0.0f, 0.0f, 0.01f);
CONSOLE_VAR_RANGED(float, kLiftMovementThreshold_rpms, CONSOLE_PATH, 0.0f, 0.0f, 0.01f);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::UpdateFrame(const RobotState& robotState)
{
  _timestamp_ms = robotState.timestamp;
  _leftTreadSpeed_mmps = robotState.lwheel_speed_mmps;
  _rightTreadSpeed_mmps = robotState.rwheel_speed_mmps;
  _headAngle_rad = robotState.headAngle;
  _liftAngle_rad = robotState.liftAngle;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeValues(const AudioProceduralFrame& previousFrame)
{
  const auto timeDelta_ms = _timestamp_ms - previousFrame._timestamp_ms;
  DEV_ASSERT(timeDelta_ms > 0, "AudioProceduralFrame.ComputeValues.InvalidFrameTimeDelta");
  if (timeDelta_ms > 0) {
    // Compute Tread vals
    ComputeAverageTreadSpeed();
    ComputeTurnSpeed();
    ComputeTreadAcceleration(previousFrame);
    // Head & Lift
    ComputeHeadSpeed(previousFrame);
    ComputeHeadAcceleration(previousFrame);
    ComputeLiftSpeed(previousFrame);
    ComputeLiftAcceleration(previousFrame);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AudioProceduralFrame::IsTreadMoving() const
{
  return ((Util::Abs(_leftTreadSpeed_mmps) > kTreadMovementThreshold_mmps) ||
          (Util::Abs(_rightTreadSpeed_mmps) > kTreadMovementThreshold_mmps));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedTreadSpeed() const
{
  return _avgTreadSpeed_mmps / kMaxTreadSpeed_mmps;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedTreadAcceleration() const
{
  const float normAccel = Util::Clamp((_treadAccel_mmpms2 / kMaxTreadAccel_mmpms2), -1.0f, 1.0f);
  return (_avgTreadSpeed_mmps < 0.0f) ? -normAccel : normAccel;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedTurnSpeed() const
{
  return Util::Min((Util::Abs(_turnSpeed_mmps) / kMaxTurnSpeed_mmps), 1.0f);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AudioProceduralFrame::IsHeadMoving() const
{
  return (Util::Abs(_headSpeed_rpms) > kHeadMovementThreshold_rpms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedHeadSpeed() const
{
  return Util::Clamp((_headSpeed_rpms / kMaxHeadSpeed_rpms), -1.0f, 1.0f);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedHeadAcceleration() const
{
  const float normAccel = Util::Clamp((_headAccel_rpms2 / kMaxHeadAccel_rpms2), -1.0f, 1.0f);
  return (_headSpeed_rpms < 0.0f) ? -normAccel : normAccel;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AudioProceduralFrame::IsLiftMoving() const
{
  return (Util::Abs(_liftSpeed_rpms) > kLiftMovementThreshold_rpms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedLiftSpeed() const
{
  return Util::Clamp((_liftSpeed_rpms / kMaxLiftSpeed_rpms), -1.0f, 1.0f);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float AudioProceduralFrame::GetNormalizedLiftAcceleration() const
{
  const float normAccel = Util::Clamp((_liftAccel_rpms2 / kMaxLiftAccel_rpms2), -1.0f, 1.0f);
  return (_liftSpeed_rpms < 0.0f) ? -normAccel : normAccel;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Private methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeAverageTreadSpeed()
{
  _avgTreadSpeed_mmps = (_leftTreadSpeed_mmps + _rightTreadSpeed_mmps) / 2.0f;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeTurnSpeed()
{
  _turnSpeed_mmps = _leftTreadSpeed_mmps - _rightTreadSpeed_mmps;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeTreadAcceleration(const AudioProceduralFrame& previousFrame)
{
  _treadAccel_mmpms2 = (_avgTreadSpeed_mmps - previousFrame._avgTreadSpeed_mmps) /
                       (_timestamp_ms - previousFrame._timestamp_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeHeadSpeed(const AudioProceduralFrame& previousFrame)
{
  _headSpeed_rpms = (_headAngle_rad - previousFrame._headAngle_rad) / (_timestamp_ms - previousFrame._timestamp_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeHeadAcceleration(const AudioProceduralFrame& previousFrame)
{
  _headAccel_rpms2 = (_headSpeed_rpms - previousFrame._headSpeed_rpms) / (_timestamp_ms - previousFrame._timestamp_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeLiftSpeed(const AudioProceduralFrame& previousFrame)
{
  _liftSpeed_rpms = (_liftAngle_rad - previousFrame._liftAngle_rad) / (_timestamp_ms - previousFrame._timestamp_ms);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioProceduralFrame::ComputeLiftAcceleration(const AudioProceduralFrame& previousFrame)
{
  _liftAccel_rpms2 = (_liftSpeed_rpms - previousFrame._liftSpeed_rpms) / (_timestamp_ms - previousFrame._timestamp_ms);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// CVS Log methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const std::string AudioProceduralFrame::CvsLogHeader()
{
  return "Timestamp_ms,LeftTread_mmps,RightTread_mmps,AveTreadSpeed_mmps,TreadAcceleration_mmpms2,TurnSpeed_mmps,"
         "HeadAngle_rad,HeadSpeed_rpms,HeadAcceleration_rpms2,LiftAngle_rad,LiftSpeed_rpms,LiftAcceleration_rpms2";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const std::string AudioProceduralFrame::CvsLogFrameData() const
{
  std::ostringstream stream;
  stream << _timestamp_ms << ',' << _leftTreadSpeed_mmps << ',' << _rightTreadSpeed_mmps << ',' << _avgTreadSpeed_mmps
  <<',' << _treadAccel_mmpms2 << ',' << _turnSpeed_mmps << ',' << _headAngle_rad << ',' << _headSpeed_rpms << ','
  << _headAccel_rpms2 << ',' << _liftAngle_rad << ',' << _liftSpeed_rpms  << ',' << _liftAccel_rpms2;
  
  return stream.str();
}
}
}
}
