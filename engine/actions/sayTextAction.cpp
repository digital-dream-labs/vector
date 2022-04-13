/**
 * File: sayTextAction.cpp
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements animation and audio cozmo-specific actions, derived from the IAction interface.
 *
 * Update: Say Text Action now uses TextToSpeechCoordinator to perform work
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/actions/sayTextAction.h"
#include "engine/animations/animationGroup/animationGroup.h"
#include "engine/animations/animationGroup/animationGroupContainer.h"
#include "engine/robot.h"
#include "coretech/common/engine/utils/timer.h"

#include "util/fileUtils/fileUtils.h"
#include "util/math/math.h"
#include "util/random/randomGenerator.h"


#define LOG_CHANNEL "SayTextAction"

#define DEBUG_SAYTEXT_ACTION 0

// Max duration of generated animation
//const float kMaxAnimationDuration_ms = 60000;  // 1 min

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SayTextAction::SayTextAction(const std::string& text,
                             const AudioTtsProcessingStyle style,
                             const float durationScalar)
: IAction("SayText",
          RobotActionType::SAY_TEXT,
          (u8)AnimTrackFlag::NO_TRACKS)
, _text(text)
, _style(style)
, _durationScalar(durationScalar)
{
  _animTrigger = AnimationTrigger::Count;
} // SayTextAction()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SayTextAction::~SayTextAction()
{
  // Cleanup TTS request, if any
  if (_ttsCoordinator != nullptr) {
    if (_ttsState == UtteranceState::Generating ||
        _ttsState == UtteranceState::Ready ||
        _ttsState == UtteranceState::Playing) {
      _ttsCoordinator->CancelUtterance( _ttsID );
    }
    _ttsCoordinator = nullptr;
  }

  // Clean up accompanying animation, if any
  if (_animAction) {
    _animAction->PrepForCompletion();
  }
} // ~SayTextAction()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SayTextAction::OnRobotSet()
{
  LOG_INFO("SayTextAction.RobotSet",
           "Text '%s' Style '%s' DurScalar %f",
           Util::HidePersonallyIdentifiableInfo(_text.c_str()),
           EnumToString(_style),
           _durationScalar);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SayTextAction::SetAnimationTrigger(AnimationTrigger trigger, u8 ignoreTracks)
{
  _animTrigger = trigger;
  _ignoreAnimTracks = ignoreTracks;
} // SetAnimationTrigger()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult SayTextAction::Init()
{
  //
  // If we have an animation, use keyframe trigger, else use manual trigger.
  //
  const auto triggerType =
    ((_animTrigger == AnimationTrigger::Count) ? UtteranceTriggerType::Manual : UtteranceTriggerType::KeyFrame);

  _ttsCoordinator = &GetRobot().GetTextToSpeechCoordinator();
  _callbackPtr = std::make_shared<CallbackType>(std::bind(&SayTextAction::TtsCoordinatorStateCallback, this, std::placeholders::_1));
  std::weak_ptr<CallbackType> weakCallback = _callbackPtr;
  auto ttsCallback = [weakCallback](const UtteranceState& state) {
    // SayTextAction may have been aborted and destroyed before the callback fires
    if( auto callback = weakCallback.lock() ) {
      (*callback)(state);
    }
  };
  _ttsID = _ttsCoordinator->CreateUtterance(_text,
                                            triggerType,
                                            _style,
                                            _durationScalar,
                                            _pitchScalar,
                                            ttsCallback);

  _actionState = SayTextActionState::Waiting;

  // When does this action expire?
  _expiration_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds() + _timeout_sec;

  LOG_INFO("SayTextAction.Init", "ttsID %d text %s", _ttsID, Util::HidePersonallyIdentifiableInfo(_text.c_str()));

  // Execution continues in CheckIfDone() below.
  // State is advanced in response to events from animation process.

  return ActionResult::SUCCESS;

} // Init()

ActionResult SayTextAction::TransitionToRunning()
{
  DEV_ASSERT(_ttsCoordinator != nullptr, "SayTextAction.TransitionToRunning.InvalidCoordinator");

  const bool ok = _ttsCoordinator->PlayUtterance(_ttsID);
  if (!ok) {
    LOG_ERROR("SayTextAction.TransitionToRunning.FailedToPlay", "Unable to play ttsID %d", _ttsID);
    _actionState = SayTextActionState::Invalid;
    return ActionResult::ABORT;
  }

  if (_animTrigger != AnimationTrigger::Count) {
    LOG_DEBUG("SayTextAction.TransitionToRunning", "ttsID %d now running with animation", _ttsID);
    _animAction = std::make_unique<TriggerAnimationAction>(_animTrigger, 1, true, _ignoreAnimTracks);
    _animAction->SetRobot(&GetRobot());
    _actionState = SayTextActionState::Running_Anim;
    return ActionResult::RUNNING;
  }

  LOG_DEBUG("SayTextAction.TransitionToRunning", "ttsID %d now running", _ttsID);
  _actionState = SayTextActionState::Running_Tts;
  return ActionResult::RUNNING;
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult SayTextAction::CheckIfDone()
{
  // Has this action expired?
  const float now_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
  if (_expiration_sec < now_sec) {
    LOG_DEBUG("SayTextAction.CheckIfDone", "ttsID %d has expired", _ttsID);
    return ActionResult::TIMEOUT;
  }

  ActionResult result;
  switch (_actionState) {
    case SayTextActionState::Invalid:
    {
      // Something has gone wrong
      LOG_DEBUG("SayTextAction.CheckIfDone", "ttsID %d is invalid", _ttsID);
      result = ActionResult::CANCELLED_WHILE_RUNNING;
      break;
    }
    case SayTextActionState::Waiting:
    {
      result = ActionResult::RUNNING;
      if (_ttsState == UtteranceState::Ready) {
        // Transition to running
        result = TransitionToRunning();
      }
      break;
    }
    case SayTextActionState::Running_Tts:
    {
      // Defer to TtS Coordinator State
      result = GetTtsCoordinatorActionState();
      break;
    }
    case SayTextActionState::Running_Anim:
    {
      // Tick animation while running, will return success when animation is completed
      result = _animAction->Update();
      // If animation has completed, defer to TTS Coordinator State
      if (result == ActionResult::SUCCESS) {
        result = GetTtsCoordinatorActionState();
      }
      break;
    }
    case SayTextActionState::Finished:
    {
      result = ActionResult::SUCCESS;
      break;
    }
  }

  return result;
} // CheckIfDone()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SayTextAction::TtsCoordinatorStateCallback(const UtteranceState& state)
{
  LOG_DEBUG("SayTextAction.TtsCoordinatorStateCallback",
            "ttsID %d now state %d (%s)",
            _ttsID, state, EnumToString(state));
  _ttsState = state;
} // TtsCoordinatorStateCallback()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ActionResult SayTextAction::GetTtsCoordinatorActionState()
{
  switch (_ttsState) {
    case UtteranceState::Invalid:
      return ActionResult::ABORT;
      break;
    case UtteranceState::Finished:
      return ActionResult::SUCCESS;
    default:
      return ActionResult::RUNNING;
      break;
  }
} // GetTtsCoordinatorActionState()


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if 0
// VIC-2151: Fit-to-duration not supported on victor
void SayTextAction::UpdateAnimationToFitDuration(const float duration_ms)
{
  if (AnimationTrigger::Count != _animationTrigger) {
    // TODO: SayTextAction is broken (VIC-360)
    while (_animation.GetLastKeyFrameTime_ms() < duration_ms && duration_ms <= kMaxAnimationDuration_ms ) {
      const Animation* nextAnim = GetAnimation(_animationTrigger, _robot);
      if (nullptr != nextAnim) {
        _animation.AppendAnimation(*nextAnim);
      }
      else {
        PRINT_NAMED_ERROR("SayTextAction.UpdateAnimationToFitDuration.GetAnimationFailed",
                          "AnimationTrigger: %s", EnumToString(_animationTrigger));
        break;
      }
    }
  }
  else {
    PRINT_NAMED_WARNING("SayTextAction.UpdateAnimationToFitDuration.InvalidAnimationTrigger", "AnimationTrigger::Count");
  }
} // UpdateAnimationToFitDuration()
#endif

} // namespace Vector
} // namespace Anki
