/*
 * File:          animEngine.cpp
 * Date:          6/26/2017
 *
 * Description:   A platform-independent container for spinning up all the pieces
 *                required to run Vector Animation Process.
 *
 * Author: Kevin Yoon
 *
 * Modifications:
 */

#include "cozmoAnim/animEngine.h"

#include "cozmoAnim/alexa/alexa.h"
#include "cozmoAnim/animComms.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/audio/microphoneAudioClient.h"
#include "cozmoAnim/audio/engineRobotAudioInput.h"
#include "cozmoAnim/audio/sdkAudioComponent.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/animation/streamingAnimationModifier.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenManager.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/perfMetricAnim.h"
#include "cozmoAnim/robotDataLoader.h"
#include "cozmoAnim/showAudioStreamStateManager.h"
#include "cozmoAnim/textToSpeech/textToSpeechComponent.h"

#include "coretech/common/engine/opencvThreading.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/vision/shared/spriteCache/spriteCache.h"
#include "audioEngine/multiplexer/audioMultiplexer.h"

#include "webServerProcess/src/webService.h"

#include "osState/osState.h"

#include "util/cpuProfiler/cpuProfiler.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL    "AnimEngine"
#define NUM_ANIM_OPENCV_THREADS 0

namespace Anki {
namespace Vector {
namespace Anim {

#if ANKI_CPU_PROFILER_ENABLED
  CONSOLE_VAR_RANGED(float, kAnimEngine_TimeMax_ms,     ANKI_CPU_CONSOLEVARGROUP, 33, 2, 33);
  CONSOLE_VAR_ENUM(u8,      kAnimEngine_TimeLogging,    ANKI_CPU_CONSOLEVARGROUP, 0, Util::CpuProfiler::CpuProfilerLogging());
#endif

AnimEngine::AnimEngine(Util::Data::DataPlatform* dataPlatform)
  : _isInitialized(false)
  , _context(std::make_unique<AnimContext>(dataPlatform))
  , _animationStreamer(std::make_unique<AnimationStreamer>(_context.get()))
{
#if ANKI_CPU_PROFILER_ENABLED
  // Initialize CPU profiler early and put tracing file at known location with no dependencies on other systems
  Anki::Util::CpuProfiler::GetInstance();
  Anki::Util::CpuThreadProfiler::SetChromeTracingFile(
      dataPlatform->pathToResource(Util::Data::Scope::Cache, "vic-anim-tracing.json").c_str());
  Anki::Util::CpuThreadProfiler::SendToWebVizCallback([&](const Json::Value& json) { _context->GetWebService()->SendToWebViz("cpuprofile", json); });
#endif

  if (Anki::Util::gTickTimeProvider == nullptr) {
    Anki::Util::gTickTimeProvider = BaseStationTimer::getInstance();
  }

  _microphoneAudioClient.reset(new Audio::MicrophoneAudioClient(_context->GetAudioController()));

#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS
  AnimComms::InitSocketBufferStats();
#endif

}

AnimEngine::~AnimEngine()
{
  _context->GetWebService()->Stop();

#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS
  AnimComms::ReportSocketBufferStats();
#endif

  if (Anki::Util::gTickTimeProvider == BaseStationTimer::getInstance()) {
    Anki::Util::gTickTimeProvider = nullptr;
  }
  BaseStationTimer::removeInstance();
}

Result AnimEngine::Init()
{
  if (_isInitialized) {
    LOG_INFO("AnimEngine.Init.ReInit", "Reinitializing already-initialized CozmoEngineImpl with new config.");
  }

  uint32_t seed = 0; // will choose random seed
# ifdef ANKI_PLATFORM_OSX
  {
    seed = 1; // Setting to non-zero value for now for repeatable testing.
  }
# endif
  _context->SetRandomSeed(seed);

  OSState::getInstance()->SetUpdatePeriod(1000);

  RobotDataLoader * dataLoader = _context->GetDataLoader();
  dataLoader->LoadConfigData();
  dataLoader->LoadNonConfigData();

  _ttsComponent = std::make_unique<TextToSpeechComponent>(_context.get());
  _context->GetMicDataSystem()->Init(*dataLoader);

  // animation streamer must be initialized after loading non config data (otherwise there are no animations loaded)
  _animationStreamer->Init(_ttsComponent.get());
  _context->GetBackpackLightComponent()->Init();

  // Create and set up EngineRobotAudioInput to receive Engine->Robot messages and broadcast Robot->Engine
  auto* audioMux = _context->GetAudioMultiplexer();
  auto regId = audioMux->RegisterInput( new Audio::EngineRobotAudioInput() );
  // Easy access to Audio Controller
  _audioControllerPtr = _context->GetAudioController();

  // Set up message handler
  auto * audioInput = static_cast<Audio::EngineRobotAudioInput*>(audioMux->GetInput(regId));
  _streamingAnimationModifier = std::make_unique<StreamingAnimationModifier>(_animationStreamer.get(), audioInput, _ttsComponent.get());

  // set up audio stream state manager
  {
    _context->GetShowAudioStreamStateManager()->SetAnimationStreamer(_animationStreamer.get());
  }


  AnimProcessMessages::Init(this, _animationStreamer.get(), _streamingAnimationModifier.get(), audioInput, _context.get());

  _context->GetWebService()->Start(_context->GetDataPlatform(),
                                   _context->GetDataLoader()->GetWebServerAnimConfig());
  FaceInfoScreenManager::getInstance()->Init(_context.get(), _animationStreamer.get());

  _context->GetAlexa()->Init(_context.get());

  const auto pm = _context->GetPerfMetric();
  pm->Init(_context->GetDataPlatform(), _context->GetWebService());
  pm->SetAnimationStreamer(_animationStreamer.get());
  if (pm->GetAutoRecord())
  {
    pm->Start();
  }

  // Make sure OpenCV isn't threading
  Result cvResult = SetNumOpencvThreads( NUM_ANIM_OPENCV_THREADS, "AnimEngine.Init" );
  if( RESULT_OK != cvResult )
  {
    return cvResult;
  }

  _sdkAudioComponent = std::make_unique<SdkAudioComponent>(_context.get());

  LOG_INFO("AnimEngine.Init.Success","Success");
  _isInitialized = true;

  return RESULT_OK;
}

Result AnimEngine::Update(const BaseStationTime_t currTime_nanosec)
{
  ANKI_CPU_TICK("AnimEngine::Update", kAnimEngine_TimeMax_ms, Util::CpuProfiler::CpuProfilerLoggingTime(kAnimEngine_TimeLogging));
  if (!_isInitialized) {
    LOG_ERROR("AnimEngine.Update", "Cannot update AnimEngine before it is initialized.");
    return RESULT_FAIL;
  }

  //
  // Declare some invariants. These components are always present after successful initialization.
  //
  DEV_ASSERT(_context, "AnimEngine.Update.InvalidContext");
  DEV_ASSERT(_ttsComponent, "AnimEngine.Update.InvalidTTSComponent");
  DEV_ASSERT(_animationStreamer, "AnimEngine.Update.InvalidAnimationStreamer");
  DEV_ASSERT(_streamingAnimationModifier, "AnimEngine.Update.InvalidStreamingAnimationModifier");
  DEV_ASSERT(_sdkAudioComponent, "AnimEngine.Update.InvalidSdkComponent");

#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS
  {
    // Update socket buffer counters
    AnimComms::UpdateSocketBufferStats();
  }
#endif

  BaseStationTimer::getInstance()->UpdateTime(currTime_nanosec);

  _context->GetWebService()->Update();

  Result result = AnimProcessMessages::Update(currTime_nanosec);
  if (RESULT_OK != result) {
    LOG_WARNING("AnimEngine.Update", "Unable to process messages (result %d)", result);
    return result;
  }

  OSState::getInstance()->Update(currTime_nanosec);

  _ttsComponent->Update();

  // Clear out sprites that have passed their cache time
  _context->GetDataLoader()->GetSpriteCache()->Update(currTime_nanosec);

  // Update animations
  _streamingAnimationModifier->ApplyAlterationsBeforeUpdate(_animationStreamer.get());

  _animationStreamer->Update();

  _streamingAnimationModifier->ApplyAlterationsAfterUpdate(_animationStreamer.get());

  // Update audio controller
  if (_audioControllerPtr != nullptr) {
    // Update mic info in Audio Engine
    const auto& micDirectionMsg = _context->GetMicDataSystem()->GetLatestMicDirectionMsg();
    _microphoneAudioClient->ProcessMessage(micDirectionMsg);
    // Tick the Audio Engine at the end of each anim frame
    _audioControllerPtr->Update();
  }

  // Update backpack lights
  _context->GetBackpackLightComponent()->Update();

#if ANKI_PROFILE_ANIMCOMMS_SOCKET_BUFFER_STATS
  {
    // Update socket buffer counters
    AnimComms::UpdateSocketBufferStats();
  }
#endif

  return RESULT_OK;
}

void AnimEngine::RegisterTickPerformance(const float tickDuration_ms,
                                         const float tickFrequency_ms,
                                         const float sleepDurationIntended_ms,
                                         const float sleepDurationActual_ms) const
{
  _context->GetPerfMetric()->Update(tickDuration_ms, tickFrequency_ms,
                                    sleepDurationIntended_ms, sleepDurationActual_ms);
}

void AnimEngine::HandleMessage(const RobotInterface::TextToSpeechPrepare & msg)
{
  DEV_ASSERT(_ttsComponent, "AnimEngine.TextToSpeechPrepare.InvalidTTSComponent");
  _ttsComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::TextToSpeechPlay & msg)
{
  DEV_ASSERT(_ttsComponent, "AnimEngine.TextToSpeechPlay.InvalidTTSComponent");
  _ttsComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::TextToSpeechCancel & msg)
{
  DEV_ASSERT(_ttsComponent, "AnimEngine.TextToSpeechCancel.InvalidTTSComponent");
  _ttsComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::SetLocale & msg)
{
  const std::string locale(msg.locale, msg.locale_length);

  LOG_INFO("AnimEngine.SetLocale", "Set locale to %s", locale.c_str());

  if (_context != nullptr) {
    _context->SetLocale(locale);
  }

  if (_ttsComponent != nullptr) {
    _ttsComponent->SetLocale(locale);
  }
}

void AnimEngine::HandleMessage(const RobotInterface::ExternalAudioChunk & msg)
{
  DEV_ASSERT(_sdkAudioComponent, "AnimEngine.ExternalAudioChunk.InvalidSDKAudioComponent");
  _sdkAudioComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::ExternalAudioComplete & msg)
{
  DEV_ASSERT(_sdkAudioComponent, "AnimEngine.ExternalAudioComplete.InvalidSDKAudioComponent");
  _sdkAudioComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::ExternalAudioCancel & msg)
{
  DEV_ASSERT(_sdkAudioComponent, "AnimEngine.ExternalAudioCancel.InvalidSDKAudioComponent");
  _sdkAudioComponent->HandleMessage(msg);
}

void AnimEngine::HandleMessage(const RobotInterface::ExternalAudioPrepare & msg)
{
  DEV_ASSERT(_sdkAudioComponent, "AnimEngine.ExternalAudioPrepare.InvalidSDKAudioComponent");
  _sdkAudioComponent->HandleMessage(msg);
}


} // namespace Anim
} // namespace Vector
} // namespace Anki
