/*
 * File: animationAudioClient.cpp
 *
 * Author: Jordan Rivas
 * Created: 09/12/17
 *
 * Description: Animation Audio Client is the interface to perform animation audio specific tasks. Provided a
 *              RobotAudioKeyFrame to handle the necessary audio functionality for that frame. It also provides an
 *              interface to abort animation audio and update (a.k.a. “tick”) the Audio Engine each frame.
 *
 * Copyright: Anki, Inc. 2017
 */


#include "audioEngine/audioTypeTranslator.h"
#include "cozmoAnim/audio/animationAudioClient.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/textToSpeech/textToSpeechComponent.h"
#include "cannedAnimLib/baseTypes/keyframe.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"
#include "util/math/math.h"


#define kEnableAudioEventProbability 1

#define ENABLE_DEBUG_LOG 0
#if ENABLE_DEBUG_LOG
  #define AUDIO_DEBUG_LOG( name, format, args... ) PRINT_CH_DEBUG( "Audio", name, format, ##args )
#else
  #define AUDIO_DEBUG_LOG( name, format, args... )
#endif

namespace Anki {
namespace Vector {
namespace Audio {

using namespace AudioEngine;
using namespace AudioMetaData;
using namespace AudioKeyFrameType;

static const AudioGameObject kAnimGameObj = ToAudioGameObject(GameObjectType::Animation);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AnimationAudioClient::AnimationAudioClient( CozmoAudioController* audioController )
: _audioController( audioController )
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AnimationAudioClient::~AnimationAudioClient()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::InitAnimation()
{
  // Clear events (if any) from the previous animation
  std::lock_guard<std::mutex> lock( _lock );
  _activeEvents.clear();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::PlayAudioKeyFrame( const RobotAudioKeyFrame& keyFrame, Util::RandomGenerator* randomGen )
{
  // Set states, switches and parameters before events events.
  // NOTE: The order is corrected when loading the animation. (see keyframe.cpp RobotAudioKeyFrame methods)
  // NOTE: If the same State, Switch or Parameter is set more then once on a single frame, last one in wins!
  const RobotAudioKeyFrame::AudioRefList& audioRefs = keyFrame.GetAudioReferencesList();
  for ( const auto& ref : audioRefs ) {
    switch ( ref.Tag ) {
      case AudioRefTag::EventGroup:
        HandleAudioRef( ref.EventGroup, randomGen );
        break;
      case AudioRefTag::State:
        HandleAudioRef( ref.State );
        break;
      case AudioRefTag::Switch:
        HandleAudioRef( ref.Switch );
        break;
      case AudioRefTag::Parameter:
        HandleAudioRef( ref.Parameter );
        break;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::HandleAudioRef( const AudioEventGroupRef& eventRef, Util::RandomGenerator* randomGen )
{
  const AudioEventGroupRef::EventDef* anEvent = eventRef.RetrieveEvent( true, randomGen );
  if (nullptr == anEvent) {
    // Chance has chosen not to play an event
    return;
  }

  // Play valid event
  const auto playId = PostCozmoEvent( anEvent->AudioEvent, eventRef.GameObject );
  if ( playId != kInvalidAudioPlayingId ) {
    // Apply volume to event
    SetCozmoEventParameter( playId, GameParameter::ParameterType::Event_Volume, anEvent->Volume );
  }
  AUDIO_DEBUG_LOG("AnimationAudioClient.PlayAudioKeyFrame",
                  "Posted audio event '%s' volume %f)",
                  EnumToString(anEvent->AudioEvent), anEvent->Volume);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::HandleAudioRef( const AudioStateRef& stateRef )
{
  _audioController->SetState( ToAudioStateGroupId( stateRef.StateGroup ), ToAudioStateId( stateRef.State ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::HandleAudioRef( const AudioSwitchRef& switchRef )
{
  _audioController->SetSwitchState( ToAudioSwitchGroupId( switchRef.SwitchGroup ),
                                    ToAudioSwitchStateId( switchRef.State ),
                                    ToAudioGameObject( switchRef.GameObject ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::HandleAudioRef( const AudioParameterRef& parameterRef )
{
  _audioController->SetParameter( ToAudioParameterId( parameterRef.Parameter ),
                                  ToAudioRTPCValue( parameterRef.Value ),
                                  ToAudioGameObject( parameterRef.GameObject ),
                                  ToAudioTimeMs( parameterRef.Time_ms ),
                                  ToAudioCurveType( parameterRef.Curve ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::AbortAnimation()
{
  if ( _audioController == nullptr ) { return; }
  const auto event = ToAudioEventId( AudioMetaData::GameEvent::GenericEvent::Play__Robot_Vic_Scene__Anim_Abort );
  _audioController->PostAudioEvent( event, kAnimGameObj );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AnimationAudioClient::HasActiveEvents() const
{
  std::lock_guard<std::mutex> lock( _lock );
  return !_activeEvents.empty();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioEngine::AudioPlayingId AnimationAudioClient::PostCozmoEvent( AudioMetaData::GameEvent::GenericEvent event,
                                                                  AudioMetaData::GameObjectType gameObject )
{
  using GenericEvent = Anki::AudioMetaData::GameEvent::GenericEvent;

  if ( _audioController == nullptr ) {
    return kInvalidAudioPlayingId;
  }

  // Are we about to play a TextToSpeech utterance?
  auto ttsID = TextToSpeechComponent::kInvalidTTSID;
  if (event == GenericEvent::Play__Robot_Vic__External_Voice_Text) {
    if (_ttsComponent != nullptr) {
      ttsID = _ttsComponent->GetActiveTTSID();
    }
  }

  if (ttsID != TextToSpeechComponent::kInvalidTTSID) {
    // Notify TTS component that keyframe has been triggered
    _ttsComponent->OnAudioPlaying(ttsID);
  }

  // Set up callback function, callback context
  const auto callbackFunc = std::bind(&AnimationAudioClient::CozmoEventCallback, this, ttsID, std::placeholders::_1);
  AudioCallbackContext* audioCallbackContext = new AudioCallbackContext();
  // Set callback flags
  audioCallbackContext->SetCallbackFlags( AudioCallbackFlag::Complete );
  // Execute callbacks synchronously (on main thread)
  audioCallbackContext->SetExecuteAsync( false );
  // Register callbacks for event
  audioCallbackContext->SetEventCallbackFunc ( [ callbackFunc = std::move(callbackFunc) ]
                                               ( const AudioCallbackContext* thisContext,
                                                 const AudioCallbackInfo& callbackInfo )
                                               {
                                                 callbackFunc( callbackInfo );
                                               } );

  const auto audioEventId = ToAudioEventId( event );
  const AudioEngine::AudioPlayingId playId = _audioController->PostAudioEvent( audioEventId,
                                                                               ToAudioGameObject( gameObject ),
                                                                               audioCallbackContext );
  // Track event playback
  AddActiveEvent( playId );

  return playId;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AnimationAudioClient::SetCozmoEventParameter( AudioEngine::AudioPlayingId playId,
                                                   AudioMetaData::GameParameter::ParameterType parameter,
                                                   AudioEngine::AudioRTPCValue value ) const
{
  if ( _audioController == nullptr ) { return false; }
  return _audioController->SetParameterWithPlayingId( ToAudioParameterId( parameter ), value, playId );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::CozmoEventCallback(const uint8_t ttsID, const AudioEngine::AudioCallbackInfo& callbackInfo)
{
  switch (callbackInfo.callbackType) {

    case AudioEngine::AudioCallbackType::Complete:
    {
      const auto& info = static_cast<const AudioCompletionCallbackInfo&>( callbackInfo );
      AUDIO_DEBUG_LOG("AnimationAudioClient.PostCozmoEvent.Callback", "%s", info.GetDescription().c_str());
      RemoveActiveEvent( info.playId );
      if (ttsID != TextToSpeechComponent::kInvalidTTSID) {
        if (_ttsComponent != nullptr) {
          _ttsComponent->OnAudioComplete(ttsID);
        }
      }
      break;
    }

    case AudioCallbackType::Error:
    {
      const auto& info = static_cast<const AudioErrorCallbackInfo&>( callbackInfo );
      PRINT_NAMED_WARNING("AnimationAudioClient.PostCozmoEvent.CallbackError", "%s", info.GetDescription().c_str());
      RemoveActiveEvent( info.playId );
      if (ttsID != TextToSpeechComponent::kInvalidTTSID) {
        if (_ttsComponent != nullptr) {
          _ttsComponent->OnAudioError(ttsID);
        }
      }
      break;
    }

    case AudioEngine::AudioCallbackType::Duration:
    {
      const auto& info = static_cast<const AudioDurationCallbackInfo&>( callbackInfo );
      PRINT_NAMED_WARNING("AnimationAudioClient.PostCozmoEvent.CallbackUnexpected", "%s", info.GetDescription().c_str());
      break;
    }

    case AudioEngine::AudioCallbackType::Marker:
    {
      const auto& info = static_cast<const AudioMarkerCallbackInfo&>( callbackInfo );
      PRINT_NAMED_WARNING("AnimationAudioClient.PostCozmoEvent.CallbackUnexpected", "%s", info.GetDescription().c_str());
      break;
    }

    case AudioEngine::AudioCallbackType::Invalid:
    {
      PRINT_NAMED_WARNING("AnimationAudioClient.PostCozmoEvent.CallbackInvalid", "%s", callbackInfo.GetDescription().c_str());
      break;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::AddActiveEvent( AudioEngine::AudioPlayingId playId )
{
  if ( playId != kInvalidAudioPlayingId ) {
    std::lock_guard<std::mutex> lock( _lock );
    _activeEvents.emplace( playId );
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AnimationAudioClient::RemoveActiveEvent( AudioEngine::AudioPlayingId playId )
{
  std::lock_guard<std::mutex> lock( _lock );
  _activeEvents.erase( playId );
}

}
}
}
