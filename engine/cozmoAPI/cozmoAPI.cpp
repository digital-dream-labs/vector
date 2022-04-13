/**
 * File: cozmoAPI.cpp
 *
 * Author: Lee Crippen
 * Created: 08/19/15
 *
 * Description: Point of entry for anything needing to interact with Vector.
 *
 * Copyright: Anki, Inc. 2015
 *
 * COZMO_PUBLIC_HEADER
 **/

#include "engine/cozmoAPI/cozmoAPI.h"
#include "engine/cozmoEngine.h"
#include "platform/robotLogUploader/robotLogUploader.h"
#include "util/ankiLab/ankiLabDef.h"
#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "CozmoAPI"

#if REMOTE_CONSOLE_ENABLED
namespace {
  void UploadDebugLogs(ConsoleFunctionContextRef context)
  {
    using namespace Anki;
    using namespace Anki::Vector;

    std::string status;
    const Result result = RobotLogUploader::UploadDebugLogs(status);

    auto * channel = context->channel;

    if (result == RESULT_OK) {
      channel->WriteLog("<a href=%s>%s</a>\n", status.c_str(), status.c_str());
    } else {
      channel->WriteLog("Unable to upload debug logs (error %d)\n", result);
      if (!status.empty()) {
        channel->WriteLog("%s\n", status.c_str());
      }
    }
  }

  CONSOLE_FUNC(UploadDebugLogs, "Debug");
}
#endif

namespace Anki {
namespace Vector {

#pragma mark --- CozmoAPI Methods ---

#if ANKI_CPU_PROFILER_ENABLED
  CONSOLE_VAR_ENUM(u8, kCozmoEngine_Logging,       ANKI_CPU_CONSOLEVARGROUP, 0, Util::CpuProfiler::CpuProfilerLogging());
#endif


bool CozmoAPI::Start(Util::Data::DataPlatform* dataPlatform, const Json::Value& config)
{
  // Game init happens in EngineInstanceRunner construction, so we get the result
  // If we already had an instance, kill it and start again
  bool gameInitResult = false;
  _engineRunner.reset();
  _engineRunner.reset(new EngineInstanceRunner(dataPlatform, config, gameInitResult));

  if (!gameInitResult)
  {
    LOG_ERROR("CozmoAPI.Start", "Error initializing new api instance!");
  }

  return gameInitResult;
}


ANKI_CPU_PROFILER_ENABLED_ONLY(const float kMaxDesiredEngineDuration = 60.0f); // Above this warn etc.


bool CozmoAPI::Update(const BaseStationTime_t currentTime_nanosec)
{
  if (!_engineRunner)
  {
    LOG_ERROR("CozmoAPI.Update", "Engine has not been started!");
    return false;
  }

  // Replace Util::CpuThreadProfiler::kLogFrequencyNever with a small value to output logging,
  // can be used with Chrome Tracing format
  ANKI_CPU_TICK("CozmoEngine", kMaxDesiredEngineDuration, Util::CpuProfiler::CpuProfilerLoggingTime(kCozmoEngine_Logging));

  return _engineRunner->Update(currentTime_nanosec);
}

uint32_t CozmoAPI::ActivateExperiment(const uint8_t* requestBuffer, size_t requestLen,
                                      uint8_t* responseBuffer, size_t responseLen)
{
  using namespace Anki::Util::AnkiLab;

  // Response buffer will be filled in by ActivateExperiment. Set default values here.
  ActivateExperimentResponse res{AssignmentStatus::Invalid, ""};
  const size_t minResponseBufferLen = res.Size();

  // Assert that parameters are valid
  ASSERT_NAMED((nullptr != requestBuffer) && (requestLen > 0),
               "Must provide a valid requestBuffer/requestBufferLen to activate experiment");
  ASSERT_NAMED((nullptr != responseBuffer) && (responseLen >= minResponseBufferLen),
               "Must provide a valid responseBuffer/responseBufferLen to activate experiment");

  if (!_engineRunner) {
    return 0;
  }

  _engineRunner->SyncWithEngineUpdate([this, &res, requestBuffer, requestLen] {

    auto* engine = _engineRunner->GetEngine();
    if (engine == nullptr) {
      return;
    }

    // Unpack request buffer
    ActivateExperimentRequest req{requestBuffer, requestLen};

    res.status = engine->ActivateExperiment(req, res.variation_key);
  });

  const size_t bytesPacked = res.Pack(responseBuffer, responseLen);
  return Anki::Util::numeric_cast<uint32_t>(bytesPacked);
}

void CozmoAPI::RegisterEngineTickPerformance(const float tickDuration_ms,
                                             const float tickFrequency_ms,
                                             const float sleepDurationIntended_ms,
                                             const float sleepDurationActual_ms) const
{
  _engineRunner->GetEngine()->RegisterEngineTickPerformance(tickDuration_ms,
                                                           tickFrequency_ms,
                                                           sleepDurationIntended_ms,
                                                           sleepDurationActual_ms);
}

CozmoAPI::~CozmoAPI()
{
  if (_engineRunner)
  {
    // We are now "owning thread" for engine message sending; this is here in case
    // messages are sent during destruction
    _engineRunner->SetEngineThread();
    _engineRunner.reset();
  }
}

#pragma mark --- EngineInstanceRunner Methods ---

CozmoAPI::EngineInstanceRunner::EngineInstanceRunner(Util::Data::DataPlatform* dataPlatform,
                                                   const Json::Value& config, bool& initResult)
: _engineInstance(new CozmoEngine(dataPlatform))
{
  const Result initResultReturn = _engineInstance->Init(config);
  if (initResultReturn != RESULT_OK) {
    LOG_ERROR("CozmoAPI.EngineInstanceRunner", "cozmo init failed with error %d", initResultReturn);
  }
  initResult = initResultReturn == RESULT_OK;
}

// Destructor must exist in cpp (even though it's empty) in order for CozmoGame unique_ptr to be defined and deletable
CozmoAPI::EngineInstanceRunner::~EngineInstanceRunner()
{
}


bool CozmoAPI::EngineInstanceRunner::Update(const BaseStationTime_t currentTime_nanosec)
{
  Result updateResult;
  {
    std::lock_guard<std::mutex> lock{_updateMutex};
    updateResult = _engineInstance->Update(currentTime_nanosec);
  }
  if (updateResult != RESULT_OK) {
    LOG_ERROR("CozmoAPI.EngineInstanceRunner.Update", "Cozmo update failed with error %d", updateResult);
  }
  return updateResult == RESULT_OK;
}

void CozmoAPI::EngineInstanceRunner::SyncWithEngineUpdate(const std::function<void ()>& func) const
{
  std::lock_guard<std::mutex> lock{_updateMutex};
  func();
}

void CozmoAPI::EngineInstanceRunner::SetEngineThread()
{
  // Instance is valid for lifetime of instance runner
  DEV_ASSERT(_engineInstance, "CozmoAPI.EngineInstanceRunner.InvalidEngineInstance");
  std::lock_guard<std::mutex> lock{_updateMutex};
  _engineInstance->SetEngineThread();
}

} // namespace Vector
} // namespace Anki
