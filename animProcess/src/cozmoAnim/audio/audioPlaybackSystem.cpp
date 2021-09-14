/***********************************************************************************************************************
 *
 *  AudioPlaybackSystem
 *  Victor / Anim
 *
 *  Created by Jarrod Hatfield on 4/10/2018
 *
 *  Description
 *  + System to load and playback recordings/audio files/etc
 *
 **********************************************************************************************************************/

// #include "cozmoAnim/audio/audioPlaybackSystem.h"
#include "audioPlaybackSystem.h"

#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/audio/audioPlaybackJob.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "audioEngine/audioTools/standardWaveDataContainer.h"
#include "audioEngine/audioTypeTranslator.h"
#include "audioEngine/plugins/ankiPluginInterface.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/threading/threadPriority.h"

#include "clad/robotInterface/messageRobotToEngine.h"

#include <thread>


namespace Anki {
namespace Vector {
namespace Audio {

using namespace AudioUtil;
using namespace AudioEngine;

namespace {
  const char* kThreadName                 = "MicPlayback";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlaybackSystem::AudioPlaybackSystem( const Anim::AnimContext* context ) :
  _animContext( context ),
  _isJobLoading( false )
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlaybackSystem::~AudioPlaybackSystem()
{
  // clear our any jobs left in the queue
  while ( !_jobQueue.empty() )
  {
    delete _jobQueue.front();
    _jobQueue.pop();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::Update( BaseStationTime_t currTime_nanosec )
{
  // monitor our current job for completion
  if ( _currentJob && _isJobLoading )
  {
    // audio job will load the data, it's our job to handle playback
    if ( _currentJob->IsComplete() )
    {
      _isJobLoading = false;
      BeginAudioPlayback();
    }
  }

  // if we're not working on a current job and we have more in the queue, start up the next job
  if ( !_currentJob )
  {
    StartNextJobInQueue();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::PlaybackAudio( const std::string& path )
{
  if ( IsValidFile( path ) )
  {
    // simply push the job onto the queue and it'll take care of itself
    AudioPlaybackJob* newJob = new AudioPlaybackJob( path );
    _jobQueue.push( newJob );

    // note: we can only play one audio clip at a time, so the audio jobs will queue until it reaches the front
    //       of the line.
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool AudioPlaybackSystem::IsValidFile( const std::string& path ) const
{
  // we have a buffer length of 255 and our path needs to fit into this buffer
  // shouldn't be a problem, but if we ever hit this we'll need to find another solution
  DEV_ASSERT( path.length() <= 255, "AudioPlaybackSystem path is too long for AnimToEngine message" );

  const bool pathLengthValid = ( path.length() <= 255 );
  const bool fileExists = Util::FileUtils::FileExists( path );

  return ( pathLengthValid && fileExists );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::StartNextJobInQueue()
{
  // simply move the first job in the queue into our current job
  if ( !_jobQueue.empty() )
  {
    // note: currently this wont stop the current job from playing as it's playing on it's own thread.
    //       maybe we want the ability to stop a playing job, but for now ignoring it
    _currentJob.reset( _jobQueue.front() );
    _jobQueue.pop();

    // this is just a single-run thread with no loop, so simply detach and let it do it's thing
    // might be worth looking into having it on a timeout to ensure it doesn't stall for some reason, but I'm not
    // worried about that at this time.
    _isJobLoading = true;
    std::thread( LoadAudioPlaybackData, _currentJob ).detach();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::LoadAudioPlaybackData( std::shared_ptr<AudioPlaybackJob> audiojob )
{
  // note: this function is called from it's own thread

  // a job is considered complete as soon as it loads the audio data
  // when the job is complete, it's up to the AudioPlaybackSystem to deal with playback
  Anki::Util::SetThreadName( pthread_self(), kThreadName );

  if ( audiojob )
  {
    audiojob->LoadAudioData();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::BeginAudioPlayback()
{
  DEV_ASSERT_MSG( _currentJob, "AudioPlaybackSystem", "No audio job is active" );

  AudioEngine::StandardWaveDataContainer* data = _currentJob->GetAudioData();
  DEV_ASSERT_MSG( nullptr != data, "AudioPlaybackSystem", "Data was not properly loaded" );

  CozmoAudioController* audioController = _animContext->GetAudioController();
  PlugIns::AnkiPluginInterface* const plugin = audioController->GetPluginInterface();
  DEV_ASSERT_MSG( nullptr != plugin, "AudioPlaybackSystem", "Invalid Plugin Interface" );

  // clear out any old audio data
  if ( plugin->WavePortalHasAudioDataInfo() )
  {
    plugin->ClearWavePortalAudioData();
  }

  // give our audio data over to the plugin and release our memory to it
  plugin->GiveWavePortalAudioDataOwnership( data );
  data->ReleaseAudioDataOwnership();

  DEV_ASSERT_MSG( plugin->WavePortalHasAudioDataInfo(), "AudioPlaybackSystem", "Data NOT transferred to audio plugin" );

  OnAudioPlaybackBegin();

  using namespace Anki::AudioMetaData;
  {
    AudioCallbackContext* callbackContext = new AudioCallbackContext();
    callbackContext->SetCallbackFlags( AudioCallbackFlag::Complete );
    callbackContext->SetExecuteAsync( false );
    callbackContext->SetEventCallbackFunc ( [this]( const AudioCallbackContext* thisContext, const AudioCallbackInfo& callbackInfo )
    {
      OnAudioPlaybackEnd();

      // we're all done with this job so nuke it
      _currentJob.reset();
    });

    // now post this message to the audio engine which tells it to play the chunk of memeory we just passed to the plugin
    const AudioEventId audioId = ToAudioEventId( GameEvent::GenericEvent::Play__Robot_Vic__External_Voice_Message );
    const AudioGameObject audioGameObject = ToAudioGameObject( GameObjectType::VoiceRecording );

    audioController->PostAudioEvent( audioId, audioGameObject, callbackContext );
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::OnAudioPlaybackBegin()
{
  if ( _currentJob )
  {
    RobotInterface::AudioPlaybackBegin event;

    const std::string& filename = _currentJob->GetFilename();
    memcpy( event.path, filename.c_str(), filename.length() );
    event.path_length = filename.length();

    AnimProcessMessages::SendAnimToEngine( std::move( event ) );
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AudioPlaybackSystem::OnAudioPlaybackEnd()
{
  if ( _currentJob )
  {
    RobotInterface::AudioPlaybackEnd event;

    const std::string& filename = _currentJob->GetFilename();
    memcpy( event.path, filename.c_str(), filename.length() );
    event.path_length = filename.length();

    AnimProcessMessages::SendAnimToEngine( std::move( event ) );
  }
}

} // Audio
} // namespace Vector
} // namespace Anki
