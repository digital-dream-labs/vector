/**
 * File: audioProceduralFrame.h
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


#ifndef __Anki_Cozmo_AudioProceduralFrame_H__
#define __Anki_Cozmo_AudioProceduralFrame_H__

#include "anki/cozmo/shared/cozmoConfig.h"
#include "util/console/consoleInterface.h"
#include "util/math/math.h"

namespace Anki {
namespace Vector {
struct RobotState;
namespace Audio {


class AudioProceduralFrame {

public:

  // Update frame's primary values
  void UpdateFrame(const RobotState& robotState);
  
  // Compute frame's derived values
  void ComputeValues(const AudioProceduralFrame& previousFrame);
  
  // Note: Must call UpdateFrame() & ComputeValues() before using getters
  uint32_t GetTimeStamp_ms() const { return _timestamp_ms; }
  float GetLeftTreadSpeed_mmps() const { return _leftTreadSpeed_mmps; }
  float GetRightTreadSpeed_mmps() const { return _rightTreadSpeed_mmps; }
  float GetAverageTreadSpeed_mmps() const { return _avgTreadSpeed_mmps; }
  float GetTreadAcceleration_mmpms2() const { return _treadAccel_mmpms2; }
  float GetTurnSpeed_mmps() const { return _turnSpeed_mmps; }
  float GetHeadAngle_rad() const { return _headAngle_rad; }
  float GetHeadSpeed_rpms() const { return _headSpeed_rpms; }
  float GetHeadAcceleration_rpms2() const { return _headAccel_rpms2; }
  float GetLiftAngle_rad() const { return _liftAngle_rad; }
  float GetLiftSpeed_rpms() const { return _liftSpeed_rpms; }
  float GetLiftAcceleration_rpms2() const { return _liftAccel_rpms2; }

  // Robot Tread & Turn methods
  bool IsTreadMoving() const;
  
  float GetNormalizedTreadSpeed() const;
  
  // Acceleration considers the direction of the movement and returns positive for increasing speeds, negative for
  // decreasing speeds
  float GetNormalizedTreadAcceleration() const;
  
  // Spin speed can be double the max tread speed because it is the difference of the 2 treads, however we want more
  // resolution for slower speeds so we cap the speed;
  // Turn speed is always positive
  float GetNormalizedTurnSpeed() const;

  // Robot Head methods
  bool IsHeadMoving() const;

  float GetNormalizedHeadSpeed() const;
  
  // Returns positive for increasing speeds, negative for decreasing speeds
  float GetNormalizedHeadAcceleration() const;

  // Robot Lift
  bool IsLiftMoving() const;
  
  float GetNormalizedLiftSpeed() const;
  
  // Returns positive for increasing speeds, negative for decreasing speeds
  float GetNormalizedLiftAcceleration() const;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // CSV Log methods
  static const std::string CvsLogHeader();
  const std::string CvsLogFrameData() const;
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


private:
  
  // NOTE: Primary variables are set by the RobotState struct from the robot.
  //       Derived variables are calculated using primary vars
  uint32_t _timestamp_ms = 0;             // Primary
  // Tread Vals
  float _leftTreadSpeed_mmps = 0.0f;      // Primary
  float _rightTreadSpeed_mmps = 0.0f;     // Primary
  float _avgTreadSpeed_mmps = 0.0f;       // Derived
  float _treadAccel_mmpms2 = 0.0f;        // Derived
  float _turnSpeed_mmps = 0.0f;           // Derived
  // Head Vals
  float _headAngle_rad = 0.0f;            // Primary
  float _headSpeed_rpms  = 0.0f;          // Derived
  float _headAccel_rpms2 = 0.0f;          // Derived
  // Lift Vals
  float _liftAngle_rad = 0.0f;            // Primary
  float _liftSpeed_rpms = 0.0f;           // Derived
  float _liftAccel_rpms2 = 0.0f;          // Derived
  
  // The methods below need to be called after setting all the primary frame's values
  void ComputeAverageTreadSpeed();
  void ComputeTurnSpeed();
  
  // The methods below need the previous frame to calculate
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // NOTE: Call after ComputeAverageTreadSpeed()
  void ComputeTreadAcceleration(const AudioProceduralFrame& previousFrame);
  
  void ComputeHeadSpeed(const AudioProceduralFrame& previousFrame);
  
  // NOTE: Call after ComputeHeadSpeed()
  void ComputeHeadAcceleration(const AudioProceduralFrame& previousFrame);
  
  void ComputeLiftSpeed(const AudioProceduralFrame& previousFrame);
  
  // NOTE: Call after ComputeLiftSpeed()
  void ComputeLiftAcceleration(const AudioProceduralFrame& previousFrame);
};

} // Audio
} // Cozmo
} // Anki

#endif // __Anki_Cozmo_AudioProceduralFrame_H__
