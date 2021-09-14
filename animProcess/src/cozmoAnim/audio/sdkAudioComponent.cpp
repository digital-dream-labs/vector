/**
 * File: sdkAudioComponent.cpp
 *
 * Author: Bruce von Kugelgen
 *
 * Description: Component wrapper to generate, cache and use wave data from an SDK message.
 *
 * Copyright: Anki, Inc. 2016-2019
 *
 */

#include "sdkAudioComponent.h"

#include "audioEngine/audioCallback.h"
#include "audioEngine/audioTools/standardWaveDataContainer.h"
#include "audioEngine/audioTypeTranslator.h"
#include "audioEngine/plugins/ankiPluginInterface.h"
#include "audioEngine/plugins/streamingWavePortalPlugIn.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/animContext.h"

#include "util/logging/logging.h"

#include <memory>

// Log options
#define LOG_CHANNEL "SDKAudio"

// Point at which we declare a potential buffer overrun
#define MAX_BUFFERED_AUDIO  (100000)
#define AUDIO_TO_BEGIN_PLAYING_SEC (0.2)

namespace {
  constexpr Anki::AudioMetaData::GameObjectType kSdkGameObject = Anki::AudioMetaData::GameObjectType::TextToSpeech;
  constexpr Anki::AudioEngine::PlugIns::StreamingWavePortalPlugIn::PluginId_t kSdkPluginId = 100;
}

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Constructor
//
SdkAudioComponent::SdkAudioComponent(const Anim::AnimContext* context) {
  DEV_ASSERT(nullptr != context, "SdkAudioComponent.InvalidContext");
  DEV_ASSERT(nullptr != context->GetAudioController(), "SdkAudioComponent.InvalidAudioController");

  _audioController = context->GetAudioController();
  _audioPrepared = false;
  _audioPosted = false;
  _audioPlaybackFinishedPtr = std::make_shared<AudioCallbackType>([this](){
    OnAudioCompleted();
  });
}

//
// Destructor
//
SdkAudioComponent::~SdkAudioComponent() {
}

//
// Called to handle incoming ExternalAudioComplete message when client signals end of streaming
//
void SdkAudioComponent::HandleMessage(const RobotInterface::ExternalAudioComplete& msg)
{
  if (ANKI_VERIFY(_audioPrepared, 
                    "SdkAudioComponent.HandleMessage.ExternalAudioComplete", 
                    "Audio stream complete message received without start")) {
    _waveData->DoneProducingData();      
  }
}

//
// Called to handle incoming ExternalAudioCancel message for user cancellation
//
void SdkAudioComponent::HandleMessage(const RobotInterface::ExternalAudioCancel& msg)
{
  if (ANKI_VERIFY(_audioPrepared, 
                    "SdkAudioComponent.HandleMessage.ExternalAudioCancel", 
                    "Audio stream cancel message received without start")) {
    CleanupAudioEngine();
    SendAnimToEngine(SDKAudioStreamingState::Cancelled);
  }
}

//
// Called to handle incoming ExternalAudioPrepare messages to start audio streaming process
//
void SdkAudioComponent::HandleMessage(const RobotInterface::ExternalAudioPrepare& msg)
{
  LOG_DEBUG("SdkAudioComponent.HandleMessage.ExternalAudioPrepare", 
           "Sample rate %d, volume %d", msg.audio_volume, msg.audio_rate );

  if (!PrepareAudioEngine(msg)) {
    LOG_DEBUG("SdkAudioComponent.HandleMessage.ExternalAudioPrepare", "Unable to prepare audio engine for streaming");
    SendAnimToEngine(SDKAudioStreamingState::PrepareFailed);
    ClearOperationData();
    return;
  }
}

//
// Called to handle incoming ExternalAudioChunk message for streaming audio data chunks
//
void SdkAudioComponent::HandleMessage(const RobotInterface::ExternalAudioChunk& msg)
{
  if (!_audioPrepared) {
    LOG_DEBUG("SdkAudioComponent.HandleMessage.ExternalAudioChunk", "Dropping chunks due to cancellation");
    return;
  }

  //add our wave data to the playing instance
  if (!AddAudioChunk(msg)) {
    CleanupAudioEngine(); 
    return;
  }

  const float totalAudioReceived_s = _totalAudioFramesReceived / (float)_audioRate;
  LOG_DEBUG("SdkAudioComponent.HandleMessage.ExternalAudioChunk", "Received (sec) %f", totalAudioReceived_s);
  if (!_audioPosted && totalAudioReceived_s > AUDIO_TO_BEGIN_PLAYING_SEC) {
      LOG_DEBUG("SdkAudioComponent.HandleMessage.ExternalAudioChunk", "Starting playback");

    // Post initial audio event for streaming
    if (!PostAudioEvent()) {
      LOG_ERROR("SdkAudioComponent.HandleMessage.ExternalAudioChunk", "Unable to post audio event Audio Streaming");
      SendAnimToEngine(SDKAudioStreamingState::PostFailed);
      CleanupAudioEngine();
      return;
    }
    _audioPosted = true;
  }
} // HandleMessage()

//
// Set volume for audio stream playback
// Value expected between 0.0 and 1.0 inclusive
//
bool SdkAudioComponent::SetPlayerVolume(float volume) const
{
  if (!ANKI_VERIFY(volume <= 1.0f && volume >= 0.0f, 
      "SdkAudioComponent.SetPlayerVolume","InvalidVolumeLevel should be between 0.0 and 1.0 inclusive")) {
    return false;
  }
  const auto parameterId = 
    AudioEngine::ToAudioParameterId( AudioMetaData::GameParameter::ParameterType::Robot_Vic_Sdk_Volume );
  const auto parameterValue = AudioEngine::ToAudioRTPCValue( volume );
  _audioController->SetParameter( parameterId, parameterValue, AudioEngine::kInvalidAudioGameObject );
  return true;
}

//
// Prepare for audio playback
//
bool SdkAudioComponent::PrepareAudioEngine(const RobotInterface::ExternalAudioPrepare& msg )
{
  if (_audioPrepared) {
    //no reentrance, only one playing at a time
    LOG_ERROR("SdkAudioComponent::PrepareAudioEngine", "Already playing audio");
    return false;
  }
  if (!SetPlayerVolume(msg.audio_volume / 100.0f)) {
    return false;
  }
  if (!ANKI_VERIFY(msg.audio_rate >= 8000 && msg.audio_rate <= 16025,
                   "SdkAudioComponent::PrepareAudioEngine", "Invalid audio rate %u", msg.audio_rate)) {
    return false;
  }

  // new waveData instance to hold data passed to wwise
  _waveData = AudioEngine::PlugIns::StreamingWavePortalPlugIn::CreateDataInstance();

  // Clear previously loaded data, setup plugin
  auto * pluginInterface = _audioController->GetPluginInterface();
  DEV_ASSERT(nullptr != pluginInterface, "SdkAudioComponent.PrepareAudioEngine.InvalidPluginInterface");
  auto * plugin = pluginInterface->GetStreamingWavePortalPlugIn();
  plugin->ClearAudioData(kSdkPluginId);
  bool result = plugin->AddDataInstance(_waveData, kSdkPluginId);
  if (!result) {
    LOG_ERROR("SdkAudioComponent::PrepareAudioEngine", "Failed to add data instance");
    return false;
  }

  _audioRate = msg.audio_rate;
  _totalAudioFramesReceived = 0;
  _audioPrepared = true;

  return true;
}

//
// Add a chunk of audio to the audio engine
//
bool SdkAudioComponent::AddAudioChunk(const RobotInterface::ExternalAudioChunk& msg) {
  //check for dangerous buffer expansion
  auto * pluginInterface = _audioController->GetPluginInterface();
  auto * plugin = pluginInterface->GetStreamingWavePortalPlugIn();
  const auto played = plugin->GetDataInstance(kSdkPluginId)->GetNumberOfFramesPlayed();
  const auto received = plugin->GetDataInstance(kSdkPluginId)->GetNumberOfFramesReceived();
  LOG_DEBUG("SdkAudioComponent::AddAudioChunk", "Played %u Received %u", played, received);
  SendAnimToEngine(SDKAudioStreamingState::ChunkAdded, received, played);

  if (received - played > MAX_BUFFERED_AUDIO) {
    LOG_ERROR("SdkAudioComponent::AddAudioChunk", "Buffer overflow %u played %u received", played, received);
    SendAnimToEngine(SDKAudioStreamingState::BufferOverflow, received, played);
    return false; 
  }

  //copy chunk into container, append to waveData
  const uint16_t input_chunk_size = msg.audio_chunk_size;
  const uint16_t wave_data_size = input_chunk_size / 2;  //size is in bytes, but buffer samples are 16 bit
  Anki::AudioEngine::StandardWaveDataContainer waveContainer(_audioRate, 1, wave_data_size); 
  waveContainer.CopyLittleEndianWaveData( (const unsigned char*)msg.audio_chunk_data, wave_data_size );
  const bool result = _waveData->AppendStandardWaveData( std::move(waveContainer ));

  if (!result) {
    LOG_ERROR("SdkAudioComponent::AddAudioChunk", "Failed to append audio data");
    SendAnimToEngine(SDKAudioStreamingState::AddAudioFailed);
    return false;
  } 

  _totalAudioFramesReceived = received + wave_data_size;

  return true;
}

//
// Send audio trigger event
//
bool SdkAudioComponent::PostAudioEvent()
{
  auto * audioCallbackContext = new AudioEngine::AudioCallbackContext();

  // Set callback flags
  audioCallbackContext->SetCallbackFlags( AudioEngine::AudioCallbackFlag::Complete );
  // Execute callbacks synchronously (on main thread)
  audioCallbackContext->SetExecuteAsync( false );
  // Register callback for event
  using namespace AudioEngine;
  std::weak_ptr<AudioCallbackType> weakOnFinished = _audioPlaybackFinishedPtr;
  audioCallbackContext->SetEventCallbackFunc( [weakOnFinished]( const AudioCallbackContext* thisContext,
                                                                     const AudioCallbackInfo& callbackInfo ) {
    // if the user logs out, this callback could fire after sdkAudioComponent is destroyed. check that here.
    auto callback = weakOnFinished.lock();
    if( callback && (*callback) ) {
      (*callback)();
    }
  });

  using AudioEvent = AudioMetaData::GameEvent::GenericEvent;
  const auto eventID = AudioEngine::ToAudioEventId( AudioEvent::Play__Robot_Vic__External_Sdk_Playback_01 );
  const auto gameObject = static_cast<AudioEngine::AudioGameObject>( kSdkGameObject );
  const auto playingID = _audioController->PostAudioEvent(eventID, gameObject, audioCallbackContext);

  if (AudioEngine::kInvalidAudioPlayingId == playingID) {
    LOG_ERROR("SdkAudioComponent.PostAudioEvent", "Failed to post eventID %u", eventID);
    return false;
  }
  LOG_DEBUG("SdkAudioComponent.PostAudioEvent", "Posted audio eventID %u playingID %d", eventID, playingID);
  return true;
}

//
// Cleanup the audio engine in cases of error or cancellation
//
void SdkAudioComponent::CleanupAudioEngine()
{
  LOG_DEBUG("SdkAudioComponent.CleanupAudioEngine", "Clean up Audio Engine");
  StopActiveAudio();
  ClearActiveAudio();
  ClearOperationData();
}

//
// Tells wave output we are done making data
//
void SdkAudioComponent::ClearOperationData()
{
  LOG_DEBUG("SdkAudioComponent.ClearOperationData", "Clear Sdk Audio");
  if (_waveData && _waveData->IsPlayingStream()) {
    //no more data coming
    _waveData->DoneProducingData();
  }

  _audioPrepared = false;
  _audioPosted = false;
  _waveData = NULL;
} 

//
// Cancel audio events
//
void SdkAudioComponent::StopActiveAudio()
{
  LOG_DEBUG("SdkAudioComponent.StopActiveAudio", "Stop active Sdk audio");
  using AudioEvent = AudioMetaData::GameEvent::GenericEvent;
  const auto eventID = AudioEngine::ToAudioEventId( AudioEvent::Stop__Robot_Vic__External_Sdk_Playback_01 );
  _audioController->StopAllAudioEvents(eventID);
}

//
// Clear data from currently playing audio
//
void SdkAudioComponent::ClearActiveAudio()
{
  LOG_DEBUG("SdkAudioComponent.ClearActiveAudio", "Clear active Sdk audio");
  auto * plugin = _audioController->GetPluginInterface()->GetStreamingWavePortalPlugIn();
  if (plugin != nullptr) {
    plugin->ClearAudioData(kSdkPluginId);
  }
}

//
// Handle a callback from the AudioEngine indicating that the SDK Audio has finished sending audio
//
void SdkAudioComponent::OnAudioCompleted()
{
  LOG_DEBUG("SdkAudioComponent.OnAudioCompleted", "AudioStreaming completion callback received");
  ClearOperationData(); // Cleanup operation's memory
  SendAnimToEngine(SDKAudioStreamingState::Completed, 0, 0);
}

//
// Send audio streaming status message to Engine process
//
bool SdkAudioComponent::SendAnimToEngine(SDKAudioStreamingState audioState, uint32_t audioSent, uint32_t audioPlayed)
{
  LOG_DEBUG("sdkAudioComponent.SendAnimToEngine", 
            "audioState %d audioSent %d audioPlayed %d", (int)audioState, audioSent, audioPlayed);
  AudioStreamStatusEvent evt;
  evt.streamResultID = audioState;
  evt.audioReceived = audioSent;  //used only for ChunkAdded messages
  evt.audioPlayed = audioPlayed;  //used only for ChunkAdded messages
  return AnimProcessMessages::SendAnimToEngine(std::move(evt));
}

}   // end namespace Anki
}   // end namespace Vector
