/**
* File: cozmoAnim/showAudioStreamStateManager.cpp
*
* Author: Kevin M. Karol
* Created: 8/3/2018
*
* Description: Communicates the current state of cloud audio streaming to the user
* and ensures expectations of related animation components are met (e.g. motion/lack there of when streaming)
*
* Copyright: Anki, Inc. 2018
**/

#include "cozmoAnim/showAudioStreamStateManager.h"

#include "micDataTypes.h"
#include "clad/types/alexaTypes.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/audio/engineRobotAudioInput.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/robotDataLoader.h"
#include "util/string/stringUtils.h"

#include "audioEngine/audioTypeTranslator.h"

namespace{
const int32_t kUseDefaultStreamingDuration = -1;
}

namespace Anki {
namespace Vector {

ShowAudioStreamStateManager::ShowAudioStreamStateManager(const Anim::AnimContext* context)
: _context(context)
, _minStreamingDuration_ms(kUseDefaultStreamingDuration)
{
  // Initialize this value to prevent errors before the TriggerResponse is first set
  _postAudioEvent.audioEvent = AudioMetaData::GameEvent::GenericEvent::Invalid;
}


ShowAudioStreamStateManager::~ShowAudioStreamStateManager()
{

}

void ShowAudioStreamStateManager::Update()
{
  {
    std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
    if(_havePendingTriggerResponse)
    {
      if(_pendingTriggerResponseHasGetIn)
      {
        StartTriggerResponseWithGetIn(_responseCallback);
      }
      else
      {
        StartTriggerResponseWithoutGetIn(_responseCallback);
      }

      _havePendingTriggerResponse = false;
      _responseCallback = nullptr;
    }
  }
}

void ShowAudioStreamStateManager::SetTriggerWordResponse(const RobotInterface::SetTriggerWordResponse& msg)
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  _postAudioEvent = msg.postAudioEvent;
  _minStreamingDuration_ms = msg.minStreamingDuration_ms;
  _shouldTriggerWordStartStream = msg.shouldTriggerWordStartStream;
  _shouldTriggerWordSimulateStream = msg.shouldTriggerWordSimulateStream;
  _getInAnimationTag = msg.getInAnimationTag;
  _getInAnimName = std::string(msg.getInAnimationName, msg.getInAnimationName_length);
}

void ShowAudioStreamStateManager::SetPendingTriggerResponseWithGetIn(OnTriggerAudioCompleteCallback callback)
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  if(_havePendingTriggerResponse)
  {
    PRINT_NAMED_WARNING("ShowAudioStreamStateManager.SetPendingTriggerResponseWithGetIn.ExistingResponse",
                        "Already have pending trigger response, overriding");
  }
  _havePendingTriggerResponse = true;
  _pendingTriggerResponseHasGetIn = true;
  _responseCallback = callback;
}

void ShowAudioStreamStateManager::SetPendingTriggerResponseWithoutGetIn(OnTriggerAudioCompleteCallback callback)
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  if(_havePendingTriggerResponse)
  {
    PRINT_NAMED_WARNING("ShowAudioStreamStateManager.SetPendingTriggerResponseWithoutGetIn.ExistingResponse",
                        "Already have pending trigger response, overriding");
  }
  _havePendingTriggerResponse = true;
  _pendingTriggerResponseHasGetIn = false;
  _responseCallback = callback;
}


void ShowAudioStreamStateManager::StartTriggerResponseWithGetIn(OnTriggerAudioCompleteCallback callback)
{
  if(!HasValidTriggerResponse()){
    if(callback){
      callback(false);
    }
    return;
  }

  auto* anim = _context->GetDataLoader()->GetCannedAnimation(_getInAnimName);
  if((_streamer != nullptr) && (anim != nullptr)){
    _streamer->SetStreamingAnimation(_getInAnimName, _getInAnimationTag);
  }else{
    PRINT_NAMED_ERROR("ShowAudioStreamStateManager.StartTriggerResponseWithGetIn.NoValidGetInAnimation",
                      "Animation not found for get in %s", _getInAnimName.c_str());
  }
  StartTriggerResponseWithoutGetIn(std::move(callback));
}


void ShowAudioStreamStateManager::StartTriggerResponseWithoutGetIn(OnTriggerAudioCompleteCallback callback)
{
  using namespace AudioEngine;

  if(!HasValidTriggerResponse()){
    if(callback){
      callback(false);
    }
    return;
  }

  Audio::CozmoAudioController* controller = _context->GetAudioController();
  if (nullptr != controller) {
    AudioCallbackContext* audioCallbackContext = nullptr;
    if (callback) {
      audioCallbackContext = new AudioCallbackContext();
      audioCallbackContext->SetCallbackFlags( AudioCallbackFlag::Complete );
      audioCallbackContext->SetExecuteAsync( false ); // Execute callbacks synchronously (on main thread)
      audioCallbackContext->SetEventCallbackFunc([callbackFunc = callback]
                                                 (const AudioCallbackContext* thisContext, const AudioCallbackInfo& callbackInfo)
      {
        callbackFunc(true);
      });
    }

    AudioPlayingId result = controller->PostAudioEvent(ToAudioEventId(_postAudioEvent.audioEvent),
                                                       ToAudioGameObject(_postAudioEvent.gameObject),
                                                       audioCallbackContext);

    // if we failed to post the earcon, we still want the callback to be called successfully since we've still
    // completed the get-in process.  the unsuccessful callback is for when no valid response exists ... in this
    // case, it DOES exists, the audio engine is just being an ass right now
    if ( AudioEngine::kInvalidAudioPlayingId == result )
    {
      callback(true);
    }
  }
  else
  {
    // even though we don't have a valid audio controller, we still had a valid trigger response so return true
    if (callback) {
      callback(true);
    }
  }
}


bool ShowAudioStreamStateManager::HasValidTriggerResponse()
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  return _postAudioEvent.audioEvent != AudioMetaData::GameEvent::GenericEvent::Invalid;
}


bool ShowAudioStreamStateManager::ShouldStreamAfterTriggerWordResponse()
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  return HasValidTriggerResponse() && _shouldTriggerWordStartStream;
}

bool ShowAudioStreamStateManager::ShouldSimulateStreamAfterTriggerWord()
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);
  return HasValidTriggerResponse() && _shouldTriggerWordSimulateStream;
}

void ShowAudioStreamStateManager::SetAlexaUXResponses(const RobotInterface::SetAlexaUXResponses& msg)
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex); // HasAnyAlexaResponse may be called off thread

  _alexaResponses.clear();
  const std::string csvResponses{msg.csvGetInAnimNames, msg.csvGetInAnimNames_length};
  const std::vector<std::string> animNames = Util::StringSplit(csvResponses, ',');
  int maxAnims = 4;
  if( !ANKI_VERIFY(animNames.size() == 4,
                   "ShowAudioStreamStateManager.SetAlexaUXResponses.UnexpectedCnt",
                   "Expecting 4 anim names, received %zu",
                   animNames.size()) )
  {
    maxAnims = std::min((int)animNames.size(), 4);
  }
  static_assert( sizeof(msg.postAudioEvents) / sizeof(msg.postAudioEvents[0]) == 4, "Expected 4 elems" );
  static_assert( sizeof(msg.getInAnimTags) / sizeof(msg.getInAnimTags[0]) == 4, "Expected 4 elems" );
  for( int i=0; i<maxAnims; ++i ) {
    AlexaInfo info;
    info.state = static_cast<AlexaUXState>(i);
    info.audioEvent = msg.postAudioEvents[i];
    info.getInAnimTag = msg.getInAnimTags[i];
    info.getInAnimName = animNames[i];

    PRINT_CH_INFO( "Alexa", "Alexa.SetAlexaUXResponses.response",
                   "%d: %s (tag %d)",
                   i,
                   info.getInAnimName.c_str(),
                   info.getInAnimTag);

    _alexaResponses.push_back( std::move(info) );
  }
}

uint32_t ShowAudioStreamStateManager::GetMinStreamingDuration()
{
  if( _minStreamingDuration_ms > kUseDefaultStreamingDuration ){
    return _minStreamingDuration_ms;
  }
  else{
    return MicData::kStreamingDefaultMinDuration_ms;
  }
}

bool ShowAudioStreamStateManager::HasAnyAlexaResponse() const
{
  std::lock_guard<std::recursive_mutex> lock(_triggerResponseMutex);

  for( const auto& info : _alexaResponses ) {
    if( info.getInAnimTag != 0 ) {
      return true;
    }
  }
  return false;
}

bool ShowAudioStreamStateManager::HasValidAlexaUXResponse(AlexaUXState state) const
{
  for( const auto& info : _alexaResponses ) {
    if( info.state == state ) {
      // unlike wake word responses, which are valid if there is an audio event, Alexa UX responses are valid if a
      // nonzero anim tag was provided.
      return (info.getInAnimTag != 0);
    }
  }
  return false;
}

bool ShowAudioStreamStateManager::StartAlexaResponse(AlexaUXState state, bool ignoreGetIn)
{
  const AlexaInfo* response = nullptr;
  for( const auto& info : _alexaResponses ) {
    // unlike wake word responses, which are valid if there is an audio event, Alexa UX responses are valid if a
    // nonzero anim tag was provided.
    if( (info.state == state) && (info.getInAnimTag != 0) ) {
      response = &info;
    }
  }

  if( response == nullptr ) {
    return false;
  }

  if( !response->getInAnimName.empty() && !ignoreGetIn ) {
    // TODO: (VIC-11516) it's possible that the UX state went back to idle for just a short while, in
    // which case the engine could be playing the get-out from the previous UX state, or worse, is
    // still in the looping animation for that ux state. it would be nice if the get-in below only
    // plays if the eyes are showing.

    auto* anim = _context->GetDataLoader()->GetCannedAnimation( response->getInAnimName );
    if( ANKI_VERIFY( (_streamer != nullptr) && (anim != nullptr),
                     "ShowAudioStreamStateManager.StartAlexaResponse.NoValidGetInAnim",
                     "Animation not found for get in %s", response->getInAnimName.c_str() ) )
    {
      const bool interruptRunning = true;
      _streamer->SetStreamingAnimation( response->getInAnimName, response->getInAnimTag, 1, 0, interruptRunning);
    }
  }

  // Only play earcons when not frozen on charger (alexa acoustic test mode)
  if( !(_onCharger && _frozenOnCharger) ) {
    Audio::CozmoAudioController* controller = _context->GetAudioController();
    if( ANKI_VERIFY(nullptr != controller, "ShowAudioStreamStateManager.StartAlexaResponse.NullAudioController",
                    "The CozmoAudioController is null so the audio event cannot be played" ) )
    {
      using namespace AudioEngine;
      const auto audioEvent = response->audioEvent.audioEvent;
      if ( audioEvent != AudioMetaData::GameEvent::GenericEvent::Invalid ) {
        controller->PostAudioEvent( ToAudioEventId( audioEvent ),
                                    ToAudioGameObject( response->audioEvent.gameObject ) );
      }
    }
  }

  return true;
}

} // namespace Vector
} // namespace Anki
