/**
 * File: proceduralAudioClient.cpp
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


#include "audioEngine/audioTypeTranslator.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "cozmoAnim/audio/audioProceduralFrame.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/audio/proceduralAudioClient.h"
#include "util/helpers/templateHelpers.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include <fstream>
#include <string>


namespace Anki {
namespace Vector {
namespace Audio {
  
#define ENABLE_TREAD_LOG 0
#define ENABLE_HEAD_LOG 0
#define ENABLE_LIFT_LOG 0
#define ALLOW_CVS_LOG 0

#if ENABLE_TREAD_LOG
#define TREAD_LOG(msg, ...) PRINT_NAMED_WARNING("ProceduralAudioClient.UpdateTreadState", msg, ##__VA_ARGS__)
#else
#define TREAD_LOG(msg, ...)
#endif

#if ENABLE_HEAD_LOG
#define HEAD_LOG(msg, ...) PRINT_NAMED_WARNING("ProceduralAudioClient.UpdateHeadState", msg, ##__VA_ARGS__)
#else
#define HEAD_LOG(msg, ...)
#endif
  
#if ENABLE_LIFT_LOG
#define LIFT_LOG(msg, ...) PRINT_NAMED_WARNING("ProceduralAudioClient.UpdateLiftState", msg, ##__VA_ARGS__)
#else
#define LIFT_LOG(msg, ...)
#endif

namespace {
  #define CONSOLE_PATH "Audio.Procedural"
  CONSOLE_VAR(bool, kEnableHeadProceduralMovement, CONSOLE_PATH, false);
  CONSOLE_VAR(bool, kEnableLiftProceduralMovement, CONSOLE_PATH, false);
  CONSOLE_VAR(bool, kEnableTreadProceduralMovement, CONSOLE_PATH, true);
  CONSOLE_VAR_RANGED(uint32_t, kTreadCoolDown_ms, CONSOLE_PATH, 65, 0, 250);
  CONSOLE_VAR_RANGED(uint32_t, kHeadCoolDown_ms, CONSOLE_PATH, 65, 0, 250);
  CONSOLE_VAR_RANGED(uint32_t, kLiftCoolDown_ms, CONSOLE_PATH, 65, 0, 250);
  
  static const AudioEngine::AudioGameObject kProceduralGameObj =
    AudioEngine::ToAudioGameObject(AudioMetaData::GameObjectType::Procedural);

  static const uint kFrameCount = 2;

#if ALLOW_CVS_LOG
  CONSOLE_VAR(bool, kEnableRobotStateLog, CONSOLE_PATH, false);
  std::ofstream outputFile;
  const size_t _reserveSize = 30;
  std::string _logBuffer[_reserveSize];
  size_t _idx = 0;
  bool _createdFile = false;
  const char* _fileName = "/tmp/RobotStateMsgLog.csv";
  
  void CreateFile()
  {
    outputFile.open(_fileName);
    outputFile << AudioProceduralFrame::CvsLogHeader() << std::endl;
  }
  
  void WriteLog()
  {
    if (!_createdFile) {
      CreateFile();
      _createdFile = true;
    }
    for (size_t logIdx = 0; logIdx < _reserveSize; ++logIdx) {
      outputFile << _logBuffer[logIdx] << std::endl;
    }
  }
  
  void AddFrame(const AudioProceduralFrame& frame)
  {
    if (kEnableRobotStateLog) {
      _logBuffer[_idx++] = frame.CvsLogFrameData();
      if (_idx >= _reserveSize) {
        WriteLog();
        _idx = 0;
      }
    }
  }
#define LOG_CVS_FRAME(frame)  AddFrame(frame)
#else
#define LOG_CVS_FRAME(frame)
#endif
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ProceduralAudioClient
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ProceduralAudioClient::ProceduralAudioClient(CozmoAudioController* audioController)
: _audioController(audioController)
, _frames(new AudioProceduralFrame[kFrameCount])
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ProceduralAudioClient::~ProceduralAudioClient()
{
  Util::SafeDeleteArray(_frames);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ProceduralAudioClient::ProcessMessage(const RobotInterface::RobotToEngine &msg)
{
  using namespace RobotInterface;
  switch (msg.tag) {
    case RobotToEngine::Tag_state:
    {
      HandleStateMessage(msg);
      break;
    }
      
    case RobotToEngine::Tag_syncRobotAck:
    {
      // Wait for syncRobotAck to indicate the robot's motors are settled and ready to start making noise
      // NOTE: This may need improvments, perhaps use motor calibration messages
      _isActive = true;
      break;
    }
    default:
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ProceduralAudioClient::HandleStateMessage(const RobotInterface::RobotToEngine& msg)
{
  // NOTE: First couple frames are unreliable because of init state.
  //       isActive is set to false while the first frames are collected.
  const AudioProceduralFrame& previousFrame = _frames[_currentFrameIdx];
  // Toggle between frames
  _currentFrameIdx = ++_currentFrameIdx % kFrameCount;
  AudioProceduralFrame& currentFrame = _frames[_currentFrameIdx];
  
  // Set Frame Data
  const auto& stateMsg = msg.state;
  currentFrame.UpdateFrame(stateMsg);
  currentFrame.ComputeValues(previousFrame);
  
  // Update Audio Engine when active
  if (_isActive) {
    UpdateHeadState(previousFrame, currentFrame);
    UpdateLiftState(previousFrame, currentFrame);
    UpdateTreadState(previousFrame, currentFrame);
  }
  
  LOG_CVS_FRAME(currentFrame);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ProceduralAudioClient::UpdateTreadState(const AudioProceduralFrame& previousFrame,
                                             const AudioProceduralFrame& currentFrame)
{
  if (!kEnableTreadProceduralMovement) { return; }
  
  using namespace AudioEngine;
  using GE = AudioMetaData::GameEvent::GenericEvent;
  using GP = AudioMetaData::GameParameter::ParameterType;

  // If the spinSpeedRTPC is > 0 audio engine interprets it as a point turn, otherwise it uses tread speed
  static const float kDefaultSpinRtpc = -0.01f;
  auto treadEvent = GE::Invalid;
  bool setRTPC = false;
  const bool inCoolDown = (currentFrame.GetTimeStamp_ms() < _treadCoolDownExpiresTimeStamp_ms);
  const bool stateUpdated = FrameStateUpdate(inCoolDown, currentFrame.IsTreadMoving(), _treadFrameState, setRTPC);
  if (stateUpdated) {
    switch (_treadFrameState) {
      case FrameState::Stopped:
      {
        treadEvent = GE::Stop__Robot_Vic_Sfx__Tread_Loop_Stop;
        _treadCoolDownExpiresTimeStamp_ms = currentFrame.GetTimeStamp_ms() + kTreadCoolDown_ms;
        TREAD_LOG("Stop");
        break;
      }

      case FrameState::Started:
      {
        treadEvent = GE::Play__Robot_Vic_Sfx__Tread_Loop_Play;
        TREAD_LOG("Start");
        break;
      }

      case FrameState::PendingStart:
      {
        // Do nothing
        TREAD_LOG("Pending Start");
        break;
      }
    }
  }
  
  if (setRTPC) {
    // Check turn state
    float spinSpeedRTPC = kDefaultSpinRtpc;
    const float turnSpeedAbs = Util::Abs(currentFrame.GetTurnSpeed_mmps());
    const float maxWheelSpeed = Util::Max(currentFrame.GetLeftTreadSpeed_mmps(), currentFrame.GetRightTreadSpeed_mmps());
    const float minWheelSpeed = Util::Min(currentFrame.GetLeftTreadSpeed_mmps(), currentFrame.GetRightTreadSpeed_mmps());
    
    if ((turnSpeedAbs > maxWheelSpeed) && (turnSpeedAbs > Util::Abs(minWheelSpeed))) {
      // Point Turn
      spinSpeedRTPC = currentFrame.GetNormalizedTurnSpeed();
    }
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Speed),
                                   currentFrame.GetNormalizedTreadSpeed(),
                                   kProceduralGameObj);
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Accelerate),
                                   currentFrame.GetNormalizedTreadAcceleration(),
                                   kProceduralGameObj);
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Spin_Speed),
                                   spinSpeedRTPC,
                                   kProceduralGameObj);
    TREAD_LOG("FRAME: %s", currentFrame.CvsLogFrameData().c_str());
  }

  if (GE::Invalid != treadEvent) {
    _audioController->PostAudioEvent(AudioEngine::ToAudioEventId(treadEvent), kProceduralGameObj);
    // After stop event reset RTPCs
    if (GE::Stop__Robot_Vic_Sfx__Tread_Loop_Stop == treadEvent) {
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Speed),
                                     0.0f,
                                     kProceduralGameObj);
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Accelerate),
                                     0.0f,
                                     kProceduralGameObj);
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Tread_Spin_Speed),
                                     kDefaultSpinRtpc,
                                     kProceduralGameObj);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ProceduralAudioClient::UpdateHeadState(const AudioProceduralFrame& previousFrame,
                                            const AudioProceduralFrame& currentFrame)
{
  if (!kEnableHeadProceduralMovement) { return; }
  
  using namespace AudioEngine;
  using GE = AudioMetaData::GameEvent::GenericEvent;
  using GP = AudioMetaData::GameParameter::ParameterType;
  
  auto headEvent = GE::Invalid;
  bool setRTPC = false;
  const bool inCoolDown = (currentFrame.GetTimeStamp_ms() < _headCoolDownExpiresTimeStamp_ms);
  const bool stateUpdated = FrameStateUpdate(inCoolDown, currentFrame.IsHeadMoving(), _headFrameState, setRTPC);
  if (stateUpdated) {
    switch (_headFrameState) {
      case FrameState::Stopped:
      {
        headEvent = GE::Stop__Robot_Vic_Sfx__Head_Loop_Stop;
        _headCoolDownExpiresTimeStamp_ms = currentFrame.GetTimeStamp_ms() + kHeadCoolDown_ms;
        HEAD_LOG("Stop");
        break;
      }
        
      case FrameState::Started:
      {
        headEvent = GE::Play__Robot_Vic_Sfx__Head_Loop_Play;
        HEAD_LOG("Start");
        break;
      }
        
      case FrameState::PendingStart:
      {
        // Do nothing
        HEAD_LOG("Pending Start");
        break;
      }
    }
  }
  
  if (setRTPC) {
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Head_Speed),
                                   currentFrame.GetNormalizedHeadSpeed(),
                                   kProceduralGameObj);
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Head_Accelerate),
                                   currentFrame.GetNormalizedHeadAcceleration(),
                                   kProceduralGameObj);
    HEAD_LOG("FRAME: %s", currentFrame.CvsLogFrameData().c_str());
  }
  
  if (GE::Invalid != headEvent) {
    _audioController->PostAudioEvent(ToAudioEventId(headEvent), kProceduralGameObj);
    // After stop event reset RTPCs
    if (GE::Stop__Robot_Vic_Sfx__Head_Loop_Stop == headEvent) {
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Head_Speed),
                                     0.0f,
                                     kProceduralGameObj);
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Head_Accelerate),
                                     0.0f,
                                     kProceduralGameObj);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ProceduralAudioClient::UpdateLiftState(const AudioProceduralFrame& previousFrame,
                                            const AudioProceduralFrame& currentFrame)
{
  if (!kEnableLiftProceduralMovement) { return; }
  
  using namespace AudioEngine;
  using GE = AudioMetaData::GameEvent::GenericEvent;
  using GP = AudioMetaData::GameParameter::ParameterType;
  
  auto liftEvent = GE::Invalid;
  bool setRTPC = false;
  const bool inCoolDown = (currentFrame.GetTimeStamp_ms() < _liftCoolDownExpiresTimeStamp_ms);
  const bool stateUpdated = FrameStateUpdate(inCoolDown, currentFrame.IsLiftMoving(), _liftFrameState, setRTPC);
  if (stateUpdated) {
    switch (_liftFrameState) {
      case FrameState::Stopped:
      {
        liftEvent = GE::Stop__Robot_Vic_Sfx__Lift_Loop_Stop;
        _liftCoolDownExpiresTimeStamp_ms = currentFrame.GetTimeStamp_ms() + kLiftCoolDown_ms;
        LIFT_LOG("Stop");
        break;
      }
        
      case FrameState::Started:
      {
        liftEvent = GE::Play__Robot_Vic_Sfx__Lift_Loop_Play;
        LIFT_LOG("Start");
        break;
      }
        
      case FrameState::PendingStart:
      {
        // Do nothing
        LIFT_LOG("Pending Start");
        break;
      }
    }
  }
  
  if (setRTPC) {
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Lift_Speed),
                                   currentFrame.GetNormalizedLiftSpeed(),
                                   kProceduralGameObj);
    _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Lift_Accelerate),
                                   currentFrame.GetNormalizedLiftAcceleration(),
                                   kProceduralGameObj);
    LIFT_LOG("FRAME: %s", currentFrame.CvsLogFrameData().c_str());
  }
  
  if (GE::Invalid != liftEvent) {
    _audioController->PostAudioEvent(ToAudioEventId(liftEvent), kProceduralGameObj);
    // After stop event reset RTPCs
    if (GE::Stop__Robot_Vic_Sfx__Lift_Loop_Stop == liftEvent) {
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Lift_Speed),
                                     0.0f,
                                     kProceduralGameObj);
      _audioController->SetParameter(ToAudioParameterId(GP::Robot_Vic_Lift_Accelerate),
                                     0.0f,
                                     kProceduralGameObj);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool ProceduralAudioClient::FrameStateUpdate(bool inCoolDown,
                                             bool isMoving,
                                             FrameState& inOut_currentState,
                                             bool& out_SetRtpc)
{
  bool didUpdate = false;
  out_SetRtpc = false;

  switch ( inOut_currentState ) {

    case FrameState::Stopped:
    {
      if (isMoving) {
        if (inCoolDown) {
          // In Cooldown and started moving
          inOut_currentState = FrameState::PendingStart;
        }
        else {
          // Start movement
          out_SetRtpc = true;
          inOut_currentState = FrameState::Started;
        }
        // Started or PendingStart state set
        didUpdate = true;
      }
      // else do nothing
      break;
    }
     
    case FrameState::PendingStart:
    {
      if (!inCoolDown && isMoving) {
        // Delayed start
        out_SetRtpc = true;
        inOut_currentState = FrameState::Started;
        didUpdate = true;
      }
      break;
    }
      
    case FrameState::Started:
    {
      if (!isMoving) {
        // Stopped
        inOut_currentState = FrameState::Stopped;
        didUpdate = true;
      }
      // Stopped or currently moving
      out_SetRtpc = true;
      break;
    }
  }
  
  return didUpdate;
}

}
}
}

