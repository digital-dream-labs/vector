/**
 * File: sayTextAction.h
 *
 * Author: Andrew Stein
 * Date:   4/23/2016
 *
 * Updated By: Jordan Rivas 06/18/18
 *
 * Description: Defines an action for having Cozmo "say" a text string.
 *
 * Update: Say Text Action now uses TextToSpeechCoordinator to perform work
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Anki_Cozmo_Actions_SayTextAction_H__
#define __Anki_Cozmo_Actions_SayTextAction_H__

#include "engine/components/textToSpeech/textToSpeechCoordinator.h"
#include "engine/actions/actionInterface.h"
#include "engine/actions/animActions.h"
#include "clad/types/textToSpeechTypes.h"
#include "util/helpers/templateHelpers.h"
#include "util/signals/simpleSignal_fwd.h"


namespace Anki {
namespace Vector {

enum class AnimationTrigger : int32_t;

class SayTextAction : public IAction
{
public:
  using AudioTtsProcessingStyle = AudioMetaData::SwitchState::Robot_Vic_External_Processing;

  // Customize the text to speech creation by setting the voice style and duration scalar.
  // Note: The duration scalar stretches the duration of the generated TtS audio. When using the unprocessed voice
  //       you can use a value around 1.0 which is the TtS generator normal speed. When using CozmoProcessing
  //       it is more common to use a value between 1.8 - 2.3 which gets sped up in the audio engine resulting in a
  //       duration close to the unprocessed voice.
  SayTextAction(const std::string& text,
                const AudioTtsProcessingStyle style = AudioTtsProcessingStyle::Default_Processed,
                const float durationScalar = 1.f);

  virtual ~SayTextAction();

  virtual f32 GetTimeoutInSeconds() const override { return _timeout_sec; }

  // Use an animation group tied to a specific GameEvent.
  // Use AnimationTrigger::Count to use built-in animation (default).
  // The animation group should contain animations that have the special audio keyframe for
  // Audio::GameEvent::GenericEvent::Play__Robot_Vic__External_Voice_Text
  void SetAnimationTrigger(AnimationTrigger trigger, u8 ignoreTracks = 0);

protected:

  // IAction interface methods
  virtual void OnRobotSet() override final;
  virtual ActionResult Init() override;
  virtual ActionResult CheckIfDone() override;

private:

  enum class SayTextActionState : uint8_t {
    Invalid,
    Waiting,
    Running_Tts,
    Running_Anim,
    Finished
  };

  // TtS Cordinator
  TextToSpeechCoordinator*        _ttsCoordinator     = nullptr;

  // TTS parameters
  uint8_t                         _ttsID              = 0; // ID 0 is reserved for "invalid".
  std::string                     _text;
  AudioTtsProcessingStyle         _style              = AudioTtsProcessingStyle::Invalid;
  float                           _durationScalar     = 1.f;
  float                           _pitchScalar        = 0.f;

  // Accompanying animation, if any
  AnimationTrigger                _animTrigger;  // Count == use built-in animation
  u8                              _ignoreAnimTracks   = (u8)AnimTrackFlag::NO_TRACKS;
  std::unique_ptr<IActionRunner>  _animAction         = nullptr;

  // Bookkeeping
  SayTextActionState              _actionState        = SayTextActionState::Invalid;
  UtteranceState                  _ttsState           = UtteranceState::Invalid;
  f32                             _timeout_sec        = 30.f;
  f32                             _expiration_sec     = 0.f;

  // Internal state machine
  void TtsCoordinatorStateCallback(const UtteranceState& state);
  ActionResult GetTtsCoordinatorActionState();
  ActionResult TransitionToRunning();

  using CallbackType = std::function<void(const UtteranceState& state)>;
  std::shared_ptr<CallbackType> _callbackPtr;

  // VIC-2151: Fit-to-duration not supported on victor
  // DEPRECATED: This feature has been moved to behaviors using TextToSpeechCoordinator
  // Append animation by stitching animation trigger group animations together until the animation's duration is
  // greater or equal to the provided
  // void UpdateAnimationToFitDuration(const float duration_ms);


}; // class SayTextAction


} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Actions_SayTextAction_H__ */
