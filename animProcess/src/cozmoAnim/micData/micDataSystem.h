/**
* File: micDataSystem.h
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

#ifndef __AnimProcess_CozmoAnim_MicDataSystem_H_
#define __AnimProcess_CozmoAnim_MicDataSystem_H_

#include "micDataTypes.h"
#include "coretech/common/shared/types.h"
#include "cozmoAnim/speechRecognizer/speechRecognizerSystem.h"
#include "util/console/consoleFunction.h"
#include "util/global/globalDefinitions.h"
#include "util/helpers/noncopyable.h"
#include "util/environment/locale.h"
#include "util/signals/signalHolder.h"

#include "clad/cloud/mic.h"
#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/types/beatDetectorTypes.h"

#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Declarations
namespace Anki {
  namespace AudioUtil {
    struct SpeechRecognizerCallbackInfo;
    struct SpeechRecognizerIgnoreReason;
  }
  namespace Vector {
    namespace CloudMic {
      class Message;
    }
    namespace MicData {
      class MicDataInfo;
      class MicDataProcessor;
    }
    namespace Anim {
      class RobotDataLoader;
    }
    namespace RobotInterface {
      struct MicData;
      struct RobotToEngine;
    }
    class SpeechRecognizerSystem;
    enum class AlexaSimpleState : uint8_t;
  }
  namespace Util {
    namespace Data {
      class DataPlatform;
    }
    class Locale;
  }
}
class LocalUdpServer;

namespace Anki {
namespace Vector {
namespace MicData {

class MicDataSystem : private Util::SignalHolder, private Anki::Util::noncopyable {
public:
  MicDataSystem(Util::Data::DataPlatform* dataPlatform,
                const Anim::AnimContext* context);
  ~MicDataSystem();
  MicDataSystem(const MicDataSystem& other) = delete;
  MicDataSystem& operator=(const MicDataSystem& other) = delete;

  void Init(const Anim::RobotDataLoader& dataLoader);

  MicData::MicDataProcessor* GetMicDataProcessor() const { return _micDataProcessor.get(); }
  SpeechRecognizerSystem* GetSpeechRecognizerSystem() const { return _speechRecognizerSystem.get(); }
  
  void ProcessMicDataPayload(const RobotInterface::MicData& payload);
  void RecordRawAudio(uint32_t duration_ms, const std::string& path, bool runFFT);
  void RecordProcessedAudio(uint32_t duration_ms, const std::string& path);
  void StartWakeWordlessStreaming(CloudMic::StreamType type, bool playGetInFromAnimProcess);
  void FakeTriggerWordDetection();
  void Update(BaseStationTime_t currTime_nanosec);

#if ANKI_DEV_CHEATS
  void SetForceRecordClip(bool newValue) { _forceRecordClip = newValue; }
  void SetLocaleDevOnly(const Util::Locale& locale) { _locale = locale; }
  void EnableTriggerHistory(bool enable);
#endif

  void ResetMicListenDirection();

  void SendMessageToEngine(std::unique_ptr<RobotInterface::RobotToEngine> msgPtr);

  void AddMicDataJob(std::shared_ptr<MicDataInfo> newJob, bool isStreamingJob = false);
  bool HasStreamingJob() const;
  std::deque<std::shared_ptr<MicDataInfo>> GetMicDataJobs() const;
  void UpdateMicJobs();
  void AudioSaveCallback(const std::string& dest);

  BeatInfo GetLatestBeatInfo();
  const Anki::Vector::RobotInterface::MicDirection& GetLatestMicDirectionMsg() const { return _latestMicDirectionMsg; }
  
  void ResetBeatDetector();
  
  void SetAlexaState(AlexaSimpleState state);
  
  void SetButtonWakeWordIsAlexa(bool isAlexa);
  
  void ToggleMicMute();
  bool IsMicMuted() const { return _micMuted; }
  
  void UpdateLocale(const Util::Locale& newLocale);
  
  void UpdateTimeZone(const std::string& newTimeZone);
  
  bool IsSpeakerPlayingAudio() const;
  
  // Get the maximum speaker 'latency', which is the max delay between when we
  // command audio to be played and it actually gets played on the speaker
  uint32_t GetSpeakerLatency_ms() const { return _speakerLatency_ms; }

  // Callback parameter is whether or not we will be streaming after
  // the trigger word is detected
  void AddTriggerWordDetectedCallback(std::function<void(bool)> callback)
    { _triggerWordDetectedCallbacks.push_back(callback); }

  // Callback parameter is whether or not the stream was started
  // True if started, False if stopped
  void AddStreamUpdatedCallback(std::function<void(bool)> callback)
    { _streamUpdatedCallbacks.push_back(callback); }

  bool HasConnectionToCloud() const;
  void RequestConnectionStatus();

  void SetBatteryLowStatus( bool isLow ) { _batteryLow = isLow; }
  void SetEnableDataCollectionSettings( bool isEnable ) { _enableDataCollection = isEnable; }

  // simulated streaming is when we make everything look like we're streaming normally, but we're not actually
  // sending any data to the cloud; this lasts for a set duration
  bool ShouldSimulateStreaming() const;

  // let's anybody who registered a callback with AddTriggerWordDetectedCallback(...) know that we've heard the
  // trigger word and are either about to start streaming, or not (either on purpose, or it was cancelled/error)
  void SetWillStream(bool willStream) const;


private:

  const Anim::AnimContext* _context;

  bool IsButtonPressAlexa() const;

  std::string _writeLocationDir = "";
  std::string _persistentFolder;
  // Members for the the mic jobs
  std::deque<std::shared_ptr<MicDataInfo>> _micProcessingJobs;
  std::shared_ptr<MicDataInfo> _currentStreamingJob;
  mutable std::recursive_mutex _dataRecordJobMutex;
  BaseStationTime_t _streamBeginTime_ns = 0;
  bool _currentlyStreaming = false;
  bool _streamingComplete = false;
#if ANKI_DEV_CHEATS
  bool _fakeStreamingState = false;
#endif
  size_t _streamingAudioIndex = 0;
  Util::Locale _locale = {"en", "US"};
  std::string _timeZone = "";

  std::unique_ptr<MicDataProcessor>       _micDataProcessor;
  std::unique_ptr<SpeechRecognizerSystem> _speechRecognizerSystem;
  std::unique_ptr<LocalUdpServer>         _udpServer;

#if ANKI_DEV_CHEATS
  bool _forceRecordClip = false;
#endif
  
  std::atomic<uint32_t> _speakerLatency_ms{0};
  
  RobotInterface::MicDirection _latestMicDirectionMsg;
  
  // Members for managing the results of async FFT processing
  struct FFTResultData {
    std::deque<std::vector<uint32_t>> _fftResultList;
    std::mutex _fftResultMutex;
  };
  std::shared_ptr<FFTResultData> _fftResultData;

  // Members for holding outgoing messages
  std::vector<std::unique_ptr<RobotInterface::RobotToEngine>> _msgsToEngine;
  std::mutex _msgsMutex;

  std::vector<std::function<void(bool)>> _triggerWordDetectedCallbacks;
  std::vector<std::function<void(bool)>> _streamUpdatedCallbacks;

  bool _batteryLow = false;
  bool _enableDataCollection = false;
  bool _buttonPressIsAlexa = false;
  AlexaSimpleState _alexaState;
  
  std::atomic<bool> _micMuted;

  // if hey vector is spoken, we'll need to abort the alexa pairing screen if it's active. The overly verbose
  // name is becuase we hardcode the "reason" that we are leaving the pairing screen based on the assumption
  // that this is triggered via a "hey vector" wakeword
  std::atomic<bool> _abortAlexaScreenDueToHeyVector;
  
#if ANKI_DEV_CHEATS
  std::list<Anki::Util::IConsoleFunction> _devConsoleFuncs;
  bool _devEnableTriggerHistory = true;
  std::list<Json::Value> _devTriggerResults;
#endif

  void SetupConsoleFuncs();
  void RecordAudioInternal(uint32_t duration_ms, const std::string& path, MicDataType type, bool runFFT);
  void ClearCurrentStreamingJob();
  float GetIncomingMicDataPercentUsed();
  void SendUdpMessage(const CloudMic::Message& msg);
  
  void SendTriggerDetectionToWebViz(const AudioUtil::SpeechRecognizerCallbackInfo& info,
                                    const AudioUtil::SpeechRecognizerIgnoreReason& ignoreReason);
  
  void SendRecentTriggerDetectionToWebViz(const std::function<void(const Json::Value&)>& sendFunc);

  void SendRecognizerDasLog(const AudioUtil::SpeechRecognizerCallbackInfo& info,
                            const char* stateStr) const;
};

} // namespace MicData
} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_MicData_MicDataSystem_H_
