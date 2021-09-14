/**
* File: micDataProcessor.cpp
*
* Author: Lee Crippen
* Created: 9/27/2017
*
* Description: Handles processing the mic samples from the robot process: combining the channels,
*              and extracting direction data.
*
* Copyright: Anki, Inc. 2017
*
*/


// Signal Essence Includes
#include "mmif.h"
#include "policy_actions.h"
#include "se_diag.h"

#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/beatDetector/beatDetector.h"
#include "cozmoAnim/faceDisplay/faceDisplay.h"
#include "cozmoAnim/micData/micDataProcessor.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/micData/micDataInfo.h"
#include "cozmoAnim/micData/micImmediateDirection.h"
#include "cozmoAnim/showAudioStreamStateManager.h"
#include "cozmoAnim/speechRecognizer/speechRecognizerSystem.h"
#include "audioUtil/speechRecognizer.h"
#include "util/console/consoleInterface.h"
#include "util/console/consoleFunction.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/ankiDefines.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/threading/threadPriority.h"
#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"
#include <list>
#include <sched.h>


namespace Anki {
namespace Vector {
namespace MicData {

namespace {
  
#define LOG_CHANNEL "Microphones"
#define CONSOLE_GROUP "MicData"

  CONSOLE_VAR(bool, kMicData_CollectRawTriggers, CONSOLE_GROUP, false);
  CONSOLE_VAR(bool, kMicData_SpeakerNoiseDisablesMics, CONSOLE_GROUP, true);

  // Time necessary for the VAD logic to wait when there's no activity, before we begin skipping processing for
  // performance. Note that this probably needs to at least be as long as the trigger, which is ~ 500-750ms.
  CONSOLE_VAR_RANGED(uint32_t, kMicData_QuietTimeCooldown_ms, CONSOLE_GROUP, 1000, 500, 10000);

#if ANKI_DEV_CHEATS

  CONSOLE_VAR(bool, kMicData_SaveRawFullIntent, CONSOLE_GROUP, false);
  CONSOLE_VAR(bool, kMicData_SaveRawFullIntent_WakeWordless, CONSOLE_GROUP, false);
  
  CONSOLE_VAR(bool, kMicData_ForceEnableMicDataProc, CONSOLE_GROUP, false);
  CONSOLE_VAR(bool, kMicData_ForceDisableMicDataProc, CONSOLE_GROUP, false);
  
  uint8_t _currentDevForcedProcesState = 0;
  CONSOLE_VAR_ENUM(uint8_t, kDevForceProcessState, CONSOLE_GROUP, _currentDevForcedProcesState,
                   "NormalOperation,None,NoProcessingSingleMic,SigEsBeamformingOff,SigEsBeamformingOn");
  
  std::list<Anki::Util::IConsoleFunction> sConsoleFuncs;

#endif // ANKI_DEV_CHEATS

  CONSOLE_VAR(bool, kBeatDetectorUseProcessedAudio, CONSOLE_GROUP, true);
  
  #define ENABLE_MIC_PROCESSING_STATE_UPDATE_LOG 0
  using MicProcessingState = MicDataProcessor::ProcessingState;
  const MicProcessingState kDefaultProcessingState = MicProcessingState::SigEsBeamformingOff;
  const MicProcessingState kLowPowerModeProcessingState = MicProcessingState::NoProcessingSingleMic;

} // anonymous namespace

CONSOLE_VAR_RANGED(float, maxProcessingTimePerDrop_ms,      "CpuProfiler", 5, 5, 32);

#if ANKI_CPU_PROFILER_ENABLED
CONSOLE_VAR_RANGED(float, maxTriggerProcTime_ms,            ANKI_CPU_CONSOLEVARGROUP, 10, 10, 32);
CONSOLE_VAR_ENUM(u8,      kMicDataProcessorRaw_Logging,     ANKI_CPU_CONSOLEVARGROUP, 0, Util::CpuProfiler::CpuProfilerLogging());
CONSOLE_VAR_ENUM(u8,      kMicDataProcessorTrigger_Logging, ANKI_CPU_CONSOLEVARGROUP, 0, Util::CpuProfiler::CpuProfilerLogging());
#endif

constexpr auto kCladMicDataTypeSize = sizeof(RobotInterface::MicData::data)/sizeof(RobotInterface::MicData::data[0]);
static_assert(kCladMicDataTypeSize == kIncomingAudioChunkSize,
              "Expecting size of MicData::data to match kIncomingAudioChunkSize");



void MicDataProcessor::SetupConsoleFuncs()
{
#if ANKI_DEV_CHEATS
  // TODO: Left method for furture console vars
#endif
}
# undef CONSOLE_GROUP


MicDataProcessor::MicDataProcessor(const Anim::AnimContext* context, MicDataSystem* micDataSystem,
                                   const std::string& writeLocation)
: _context(context)
, _micDataSystem(micDataSystem)
, _writeLocationDir(writeLocation)
, _micImmediateDirection(std::make_unique<MicImmediateDirection>())
, _beatDetector(std::make_unique<BeatDetector>())
{
  // Init the various SE processing
  MMIfInit(0, nullptr);
  InitVAD();

  // Cache off the indices of the SE processing variables we will be accessing
  _bestSearchBeamIndex = SEDiagGetIndex("fdsearch_best_beam_index");
  _bestSearchBeamConfidence = SEDiagGetIndex("fdsearch_best_beam_confidence");
  _selectedSearchBeamIndex = SEDiagGetIndex("search_result_best_beam_index");
  _selectedSearchBeamConfidence = SEDiagGetIndex("search_result_best_beam_confidence");
  _searchConfidenceState = SEDiagGetIndex("fdsearch_confidence_state");
  _policyFallbackFlag = SEDiagGetIndex("policy_fallback_flag");
  
  SetupConsoleFuncs();
}

void MicDataProcessor::Init()
{
  ASSERT_NAMED(_micDataSystem != nullptr, "MicDataProcessor.Init._micDataSystem.IsNull");
  ASSERT_NAMED(_micDataSystem->GetSpeechRecognizerSystem() != nullptr,
               "MicDataProcessor.Init._micDataSystem.GetSpeechRecognizerSystem.IsNull");
  // Link recognizer
  _speechRecognizerSystem = _micDataSystem->GetSpeechRecognizerSystem();
  
  // Set initial processing state
  SetActiveMicDataProcessingState(kDefaultProcessingState);
  
  // Start the thread doing the SE processing of audio
  _processThread = std::thread(&MicDataProcessor::ProcessRawLoop, this);
  
  // Start the thread doing the Sensory processing of audio
  _processTriggerThread = std::thread(&MicDataProcessor::ProcessTriggerLoop, this);
}

void MicDataProcessor::InitVAD()
{
  _sVadConfig.reset(new SVadConfig_t());
  _sVadObject.reset(new SVadObject_t());

  /* set up VAD */
  SVadSetDefaultConfig(_sVadConfig.get(), kSamplesPerBlockPerChannel, (float)AudioUtil::kSampleRate_hz);
  _sVadConfig->AbsThreshold = 250.0f; // was 400
  _sVadConfig->HangoverCountDownStart = 10;  // was 25, make 25 blocks (1/4 second) to see it actually end a couple times
  SVadInit(_sVadObject.get(), _sVadConfig.get());
}

void MicDataProcessor::VoiceTriggerWordDetection(const AudioUtil::SpeechRecognizerCallbackInfo& info)
{
  TriggerWordDetectCallback(TriggerWordDetectSource::Voice, info);
}
  
void MicDataProcessor::FakeTriggerWordDetection(bool fromMute)
{
  const AudioUtil::SpeechRecognizerCallbackInfo info {
    .result       = "",
    .startTime_ms = 0,
    .endTime_ms   = 0,
    .score        = 0.0f
  };
  const auto source = fromMute ? TriggerWordDetectSource::ButtonFromMute : TriggerWordDetectSource::Button;
  TriggerWordDetectCallback(source, info);
}

void MicDataProcessor::GetLatestMicDirectionData(MicDirectionData& out_lastSample,
                                                 DirectionIndex& out_dominantDirection) const
{
  out_lastSample = _micImmediateDirection->GetLatestSample();
  out_dominantDirection = _micImmediateDirection->GetDominantDirection();
}
  
void MicDataProcessor::TriggerWordDetectCallback(TriggerWordDetectSource source,
                                                 const AudioUtil::SpeechRecognizerCallbackInfo& info)
{
  ShowAudioStreamStateManager* showStreamState = _context->GetShowAudioStreamStateManager();
  // Ignore extra triggers during streaming
  if (_micDataSystem->HasStreamingJob() || !showStreamState->HasValidTriggerResponse())
  {
    return;
  }
  
  // By the time the earcon completes, engine may have changed its response, so assume the decision to stream
  // should be based on the engine-requested state at the time of the trigger word callback. Ugh.
  // Anyway, we should cache shouldStream and use it in earConCallback
  bool shouldStream = showStreamState->ShouldStreamAfterTriggerWordResponse();
  
  // Start command stream after EarCon completes
  auto earConCallback = [this,shouldStream](bool success) {
    // If we didn't succeed, it means that we didn't have a wake word response setup
    if (success) {
      RobotTimeStamp_t mostRecentTimestamp = CreateTriggerWordDetectedJobs(shouldStream);
      LOG_INFO("MicDataProcessor.TWCallback", "Timestamp %d", (TimeStamp_t)mostRecentTimestamp);
    }
    else {
      // since we're not opening up a stream, we need to reset the streaming light since it get's turned on
      // when we hear the trigger word
      _micDataSystem->SetWillStream(false);
      LOG_WARNING("MicDataProcessor.TWCallback", "Don't have a wake word response setup");
    }
  };
  
  const bool muteButton = (source == TriggerWordDetectSource::ButtonFromMute);
  const bool buttonPress = (source == TriggerWordDetectSource::Button) || muteButton;
  if( muteButton ) {
    // don't play the get-in if this trigger word started from mute, because the mute animation should be playing
    showStreamState->SetPendingTriggerResponseWithoutGetIn(earConCallback);
  } else {
    showStreamState->SetPendingTriggerResponseWithGetIn(earConCallback);
  }

  const auto currentDirection = _micImmediateDirection->GetDominantDirection();
  const bool willStreamAudio = showStreamState->ShouldStreamAfterTriggerWordResponse() &&
                               !_micDataSystem->ShouldSimulateStreaming();

  // Set up a message to send out about the triggerword
  RobotInterface::TriggerWordDetected twDetectedMessage;
  twDetectedMessage.direction = currentDirection;
  twDetectedMessage.isButtonPress = buttonPress;
  twDetectedMessage.fromMute = muteButton;
  twDetectedMessage.triggerScore = (uint32_t) info.score;
  twDetectedMessage.willOpenStream = willStreamAudio;
  auto engineMessage = std::make_unique<RobotInterface::RobotToEngine>(std::move(twDetectedMessage));
  _micDataSystem->SendMessageToEngine(std::move(engineMessage));

  // Tell signal essence software to lock in on the current direction if it's known
  // NOTE: This is disabled for now as we've gotten better accuracy with the direction of the intent
  // that happens after this point, so it is currently not desireable to lock in the supposed trigger direction,
  // as that direction can be incorrect due to motor noise, speaker noise, etc.
  // The code is left here with this comment to make resurrecting this functionality in the SE software easier in the
  // future, if desired.
  // if (currentDirection != kDirectionUnknown)
  // {
  //   std::lock_guard<std::mutex> lock(_seInteractMutex);
  //   PolicySetKeyPhraseDirection(currentDirection);
  // }

  LOG_INFO("MicDataProcessor.TWCallback", "Direction index %d", currentDirection);
}
  
RobotTimeStamp_t MicDataProcessor::CreateStreamJob(CloudMic::StreamType streamType,
                                                  uint32_t overlapLength_ms)
{
  // Setup Job
  auto newJob = std::make_shared<MicDataInfo>();
  newJob->_writeLocationDir = Util::FileUtils::FullFilePath({_writeLocationDir, "triggeredCapture"});
  newJob->_writeNameBase = ""; //use autogen names
  newJob->_numMaxFiles = 100;
  newJob->_type = streamType;
  bool saveToFile = false;
#if ANKI_DEV_CHEATS
  saveToFile = true;
  // Simplify stream type
  bool saveRawFullStream = false;
  switch (streamType) {
    case CloudMic::StreamType::Normal:
      saveRawFullStream = kMicData_SaveRawFullIntent;
      break;
    case CloudMic::StreamType::Blackjack:
    case CloudMic::StreamType::KnowledgeGraph:
      saveRawFullStream = kMicData_SaveRawFullIntent_WakeWordless;
      break;
  }

  if (saveRawFullStream) {
    newJob->EnableDataCollect(MicDataType::Raw, true);
  }
  newJob->_audioSaveCallback = std::bind(&MicDataSystem::AudioSaveCallback, _micDataSystem, std::placeholders::_1);
#endif
  newJob->EnableDataCollect(MicDataType::Processed, saveToFile);
  newJob->SetTimeToRecord(MicDataInfo::kMaxRecordTime_ms);
  newJob->SetAudioFadeInTime(MicDataInfo::kDefaultAudioFadeIn_ms);
  
  // Copy the current audio chunks in the trigger overlap buffer
  // The immediate buffer is bigger than just the overlap time (time right after trigger end but before trigger was
  // recognized), so that the immediate buffer also contains the trigger itself. So here we set our start index to
  // only capture that in-between time, and push it into the streaming job for intent matching
  std::lock_guard<std::mutex> lock(_procAudioXferMutex);
  DEV_ASSERT(_procAudioRawComplete >= _procAudioXferCount,
             "MicDataProcessor.CreateStreamJob.AudioProcIdx");
  
  if (overlapLength_ms > 0) {
    const auto overlapCount = overlapLength_ms / kTimePerChunk_ms;
    const auto maxIndex = _procAudioRawComplete - _procAudioXferCount;
    size_t triggerOverlapStartIdx = (maxIndex > overlapCount) ? (maxIndex - overlapCount) : 0;
    
    for (size_t i=triggerOverlapStartIdx; i<maxIndex; ++i)
    {
      const auto& audioBlock = _immediateAudioBuffer[i].audioBlock;
      newJob->CollectProcessedAudio(audioBlock.data(), audioBlock.size());
    }

    // Copy the current audio chunks in the trigger overlap buffer
    for (size_t i=0; i<_immediateAudioBuffer.size(); ++i)
    {
      const auto& audioBlock = _immediateAudioBuffer[i].audioBlock;
      newJob->CollectRawAudio(audioBlock.data(), audioBlock.size());
    }
  }

  const bool isStreamingJob = true;
  _micDataSystem->AddMicDataJob(newJob, isStreamingJob);
  
  RobotTimeStamp_t mostRecentTimestamp = _immediateAudioBuffer[_procAudioRawComplete-1].timestamp;
  return mostRecentTimestamp;
}

RobotTimeStamp_t MicDataProcessor::CreateTriggerWordDetectedJobs(bool shouldStream)
{
  RobotTimeStamp_t mostRecentTimestamp = 0;
  if (shouldStream)
  {
    // First we create the job responsible for streaming the intent after the trigger
    mostRecentTimestamp = CreateStreamJob(CloudMic::StreamType::Normal, kTriggerOverlapSize_ms);
  } else {
    LOG_INFO("MicDataProcessor.CreateTriggerWordDetectedJobs.NoStreaming", "Not adding streaming jobs because disabled");
  }

  // Now we set up the optional job for recording _just_ the trigger that was just recognized
  bool saveTriggerOnly = false;
# if ANKI_DEV_CHEATS
  saveTriggerOnly = true;
# endif // ANKI_DEV_CHEATS
  
  if (saveTriggerOnly)
  {
    std::lock_guard<std::mutex> lock(_procAudioXferMutex);
    
    auto triggerJob = std::make_shared<MicDataInfo>();
    triggerJob->_writeLocationDir = Util::FileUtils::FullFilePath({_writeLocationDir, "triggersOnly"});
    triggerJob->_writeNameBase = ""; // Use the autogen names in this subfolder
    triggerJob->_numMaxFiles = 100;
    triggerJob->EnableDataCollect(MicDataType::Processed, saveTriggerOnly);
    if (kMicData_CollectRawTriggers)
    {
      triggerJob->EnableDataCollect(MicDataType::Raw, saveTriggerOnly);
    }
    triggerJob->_audioSaveCallback = std::bind(&MicDataSystem::AudioSaveCallback, _micDataSystem, std::placeholders::_1);
    
    // We only record a little extra time beyond what we're stuffing in below
    constexpr uint32_t timeAfterTriggerEnd_ms = 170;
    triggerJob->SetTimeToRecord(timeAfterTriggerEnd_ms);
    const auto maxIndex = _procAudioRawComplete - _procAudioXferCount;
    for (size_t i=0; i<maxIndex; ++i)
    {
      const auto& audioBlock = _immediateAudioBuffer[i].audioBlock;
      triggerJob->CollectProcessedAudio(audioBlock.data(), audioBlock.size());
    }
    for (size_t i=0; i<_immediateAudioBuffer.size(); ++i)
    {
      const auto& audioBlock = _immediateAudioBuffer[i].audioBlock;
      triggerJob->CollectRawAudio(audioBlock.data(), audioBlock.size());
    }
    const auto notStreamingJob = false;
    _micDataSystem->AddMicDataJob(triggerJob, notStreamingJob);
  }

  return mostRecentTimestamp;
}

MicDataProcessor::~MicDataProcessor()
{
  _processThreadStop = true;
  _xferAvailableCondition.notify_all();
  _dataReadyCondition.notify_all();
  _processThread.join();
  _processTriggerThread.join();

  MMIfDestroy();
}

void MicDataProcessor::ProcessRawAudio(RobotTimeStamp_t timestamp,
                                       const AudioUtil::AudioSample* audioChunk,
                                       uint32_t robotStatus,
                                       float robotAngle)
{
  ANKI_CPU_PROFILE("MicDataProcessor::ProcessRawAudio");
  TimedMicData* nextSampleSpot = nullptr;
  {
    // Note we don't bother to free any slots here that have been consumed (by comparing size to _procAudioXferCount)
    // because it's unnecessary with the circular buffer.

    std::unique_lock<std::mutex> lock(_procAudioXferMutex);
    auto xferAvailableCheck = [this] () {
      return _processThreadStop || _procAudioXferCount < _immediateAudioBuffer.capacity();
    };
    _xferAvailableCondition.wait(lock, xferAvailableCheck);

    if (_processThreadStop) {
      return;
    }

    // Now we can be sure we have a free slot, so go ahead and grab it
    if (_immediateAudioBuffer.size() < _immediateAudioBuffer.capacity()) {
        _procAudioRawComplete = _immediateAudioBuffer.size();
    } else {
        _procAudioRawComplete = _immediateAudioBuffer.size() - 1;
    }
    nextSampleSpot = &_immediateAudioBuffer.push_back();
  }

  TimedMicData& nextSample = *nextSampleSpot;
  nextSample.timestamp = timestamp;
  MicDirectionData directionResult = ProcessMicrophonesSE(
    audioChunk,
    nextSample.audioBlock.data(),
    robotStatus,
    robotAngle);

  // Feed the samples to the beat detector. Optionally either use a raw single channel (the first quarter of the
  // un-interleaved audio block) or the processed audio block
  auto* audioSource = kBeatDetectorUseProcessedAudio ? nextSample.audioBlock.data() : audioChunk;
  
  UpdateBeatDetector(audioSource, kSamplesPerBlockPerChannel);
  
  // Now we're done filling out this slot, update the count so it can be consumed
  {
    std::lock_guard<std::mutex> lock(_procAudioXferMutex);
    ++_procAudioXferCount;
    _procAudioRawComplete = _immediateAudioBuffer.size();
  }
  _dataReadyCondition.notify_all();

  // Store off this most recent result in our immedate direction tracking
  _micImmediateDirection->AddDirectionSample(directionResult);

  // Set up a message to send out about the direction
  RobotInterface::MicDirection newMessage;
  newMessage.timestamp = (TimeStamp_t)timestamp;
  newMessage.direction = directionResult.winningDirection;
  newMessage.confidence = directionResult.winningConfidence;
  newMessage.selectedDirection = directionResult.selectedDirection;
  newMessage.selectedConfidence = directionResult.selectedConfidence;
  newMessage.activeState = directionResult.activeState;
  newMessage.latestPowerValue = directionResult.latestPowerValue;
  newMessage.latestNoiseFloor = directionResult.latestNoiseFloor;
  std::copy(
    directionResult.confidenceList.begin(),
    directionResult.confidenceList.end(),
    newMessage.confidenceList);
  
  auto engineMessage = std::make_unique<RobotInterface::RobotToEngine>(std::move(newMessage));
  _micDataSystem->SendMessageToEngine(std::move(engineMessage));
}

MicDirectionData MicDataProcessor::ProcessMicrophonesSE(const AudioUtil::AudioSample* audioChunk,
                                                        AudioUtil::AudioSample* bufferOut,
                                                        uint32_t robotStatus,
                                                        float robotAngle)
{
  std::lock_guard<std::mutex> lock(_seInteractMutex);
  PolicySetAbsoluteOrientation(robotAngle);
  // Note that currently we are only monitoring the moving flag. We _could_ also discard mic data when the robot
  // is picked up, but that is being evaluated with design before implementation, see VIC-1219
  const bool robotIsMoving = static_cast<bool>(robotStatus & (uint32_t)RobotStatusFlag::IS_MOVING);
  const bool robotStoppedMoving = !robotIsMoving && _robotWasMoving;
  _isInLowPowerMode = static_cast<bool>(robotStatus & (uint32_t)RobotStatusFlag::CALM_POWER_MODE);
  _robotWasMoving = robotIsMoving;

  // Check if we are playing audio through the speaker. Add a small delay after the speaker
  // stops 'playing', since it's possible that the speaker is still actually playing stuff
  // for a small time after this starts to return false.
  const auto speakerCooldown_ms = _micDataSystem->GetSpeakerLatency_ms();
  const auto speakerCooldownLimit = speakerCooldown_ms / kTimePerChunk_ms;
  if (_micDataSystem->IsSpeakerPlayingAudio()) {
    _isSpeakerActive = true;
    _speakerCooldownCnt = speakerCooldownLimit;
  } else if (_speakerCooldownCnt-- == 0) {
    _isSpeakerActive = false;
  }
  
  const bool speakerStoppedPlaying = !_isSpeakerActive && _wasSpeakerActive;
  _wasSpeakerActive = _isSpeakerActive;
  
  // The robot is either moving or playing audio
  const bool hasRobotNoise = (robotIsMoving || (_isSpeakerActive && kMicData_SpeakerNoiseDisablesMics));

  if (robotStoppedMoving || speakerStoppedPlaying)
  {
    // When the robot has stopped moving (and the gears are no longer making noise) or the speaker has just
    // stopped playing audio, we reset the mic direction confidence values to be based on non-noisy data
    MMIfResetLocationSearch();
  }

  // We only care about checking one channel, and since the channel data is uninterleaved when passed in here,
  // we simply give the start of the buffer as the input to run the vad detection on
  float latestPowerValue = 0.f;
  float latestNoiseFloor = 0.f;
  int activityFlag = 0;
  {
    ANKI_CPU_PROFILE("ProcessVAD");

    // Note while we _can_ pass a confidence value here adjusted while the robot is moving, we'd rather err on the side
    // of always thinking we hear a voice when the robot moves or plays audio from the speaker, so we maximize our
    // chances of hearing any triggers over the noise. So when the robot is moving, ignore the VAD, and instead just set
    // activity to true.
    const float vadConfidence = 1.0f;
    activityFlag = DoSVad(_sVadObject.get(),           // object
                          vadConfidence,               // confidence it is okay to measure noise floor, i.e. no known activity like gear noise
                          (int16_t*)audioChunk);       // pointer to input data
    latestPowerValue = _sVadObject->AvePowerInBlock;
    latestNoiseFloor = _sVadObject->NoiseFloor;
  
    if (hasRobotNoise)
    {
      activityFlag = 1;
    }

    // Keep a counter from the last active vad flag. When it hits 0 don't bother doing
    // the trigger recognition, then reset the counter when the flag is active again
    const auto vadCountdown_ms = kMicData_QuietTimeCooldown_ms;
    const auto vadCountdownLimit = vadCountdown_ms / kTimePerChunk_ms;
    if (activityFlag != 0)
    {
      _vadCountdown = vadCountdownLimit;
    }
    else if (_vadCountdown > 0)
    {
      --_vadCountdown;
    }

    if (_vadCountdown != 0)
    {
      activityFlag = 1;
    }
  }

  // Determine mic processing state
  ProcessingState processingState = _activeProcState;

  if (!_isInLowPowerMode) {
    // Update preferred processing state for robot noise state
    processingState = hasRobotNoise ? ProcessingState::SigEsBeamformingOff : ProcessingState::SigEsBeamformingOn;
  }
  else {
    processingState = kLowPowerModeProcessingState;
  }
  
#if ANKI_DEV_CHEATS
  
  // Allow overriding (for testing) to force enable or disable mic data processing & force processing state
  if (kMicData_ForceEnableMicDataProc)
  {
    activityFlag = 1;
  }
  else if (kMicData_ForceDisableMicDataProc)
  {
    activityFlag = 0;
  }
  
  // Update to dev process state
  if ((kDevForceProcessState > 0) || (kDevForceProcessState != _currentDevForcedProcesState)) {
    switch (kDevForceProcessState) {
      case 0:
        // Go back to normal operation mode, do nothing
        break;
      case 1:
        processingState = ProcessingState::None;
        break;
      case 2:
        processingState = ProcessingState::NoProcessingSingleMic;
        break;
      case 3:
        processingState = ProcessingState::SigEsBeamformingOff;
        break;
      case 4:
        processingState = ProcessingState::SigEsBeamformingOn;
        break;
      default:
        // Do Nothing
        break;
    }
    _currentDevForcedProcesState = kDevForceProcessState;
  }
  
#endif
  
  // Update State
  SetActiveMicDataProcessingState(processingState);
  bool directionIsAvailable = false;
  
  switch (_activeProcState) {
    case ProcessingState::None:
    {
      // Use raw mic data from single source
      ANKI_CPU_PROFILE("ProcessRawSingleMicrophoneCopy");
      memcpy(bufferOut, audioChunk, sizeof(AudioUtil::AudioSample) * kSamplesPerBlockPerChannel);
      break;
    }
    case ProcessingState::NoProcessingSingleMic:
    {
      // Remove the DC bias and apply some gain to the first mic channel and pass it along
      ANKI_CPU_PROFILE("ProcessSingleMicrophone");
      
      // Our bias is stored as a fixed-point value
      constexpr int iirCoefPower = 10;
      constexpr int iirMult = 1023; // (2 ^ iirCoefPower) - 1
      static int bias = audioChunk[0] << iirCoefPower;
      for (int i=0; i<kSamplesPerBlockPerChannel; ++i)
      {
        // First update our bias with the latest audio sample
        bias = ((bias * iirMult) >> iirCoefPower) + audioChunk[i];
        // Push out the next sample using the updated bias
        bufferOut[i] = audioChunk[i] - (bias >> iirCoefPower);
        // Gain multiplier of 8 gives good results
        bufferOut[i] <<= 3;
      }
      break;
    }
    case ProcessingState::SigEsBeamformingOff:
    case ProcessingState::SigEsBeamformingOn:
    {
      // Signal Essense Processing
      static const std::array<
          AudioUtil::AudioSample, 
          kSamplesPerBlockPerChannel * kNumInputChannels> dummySpeakerOut{};
      {
        ANKI_CPU_PROFILE("ProcessMicrophonesSE");
        // Process the current audio block with SE software
        MMIfProcessMicrophones(dummySpeakerOut.data(), audioChunk, bufferOut);
      }
      directionIsAvailable = true;
      break;
    }
  }

  MicDirectionData result{};
  result.activeState = activityFlag;
  result.latestPowerValue = latestPowerValue;
  result.latestNoiseFloor = latestNoiseFloor;

  // When we know the robot is making noise (via moving or speaker is playing) or not using a Signal Essense processing
  // we clear direction data
  if (hasRobotNoise || !directionIsAvailable)
  {
    result.winningDirection = result.selectedDirection = kDirectionUnknown;
  }
  else
  {
    const auto latestDirection = SEDiagGetUInt16(_bestSearchBeamIndex);
    const auto latestConf = SEDiagGetInt16(_bestSearchBeamConfidence);
    const auto selectedDirection = SEDiagGetUInt16(_selectedSearchBeamIndex);
    const auto selectedConf = SEDiagGetInt16(_selectedSearchBeamConfidence);
    const auto* searchConfState = SEDiagGet(_searchConfidenceState);
    result.winningDirection = latestDirection;
    result.winningConfidence = latestConf;
    result.selectedDirection = selectedDirection;
    result.selectedConfidence = selectedConf;
    const auto* confListSrc = reinterpret_cast<const float*>(searchConfState->u.vp);
    // NOTE currently SE only calculates the 12 main directions (not "unknown" or directly above the mics)
    // so we only copy the 12 main directions
    std::copy(confListSrc, confListSrc + kLastValidIndex + 1, result.confidenceList.begin());
  }
  return result;
}

void MicDataProcessor::ProcessRawLoop()
{
  Anki::Util::SetThreadName(pthread_self(), "MicProcRaw");
  
#if defined(ANKI_PLATFORM_VICOS)
  // Setup the thread's affinity mask
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(2, &cpu_set);
  int error = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
  if (error != 0) {
    LOG_ERROR("MicDataProcessor.ProcessRawLoop", "SetAffinityMaskError %d", error);
  }
#endif
  
  static constexpr uint32_t expectedAudioDropsPerAnimLoop = 7;
  static const uint32_t maxProcTime_ms = expectedAudioDropsPerAnimLoop * maxProcessingTimePerDrop_ms;
  const auto maxProcTime = std::chrono::milliseconds(maxProcTime_ms);
  while (!_processThreadStop)
  {
    ANKI_CPU_TICK("MicDataProcessorRaw", maxProcTime_ms, Util::CpuProfiler::CpuProfilerLoggingTime(kMicDataProcessorRaw_Logging));
    const auto start = std::chrono::steady_clock::now();
  
    // Switch which buffer we're processing if it's empty
    {
      std::lock_guard<std::mutex> lock(_rawMicDataMutex);
      if (_rawAudioBuffers[_rawAudioProcessingIndex].empty())
      {
        _rawAudioProcessingIndex = (_rawAudioProcessingIndex == 1) ? 0 : 1;
      }
    }

    auto& rawAudioToProcess = _rawAudioBuffers[_rawAudioProcessingIndex];
    while (rawAudioToProcess.size() > 0)
    {
      ANKI_CPU_PROFILE("ProcessLoop");

      const auto& nextData = rawAudioToProcess.front();
      const auto* audioChunk = nextData.data;
      
      // Copy the current set of jobs we have for recording audio, so the list can be added to while processing
      // continues
      std::deque<std::shared_ptr<MicDataInfo>> jobs = _micDataSystem->GetMicDataJobs();
      // Collect the raw audio if desired
      for (auto& job : jobs)
      {
        job->CollectRawAudio(audioChunk, kIncomingAudioChunkSize);
      }

      _speechRecognizerSystem->UpdateNotch(audioChunk, kIncomingAudioChunkSize);
      
      // Factory test doesn't need to do any mic processing, it just uses raw data
      if(!FACTORY_TEST)
      {
        // Process the audio into a single channel, and collect it if desired
        (void) ProcessRawAudio(
          nextData.timestamp,
          audioChunk,
          nextData.robotStatusFlags,
          nextData.robotRotationAngle);
      }
      
      _micDataSystem->UpdateMicJobs();
      
      rawAudioToProcess.pop_front();
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsedTime = (end - start);
    if (elapsedTime < maxProcTime)
    {
      std::this_thread::sleep_for(maxProcTime - elapsedTime);
    }
  }
}

void MicDataProcessor::ProcessTriggerLoop()
{
  Anki::Util::SetThreadName(pthread_self(), "MicProcTrigger");
  
#if defined(ANKI_PLATFORM_VICOS)
  // Setup the thread's affinity mask
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(1, &cpu_set);
  int error = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
  if (error != 0) {
    LOG_ERROR("MicDataProcessor.ProcessTriggerLoop", "SetAffinityMaskError %d", error);
  }
#endif
  
  while (!_processThreadStop)
  {
    ANKI_CPU_TICK("MicDataProcessorTrigger", maxTriggerProcTime_ms, Util::CpuProfiler::CpuProfilerLoggingTime(kMicDataProcessorTrigger_Logging));
    ANKI_CPU_PROFILE("ProcessTriggerLoop");
    TimedMicData* readyDataSpot = nullptr;
    {
      ANKI_CPU_PROFILE("WaitForData");
      std::unique_lock<std::mutex> lock(_procAudioXferMutex);
      auto dataReadyCheck = [this] () { return _processThreadStop || _procAudioXferCount > 0; };
      _dataReadyCondition.wait(lock, dataReadyCheck);

      // Special case if we're being signalled to shut down
      if (_processThreadStop)
      {
        return;
      }

      // Grab a handle to the next available data that's been processed out of raw but not yet
      // "transferred" to the trigger word recognition processing
      readyDataSpot = &_immediateAudioBuffer[_procAudioRawComplete - _procAudioXferCount];
    }

    const auto& processedAudio = readyDataSpot->audioBlock;
    std::deque<std::shared_ptr<MicDataInfo>> jobs = _micDataSystem->GetMicDataJobs();
    for (auto& job : jobs)
    {
      job->CollectProcessedAudio(processedAudio.data(), processedAudio.size());
    }
    
    // Run the trigger detection, which will use the callback defined above
    {
      ANKI_CPU_PROFILE("RecognizeTriggerWord");
      // Note we skip it if there is no activity as of the latest processed audioblock
      _speechRecognizerSystem->Update(processedAudio.data(),
                                      (unsigned int)processedAudio.size(),
                                      (_micImmediateDirection->GetLatestSample().activeState != 0));
    }

    // Now we're done using this audio with the recognizer, so let it go
    {
      std::lock_guard<std::mutex> lock(_procAudioXferMutex);
      --_procAudioXferCount;
    }
    _xferAvailableCondition.notify_all();
  }
}
  
void MicDataProcessor::UpdateBeatDetector(const AudioUtil::AudioSample* const samples, const uint32_t nSamples)
{
  ANKI_CPU_PROFILE("BeatDetectorUpdate");

  // Only run the beat detector if we are not in low power mode
  if (_isInLowPowerMode) {
    if (_beatDetector->IsRunning()) {
      _beatDetector->Stop();
    }
  } else {
    if (!_beatDetector->IsRunning()) {
      _beatDetector->Start();
    }
    
    const bool beatDetected = _beatDetector->AddSamples(samples, nSamples);
    if (beatDetected) {
      auto beatMessage = RobotInterface::BeatDetectorState{_beatDetector->GetLatestBeat()};
      auto engineMessage = std::make_unique<RobotInterface::RobotToEngine>(std::move(beatMessage));
      _micDataSystem->SendMessageToEngine(std::move(engineMessage));
    }
  }
}
  
void MicDataProcessor::ProcessMicDataPayload(const RobotInterface::MicData& payload)
{
  // Store off this next job
  std::lock_guard<std::mutex> lock(_rawMicDataMutex);
  if (!_muteMics) {
    // Use whichever buffer is currently _not_ being processed
    auto& bufferToUse = (_rawAudioProcessingIndex == 1) ? _rawAudioBuffers[0] : _rawAudioBuffers[1];
    RobotInterface::MicData& nextJob = bufferToUse.push_back();
    nextJob = payload;
  }
}
  
void MicDataProcessor::MuteMics(bool mute)
{
  std::lock_guard<std::mutex> lock(_rawMicDataMutex);
  _muteMics = mute;
}

void MicDataProcessor::ResetMicListenDirection()
{
  std::lock_guard<std::mutex> lock(_seInteractMutex);
  PolicyDoAutoSearch();
}

float MicDataProcessor::GetIncomingMicDataPercentUsed()
{
  std::lock_guard<std::mutex> lock(_rawMicDataMutex);
  // Use whichever buffer is currently _not_ being processed
  const auto inUseIndex = (_rawAudioProcessingIndex == 1) ? 0 : 1;
  const auto& bufferInUse = _rawAudioBuffers[inUseIndex];
  const auto updatedFullness = ((float)bufferInUse.size()) / ((float)bufferInUse.capacity());
  // Cache the current fullness for this buffer and use the greater of the two buffer fullnesses
  // This way the "fullness" returned is less variable and better covers the worst case
  _rawAudioBufferFullness[inUseIndex] = updatedFullness;
  return MAX(_rawAudioBufferFullness[0], _rawAudioBufferFullness[1]);
}

void MicDataProcessor::SetActiveMicDataProcessingState(MicDataProcessor::ProcessingState state)
{
  // Set the correct flag for Signal Essence lib version
#if SE_V009
  // v009
  static const FallbackFlag_t kEcho_Cancel_Flag = FBF_FORCE_ECHO_CANCEL_WITH_NR;
#else
  // v008
  static const FallbackFlag_t kEcho_Cancel_Flag = FBF_FORCE_ECHO_CANCEL;
#endif
  
  if (state != _activeProcState) {
    if (ENABLE_MIC_PROCESSING_STATE_UPDATE_LOG) {
      LOG_INFO("MicDataProcessor.SetActiveMicDataProcessingState", "Current state '%s' new state '%s'",
                       GetProcessingStateName(_activeProcState), GetProcessingStateName(state));
    }
    
    switch (state) {
      case ProcessingState::None:
      case ProcessingState::NoProcessingSingleMic:
        // Do Nothing
        break;
      case ProcessingState::SigEsBeamformingOff:
      case ProcessingState::SigEsBeamformingOn:
      {
        const bool shouldUseFallbackPolicy = (state == ProcessingState::SigEsBeamformingOff);
        const FallbackFlag_t policySetting = shouldUseFallbackPolicy ? kEcho_Cancel_Flag : FBF_AUTO_SELECT;
        SEDiagSetEnumAsInt(_policyFallbackFlag, policySetting);
        break;
      }
    }
    _activeProcState = state;
  }
}
  
const char* MicDataProcessor::GetProcessingStateName(MicDataProcessor::ProcessingState state) const
{
  switch (state) {
    case ProcessingState::None:
      return "None";
    case ProcessingState::NoProcessingSingleMic:
      return "NoProcessingSingleMic";
    case ProcessingState::SigEsBeamformingOff:
      return "SigEsBeamformingOff";
    case ProcessingState::SigEsBeamformingOn:
      return "SigEsBeamformingOn";
  }
  return "";
}


} // namespace MicData
} // namespace Vector
} // namespace Anki
