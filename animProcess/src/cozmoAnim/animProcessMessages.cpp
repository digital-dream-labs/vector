/**
 * File: animProcessMessages.cpp
 *
 * Author: Kevin Yoon
 * Created: 6/30/2017
 *
 * Description: Shuttles messages between engine and robot processes.
 *              Responds to engine messages pertaining to animations
 *              and inserts messages as appropriate into robot-bound stream.
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/animComms.h"
#include "cozmoAnim/robotDataLoader.h"

#include "cozmoAnim/alexa/alexa.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/animation/streamingAnimationModifier.h"
#include "cozmoAnim/audio/engineRobotAudioInput.h"
#include "cozmoAnim/audio/audioPlaybackSystem.h"
#include "cozmoAnim/audio/proceduralAudioClient.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animEngine.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"
#include "cozmoAnim/connectionFlow.h"
#include "cozmoAnim/faceDisplay/faceDisplay.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenManager.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/showAudioStreamStateManager.h"
#include "audioEngine/multiplexer/audioMultiplexer.h"

#include "coretech/common/engine/utils/timer.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"

#include "clad/cloud/mic.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/robotInterface/messageEngineToRobotTag.h"
#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"
#include "clad/robotInterface/messageEngineToRobot_sendAnimToRobot_helper.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/factory/emrHelper.h"
#include "anki/cozmo/shared/factory/faultCodes.h"

#include "osState/osState.h"

#include "util/console/consoleInterface.h"
#include "util/console/consoleSystem.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/dispatchQueue/dispatchQueue.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/messageProfiler/messageProfiler.h"

#include <unistd.h>

// Log options
#define LOG_CHANNEL    "AnimProcessMessages"

// Trace options
// #define LOG_TRACE(name, format, ...) LOG_DEBUG(name, format, ##__VA_ARGS__)
#define LOG_TRACE(name, format, ...) {}

// Anonymous namespace for private declarations
namespace {

  static const int kNumTicksToShutdown = 5;
  int _countToShutdown = -1;

  // For comms with engine
  constexpr int MAX_PACKET_BUFFER_SIZE = 2048;
  u8 pktBuffer_[MAX_PACKET_BUFFER_SIZE];

  Anki::Vector::Anim::AnimEngine*             _animEngine = nullptr;
  Anki::Vector::Anim::AnimationStreamer*            _animStreamer = nullptr;
  Anki::Vector::Anim::StreamingAnimationModifier*   _streamingAnimationModifier = nullptr;
  Anki::Vector::Audio::EngineRobotAudioInput* _engAudioInput = nullptr;
  Anki::Vector::Audio::ProceduralAudioClient* _proceduralAudioClient = nullptr;
  const Anki::Vector::Anim::AnimContext*      _context = nullptr;

  bool _connectionFlowInited = false;

  // The maximum amount of time that can elapse in between
  // receipt of RobotState messages before the anim process
  // considers the robot process to have disconnected
  const float kNoRobotStateDisconnectTimeout_sec = 2.f;
  float _pendingRobotDisconnectTime_sec = -1.f;

  // Whether or not engine has finished loading and is ready to do things
  bool _engineLoaded = false;

  // Whether or not we have already told the boot anim to stop
  bool _bootAnimStopped = false;

#if REMOTE_CONSOLE_ENABLED
  Anki::Util::Dispatch::Queue* _dispatchQueue = nullptr;

  static void ListAnimations(ConsoleFunctionContextRef context)
  {
    context->channel->WriteLog("<html>\n");
    context->channel->WriteLog("<h1>Animations</h1>\n");
    std::vector<std::string> names = _context->GetDataLoader()->GetAnimationNames();
    for(const auto& name : names) {
      std::string url = "consolefunccall?func=playanimation&args="+name+"+1";
      std::string html = "<a href=\""+url+"\">"+name+"</a>&nbsp\n";
      context->channel->WriteLog("%s", html.c_str());
    }
    context->channel->WriteLog("</html>\n");
  }

  static void PlayAnimation(ConsoleFunctionContextRef context)
  {
    const char* name = ConsoleArg_Get_String(context, "name");
    if (name) {

      int numLoops = ConsoleArg_GetOptional_Int(context, "numLoops", 1);

      if (!_dispatchQueue) {
        _dispatchQueue = Anki::Util::Dispatch::Create("AddAnimation", Anki::Util::ThreadPriority::Low);
      }
      Anki::Util::Dispatch::Async(_dispatchQueue, [name, numLoops] {
        _animStreamer->SetPendingStreamingAnimation(name, numLoops);
      });

      char numLoopsStr[4+1];
      snprintf(numLoopsStr, sizeof(numLoopsStr), "%d", (numLoops > 9999) ? 9999 : numLoops);
      std::string text = std::string("Playing ")+name+" "+numLoopsStr+" times<br>";
      context->channel->WriteLog("%s", text.c_str());
    } else {
      context->channel->WriteLog("PlayAnimation name not specified.");
    }
  }

  static void AddAnimation(ConsoleFunctionContextRef context)
  {
    const char* path = ConsoleArg_Get_String(context, "path");
    if (path) {
      const std::string animationFolder = _context->GetDataPlatform()->pathToResource(Anki::Util::Data::Scope::Resources, "/assets/animations/");
      std::string animationPath = animationFolder + path;

      if (!_dispatchQueue) {
        _dispatchQueue = Anki::Util::Dispatch::Create("AddAnimation", Anki::Util::ThreadPriority::Low);
      }

      Anki::Util::Dispatch::Async(_dispatchQueue, [animationPath] {
        // _context: global in scope
        // animationPath: local in scope, on our heap
        // GetDataLoader: downwards, self contained and threaded all by itself
        _context->GetDataLoader()->LoadAnimationFile(animationPath.c_str());
      });

      std::string text = "Adding animation ";
      text += animationPath;
      context->channel->WriteLog("%s", text.c_str());
    } else {
      context->channel->WriteLog("AddAnimation file not specified.");
    }
  }

  static void ShowCurrentAnimation(ConsoleFunctionContextRef context)
  {
    const std::string currentAnimation = _animStreamer->GetStreamingAnimationName();
    context->channel->WriteLog("<html>\n");
    context->channel->WriteLog("%s", currentAnimation.c_str());
    context->channel->WriteLog("</html>\n");
  }

  static void AbortCurrentAnimation(ConsoleFunctionContextRef context)
  {
    const std::string currentAnimation = _animStreamer->GetStreamingAnimationName();
    _animStreamer -> Abort();
    context->channel->WriteLog("<html>\n");
    context->channel->WriteLog("%s", currentAnimation.c_str());
    context->channel->WriteLog("</html>\n");
  }

  CONSOLE_FUNC(ListAnimations, "Animations");
  CONSOLE_FUNC(PlayAnimation, "Animations", const char* name, optional int numLoops);
  CONSOLE_FUNC(AddAnimation, "Animations", const char* path);
  CONSOLE_FUNC(ShowCurrentAnimation, "Animations");
  CONSOLE_FUNC(AbortCurrentAnimation, "Animations");

  void RecordMicDataClip(ConsoleFunctionContextRef context)
  {
    auto * micDataSystem = _context->GetMicDataSystem();
    if (micDataSystem != nullptr)
    {
      micDataSystem->SetForceRecordClip(true);
    }
  }
  CONSOLE_FUNC(RecordMicDataClip, "MicData");
#endif // REMOTE_CONSOLE_ENABLED
}

namespace Anki {
namespace Vector {

// Note that these are send attempt counts, not a count of successful sends
uint32_t AnimProcessMessages::_messageCountAnimToRobot = 0;
uint32_t AnimProcessMessages::_messageCountAnimToEngine = 0;
uint32_t AnimProcessMessages::_messageCountRobotToAnim = 0;
uint32_t AnimProcessMessages::_messageCountEngineToAnim = 0;


// ========== START OF PROCESSING MESSAGES FROM ENGINE ==========
// #pragma mark "EngineToRobot Handlers"

void Process_checkCloudConnectivity(const Anki::Vector::RobotInterface::CheckCloudConnectivity& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem != nullptr) {
    micDataSystem->RequestConnectionStatus();
  }
}

void Process_setFullAnimTrackLockState(const Anki::Vector::RobotInterface::SetFullAnimTrackLockState& msg)
{
  //LOG_DEBUG("AnimProcessMessages.Process_setFullAnimTrackLockState", "0x%x", msg.whichTracks);
  _animStreamer->SetLockedTracks(msg.trackLockState);
}

void Process_addAnim(const Anki::Vector::RobotInterface::AddAnim& msg)
{
  const std::string path(msg.animPath, msg.animPath_length);

  LOG_INFO("AnimProcessMessages.Process_addAnim",
           "Animation File: %s", path.c_str());

  _context->GetDataLoader()->LoadAnimationFile(path);
}

void Process_playAnim(const Anki::Vector::RobotInterface::PlayAnim& msg)
{
  const std::string animName(msg.animName, msg.animName_length);

  LOG_INFO("AnimProcessMessages.Process_playAnim",
           "Anim: %s, Tag: %d",
           animName.c_str(), msg.tag);

  const bool interruptRunning = true;
  const bool overrideAllSpritesToEyeHue = msg.renderInEyeHue;
  _animStreamer->SetStreamingAnimation(animName,
                                       msg.tag,
                                       msg.numLoops,
                                       msg.startAt_ms,
                                       interruptRunning,
                                       overrideAllSpritesToEyeHue);
}

void Process_abortAnimation(const Anki::Vector::RobotInterface::AbortAnimation& msg)
{
  LOG_INFO("AnimProcessMessages.Process_abortAnimation", "Tag: %d", msg.tag);
  _animStreamer->Abort(msg.tag);
}

void Process_displayProceduralFace(const Anki::Vector::RobotInterface::DisplayProceduralFace& msg)
{
  ProceduralFace procFace;
  procFace.SetFromMessage(msg.faceParams);
  _animStreamer->SetProceduralFace(procFace, msg.duration_ms);
}

void Process_setFaceHue(const Anki::Vector::RobotInterface::SetFaceHue& msg)
{
  ProceduralFace::SetHue(msg.hue);
}

void Process_setFaceSaturation(const Anki::Vector::RobotInterface::SetFaceSaturation& msg)
{
  ProceduralFace::SetSaturation(msg.saturation);
}

void Process_displayFaceImageBinaryChunk(const Anki::Vector::RobotInterface::DisplayFaceImageBinaryChunk& msg)
{
  _animStreamer->Process_displayFaceImageChunk(msg);
}

void Process_displayFaceImageGrayscaleChunk(const Anki::Vector::RobotInterface::DisplayFaceImageGrayscaleChunk& msg)
{
  _animStreamer->Process_displayFaceImageChunk(msg);
}

void Process_displayFaceImageRGBChunk(const Anki::Vector::RobotInterface::DisplayFaceImageRGBChunk& msg)
{
  _animStreamer->Process_displayFaceImageChunk(msg);
}

void Process_playAnimWithSpriteBoxRemaps(const Anki::Vector::RobotInterface::PlayAnimWithSpriteBoxRemaps& msg)
{
  _animStreamer->Process_playAnimWithSpriteBoxRemaps(msg);
}

void Process_playAnimWithSpriteBoxKeyFrames(const Anki::Vector::RobotInterface::PlayAnimWithSpriteBoxKeyFrames& msg)
{
  _animStreamer->Process_playAnimWithSpriteBoxKeyFrames(msg);
}

void Process_addSpriteBoxKeyFrames(const Anki::Vector::RobotInterface::AddSpriteBoxKeyFrames& msg)
{
  _animStreamer->Process_addSpriteBoxKeyFrames(msg);
}

void Process_enableKeepFaceAlive(const Anki::Vector::RobotInterface::EnableKeepFaceAlive& msg)
{
  _animStreamer->EnableKeepFaceAlive(msg.enable, msg.disableTimeout_ms);
}

void Process_setKeepFaceAliveFocus(const Anki::Vector::RobotInterface::SetKeepFaceAliveFocus& msg)
{
  _animStreamer->SetKeepFaceAliveFocus(msg.enable);
}

void Process_addOrUpdateEyeShift(const Anki::Vector::RobotInterface::AddOrUpdateEyeShift& msg)
{
  _animStreamer->ProcessAddOrUpdateEyeShift(msg);
}

void Process_removeEyeShift(const Anki::Vector::RobotInterface::RemoveEyeShift& msg)
{
  _animStreamer->ProcessRemoveEyeShift(msg);
}

void Process_addSquint(const Anki::Vector::RobotInterface::AddSquint& msg)
{
  _animStreamer->ProcessAddSquint(msg);
}

void Process_removeSquint(const Anki::Vector::RobotInterface::RemoveSquint& msg)
{
  _animStreamer->ProcessRemoveSquint(msg);
}

void Process_postAudioEvent(const Anki::AudioEngine::Multiplexer::PostAudioEvent& msg)
{
  _engAudioInput->HandleMessage(msg);
}

void Process_stopAllAudioEvents(const Anki::AudioEngine::Multiplexer::StopAllAudioEvents& msg)
{
  _engAudioInput->HandleMessage(msg);
}

void Process_postAudioGameState(const Anki::AudioEngine::Multiplexer::PostAudioGameState& msg)
{
  _engAudioInput->HandleMessage(msg);
}

void Process_postAudioSwitchState(const Anki::AudioEngine::Multiplexer::PostAudioSwitchState& msg)
{
  _engAudioInput->HandleMessage(msg);
}

void Process_postAudioParameter(const Anki::AudioEngine::Multiplexer::PostAudioParameter& msg)
{
  _engAudioInput->HandleMessage(msg);
}

void Process_setDebugConsoleVarMessage(const Anki::Vector::RobotInterface::SetDebugConsoleVarMessage& msg)
{
  // We are using messages generated by the CppLite emitter here, which does not support
  // variable length arrays. CLAD also doesn't have a char, so the "strings" in this message
  // are actually arrays of uint8's. Thus we need to do this reinterpret cast here.
  // In some future world, ideally we avoid all this and use, for example, a web interface to
  // set/access console vars, instead of passing around via CLAD messages.
  const char* varName  = reinterpret_cast<const char *>(msg.varName);
  const char* tryValue = reinterpret_cast<const char *>(msg.tryValue);

  // TODO: Ideally, we'd send back a verify message that we (failed to) set this

  Anki::Util::IConsoleVariable* consoleVar = Anki::Util::ConsoleSystem::Instance().FindVariable(varName);
  if (consoleVar && consoleVar->ParseText(tryValue))
  {
    //SendVerifyDebugConsoleVarMessage(_externalInterface, varName, consoleVar->ToString().c_str(), consoleVar, true);
    LOG_INFO("AnimProcessMessages.Process_setDebugConsoleVarMessage.Success", "'%s' set to '%s'", varName, tryValue);
  }
  else
  {
    LOG_WARNING("AnimProcessMessages.Process_setDebugConsoleVarMessage.Fail", "Error setting '%s' to '%s'",
                varName, tryValue);
    //      SendVerifyDebugConsoleVarMessage(_externalInterface, msg.varName.c_str(),
    //                                       consoleVar ? "Error: Failed to Parse" : "Error: No such variable",
    //                                       consoleVar, false);
  }
}

void Process_startRecordingMicsRaw(const Anki::Vector::RobotInterface::StartRecordingMicsRaw& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem == nullptr)
  {
    return;
  }

  micDataSystem->RecordRawAudio(msg.duration_ms,
                                std::string(msg.path,
                                            msg.path_length),
                                msg.runFFT);
}

void Process_startRecordingMicsProcessed(const Anki::Vector::RobotInterface::StartRecordingMicsProcessed& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem == nullptr)
  {
    return;
  }

  micDataSystem->RecordProcessedAudio(msg.duration_ms,
                                      std::string(msg.path,
                                                  msg.path_length));
}

void Process_startWakeWordlessStreaming(const Anki::Vector::RobotInterface::StartWakeWordlessStreaming& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if(micDataSystem == nullptr){
    return;
  }

  micDataSystem->StartWakeWordlessStreaming(static_cast<CloudMic::StreamType>(msg.streamType),
                                            msg.playGetInFromAnimProcess);
}

void Process_setTriggerWordResponse(const Anki::Vector::RobotInterface::SetTriggerWordResponse& msg)
{
  auto* showStreamStateManager = _context->GetShowAudioStreamStateManager();
  if(showStreamStateManager == nullptr){
    return;
  }

  showStreamStateManager->SetTriggerWordResponse(msg);
}

void Process_setAlexaUXResponses(const Anki::Vector::RobotInterface::SetAlexaUXResponses& msg)
{
  auto* showStreamStateManager = _context->GetShowAudioStreamStateManager();
  if( showStreamStateManager != nullptr ) {
    showStreamStateManager->SetAlexaUXResponses( msg );
  }
}

void Process_resetBeatDetector(const Anki::Vector::RobotInterface::ResetBeatDetector& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem != nullptr) {
    micDataSystem->ResetBeatDetector();
  }
}

void Process_setAlexaUsage(const Anki::Vector::RobotInterface::SetAlexaUsage& msg)
{
  auto* alexa = _context->GetAlexa();
  if (alexa != nullptr) {
    alexa->SetAlexaUsage( msg.optedIn );
  }
}

void Process_setButtonWakeWord(const Anki::Vector::RobotInterface::SetButtonWakeWord& msg)
{
  auto* micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem != nullptr) {
    micDataSystem->SetButtonWakeWordIsAlexa( msg.isAlexa );
  }
}

void Process_setLCDBrightnessLevel(const Anki::Vector::RobotInterface::SetLCDBrightnessLevel& msg)
{
  FaceDisplay::getInstance()->SetFaceBrightness(msg.level);
}

void Process_playbackAudioStart(const Anki::Vector::RobotInterface::StartPlaybackAudio& msg)
{
  using namespace Audio;

  AudioPlaybackSystem* audioPlayer = _context->GetAudioPlaybackSystem();
  if (audioPlayer == nullptr)
  {
    return;
  }

  audioPlayer->PlaybackAudio( std::string(msg.path, msg.path_length) );
}

void Process_drawTextOnScreen(const Anki::Vector::RobotInterface::DrawTextOnScreen& msg)
{
  FaceInfoScreenManager::getInstance()->SetCustomText(msg);
}

void Process_runDebugConsoleFuncMessage(const Anki::Vector::RobotInterface::RunDebugConsoleFuncMessage& msg)
{
  // We are using messages generated by the CppLite emitter here, which does not support
  // variable length arrays. CLAD also doesn't have a char, so the "strings" in this message
  // are actually arrays of uint8's. Thus we need to do this reinterpret cast here.
  // In some future world, ideally we avoid all this and use, for example, a web interface to
  // set/access console vars, instead of passing around via CLAD messages.
  const char* funcName  = reinterpret_cast<const char *>(msg.funcName);
  const char* funcArgs = reinterpret_cast<const char *>(msg.funcArgs);

  // TODO: Ideally, we'd send back a verify message that we (failed to) set this
  Anki::Util::IConsoleFunction* consoleFunc = Anki::Util::ConsoleSystem::Instance().FindFunction(funcName);
  if (consoleFunc) {
    enum { kBufferSize = 512 };
    char buffer[kBufferSize];
    const uint32_t res = NativeAnkiUtilConsoleCallFunction(funcName, funcArgs, kBufferSize, buffer);
    LOG_INFO("AnimProcessMessages.Process_runDebugConsoleFuncMessage", "%s '%s' set to '%s'",
                     (res != 0) ? "Success" : "Failure", funcName, funcArgs);
  }
  else
  {
    LOG_WARNING("AnimProcessMessages.Process_runDebugConsoleFuncMessage.NoConsoleFunc", "No Func named '%s'",funcName);
  }
}

void Process_externalAudioChunk(const RobotInterface::ExternalAudioChunk& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_externalAudioPrepare(const RobotInterface::ExternalAudioPrepare& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_externalAudioComplete(const RobotInterface::ExternalAudioComplete& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_externalAudioCancel(const RobotInterface::ExternalAudioCancel& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_textToSpeechPrepare(const RobotInterface::TextToSpeechPrepare& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_textToSpeechPlay(const RobotInterface::TextToSpeechPlay& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_textToSpeechCancel(const RobotInterface::TextToSpeechCancel& msg)
{
  _animEngine->HandleMessage(msg);
}

void Process_setConnectionStatus(const Anki::Vector::SwitchboardInterface::SetConnectionStatus& msg)
{
  using namespace SwitchboardInterface;
  auto* bc = _context->GetBackpackLightComponent();
  bc->SetPairingLight((msg.status == ConnectionStatus::START_PAIRING ||
                       msg.status == ConnectionStatus::SHOW_PRE_PIN ||
                       msg.status == ConnectionStatus::SHOW_PIN));

  UpdateConnectionFlow(std::move(msg), _animStreamer, _context);
}

void Process_showUrlFace(const RobotInterface::ShowUrlFace& msg)
{
  if(msg.show) {
    using namespace SwitchboardInterface;
    Anki::Vector::SwitchboardInterface::SetConnectionStatus connMsg;
    connMsg.status = ConnectionStatus::SHOW_URL_FACE;

    UpdateConnectionFlow(std::move(connMsg), _animStreamer, _context);
  }
}
  
void Process_exitCCScreen(const RobotInterface::ExitCCScreen& msg)
{
  FaceInfoScreenManager::getInstance()->ExitCCScreen(_animStreamer);
}


void Process_setBLEPin(const Anki::Vector::SwitchboardInterface::SetBLEPin& msg)
{
  SetBLEPin(msg.pin);
}

void Process_rangeDataToDisplay(const Anki::Vector::RobotInterface::RangeDataToDisplay& msg)
{
  FaceInfoScreenManager::getInstance()->DrawToF(msg.data);
}

void Process_sendBLEConnectionStatus(const Anki::Vector::SwitchboardInterface::SendBLEConnectionStatus& msg)
{
  // todo
}

void Process_alterStreamingAnimation(const RobotInterface::AlterStreamingAnimationAtTime& msg)
{
  _streamingAnimationModifier->HandleMessage(msg);
}

void Process_setLocale(const RobotInterface::SetLocale& msg)
{
  DEV_ASSERT(_animEngine != nullptr, "AnimProcessMessages.SetLocale.InvalidEngine");
  _animEngine->HandleMessage(msg);
}

void Process_batteryStatus(const RobotInterface::BatteryStatus& msg)
{
  _context->GetBackpackLightComponent()->UpdateBatteryStatus(msg);
  _context->GetMicDataSystem()->SetBatteryLowStatus(msg.isLow);
}

void Process_acousticTestEnabled(const Anki::Vector::RobotInterface::AcousticTestEnabled& msg)
{
  bool enabled = msg.enabled;
  _animStreamer->SetFrozenOnCharger( enabled );
  auto* alexa = _context->GetAlexa();
  if( alexa != nullptr ) {
    alexa->SetFrozenOnCharger( enabled );
  }
  auto* showStreamStateManager = _context->GetShowAudioStreamStateManager();
  if( showStreamStateManager != nullptr ) {
    showStreamStateManager->SetFrozenOnCharger( enabled );
  }
}

void Process_triggerBackpackAnimation(const RobotInterface::TriggerBackpackAnimation& msg)
{
  _context->GetBackpackLightComponent()->SetBackpackAnimation(msg.trigger);
}

void Process_engineFullyLoaded(const RobotInterface::EngineFullyLoaded& msg)
{
  _engineLoaded = true;

  FaceInfoScreenManager::getInstance()->OnEngineLoaded();

  auto* alexa = _context->GetAlexa();
  if( alexa != nullptr ) {
    alexa->OnEngineLoaded();
  }
}

void Process_selfTestEnd(const RobotInterface::SelfTestEnd& msg)
{
  FaceInfoScreenManager::getInstance()->SelfTestEnd(_animStreamer);
}

void Process_enableMirrorModeScreen(const RobotInterface::EnableMirrorModeScreen& msg)
{
  FaceInfoScreenManager::getInstance()->EnableMirrorModeScreen(msg.enable);
}

void Process_updatedSettings(const RobotInterface::UpdatedSettings& msg)
{
  using namespace RobotInterface;
  switch (msg.settingBeingChanged)
  {
    case SettingBeingChanged::SETTING_ENABLE_DATA_COLLECTION:
      _context->GetMicDataSystem()->SetEnableDataCollectionSettings(msg.enableDataCollection);
      break;
    case SettingBeingChanged::SETTING_TIME_ZONE:
      std::string timeZone{msg.timeZone, msg.timeZone_length};
      _context->GetMicDataSystem()->UpdateTimeZone(timeZone);
      break;
  }
}

void Process_fakeWakeWordFromExternalInterface(const RobotInterface::FakeWakeWordFromExternalInterface& msg)
{
  _context->GetMicDataSystem()->FakeTriggerWordDetection();
}

void AnimProcessMessages::ProcessMessageFromEngine(const RobotInterface::EngineToRobot& msg)
{
  //LOG_WARNING("AnimProcessMessages.ProcessMessageFromEngine", "%d", msg.tag);
  bool forwardToRobot = false;
  switch (msg.tag)
  {
    case RobotInterface::EngineToRobot::Tag_absLocalizationUpdate:
    {
      forwardToRobot = true;
      _context->GetMicDataSystem()->ResetMicListenDirection();
      break;
    }
    case RobotInterface::EngineToRobot::Tag_calmPowerMode:
    {
      // Remember the power mode specified by engine so that we can go back to
      // it when pairing/debug screens are exited.
      // Only relay the power mode to robot process if not already in pairing/debug screen.
      FaceInfoScreenManager::getInstance()->SetCalmPowerModeOnReturnToNone(msg.calmPowerMode);
      forwardToRobot = FaceInfoScreenManager::getInstance()->GetCurrScreenName() == ScreenName::None;
      break;
    }
    case RobotInterface::EngineToRobot::Tag_setBackpackLights:
    {
      // Intercept the SetBackpackLights message from engine
      _context->GetBackpackLightComponent()->SetBackpackAnimation({msg.setBackpackLights});
      break;
    }

#include "clad/robotInterface/messageEngineToRobot_switch_from_0x50_to_0xAF.def"

    default:
      forwardToRobot = true;
      break;
  }

  if (forwardToRobot) {
    // Send message along to robot if it wasn't handled here
    AnimComms::SendPacketToRobot((char*)msg.GetBuffer(), msg.Size());
  }

} // ProcessMessageFromEngine()


// ========== END OF PROCESSING MESSAGES FROM ENGINE ==========


// ========== START OF PROCESSING MESSAGES FROM ROBOT ==========
// #pragma mark "RobotToEngine handlers"

static void ProcessMicDataMessage(const RobotInterface::MicData& payload)
{
  FaceInfoScreenManager::getInstance()->DrawMicInfo(payload);

  auto * micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem != nullptr)
  {
    micDataSystem->ProcessMicDataPayload(payload);
  }
}

static void HandleRobotStateUpdate(const Anki::Vector::RobotState& robotState)
{
  _pendingRobotDisconnectTime_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds() + kNoRobotStateDisconnectTimeout_sec;

  FaceInfoScreenManager::getInstance()->Update(robotState);

#if ANKI_DEV_CHEATS
  auto * micDataSystem = _context->GetMicDataSystem();
  if (micDataSystem != nullptr)
  {
    const bool isMicFace = FaceInfoScreenManager::getInstance()->GetCurrScreenName() == ScreenName::MicDirectionClock;
    if (isMicFace)
    {
      const auto liftHeight_mm = ConvertLiftAngleToLiftHeightMM(robotState.liftAngle);
      if (LIFT_HEIGHT_CARRY-1.f <= liftHeight_mm)
      {
        micDataSystem->SetForceRecordClip(true);
      }
    }
  }
#endif
}

void AnimProcessMessages::ProcessMessageFromRobot(const RobotInterface::RobotToEngine& msg)
{
  const auto tag = msg.tag;
  switch (tag)
  {
    case RobotInterface::RobotToEngine::Tag_robotServerDisconnect:
    {
      AnimComms::DisconnectRobot();
    }
    break;
    case RobotInterface::RobotToEngine::Tag_prepForShutdown:
    {
      PRINT_NAMED_INFO("AnimProcessMessages.ProcessMessageFromRobot.Shutdown","");
      // Need to wait a couple ticks before actually shutting down so that this message
      // can be forwarded up to engine
      _countToShutdown = kNumTicksToShutdown;
    }
    break;
    case RobotInterface::RobotToEngine::Tag_micData:
    {
      const auto& payload = msg.micData;
      ProcessMicDataMessage(payload);
      return;
    }
    break;
    case RobotInterface::RobotToEngine::Tag_state:
    {
      HandleRobotStateUpdate(msg.state);
      const bool onChargerContacts = (msg.state.status & (uint32_t)RobotStatusFlag::IS_ON_CHARGER);
      _animStreamer->SetOnCharger(onChargerContacts);
      auto* showStreamStateManager = _context->GetShowAudioStreamStateManager();
      if (showStreamStateManager != nullptr)
      {
        showStreamStateManager->SetOnCharger( onChargerContacts );
      }
      auto* alexa = _context->GetAlexa();
      if (alexa != nullptr)
      {
        alexa->SetOnCharger( onChargerContacts );
      }

    }
    break;
    case RobotInterface::RobotToEngine::Tag_stillAlive:
    {
      _pendingRobotDisconnectTime_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds() + kNoRobotStateDisconnectTimeout_sec;
    }
    break;
    case RobotInterface::RobotToEngine::Tag_robotStopped:
    {
      LOG_INFO("AnimProcessMessages.ProcessMessageFromRobot.RobotStopped", "Abort animation");
      _animStreamer->Abort();
    }
    break;
    case RobotInterface::RobotToEngine::Tag_syncRobotAck:
    {
      std::string version((const char*)&msg.syncRobotAck.sysconVersion, 16);
      FaceInfoScreenManager::getInstance()->SetSysconVersion(version);
    }
    break;
    default:
    {

    }
    break;
  }

  // Forward to engine
  SendAnimToEngine(msg);

} // ProcessMessageFromRobot()

// ========== END OF PROCESSING MESSAGES FROM ROBOT ==========

// ========== START OF CLASS METHODS ==========
// #pragma mark "Class methods"

Result AnimProcessMessages::Init(Anim::AnimEngine* animEngine,
                                 Anim::AnimationStreamer* animStreamer,
                                 Anim::StreamingAnimationModifier* streamingAnimationModifier,
                                 Audio::EngineRobotAudioInput* audioInput,
                                 const Anim::AnimContext* context)
{
  // Preconditions
  DEV_ASSERT(nullptr != animEngine, "AnimProcessMessages.Init.InvalidAnimEngine");
  DEV_ASSERT(nullptr != animStreamer, "AnimProcessMessages.Init.InvalidAnimStreamer");
  DEV_ASSERT(nullptr != audioInput, "AnimProcessMessages.Init.InvalidAudioInput");
  DEV_ASSERT(nullptr != context, "AnimProcessMessages.Init.InvalidAnimContext");

  // Setup robot and engine sockets
  AnimComms::InitComms();

  _animEngine             = animEngine;
  _animStreamer           = animStreamer;
  _streamingAnimationModifier  = streamingAnimationModifier;
  _proceduralAudioClient  = _animStreamer->GetProceduralAudioClient();
  _engAudioInput          = audioInput;
  _context                = context;

  _connectionFlowInited = InitConnectionFlow(_animStreamer);

  return RESULT_OK;
}

Result AnimProcessMessages::MonitorConnectionState(BaseStationTime_t currTime_nanosec)
{
  // If nonzero, we are scheduled to display a NO_ENGINE_COMMS fault code
  static BaseStationTime_t displayFaultCodeTime_nanosec = 0;

  // Amount of time for which we must be disconnected from the engine in order
  // to display the NO_ENGINE_COMMS fault code.
  static const BaseStationTime_t kDisconnectedTimeout_ns = Util::SecToNanoSec(5.f);

  // Check for changes in connection state to engine and send RobotAvailable
  // message when engine connects
  static bool wasConnected = false;
  const bool isConnected = AnimComms::IsConnectedToEngine();
  if (!wasConnected && isConnected) {
    LOG_INFO("AnimProcessMessages.MonitorConnectionState", "Robot now available");
    RobotInterface::SendAnimToEngine(RobotAvailable());

    // Clear any scheduled fault code display
    displayFaultCodeTime_nanosec = 0;

    wasConnected = true;
  } else if (wasConnected && !isConnected) {
    // We've just become unconnected. Start a timer to display the fault
    // code on the face at the desired time.
    displayFaultCodeTime_nanosec = currTime_nanosec + kDisconnectedTimeout_ns;

    PRINT_NAMED_WARNING("AnimProcessMessages.MonitorConnectionState.DisconnectedFromEngine",
                        "We have become disconnected from engine process. Displaying a fault code in %.1f seconds.",
                        Util::NanoSecToSec(kDisconnectedTimeout_ns));

    wasConnected = false;
  }

  // Display fault code if necessary
  if ((displayFaultCodeTime_nanosec > 0) &&
      (currTime_nanosec > displayFaultCodeTime_nanosec)) {
    displayFaultCodeTime_nanosec = 0;
    FaultCode::DisplayFaultCode(FaultCode::NO_ENGINE_COMMS);
  }

  return RESULT_OK;
}

Result AnimProcessMessages::Update(BaseStationTime_t currTime_nanosec)
{
  if(_countToShutdown > 0)
  {
    if(--_countToShutdown == 0)
    {
      LOG_INFO("AnimProcessMessages.Update.Shutdown","");
      // RESULT_SHUTDOWN will kick us out of the main update loop
      // and cause the process to exit cleanly
      return RESULT_SHUTDOWN;
    }
  }

  ANKI_CPU_PROFILE("AnimProcessMessages::Update");

  _messageCountAnimToRobot = 0;
  _messageCountAnimToEngine = 0;
  _messageCountRobotToAnim = 0;
  _messageCountEngineToAnim = 0;

  // Keep trying to init the connection flow until it works
  // which will be when the robot name has been set by switchboard
  if(!_connectionFlowInited)
  {
    _connectionFlowInited = InitConnectionFlow(_animStreamer);
  }

  if (!AnimComms::IsConnectedToRobot()) {
    static bool faultCodeDisplayed = false;
    if (!faultCodeDisplayed) {
      LOG_WARNING("AnimProcessMessages.Update.NoConnectionToRobot", "");
      FaultCode::DisplayFaultCode(FaultCode::NO_ROBOT_COMMS);
      faultCodeDisplayed = true;
    }
  } else if ((_pendingRobotDisconnectTime_sec > 0.f) &&
      (BaseStationTimer::getInstance()->GetCurrentTimeInSeconds() > _pendingRobotDisconnectTime_sec)) {
    // Disconnect robot if it hasn't been heard from in a while
    LOG_WARNING("AnimProcessMessages.Update.RobotStateTimeout", "Disconnecting robot");
    AnimComms::DisconnectRobot();
    _pendingRobotDisconnectTime_sec = -1.f;
  }

  MonitorConnectionState(currTime_nanosec);

  _context->GetMicDataSystem()->Update(currTime_nanosec);
  _context->GetAudioPlaybackSystem()->Update(currTime_nanosec);
  _context->GetShowAudioStreamStateManager()->Update();
  _context->GetAlexa()->Update();

  // Process incoming messages from engine
  u32 dataLen;

  // Process messages from engine
  {
    ANKI_CPU_PROFILE("ProcessMessageFromEngine");

    while((dataLen = AnimComms::GetNextPacketFromEngine(pktBuffer_, MAX_PACKET_BUFFER_SIZE)) > 0)
    {
      ++_messageCountEngineToAnim;
      Anki::Vector::RobotInterface::EngineToRobot msg;
      memcpy(msg.GetBuffer(), pktBuffer_, dataLen);
      if (msg.Size() != dataLen) {
        LOG_WARNING("AnimProcessMessages.Update.EngineToRobot.InvalidSize",
                    "Invalid message size from engine (%d != %d)",
                    msg.Size(), dataLen);
        continue;
      }
      if (!msg.IsValid()) {
        LOG_WARNING("AnimProcessMessages.Update.EngineToRobot.InvalidData", "Invalid message from engine");
        continue;
      }
      ProcessMessageFromEngine(msg);
    }
  }

  // Process messages from robot
  {
    ANKI_CPU_PROFILE("ProcessMessageFromRobot");

    while ((dataLen = AnimComms::GetNextPacketFromRobot(pktBuffer_, MAX_PACKET_BUFFER_SIZE)) > 0)
    {
      ++_messageCountRobotToAnim;
      Anki::Vector::RobotInterface::RobotToEngine msg;
      memcpy(msg.GetBuffer(), pktBuffer_, dataLen);
      if (msg.Size() != dataLen) {
        LOG_WARNING("AnimProcessMessages.Update.RobotToEngine.InvalidSize",
                    "Invalid message size from robot (%d != %d)",
                    msg.Size(), dataLen);
        continue;
      }
      if (!msg.IsValid()) {
        LOG_WARNING("AnimProcessMessages.Update.RobotToEngine.InvalidData", "Invalid message from robot");
        continue;
      }
      ProcessMessageFromRobot(msg);
      _proceduralAudioClient->ProcessMessage(msg);
    }
  }

#if FACTORY_TEST
#if defined(SIMULATOR)
  // Simulator never has EMR
  FaceInfoScreenManager::getInstance()->SetShouldDrawFAC(false);
#else
  FaceInfoScreenManager::getInstance()->SetShouldDrawFAC(!Factory::GetEMR()->fields.PACKED_OUT_FLAG);
#endif
#endif

  // If the boot anim has not already been stopped,
  // MicDataSystem has a cloud connection,
  // Engine has synced with the robot, and
  // Engine is fully loaded and ready
  // then stop the boot animation
  if(!_bootAnimStopped &&
     _context->GetMicDataSystem()->HasConnectionToCloud() &&
     _engineLoaded)
  {
    _bootAnimStopped = true;
    FaceDisplay::getInstance()->StopBootAnim();
  }

  return RESULT_OK;
}



bool AnimProcessMessages::SendAnimToRobot(const RobotInterface::EngineToRobot& msg)
{
  static Util::MessageProfiler msgProfiler("AnimProcessMessages::SendAnimToRobot");
  LOG_TRACE("AnimProcessMessages.SendAnimToRobot", "Send tag %d size %u", msg.tag, msg.Size());
  bool result = AnimComms::SendPacketToRobot(msg.GetBuffer(), msg.Size());
  if (result) {
    msgProfiler.Update(msg.tag, msg.Size());
  } else {
    msgProfiler.ReportOnFailure();
  }
  ++_messageCountAnimToRobot;
  return result;
}

bool AnimProcessMessages::SendAnimToEngine(const RobotInterface::RobotToEngine & msg)
{
  static Util::MessageProfiler msgProfiler("AnimProcessMessages::SendAnimToEngine");

  LOG_TRACE("AnimProcessMessages.SendAnimToEngine", "Send tag %d size %u", msg.tag, msg.Size());
  bool result = AnimComms::SendPacketToEngine(msg.GetBuffer(), msg.Size());
  if (result) {
    msgProfiler.Update(msg.tag, msg.Size());
  } else {
    msgProfiler.ReportOnFailure();
  }
  ++_messageCountAnimToEngine;
  return result;
}

} // namespace Vector
} // namespace Anki
