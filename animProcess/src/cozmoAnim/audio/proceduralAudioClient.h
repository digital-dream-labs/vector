/**
 * File: proceduralAudioClient.h
 *
 * Author: Jordan Rivas
 * Created: 03/15/18
 *
 * Description: Procedural Audio Client handles robot state driven audio features. By intercepting robot to engine
 *              messages the audio client can track robot state and events to perform audio tasks.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#ifndef __Anki_Cozmo_ProceduralAudioClient_H__
#define __Anki_Cozmo_ProceduralAudioClient_H__

#include "anki/cozmo/shared/cozmoConfig.h"
#include "audioEngine/audioTypes.h"


namespace Anki {
namespace Vector {
namespace RobotInterface {
struct RobotToEngine;
}
namespace Audio {
class CozmoAudioController;
class AudioProceduralFrame;


class ProceduralAudioClient {

public:

  ProceduralAudioClient(CozmoAudioController* audioController);
  ~ProceduralAudioClient();

  bool GetIsActive() const { return _isActive; }

  void ProcessMessage(const RobotInterface::RobotToEngine& msg);


private:

  // Track movement state of robot's treads, head & lift
  enum class FrameState {
    Stopped,
    PendingStart,
    Started
  };

  CozmoAudioController*  _audioController = nullptr;
  size_t _currentFrameIdx = 0;
  AudioProceduralFrame* _frames = nullptr;
  bool _isActive = false;

  // Track frame states and cool down
  FrameState _treadFrameState = FrameState::Stopped;
  FrameState _headFrameState   = FrameState::Stopped;
  FrameState _liftFrameState   = FrameState::Stopped;
  
  uint32_t _treadCoolDownExpiresTimeStamp_ms = 0;
  uint32_t _headCoolDownExpiresTimeStamp_ms  = 0;
  uint32_t _liftCoolDownExpiresTimeStamp_ms  = 0;
  
  
  // Handle Robot Messages
  void HandleStateMessage(const RobotInterface::RobotToEngine& msg);
  
  // Update Procedural Audio component states
  void UpdateTreadState(const AudioProceduralFrame& previousFrame, const AudioProceduralFrame& currentFrame);
  void UpdateHeadState(const AudioProceduralFrame& previousFrame, const AudioProceduralFrame& currentFrame);
  void UpdateLiftState(const AudioProceduralFrame& previousFrame, const AudioProceduralFrame& currentFrame);
  
  // Helper method to identify movement state changes
  // return true of inOut_currentState was updated
  bool FrameStateUpdate(bool inCoolDown, bool isMoving, FrameState& inOut_currentState, bool& out_SetRtpc);
};

}
}
}

#endif // __Anki_Cozmo_ProceduralAudioClient_H__
