/**
 * File: cozmoAudioController.cpp
 *
 * Author: Jordan Rivas
 * Created: 09/07/2017
 *
 * Description: Cozmo interface to Audio Engine
 *              - Subclass of AudioEngine::AudioEngineController
 *              - Implement Cozmo specific audio functionality
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cozmoAnim/audio/cozmoAudioController.h"

#include "audioEngine/audioTypeTranslator.h"
#include "coretech/common/engine/utils/timer.h"
#include "cozmoAnim/animContext.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "audioEngine/audioScene.h"
#include "audioEngine/soundbankLoader.h"
#include "clad/audio/audioGameObjectTypes.h"
#include "json/json.h"
#include "util/console/consoleInterface.h"
#include "util/environment/locale.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/helpers/ankiDefines.h"
#include "util/helpers/templateHelpers.h"
#include "util/math/numericCast.h"
#include "util/time/universalTime.h"
#include "webServerProcess/src/webService.h"
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>


// Allow the build to include/exclude the audio libs
//#define EXCLUDE_ANKI_AUDIO_LIBS 0

#ifndef EXCLUDE_ANKI_AUDIO_LIBS

#define USE_AUDIO_ENGINE 1
#include "audioEngine/plugins/ankiPluginInterface.h"
#include "audioEngine/plugins/hijackAudioPlugIn.h"
#include "audioEngine/plugins/wavePortalPlugIn.h"
#else
// If we're excluding the audio libs, don't link any of the audio engine
#define USE_AUDIO_ENGINE 0
#endif

namespace {
static Anki::Vector::Audio::CozmoAudioController* sThis = nullptr;
static std::string sWritePath;
const std::string kProfilerCaptureFileName          = "VictorProfilerSession";
const std::string kProfilerCaptureFileExtension     = "prof";
const std::string kAudioOutputCaptureFileName       = "VictorOutputSession";
const std::string kAudioOutputCaptureFileExtension  = "wav";

using APT = Anki::AudioMetaData::GameParameter::ParameterType;
// Consumable Parameters
const std::set<APT> kConsumableParameters =
  { APT::Robot_Vic_Meter_Bus_Sfx, APT::Robot_Vic_Meter_Bus_Tts, APT::Robot_Vic_Meter_Bus_Vo };
const char* const kWebVizModuleName = "audioevents";

// Null-safe wrapper for Anki::AudioMetaData::EnumToString()
std::string ToString(Anki::AudioMetaData::GameObjectType v)
{
  const char * str = Anki::AudioMetaData::EnumToString(v);
  if (str != nullptr) {
    return str;
  }
  return std::to_string(static_cast<uint64_t>(v));
}

// Null-safe wrapper for Anki::AudioMetaData::GameEvent::EnumToString()
std::string ToString(Anki::AudioMetaData::GameEvent::GenericEvent v)
{
  const char * str = Anki::AudioMetaData::GameEvent::EnumToString(v);
  if (str != nullptr) {
    return str;
  }
  return std::to_string(static_cast<uint64_t>(v));
}

// Null-safe wrapper for Anki::AudioMetaData::GameState::EnumToString()
std::string ToString(Anki::AudioMetaData::GameState::StateGroupType v)
{
  const char * str = Anki::AudioMetaData::GameState::EnumToString(v);
  if (str != nullptr) {
    return str;
  }
  return std::to_string(static_cast<uint64_t>(v));
}

// Null-safe wrapper for Anki::AudioMetaData::SwitchState::EnumToString()
std::string ToString(Anki::AudioMetaData::SwitchState::SwitchGroupType v)
{
  const char * str = Anki::AudioMetaData::SwitchState::EnumToString(v);
  if (str != nullptr) {
    return str;
  }
  return std::to_string(static_cast<uint64_t>(v));
}

}

namespace Anki {
namespace Vector {
namespace Audio {

using namespace AudioEngine;
using Language = Anki::Util::Locale::Language;


#if USE_AUDIO_ENGINE
// Setup Ak Logging callback
static void AudioEngineLogCallback( uint32_t, const char*, ErrorLevel, AudioPlayingId, AudioGameObject );
#endif


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace Console {
// Console Vars
#define CONSOLE_PATH "Audio.Controller"
CONSOLE_VAR( bool, kWriteAudioProfilerCapture, CONSOLE_PATH, false );
CONSOLE_VAR( bool, kWriteAudioOutputCapture, CONSOLE_PATH, false );
CONSOLE_VAR_RANGED(uint8_t, kWriteAudioProfilerMaxLogCount, CONSOLE_PATH, 3, 1, 5);
CONSOLE_VAR_RANGED(uint8_t, kWriteAudioOutputMaxLogCount, CONSOLE_PATH, 1, 1, 5);

#if REMOTE_CONSOLE_ENABLED
// Console Functions
// Session Logs
void SetWriteAudioProfilerCapture( ConsoleFunctionContextRef context )
{
  kWriteAudioProfilerCapture = ConsoleArg_Get_Bool( context, "writeProfiler" );
  if ( sThis != nullptr ) {
    sThis->WriteProfilerCapture( kWriteAudioProfilerCapture );
  }
}

void SetWriteAudioOutputCapture( ConsoleFunctionContextRef context )
{
  kWriteAudioOutputCapture = ConsoleArg_Get_Bool( context, "writeOutput" );
  if ( sThis != nullptr ) {
    sThis->WriteAudioOutputCapture( kWriteAudioOutputCapture );
  }
}

void DeleteAudioProfilerCaptures( ConsoleFunctionContextRef context )
{
  const auto files = Util::FileUtils::FilesInDirectory( sWritePath, true, kProfilerCaptureFileExtension.c_str() );
  for ( const auto& file : files ) {
    Util::FileUtils::DeleteFile( file );
  }
}

void DeleteAudioOutputCaptures( ConsoleFunctionContextRef context )
{
  const auto files = Util::FileUtils::FilesInDirectory( sWritePath, true, kAudioOutputCaptureFileExtension.c_str() );
  for ( const auto& file : files ) {
    Util::FileUtils::DeleteFile( file );
  }
}

void TestAudio_PinkNoise( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    sThis->PostAudioEvent( ToAudioEventId(AudioMetaData::GameEvent::GenericEvent::Play__Dev_Robot__Pink_1Sec),
                           ToAudioGameObject(AudioMetaData::GameObjectType::Default) );
  }
}

// Generic Audio Interface
void PostAudioEvent( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    const char* event = ConsoleArg_Get_String( context, "event" );
    const auto defaultGameObj = static_cast<uint64_t>(AudioMetaData::GameObjectType::Default);
    const uint64_t gameObjectId = ConsoleArg_GetOptional_UInt64( context,
                                                                 "gameObjectId",
                                                                 defaultGameObj );
    sThis->PostAudioEvent( event, gameObjectId );
  }
}

void SetAudioState( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    const char* stateGroup = ConsoleArg_Get_String( context, "stateGroup" );
    const char* state = ConsoleArg_Get_String( context, "state" );
    sThis->SetState( AudioEngineController::GetAudioIdFromString( stateGroup ),
                     AudioEngineController::GetAudioIdFromString( state ) );
  }
}

void SetAudioSwitchState( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    const char* switchGroup = ConsoleArg_Get_String( context, "switchGroup" );
    const char* state = ConsoleArg_Get_String( context, "state" );
    const uint64_t gameObjectId = ConsoleArg_Get_UInt64( context, "gameObjectId" );
    sThis->SetSwitchState( AudioEngineController::GetAudioIdFromString( switchGroup ),
                           AudioEngineController::GetAudioIdFromString( state ),
                           gameObjectId );
  }
}

void SetAudioParameter( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    const char* parameter = ConsoleArg_Get_String( context, "parameter" );
    const float value = ConsoleArg_Get_Float( context, "value" );
    uint64_t gameObjectId = ConsoleArg_GetOptional_UInt64( context, "gameObjectId", kInvalidAudioGameObject );
    sThis->SetParameter( AudioEngineController::GetAudioIdFromString( parameter ), value, gameObjectId );
  }
}

void StopAllAudioEvents( ConsoleFunctionContextRef context )
{
  if ( sThis != nullptr ) {
    uint64_t gameObjectId = ConsoleArg_GetOptional_UInt64( context, "gameObjectId", kInvalidAudioGameObject );
    sThis->StopAllAudioEvents( gameObjectId );
  }
}

// Register console var func
CONSOLE_FUNC( SetWriteAudioProfilerCapture, CONSOLE_PATH, bool writeProfiler );
CONSOLE_FUNC( SetWriteAudioOutputCapture, CONSOLE_PATH, bool writeOutput );
CONSOLE_FUNC( DeleteAudioProfilerCaptures, CONSOLE_PATH );
CONSOLE_FUNC( DeleteAudioOutputCaptures, CONSOLE_PATH );
CONSOLE_FUNC( TestAudio_PinkNoise, CONSOLE_PATH );
CONSOLE_FUNC( PostAudioEvent, CONSOLE_PATH, const char* event, optional uint64 gameObjectId );
CONSOLE_FUNC( SetAudioState, CONSOLE_PATH, const char* stateGroup, const char* state );
CONSOLE_FUNC( SetAudioSwitchState, CONSOLE_PATH, const char* switchGroup, const char* state, uint64 gameObjectId );
CONSOLE_FUNC( SetAudioParameter, CONSOLE_PATH, const char* parameter, float value, optional uint64 gameObjectId );
CONSOLE_FUNC( StopAllAudioEvents, CONSOLE_PATH, optional uint64 gameObjectId );
#endif // REMOTE_CONSOLE_ENABLED
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// CozmoAudioController
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CozmoAudioController::CozmoAudioController( const Anim::AnimContext* context )
: _animContext( context )
{
#if USE_AUDIO_ENGINE
  SetupConfig config{};
  {
    DEV_ASSERT(nullptr != _animContext, "CozmoAudioController.CozmoAudioController.AnimContext.IsNull");

    const Util::Data::DataPlatform* dataPlatform = _animContext->GetDataPlatform();
    const std::string assetPath = dataPlatform->pathToResource(Util::Data::Scope::Resources, "sound" );
    sWritePath = dataPlatform->pathToResource(Util::Data::Scope::Cache, "sound");
    PRINT_CH_INFO("Audio", "CozmoAudioController.CozmoAudioController", "AssetPath '%s'", assetPath.c_str());
    PRINT_CH_INFO("Audio", "CozmoAudioController.CozmoAudioController", "WritePath '%s'", sWritePath.c_str());
    // If assets don't exist don't init the Audio engine
    const bool assetsExist = Util::FileUtils::DirectoryExists( assetPath );
    if ( !assetsExist ) {
      PRINT_NAMED_ERROR("CozmoAudioController.CozmoAudioController", "Audio Assets do NOT exist - Ignore if Unit Test");
      return;
    }
    // Create sound bank loader
    _soundbankLoader.reset(new SoundbankLoader(*this, assetPath));

    // Config Engine
    // Read/Write Asset path
    config.assetFilePath = assetPath;
    config.writeFilePath = sWritePath;

    // Cozmo uses default audio locale regardless of current context.
    // Locale-specific adjustments are made by setting GameState::External_Language
    // below.
    config.audioLocale = AudioLocaleType::EnglishUS;

    // Engine Memory
#if defined(ANKI_PLATFORM_OSX)
    // Webots play room
    config.defaultMemoryPoolSize      = ( 8 * 1024 * 1024 );  //  8 MB
    config.defaultLEMemoryPoolSize    = ( 16 * 1024 * 1024 ); // 16 MB
    config.ioMemorySize               = ( 4 * 1024 * 1024 );  //  4 MB
#else
    // Other Platforms
    config.defaultMemoryPoolSize      = ( 3 * 1024 * 1024 );  //  3 MB
    config.defaultLEMemoryPoolSize    = ( 6 * 1024 * 1024 );  //  6 MB
    config.ioMemorySize               = ( 2 * 1024 * 1024 );  //  2 MB

#if defined(ANKI_PLATFORM_VICOS)
    // Robot
    // Disk Read
    config.ioMemoryGranularitySize    = ( 32 * 1024 );        // 32 KB
    config.defaultPlaybackLookAhead   = 2;
    // Threading | Lower Engein
    config.threadLowEngine.SetAffinityMaskCpuId( 2 );
    // Scheduler
    // Allow to run on all cores except the core that Lower Engine runs on
    config.threadScheduler.SetAffinityMaskCpuId( 0 );
    config.threadScheduler.SetAffinityMaskCpuId( 1 );
    config.threadScheduler.SetAffinityMaskCpuId( 3 );
    // Bank Manager
    // Allow to run on all cores except the core that Lower Engine runs on
    config.threadBankManager.SetAffinityMaskCpuId( 0 );
    config.threadBankManager.SetAffinityMaskCpuId( 1 );
    config.threadBankManager.SetAffinityMaskCpuId( 3 );

#endif // defined(ANKI_PLATFORM_VICOS)
#endif // defined(ANKI_PLATFORM_OSX)

    // Performance
    config.sampleRate         = 32000;
    config.bufferSize         = 1024;
    config.defaultMaxNumPools = 30;
    // Systems
    config.enableGameSyncPreparation  = false;
    config.enableStreamCache          = true;
    config.enableMusicEngine          = false; // Not using music system

    // Start your Engines!!!
    InitializeAudioEngine( config );

    // If we're using the audio engine, assert that it was successfully initialized.
    DEV_ASSERT(IsInitialized(), "CozmoAudioController.Initialize Audio Engine fail");
  }

  // The audio engine was initialized correctly, so now let's setup everything else
  if ( IsInitialized() )
  {
    // Setup Engine Logging callback
    SetLogOutput( ErrorLevel::All, &AudioEngineLogCallback );

    InitializePluginInterface();
    GetPluginInterface()->SetupAkAlsaSinkPlugIn();

#if defined(ANKI_PLATFORM_VICOS)
    // Robot - Threading
    // Run on the same CPU as audio low engine
    GetPluginInterface()->SetupAkAlsaSinkPlugIn( config.threadLowEngine.affinityMask );
#endif

    GetPluginInterface()->SetupStreamingWavePortalPlugIn();

    // TBD VIC-5253: Retire non-streaming WavePortal after switch to streaming
    GetPluginInterface()->SetupWavePortalPlugIn();

    // Load audio sound bank metadata
    // NOTE: This will slightly change when we implement RAMS
    if ( _soundbankLoader.get() != nullptr ) {
      _soundbankLoader->LoadDefaultSoundbanks();
    }

    // Use Console vars to control profiling settings
    if ( Console::kWriteAudioProfilerCapture ) {
      WriteProfilerCapture( true );
    }
    if ( Console::kWriteAudioOutputCapture ) {
      WriteAudioOutputCapture( true );
    }

    RegisterCladGameObjectsWithAudioController();
    SetDefaultListeners( { ToAudioGameObject( AudioMetaData::GameObjectType::Victor_Listener ) } );
    SetupConsumableAudioParameters();

    using namespace AudioMetaData::SwitchState;

    SetSwitchState( ToAudioSwitchGroupId( SwitchGroupType::Robot_Vic_External_Input_Source ),
                    ToAudioSwitchStateId( (GenericSwitch) Robot_Vic_External_Input_Source::Streaming_Wave_Portal ),
                    ToAudioGameObject(AudioMetaData::GameObjectType::TextToSpeech) );

    SetSwitchState( ToAudioSwitchGroupId( SwitchGroupType::Robot_Vic_External_Input_Source ),
                    ToAudioSwitchStateId( (GenericSwitch) Robot_Vic_External_Input_Source::Streaming_Wave_Portal ),
                    ToAudioGameObject(AudioMetaData::GameObjectType::Animation) );
  }
  if (sThis == nullptr) {
    sThis = this;
  }
  else {
    PRINT_NAMED_ERROR("CozmoAudioController", "sThis.NotNull");
  }
#else
  PRINT_NAMED_WARNING("CozmoAudioController", "Audio Engine is disabled - EXCLUDE_ANKI_AUDIO_LIBS flag is set");
#endif // USE_AUDIO_ENGINE
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CozmoAudioController::~CozmoAudioController()
{
  _animContext = nullptr;
  if (sThis == this) {
    sThis = nullptr;
  }
  else {
    PRINT_NAMED_ERROR("~CozmoAudioController", "sThis.NotEqualToInstance");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::WriteProfilerCapture( bool write )
{
  std::string uniqueName; // Only need to set name when write is true
  if ( write ) {
    // Remove old captures
    const auto maxLogCount = Console::kWriteAudioProfilerMaxLogCount - 1; // Make room for new file
    RemoveCaptureFiles( sWritePath, kProfilerCaptureFileExtension, maxLogCount );
    // Create new log file name
    const auto dateTimeStr = CreateFormattedUtcDateTimeString();
    uniqueName = kProfilerCaptureFileName + '_' + dateTimeStr + '.' +  kProfilerCaptureFileExtension;
  }

  return AudioEngineController::WriteProfilerCapture( write, uniqueName );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::WriteAudioOutputCapture( bool write )
{
  std::string uniqueName; // Only need to set name when write is true
  if ( write ) {
    // Remove old captures
    const auto maxLogCount = Console::kWriteAudioOutputMaxLogCount - 1; // Make room for new file
    RemoveCaptureFiles( sWritePath, kAudioOutputCaptureFileExtension, maxLogCount );
    // Create new log file name
    const auto dateTimeStr = CreateFormattedUtcDateTimeString();
    uniqueName = kAudioOutputCaptureFileName + '_' + dateTimeStr + '.' +  kAudioOutputCaptureFileExtension;
  }
  return AudioEngineController::WriteAudioOutputCapture( write, uniqueName );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CozmoAudioController::RemoveCaptureFiles( const std::string& dirPath,
                                               const std::string& fileExtension,
                                               uint8_t maxCount )
{
  using namespace Util;
  const auto files = FileUtils::FilesInDirectory( dirPath, true, fileExtension.c_str() );
  if ( files.size() > maxCount ) {
    // Need to delete files
    using fileTime_t = unsigned long;
    std::map<fileTime_t, const std::string&, std::greater<fileTime_t> > fileTimeMap;
    // Get file creation time
    for ( const auto& file : files ) {
      struct stat info;
      if( stat(file.c_str(), &info) != 0 ) {
        PRINT_NAMED_WARNING("CozmoAudioController.RemoveCaptureFiles", "Unable to get file info '%s'", file.c_str());
        FileUtils::DeleteFile( file );
        continue;
      }
      // Add files into ordered map, sort newest to oldest
      fileTime_t birthTime = 0;
#if defined(ANKI_PLATFORM_VICOS)
      birthTime = static_cast<fileTime_t>( info.st_mtime );
#else
      birthTime = static_cast<fileTime_t>( info.st_birthtimespec.tv_sec );
#endif
      fileTimeMap.emplace( birthTime, file );
    }
    // Remove old logs
    if ( fileTimeMap.size() > maxCount ) {
      auto deleteIt = fileTimeMap.begin();
      std::advance( deleteIt, maxCount ); // Go to the first element we want to delete
      // Delete remaining files
      for ( ; deleteIt != fileTimeMap.end(); ++deleteIt ) {
        FileUtils::DeleteFile( deleteIt->second );
      }
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string CozmoAudioController::CreateFormattedUtcDateTimeString()
{
  using namespace std::chrono;
  const time_t now = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
  tm* time = gmtime( &now );
  if( nullptr == time ) {
    PRINT_NAMED_ERROR("CozmoAudioController.CreateFormattedUtcDateTimeString.UTC.Invalid",
                      "gmtime returned null. Error: %s",
                      strerror(errno));
    return "";
  }
  // Creat Data String "00-00-00_00-00-00_XXX"
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << (time->tm_mon + 1) << '-' << std::setw(2) << time->tm_mday << '-'
  << std::setw(2) << (time->tm_year - 100) << '_' << std::setw(2) << time->tm_hour << '-'
  << std::setw(2) << time->tm_min << '-' << time->tm_sec << '_' << time->tm_zone;
  return ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::ActivateParameterValueUpdates( bool activate )
{
  bool success = false;
  if ( ParameterUpdatesIsActive() == activate ) {
    // Active state did not change
    return success;
  }

  if ( activate ) {
    // Register callbacks
    AudioEngineCallbackFunc callbackFunc = [this]( AudioEngineCallbackId callbackId,
                                                   AudioEngineCallbackFlag callbackType )
    {
      using namespace AudioEngine;
      for ( auto& paramKVP : _consumableParameterValues ) {
        AudioRTPCValue val;
        AudioRTPCValueType type = AudioRTPCValueType::Global;
        bool success = this->GetParameterValue( ToAudioParameterId( paramKVP.first ),
                                                kInvalidAudioGameObject,
                                                kInvalidAudioPlayingId,
                                                val,
                                                type );
        if ( success ) {
          paramKVP.second = val;
        }
      }
    };
    _parameterUpdateCallbackId = RegisterGlobalCallback( AudioEngine::AudioEngineCallbackFlag::EndFrameRender,
                                                         std::move( callbackFunc ) );
    success = ParameterUpdatesIsActive();
  }
  else {
    // Unregister callbacks
    success = UnregisterGlobalCallback( _parameterUpdateCallbackId );
    _parameterUpdateCallbackId = kInvalidAudioEngineCallbackId;
  }
  return success;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::GetActivatedParameterValue( AudioMetaData::GameParameter::ParameterType parameter,
                                                       AudioEngine::AudioRTPCValue& out_value )
{
  bool success = false;
  out_value = 0.0f;
  if ( !ParameterUpdatesIsActive() ) {
    // Not Active
    return success;
  }

  const auto paramIt = _consumableParameterValues.find( parameter );
  if ( paramIt != _consumableParameterValues.end() ) {
    success = true;
    out_value = paramIt->second;
  }
  return success;
}

// Private Methods
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CozmoAudioController::RegisterCladGameObjectsWithAudioController()
{
  using GameObjectType = AudioMetaData::GameObjectType;

  // Enumerate through GameObjectType Enums
  for ( uint32_t aGameObj = static_cast<AudioGameObject>(GameObjectType::Default);
        aGameObj < static_cast<uint32_t>(GameObjectType::End);
        ++aGameObj ) {
    // Register GameObjectType
    bool success = RegisterGameObject( static_cast<AudioGameObject>( aGameObj ),
                                       std::string(EnumToString(static_cast<GameObjectType>( aGameObj ))) );
    if ( !success ) {
      PRINT_NAMED_ERROR("CozmoAudioController.RegisterCladGameObjectsWithAudioController",
                        "Registering GameObjectId: %ul - %s was unsuccessful",
                        aGameObj, EnumToString(static_cast<GameObjectType>(aGameObj)));
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CozmoAudioController::SetupConsumableAudioParameters()
{
  _parameterUpdateCallbackId = kInvalidAudioEngineCallbackId;
  _consumableParameterValues.clear();
  for ( const auto consumableParameter : kConsumableParameters ) {
    _consumableParameterValues.emplace( consumableParameter, 0.0f );
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlayingId CozmoAudioController::PostAudioEvent( const std::string& eventName,
                                                     AudioGameObject gameObjectId,
                                                     AudioCallbackContext* callbackContext )
{
  AudioPlayingId ret = AudioEngineController::PostAudioEvent( eventName, gameObjectId, callbackContext );

  if( ANKI_DEV_CHEATS ) {
    auto* webservice = _animContext->GetWebService();
    if( (webservice != nullptr) && webservice->IsWebVizClientSubscribed(kWebVizModuleName) ) {
      Json::Value toSend;
      toSend["type"] = "PostAudioEvent";
      toSend["time"] = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      toSend["eventName"] = eventName;
      toSend["gameObjectId"] = ToString(static_cast<AudioMetaData::GameObjectType>(gameObjectId));
      toSend["hasCallback"] = (callbackContext != nullptr);
      // Note: this hypothetically could flood wifi, but only if the webviz tab is open. Ideally there
      // would be an update call in this class to flush accumulated events. We can add one if this
      // ends up being problematic, possibly one that is called from the webviz update loop.
      // Same goes for all the other methods below
      webservice->SendToWebViz( kWebVizModuleName, toSend );
    }
  }

  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AudioPlayingId CozmoAudioController::PostAudioEvent( AudioEventId eventId,
                                                     AudioGameObject gameObjectId,
                                                     AudioCallbackContext* callbackContext )
{
  AudioPlayingId ret = AudioEngineController::PostAudioEvent( eventId, gameObjectId, callbackContext );

  if( ANKI_DEV_CHEATS ) {
    auto* webservice = _animContext->GetWebService();
    if( (webservice != nullptr) && webservice->IsWebVizClientSubscribed(kWebVizModuleName) ) {
      Json::Value toSend;
      toSend["type"] = "PostAudioEvent";
      toSend["time"] = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      toSend["eventName"] = ToString(static_cast<AudioMetaData::GameEvent::GenericEvent>(eventId));
      toSend["gameObjectId"] = ToString(static_cast<AudioMetaData::GameObjectType>(gameObjectId));
      toSend["hasCallback"] = (callbackContext != nullptr);
      webservice->SendToWebViz( kWebVizModuleName, toSend );
    }
  }

  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CozmoAudioController::StopAllAudioEvents( AudioGameObject gameObjectId )
{
  AudioEngineController::StopAllAudioEvents( gameObjectId );

  if( ANKI_DEV_CHEATS ) {
    auto* webservice = _animContext->GetWebService();
    if( (webservice != nullptr) && webservice->IsWebVizClientSubscribed(kWebVizModuleName) ) {
      Json::Value toSend;
      toSend["type"] = "StopAllAudioEvents";
      toSend["time"] = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      toSend["gameObjectId"] = ToString(static_cast<AudioMetaData::GameObjectType>(gameObjectId));
      webservice->SendToWebViz( kWebVizModuleName, toSend );
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::SetState( AudioStateGroupId stateGroupId,
                                     AudioStateId stateId ) const
{
  bool ret = AudioEngineController::SetState( stateGroupId, stateId );

  if( ANKI_DEV_CHEATS ) {
    auto* webservice = _animContext->GetWebService();
    if( (webservice != nullptr) && webservice->IsWebVizClientSubscribed(kWebVizModuleName) ) {
      Json::Value toSend;
      toSend["type"] = "SetState";
      toSend["time"] = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      toSend["stateGroupId"] = ToString(static_cast<AudioMetaData::GameState::StateGroupType>(stateGroupId));
      toSend["stateId"] = stateId; // no string mapping
      webservice->SendToWebViz( kWebVizModuleName, toSend );
    }
  }

  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CozmoAudioController::SetSwitchState( AudioSwitchGroupId switchGroupId,
                                           AudioSwitchStateId switchStateId,
                                           AudioGameObject gameObjectId ) const
{
  bool ret = AudioEngineController::SetSwitchState( switchGroupId, switchStateId, gameObjectId );

  if( ANKI_DEV_CHEATS ) {
    auto* webservice = _animContext->GetWebService();
    if( (webservice != nullptr) && webservice->IsWebVizClientSubscribed(kWebVizModuleName) ) {
      Json::Value toSend;
      toSend["type"] = "SetSwitchState";
      toSend["time"] = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
      toSend["switchGroupId"] = ToString(static_cast<AudioMetaData::SwitchState::SwitchGroupType>(switchGroupId));
      toSend["switchStateId"] = switchStateId; // no string mapping
      toSend["gameObjectId"] = ToString(static_cast<AudioMetaData::GameObjectType>(gameObjectId));
      webservice->SendToWebViz( kWebVizModuleName, toSend );
    }
  }

  return ret;
}


#if USE_AUDIO_ENGINE
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Setup Ak Logging callback
void AudioEngineLogCallback( uint32_t akErrorCode,
                             const char* errorMessage,
                             AudioEngine::ErrorLevel errorLevel,
                             AudioEngine::AudioPlayingId playingId,
                             AudioEngine::AudioGameObject gameObjectId )
{
  std::ostringstream logStream;
  logStream << "ErrorCode: " << akErrorCode << " Message: '" << ((nullptr != errorMessage) ? errorMessage : "")
  << "' LevelBitFlag: " << (uint32_t)errorLevel << " PlayingId: " << playingId << " GameObjId: " << gameObjectId;

  if (((uint32_t)errorLevel & (uint32_t)ErrorLevel::Message) == (uint32_t)ErrorLevel::Message) {
    PRINT_CH_INFO(CozmoAudioController::kLogChannelName,
                  "CozmoAudioController.AudioEngineLog",
                  "%s", logStream.str().c_str());
  }

  if ( ( (uint32_t)errorLevel & (uint32_t)ErrorLevel::Error ) == (uint32_t)ErrorLevel::Error ) {
    PRINT_NAMED_WARNING("CozmoAudioController.AudioEngineError", "%s", logStream.str().c_str());
  }
}
#endif


} // Audio
} // Cozmo
} // Anki
