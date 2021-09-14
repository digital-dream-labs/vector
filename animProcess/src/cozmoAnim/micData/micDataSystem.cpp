/**
* File: micDataSystem.cpp
*
* Author: Lee Crippen
* Created: 03/26/2018
*
* Description: Handles Updates to mic data processing, streaming collection jobs, and generally acts
*              as messaging/access hub.
*
* Copyright: Anki, Inc. 2018
*
*/

#include "coretech/messaging/shared/LocalUdpServer.h"
#include "coretech/messaging/shared/socketConstants.h"

#include "audioEngine/audioCallback.h"
#include "audioEngine/audioTypeTranslator.h"
#include "audioUtil/speechRecognizer.h"
#include "cozmoAnim/alexa/alexa.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/audio/cozmoAudioController.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"
#include "cozmoAnim/beatDetector/beatDetector.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenManager.h"
#include "cozmoAnim/micData/micDataInfo.h"
#include "cozmoAnim/micData/micDataProcessor.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/showAudioStreamStateManager.h"

#include "audioEngine/plugins/ankiPluginInterface.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "osState/osState.h"

#include "util/console/consoleInterface.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/math/math.h"

#include "webServerProcess/src/webService.h"

#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"

#include <iomanip>
#include <sstream>

namespace {
#define LOG_CHANNEL "Microphones"
#define CONSOLE_GROUP "MicData"
#define RECOGNIZER_CONSOLE_GROUP "SpeechRecognizer"

#if ANKI_DEV_CHEATS
CONSOLE_VAR_RANGED(u32, kMicData_ClipRecordTime_ms, CONSOLE_GROUP, 4000, 500, 15000);
CONSOLE_VAR(bool, kSuppressTriggerResponse, RECOGNIZER_CONSOLE_GROUP, false);
#endif // ANKI_DEV_CHEATS

const std::string kMicSettingsFile = "micMuted";
const std::string kSpeechRecognizerWebvizName = "speechrecognizersys";
}

namespace Anki {
namespace Vector {
  
// VIC-13319 remove
CONSOLE_VAR_EXTERN(bool, kAlexaEnabledInUK);
CONSOLE_VAR_EXTERN(bool, kAlexaEnabledInAU);
  
namespace MicData {

constexpr auto kCladMicDataTypeSize = sizeof(RobotInterface::MicData::data)/sizeof(RobotInterface::MicData::data[0]);
static_assert(kCladMicDataTypeSize == kIncomingAudioChunkSize, 
              "Expecting size of MicData::data to match DeinterlacedAudioChunk");

static_assert(
  std::is_same<std::remove_reference<decltype(RobotInterface::MicDirection::confidenceList[0])>::type,
  decltype(MicDirectionData::confidenceList)::value_type>::value,
  "Expecting type of RobotInterface::MicDirection::confidenceList items "\
  "to match MicDirectionData::confidenceList items");

constexpr auto kMicDirectionConfListSize = sizeof(RobotInterface::MicDirection::confidenceList);
constexpr auto kMicDirectionConfListItemSize = sizeof(RobotInterface::MicDirection::confidenceList[0]);
static_assert(
  kMicDirectionConfListSize / kMicDirectionConfListItemSize ==
  decltype(MicDirectionData::confidenceList)().size(),
  "Expecting length of RobotInterface::MicDirection::confidenceList to match MicDirectionData::confidenceList");

void MicDataSystem::SetupConsoleFuncs()
{
#if ANKI_DEV_CHEATS
  const auto enableTriggerHistoryFunc = [this](ConsoleFunctionContextRef context)
  {
    const bool enable = ConsoleArg_Get_Bool( context, "enable" );
    EnableTriggerHistory(enable);
    context->channel->WriteLog("EnableRecentTriggers %s", enable ? "enabled" : "disable");
  };
  _devConsoleFuncs.emplace_front("EnableTriggerResults", std::move(enableTriggerHistoryFunc),
                                 RECOGNIZER_CONSOLE_GROUP, "bool enable");
  
  const auto clearMicDataFunc = [this](ConsoleFunctionContextRef context)
  {
    if (!_writeLocationDir.empty()) {
      Anki::Util::FileUtils::RemoveDirectory(_writeLocationDir);
      context->channel->WriteLog("Removed directory '%s'", _writeLocationDir.c_str());
    }
  };
  _devConsoleFuncs.emplace_front("ClearMicData", std::move(clearMicDataFunc), CONSOLE_GROUP".zHiddenForSafety", "");
#endif
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MicDataSystem::MicDataSystem(Util::Data::DataPlatform* dataPlatform,
                             const Anim::AnimContext* context)
: _context(context)
, _udpServer(new LocalUdpServer())
, _fftResultData(new FFTResultData())
, _alexaState(AlexaSimpleState::Disabled)
, _micMuted(false)
, _abortAlexaScreenDueToHeyVector(false)
{
  const std::string& dataWriteLocation = dataPlatform->pathToResource(Util::Data::Scope::Cache, "micdata");
  const std::string& triggerDataDir = dataPlatform->pathToResource(Util::Data::Scope::Resources, "assets");
  _writeLocationDir = dataWriteLocation;
  _micDataProcessor.reset(new MicDataProcessor(_context, this, dataWriteLocation));
  _speechRecognizerSystem.reset(new SpeechRecognizerSystem(_context, this, triggerDataDir));
  
  _persistentFolder = Util::FileUtils::AddTrailingFileSeparator( dataPlatform->pathToResource(Util::Data::Scope::Persistent, "") );

  if (!_writeLocationDir.empty())
  {
  #if ANKI_DEV_CHEATS
    Util::FileUtils::CreateDirectory(_writeLocationDir);
  #endif
  }

  const RobotID_t robotID = OSState::getInstance()->GetRobotID();
  const std::string sockName = std::string{MIC_SERVER_BASE_PATH} + (robotID == 0 ? "" : std::to_string(robotID));
  _udpServer->SetBindClients(false);
  const bool udpSuccess = _udpServer->StartListening(sockName);
  ANKI_VERIFY(udpSuccess,
              "MicDataSystem.Constructor.UdpStartListening",
              "Failed to start listening on socket %s",
              sockName.c_str());
}

void MicDataSystem::Init(const Anim::RobotDataLoader& dataLoader)
{
  // SpeechRecognizerSystem
  SpeechRecognizerSystem::TriggerWordDetectedCallback callback = [this] (const AudioUtil::SpeechRecognizerCallbackInfo& info) {
    
 #if ANKI_DEV_CHEATS
    SendTriggerDetectionToWebViz(info, {});
    if (kSuppressTriggerResponse) {
      return;
    }
#endif
    
    if( _alexaState == AlexaSimpleState::Active ) {
      // Don't run "hey vector" when alexa is in the middle of an interaction, or if the mic is muted
      return;
    }
    
    // saying "hey vector" should exit certain alexa debug screens and cancel auth. FaceInfoScreen isn't
    // currently set up to handle threads, so set a flag that is handled in Update()
    _abortAlexaScreenDueToHeyVector = true;
    
    _micDataProcessor->VoiceTriggerWordDetection( info );
    SendRecognizerDasLog( info, nullptr );
  };
  _speechRecognizerSystem->InitVector(dataLoader, _locale, callback);
  _micDataProcessor->Init();
  
  if( Util::FileUtils::FileExists(_persistentFolder + kMicSettingsFile) ) {
    ToggleMicMute();
  }
  
#if ANKI_DEV_CHEATS
  auto* webService = _context->GetWebService();
  if( webService ) {
    AddSignalHandle(webService->OnWebVizSubscribed(kSpeechRecognizerWebvizName).ScopedSubscribe(
      [this] (const auto& sendFunc) { SendRecentTriggerDetectionToWebViz(sendFunc); }
    ));
  }
  SetupConsoleFuncs();
#endif
}

MicDataSystem::~MicDataSystem()
{
  // Tear down the mic data processor explicitly first, because it uses functionality owned by MicDataSystem
  _micDataProcessor.reset();

  _udpServer->StopListening();
}

void MicDataSystem::ProcessMicDataPayload(const RobotInterface::MicData& payload)
{
  _micDataProcessor->ProcessMicDataPayload(payload);
}

void MicDataSystem::RecordRawAudio(uint32_t duration_ms, const std::string& path, bool runFFT)
{
  RecordAudioInternal(duration_ms, path, MicDataType::Raw, runFFT);
}

void MicDataSystem::RecordProcessedAudio(uint32_t duration_ms, const std::string& path)
{
  RecordAudioInternal(duration_ms, path, MicDataType::Processed, false);
}

void MicDataSystem::StartWakeWordlessStreaming(CloudMic::StreamType type, bool playGetInFromAnimProcess)
{
  
  if(HasStreamingJob())
  {
    // We "fake" having a streaming job in order to achieve the "feel" of a minimum streaming time for UX
    // reasons (I think?). This means that HasStreamingJob() may actually lie, so if we have a job, but have
    // completed streaming, and the engine is requesting a wakewordless stream (e.g. knowledge graph), we
    // should clear it now. This is a workaround to fix VIC-13402 (a blocker for R1.4.1)

    // TODO:(bn) VIC-13438 this is tech debt and should be cleaned up

    if( _currentlyStreaming &&
        _streamingComplete ) {

      LOG_INFO("MicDataSystem.StartWakeWordlessStreaming.OverlappingWithFakeStream.Opening",
               "Request came in overlapping with a 'fake' extended request, so cancel it before starting a new one");
      ClearCurrentStreamingJob();
    }
    else {
      LOG_WARNING("MicDataSystem.StartWakeWordlessStreaming.OverlappingStreamRequests",
                  "Received StartWakeWorldlessStreaming message from engine, but micDataSystem is already streaming (not faking to extend the stream)");
      return;
    }
  }

  // we want to start the stream AFTER the audio is complete so that it is not captured in the stream
  auto callback = [this,type]( bool success )
  {
    // if we didn't succeed, it means that we didn't have a wake word response setup
    if(success){
      // it would be highly unlikely that we started another streaming job while waiting for the earcon,
      // but doesn't hurt to check
      if (!HasStreamingJob()) {
        _micDataProcessor->CreateStreamJob(type, kTriggerLessOverlapSize_ms);
        LOG_INFO("MicDataSystem.StartWakeWordlessStreaming.StartStreaming",
                 "Starting Wake Wordless streaming");
      }
      else {
        LOG_WARNING("MicDataSystem.StartWakeWordlessStreaming.OverlappingStreamRequests",
                    "Started streaming job while waiting for StartTriggerResponseWithoutGetIn callback");
        SetWillStream(false);
      }
    }
    else {
      LOG_WARNING("MicDataSystem.StartWakeWordlessStreaming.CantStreamToCloud",
                  "Wakewordless streaming request received, but incapable of opening the cloud stream, so ignoring request");
      SetWillStream(false);
    }
  };

  ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
  if(showStreamState->HasValidTriggerResponse()){
    SetWillStream(true);
  }

  if(playGetInFromAnimProcess){
    showStreamState->SetPendingTriggerResponseWithGetIn(callback);
  }
  else {
    showStreamState->SetPendingTriggerResponseWithoutGetIn(callback);
  }
}

void MicDataSystem::FakeTriggerWordDetection()
{
  // completely ignore _micMuted and IsButtonPressAlexa() and stop alerts no matter what
  Alexa* alexa = _context->GetAlexa();
  ASSERT_NAMED(alexa != nullptr, "");
  if( alexa->StopAlertIfActive() ) {
    return;
  }
  
  const bool wasMuted = _micMuted;
  if( _micMuted ) {
    // A single press when muted should unmute and then trigger a wakeword.
    // This is an annoying code path since FaceInfoScreenManager::ToggleMute calls back into
    // MicDataSystem. But FaceInfoScreenManager is already set up to check for various button clicks...
    FaceInfoScreenManager::getInstance()->ToggleMute("SINGLE_PRESS");
    DEV_ASSERT( !_micMuted, "MicDataSystem.FakeTriggerWordDetect.StillMuted" );
  }
  
  if( IsButtonPressAlexa() ) {
    ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
    if( showStreamState->HasAnyAlexaResponse() ) {
      // "Alexa" button press
      alexa->NotifyOfTapToTalk( wasMuted );
    }
  }
  else {
    // "Hey Vector" button press
    // This next check is probably not necessary, but for symmetry, the hey vector button press shouldn't trigger
    // if alexa is in the middle of an interaction
    if( _alexaState != AlexaSimpleState::Active ) {
      _micDataProcessor->FakeTriggerWordDetection( wasMuted );
    }
  }
}

void MicDataSystem::RecordAudioInternal(uint32_t duration_ms, const std::string& path, MicDataType type, bool runFFT)
{
  MicDataInfo* newJob = new MicDataInfo{};

  // If the input path has a file separator, remove the filename at the end and use as the write directory
  if (path.find('/') != std::string::npos)
  {
    std::string nameBase = Util::FileUtils::GetFileName(path);
    newJob->_writeLocationDir = path.substr(0, path.length() - nameBase.length());
    newJob->_writeNameBase = nameBase;
  }
  else
  {
    // otherwise used the saved off write directory, and just the input path as the name base
    newJob->_writeLocationDir = _writeLocationDir;
    newJob->_writeNameBase = path;
  }
  newJob->_audioSaveCallback = std::bind(&MicDataSystem::AudioSaveCallback, this, std::placeholders::_1);

  newJob->EnableDataCollect(type, true);
  newJob->SetTimeToRecord(duration_ms);
  newJob->_doFFTProcess = runFFT;
  if (runFFT)
  {
    auto weakptr = std::weak_ptr<FFTResultData>(_fftResultData);
    newJob->_rawAudioFFTCallback = [weakdata = std::move(weakptr)] (std::vector<uint32>&& result) {
      if (auto resultdata = weakdata.lock())
      {
        std::lock_guard<std::mutex> _lock(resultdata->_fftResultMutex);
        resultdata->_fftResultList.push_back(std::move(result));
      }
    };
  }

  {
    std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
    _micProcessingJobs.push_back(std::shared_ptr<MicDataInfo>(newJob));
  }
}

void MicDataSystem::Update(BaseStationTime_t currTime_nanosec)
{
  _fftResultData->_fftResultMutex.lock();
  while (_fftResultData->_fftResultList.size() > 0)
  {
    auto result = std::move(_fftResultData->_fftResultList.front());
    _fftResultData->_fftResultList.pop_front();
    _fftResultData->_fftResultMutex.unlock();

    // Populate the fft result message
    auto msg = RobotInterface::AudioFFTResult();

    for(uint8_t i = 0; i < result.size(); ++i)
    {
      msg.result[i] = result[i];
    }
    RobotInterface::SendAnimToEngine(std::move(msg));

    _fftResultData->_fftResultMutex.lock();
  }
  _fftResultData->_fftResultMutex.unlock();

  bool receivedStopMessage = false;
  static constexpr size_t kMaxReceiveBytes = 2000;
  uint8_t receiveArray[kMaxReceiveBytes];

  const ssize_t bytesReceived = _udpServer->Recv((char*)receiveArray, kMaxReceiveBytes);
  if (bytesReceived > 0) {
    CloudMic::Message msg{receiveArray, (size_t)bytesReceived};
    switch (msg.GetTag()) {

      case CloudMic::MessageTag::stopSignal:
        LOG_INFO("MicDataSystem.Update.RecvCloudProcess.StopSignal", "");
        receivedStopMessage = true;
        break;

      #if ANKI_DEV_CHEATS
      case CloudMic::MessageTag::testStarted:
      {
        LOG_INFO("MicDataSystem.Update.RecvCloudProcess.FakeTrigger", "");
        _fakeStreamingState = true;

        // Set up a message to send out about the triggerword
        RobotInterface::TriggerWordDetected twDetectedMessage;
        twDetectedMessage.direction = kFirstIndex;
        // TODO:(bn) check stream state here? Currently just assuming streaming is on
        twDetectedMessage.willOpenStream = true;
        auto engineMessage = std::make_unique<RobotInterface::RobotToEngine>(std::move(twDetectedMessage));
        {
          std::lock_guard<std::mutex> lock(_msgsMutex);
          _msgsToEngine.push_back(std::move(engineMessage));
        }
        break;
      }
      #endif
      case CloudMic::MessageTag::connectionResult:
      {
        LOG_INFO("MicDataSystem.Update.RecvCloudProcess.connectionResult", "%s", msg.Get_connectionResult().status.c_str());
        FaceInfoScreenManager::getInstance()->SetNetworkStatus(msg.Get_connectionResult().code);

        // Send the results back to engine
        ReportCloudConnectivity msgToEngine;
        msgToEngine.code            = static_cast<ConnectionCode>(msg.Get_connectionResult().code);
        msgToEngine.numPackets      = msg.Get_connectionResult().numPackets;
        msgToEngine.expectedPackets = msg.Get_connectionResult().expectedPackets;
        RobotInterface::SendAnimToEngine(msgToEngine);
        break;
      }

      default:
        LOG_INFO("MicDataSystem.Update.RecvCloudProcess.UnexpectedSignal", "0x%x 0x%x", receiveArray[0], receiveArray[1]);
        receivedStopMessage = true;
        break;
    }
  }

#if ANKI_DEV_CHEATS
  uint32_t recordingSecondsRemaining = 0;
  static std::shared_ptr<MicDataInfo> _saveJob;
  if (_saveJob != nullptr)
  {
    if (_saveJob->CheckDone())
    {
      _saveJob = nullptr;
      _forceRecordClip = false;
    }
    else
    {
      recordingSecondsRemaining = (_saveJob->GetTimeToRecord_ms() - _saveJob->GetTimeRecorded_ms()) / 1000;
    }
  }

  if (_forceRecordClip && nullptr == _saveJob)
  {
    MicDataInfo* newJob = new MicDataInfo{};
    newJob->_writeLocationDir = Util::FileUtils::FullFilePath({_writeLocationDir, "debugCapture"});
    newJob->_writeNameBase = ""; // Use the autogen names in this subfolder
    newJob->_numMaxFiles = 100;
    newJob->EnableDataCollect(MicDataType::Processed, true);
    newJob->EnableDataCollect(MicDataType::Raw, true);
    newJob->SetTimeToRecord(kMicData_ClipRecordTime_ms);
    newJob->SetAudioFadeInTime(MicDataInfo::kDefaultAudioFadeIn_ms);

    {
      std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
      _micProcessingJobs.push_back(std::shared_ptr<MicDataInfo>(newJob));
      _saveJob = _micProcessingJobs.back();
    }
  }

  // Minimum length of time to display the "trigger heard" symbol on the mic data debug screen
  // (is extended by streaming)
  constexpr uint32_t kTriggerDisplayTime_ns = 1000 * 1000 * 2000; // 2 seconds
  static BaseStationTime_t endTriggerDispTime_ns = 0;
  if (endTriggerDispTime_ns > 0 && endTriggerDispTime_ns < currTime_nanosec)
  {
    endTriggerDispTime_ns = 0;
  }
#endif

  // lock the job mutex
  {
    std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);

    //  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    // ... this block is where we kick off a new stream to the cloud ...

    // check if the pointer to the currently streaming job is valid
    if (!_currentlyStreaming && HasStreamingJob()
      #if ANKI_DEV_CHEATS
        && !_forceRecordClip
      #endif
       )
    {
      #if ANKI_DEV_CHEATS
        endTriggerDispTime_ns = currTime_nanosec + kTriggerDisplayTime_ns;
      #endif
      if (_udpServer->HasClient())
      {
        _currentlyStreaming = true;
        _streamingComplete = ShouldSimulateStreaming();
        _streamingAudioIndex = 0;

        // even though this isn't necessarily the exact frame the backpack lights begin (since that's done in a different
        // thread), it doesn't make a noticeable difference since this is an arbitrary number and doesn't need to be precise
        _streamBeginTime_ns = currTime_nanosec;

        // Send out the message announcing the trigger word has been detected
        auto hw = CloudMic::Hotword{CloudMic::StreamType::Normal, _locale.ToString(),
                                    _timeZone, !_enableDataCollection};
        if (_currentStreamingJob != nullptr) {
          hw.mode = _currentStreamingJob->_type;
        }
        SendUdpMessage(CloudMic::Message::Createhotword(std::move(hw)));
        LOG_INFO("MicDataSystem.Update.StreamingStart", "");
      }
      else
      {
        ClearCurrentStreamingJob();
        LOG_INFO("MicDataSystem.Update.StreamingStartIgnored", "Ignoring stream start as no clients connected.");
      }
    }

    //  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    // ... this block is where we actually do the streaming of the mic data up to the cloud ...

    if (_currentlyStreaming)
    {
      bool realStreamHasFinished = false;

      // Are we done with what we want to stream?
      if (!_streamingComplete)
      {
        static constexpr size_t kMaxRecordNumChunks = (kStreamingTimeout_ms / kTimePerChunk_ms) + 1;
        const bool didTimeout = _streamingAudioIndex >= kMaxRecordNumChunks;
        if (receivedStopMessage || didTimeout)
        {
          _streamingComplete = true;
          realStreamHasFinished = true;

          if (didTimeout)
          {
            SendUdpMessage(CloudMic::Message::CreateaudioDone({}));
          }
          LOG_INFO("MicDataSystem.Update.StreamingEnd", "%zu ms", _streamingAudioIndex * kTimePerChunk_ms);
          #if ANKI_DEV_CHEATS
            _fakeStreamingState = false;
          #endif
        }
        else
        {
        #if ANKI_DEV_CHEATS
          if (!_fakeStreamingState)
        #endif
          {
            // Copy any new data that has been pushed onto the currently streaming job
            AudioUtil::AudioChunkList newAudio = _currentStreamingJob->GetProcessedAudio(_streamingAudioIndex);
            _streamingAudioIndex += newAudio.size();

            // Send the audio to any clients we've got
            if (_udpServer->HasClient())
            {
              for(const auto& audioChunk : newAudio)
              {
                SendUdpMessage(CloudMic::Message::Createaudio(CloudMic::AudioData{audioChunk}));
              }
            }
          }
        }
      }

      // we want to extend the streaming state so that it at least appears to be streaming for a minimum duration
      // here we hold onto the streaming job until we've reached that minimum duration
      // note: the streaming job will not actually be recording, we're simply holding it so we don't start a new job

      // TODO:(bn) VIC-13438 this is tech debt and has caused bugs and confusion and should be cleaned up
      if (_streamingComplete)
      {
        // our stream is complete, so clear out the current stream as long as our minimum streaming time has elapsed
        uint32_t minStreamingDuration_ms = _context->GetShowAudioStreamStateManager()->GetMinStreamingDuration();
        const BaseStationTime_t minStreamDuration_ns = minStreamingDuration_ms * 1000 * 1000;
        const BaseStationTime_t minStreamEnd_ns = _streamBeginTime_ns + minStreamDuration_ns;
        if ( ( currTime_nanosec >= minStreamEnd_ns ) || realStreamHasFinished )
        {
          LOG_INFO("MicDataSystem.Update.StreamingComplete.ClearJob", "Clearing streaming job now that enough time has elapsed");
          ClearCurrentStreamingJob();
        }
      }
    }
  }

  // Send out any messages we have to the engine
  std::vector<std::unique_ptr<RobotInterface::RobotToEngine>> stolenMessages;
  {
    std::lock_guard<std::mutex> lock(_msgsMutex);
    stolenMessages = std::move(_msgsToEngine);
    _msgsToEngine.clear();
  }

  #if ANKI_DEV_CHEATS
    // Store off a copy of (one of) the micDirectionData from this update for debug drawing
    bool updatedMicDirection = false;
  #endif
  for (const auto& msg : stolenMessages)
  {
    if (msg->tag == RobotInterface::RobotToEngine::Tag_triggerWordDetected)
    {
      RobotInterface::SendAnimToEngine(msg->triggerWordDetected);

      ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
      SetWillStream(showStreamState->ShouldStreamAfterTriggerWordResponse());
    }
    else if (msg->tag == RobotInterface::RobotToEngine::Tag_micDirection)
    {
      _latestMicDirectionMsg = msg->micDirection;
      #if ANKI_DEV_CHEATS
        updatedMicDirection = true;
      #endif
      RobotInterface::SendAnimToEngine(msg->micDirection);
    }
    else if (msg->tag == RobotInterface::RobotToEngine::Tag_beatDetectorState)
    {
      RobotInterface::SendAnimToEngine(msg->beatDetectorState);
    }
    else
    {
      DEV_ASSERT_MSG(false,
                     "MicDataSystem.Update.UnhandledOutgoingMessageType",
                     "%s", RobotInterface::RobotToEngine::TagToString(msg->tag));
    }
  }

  const auto& rawBufferFullness = GetIncomingMicDataPercentUsed();
  RobotInterface::MicDataState micDataState{};
  micDataState.rawBufferFullness = rawBufferFullness;
  RobotInterface::SendAnimToEngine(micDataState);

  #if ANKI_DEV_CHEATS
    if (updatedMicDirection || recordingSecondsRemaining != 0)
    {
      FaceInfoScreenManager::getInstance()->DrawConfidenceClock(
        _latestMicDirectionMsg,
        rawBufferFullness,
        recordingSecondsRemaining,
        endTriggerDispTime_ns != 0 || _currentlyStreaming);
    }
  #endif
  
  if (_abortAlexaScreenDueToHeyVector) {
    _abortAlexaScreenDueToHeyVector = false;
    Alexa* alexa = _context->GetAlexa();
    if( alexa != nullptr ) {
      // sign out before we change the info screen so the reason is more descriptive
      alexa->CancelPendingAlexaAuth("VECTOR_WAKEWORD");
    }
    FaceInfoScreenManager::getInstance()->EnableAlexaScreen(ScreenName::None,"","");
  }

  // Try to retrieve the speaker latency from the AkAlsaSink plugin. We
  // only need to actually call into the plugin once (or until we get a
  // nonzero latency), since the latency does not change during runtime.
#ifndef SIMULATOR
  if (_speakerLatency_ms == 0) {
    if (_context != nullptr) {
      const auto* audioController = _context->GetAudioController();
      if (audioController != nullptr) {
        const auto* pluginInterface = audioController->GetPluginInterface();
        if (pluginInterface != nullptr) {
          _speakerLatency_ms = pluginInterface->AkAlsaSinkGetSpeakerLatency_ms();
          if (_speakerLatency_ms != 0) {
            LOG_INFO("MicDataSystem.Update.SpeakerLatency",
                     "AkAlsaSink plugin reporting a max speaker latency of %u",
                     (uint32_t) _speakerLatency_ms);
          }
        }
      }
    }
  }
#endif
}

void MicDataSystem::SetWillStream(bool willStream) const
{
  for(auto func : _triggerWordDetectedCallbacks)
  {
    if(func != nullptr)
    {
      func(willStream);
    }
  }
}

void MicDataSystem::ClearCurrentStreamingJob()
{
  {
    std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
    _currentlyStreaming = false;
    if (_currentStreamingJob != nullptr)
    {
      _currentStreamingJob->SetTimeToRecord(0);
      _currentStreamingJob = nullptr;

      for(auto func : _streamUpdatedCallbacks)
      {
        if(func != nullptr)
        {
          func(false);
        }
      }
    }
  }

  ResetMicListenDirection();
}

void MicDataSystem::ResetMicListenDirection()
{
  _micDataProcessor->ResetMicListenDirection();
}

float MicDataSystem::GetIncomingMicDataPercentUsed()
{
  return _micDataProcessor->GetIncomingMicDataPercentUsed();
}

void MicDataSystem::SendMessageToEngine(std::unique_ptr<RobotInterface::RobotToEngine> msgPtr)
{
  std::lock_guard<std::mutex> lock(_msgsMutex);
  _msgsToEngine.push_back(std::move(msgPtr));
}

bool MicDataSystem::HasStreamingJob() const
{
  std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
  return (_currentStreamingJob != nullptr
  #if ANKI_DEV_CHEATS
    || _fakeStreamingState || _forceRecordClip
  #endif
  );
}

void MicDataSystem::AddMicDataJob(std::shared_ptr<MicDataInfo> newJob, bool isStreamingJob)
{
  std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
  _micProcessingJobs.push_back(std::shared_ptr<MicDataInfo>(newJob));
  if (isStreamingJob)
  {
    _currentStreamingJob = _micProcessingJobs.back();

    for(auto func : _streamUpdatedCallbacks)
    {
      if(func != nullptr)
      {
        func(true);
      }
    }
  }
}

std::deque<std::shared_ptr<MicDataInfo>> MicDataSystem::GetMicDataJobs() const
{
  std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
  return _micProcessingJobs;
}

void MicDataSystem::UpdateMicJobs()
{
  std::lock_guard<std::recursive_mutex> lock(_dataRecordJobMutex);
  // Check if each of the jobs are done, removing the ones that are
  auto jobIter = _micProcessingJobs.begin();
  while (jobIter != _micProcessingJobs.end())
  {
    (*jobIter)->UpdateForNextChunk();
    bool jobDone = (*jobIter)->CheckDone();
    if (jobDone)
    {
      jobIter = _micProcessingJobs.erase(jobIter);
    }
    else
    {
      ++jobIter;
    }
  }
}

void MicDataSystem::AudioSaveCallback(const std::string& dest)
{
  if (_udpServer->HasClient())
  {
    SendUdpMessage(CloudMic::Message::CreatedebugFile(CloudMic::Filename{dest}));
  }

  // let the world know our recording is now complete
  {
    // we have a buffer length of 255 and our path needs to fit into this buffer
    // shouldn't be a problem, but if we ever hit this we'll need to find another solution
    DEV_ASSERT( dest.length() <= 255, "MicDataSystem::AudioSaveCallback path is too long for MicRecordingComplete message" );

    RobotInterface::MicRecordingComplete event;

    memcpy( event.path, dest.c_str(), dest.length() );
    event.path_length = dest.length();

    AnimProcessMessages::SendAnimToEngine( std::move( event ) );
  }
}

BeatInfo MicDataSystem::GetLatestBeatInfo()
{
  return _micDataProcessor->GetBeatDetector().GetLatestBeat();
}

void MicDataSystem::ResetBeatDetector()
{
  _micDataProcessor->GetBeatDetector().Start();
}
  
void MicDataSystem::SetAlexaState(AlexaSimpleState state)
{
  const auto oldState = _alexaState;
  _alexaState = state;
  const bool enabled = (_alexaState != AlexaSimpleState::Disabled);
  
  if ((oldState == AlexaSimpleState::Disabled) && enabled) {
    const auto callback = [this] (const AudioUtil::SpeechRecognizerCallbackInfo& info,
                                  const AudioUtil::SpeechRecognizerIgnoreReason& ignore)
    {
      LOG_INFO("MicDataSystem.SetAlexaState.TriggerWordDetectCallback", "info - %s", info.Description().c_str());
      
#if ANKI_DEV_CHEATS
      SendTriggerDetectionToWebViz(info, ignore);
      if (kSuppressTriggerResponse) {
        return;
      }
#endif
      
      if( ignore || HasStreamingJob() ) {
        // Don't run alexa wakeword if
        // 1. there's a "hey vector" streaming job
        // 2. if the mic is muted
        // 3. ignore flag is ture, either playback recognizer triggered positive or there is a "notch"
        return;
      }
      Alexa* alexa = _context->GetAlexa();
      ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
      if( (alexa != nullptr) && showStreamState->HasAnyAlexaResponse() ) {
        alexa->NotifyOfWakeWord( info.startSampleIndex, info.endSampleIndex );
      }
      SendRecognizerDasLog( info, EnumToString(_alexaState) );
    };
    _speechRecognizerSystem->ActivateAlexa(_locale, callback);
  }
  else if((oldState != AlexaSimpleState::Disabled) && !enabled) {
    // Disable "Alexa" wake word in SpeechRecognizerSystem
    _speechRecognizerSystem->DisableAlexa();
  }
  
  const bool active = (_alexaState == AlexaSimpleState::Active);
  // UK/AU seem to be worse at handling self-loops
  if (((_locale.GetCountry() == Util::Locale::CountryISO2::GB) && kAlexaEnabledInUK)
      || ((_locale.GetCountry() == Util::Locale::CountryISO2::AU) && kAlexaEnabledInAU)) {
    _speechRecognizerSystem->ToggleNotchDetector( active );
  }
  else {
    // results in some unnecessary method calls but this method does little
    _speechRecognizerSystem->ToggleNotchDetector( false );
  }
}
  
void MicDataSystem::ToggleMicMute()
{
  // TODO (VIC-11587): we could save some CPU if the wake words recognizers are actually disabled here. For now, we
  // don't feed the raw audio buffer when receiving messages from robot process. Which stops running the mic processor
  // and recognizers methods, therefore, saving CPU. However, mic threads are still runing.
  _micMuted = !_micMuted;
  _micDataProcessor->MuteMics(_micMuted);
  
  // play audio event for changing mic mute state
  auto* audioController = _context->GetAudioController();
  if (audioController != nullptr) {
    using namespace AudioEngine;
    using GenericEvent = AudioMetaData::GameEvent::GenericEvent;
    const auto eventID = ToAudioEventId( _micMuted
                                         ? GenericEvent::Play__Robot_Vic_Alexa__Sfx_Sml_State_Privacy_Mode_On
                                         : GenericEvent::Play__Robot_Vic_Alexa__Sfx_Sml_State_Privacy_Mode_Off );
    const auto gameObject = ToAudioGameObject(AudioMetaData::GameObjectType::Default);
    audioController->PostAudioEvent( eventID, gameObject );
  }

  // Note that Alexa also has a method to stopStreamingMicrophoneData, but without the wakeword,
  // the samples go no where. Also check if it saves CPU to drop samples. Note that the time indices
  // for the wake word bookends might be wrong afterwards.
  
  // toggle backpack lights
  if( _context != nullptr ) {
    auto* bplComp = _context->GetBackpackLightComponent();
    if( bplComp != nullptr ) {
      bplComp->SetMicMute( _micMuted );
    }
  }
  
  // add/remove persistent file
  const auto muteFile = _persistentFolder + kMicSettingsFile;
  if( _micMuted ) {
    Util::FileUtils::TouchFile( muteFile );
  } else if( Util::FileUtils::FileExists( muteFile ) ) {
    Util::FileUtils::DeleteFile( muteFile );
  }
}
  
void MicDataSystem::SetButtonWakeWordIsAlexa(bool isAlexa)
{
  _buttonPressIsAlexa = isAlexa;
}

bool MicDataSystem::IsButtonPressAlexa() const
{
  // Instead of only using _buttonPressIsAlexa, also check whether alexa has been opted in.
  // If the user sets the button to alexa, clears user data, reverts to factory and then
  // OTAs to latest, Alexa's init sequence will message engine that alexa is disabled, which
  // sets the button functionality back to hey vector. But if jdocs settings are pulled _after_
  // that, it can switch back to alexa. For now, we check here instead of having engine's 
  // SettingsManager check the AlexaComponent's auth state, since that is tied to the
  // order of messages received from anim and so would need to track more state.
  // As a result, the user's button setting will still be set to alexa, even if alexa is disabled.
  // However, currently the app doesnt show this setting if alexa is disabled, to it will be
  // functionally equivalent to the user.
  // TODO (VIC-12527): handle this in engine instead (or in addition to here, since this
  // extra check doesnt actually hurt if the app doesn't show the setting and no other anim
  // components are listening to SetButtonWakeWordIsAlexa)
  Alexa* alexa = _context->GetAlexa();
  return _buttonPressIsAlexa && (alexa != nullptr) && alexa->IsOptedIn();
}

void MicDataSystem::SendUdpMessage(const CloudMic::Message& msg)
{
  std::vector<uint8_t> buf(msg.Size());
  msg.Pack(buf.data(), buf.size());
  _udpServer->Send((const char*)buf.data(), buf.size());
}

void MicDataSystem::UpdateLocale(const Util::Locale& newLocale)
{
  _locale = newLocale;
  _speechRecognizerSystem->UpdateTriggerForLocale(newLocale);
}

void MicDataSystem::UpdateTimeZone(const std::string& newTimeZone)
{
  _timeZone = newTimeZone;
}

bool MicDataSystem::IsSpeakerPlayingAudio() const
{
  if (_context != nullptr) {
    const auto* audioController = _context->GetAudioController();
    if (audioController != nullptr) {
      const auto* pluginInterface = audioController->GetPluginInterface();
      if (pluginInterface != nullptr) {
        return pluginInterface->AkAlsaSinkIsUsingSpeaker();
      }
    }
  }
  return false;
}

bool MicDataSystem::HasConnectionToCloud() const
{
  return _udpServer->HasClient();
}

bool MicDataSystem::ShouldSimulateStreaming() const
{
  if( _batteryLow ) {
    return true;
  } else {
    ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
    const bool fakeIt = showStreamState->ShouldSimulateStreamAfterTriggerWord();
    return fakeIt;
  }
}

void MicDataSystem::RequestConnectionStatus()
{
  if (_udpServer->HasClient())
  {
    LOG_INFO("MicDataSystem.RequestConnectionStatus", "");
    SendUdpMessage( CloudMic::Message::CreateconnectionCheck({}) );
  }
}
  
void MicDataSystem::SendTriggerDetectionToWebViz(const AudioUtil::SpeechRecognizerCallbackInfo& info,
                                                 const AudioUtil::SpeechRecognizerIgnoreReason& ignoreReason)
{
#if ANKI_DEV_CHEATS
  if ( _context != nullptr ) {
    Json::Value data;
    data["result"] = info.result;
    data["startTime_ms"] = info.startTime_ms;
    data["endTime_ms"] = info.endTime_ms;
    data["startSampleIndex"] = info.startSampleIndex;
    data["endSampleIndex"] = info.endSampleIndex;
    data["score"] = info.score;
    data["notch"] = ignoreReason.notch;
    data["playback"] = ignoreReason.playback;
    
    if (_devEnableTriggerHistory) {
      // don't let result buffer grow infinitely
      if ( _devTriggerResults.size() >= 10 ) {
        _devTriggerResults.pop_front();
      }
      _devTriggerResults.emplace_back( std::move(data) );
    }
    
    auto* webService = _context->GetWebService();
    if( webService != nullptr && webService->IsWebVizClientSubscribed( kSpeechRecognizerWebvizName ) ) {
      const Json::Value& webVizData = _devEnableTriggerHistory ? _devTriggerResults.back() : data;
      webService->SendToWebViz( kSpeechRecognizerWebvizName, webVizData );
    }
  }
#endif
}
  
void MicDataSystem::SendRecentTriggerDetectionToWebViz(const std::function<void(const Json::Value&)>& sendFunc)
{
#if ANKI_DEV_CHEATS
  Json::Value value{Json::arrayValue};
  for (const auto& val : _devTriggerResults) {
    value.append(val);
  }
  sendFunc(value);
#endif
}

#if ANKI_DEV_CHEATS
void MicDataSystem::EnableTriggerHistory(bool enable)
{
  _devEnableTriggerHistory = enable;
  if ( !_devEnableTriggerHistory ) {
    _devTriggerResults.clear();
  }
}
#endif

void MicDataSystem::SendRecognizerDasLog(const AudioUtil::SpeechRecognizerCallbackInfo& info,
                                         const char* stateStr) const
{
  MicData::MicDirectionData directionData;
  MicData::DirectionIndex dominantDirection;
  _micDataProcessor->GetLatestMicDirectionData(directionData, dominantDirection);
  DASMSG( speech_recognized, "mic_data_system.speech_trigger_recognized", "Voice trigger recognized" );
  DASMSG_SET( s1, info.result.c_str(), "Recognized result" );
  DASMSG_SET( s2, (stateStr != nullptr) ? stateStr : "", "Current Alexa UX State");
  DASMSG_SET( s3, std::to_string(info.score).c_str(), "Recognizer Score");
  DASMSG_SET( i1, dominantDirection, "Dominant Direction Index [0, 11], 12 is Unknown Direction" );
  DASMSG_SET( i2, directionData.selectedDirection, "Selected Direction Index [0, 11], 12 is Unknown Direction" );
  DASMSG_SET( i3, static_cast<int>(directionData.latestPowerValue),
              "Latest power value, calculate dB by log(val) * 10" );
  DASMSG_SET( i4, static_cast<int>(directionData.latestNoiseFloor),
              "Latest floor noise value, calculate dB by log(val) * 10" );
  DASMSG_SEND();
}

} // namespace MicData
} // namespace Vector
} // namespace Anki
