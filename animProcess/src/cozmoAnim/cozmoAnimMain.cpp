/**
* File: cozmoAnimMain.cpp
*
* Author: Kevin Yoon
* Created: 6/26/17
*
* Description: Cozmo Anim Process on Victor
*
* Copyright: Anki, inc. 2017
*
*/

#include "cozmoAnim/animEngine.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/logging/channelFilter.h"
#include "util/logging/victorLogger.h"

#include "platform/common/diagnosticDefines.h"
#include "platform/victorCrashReports/victorCrashReporter.h"

#include <thread>
#include <unistd.h>
#include <csignal>

using namespace Anki;
using namespace Anki::Vector;

#define LOG_PROCNAME "vic-anim"
#define LOG_CHANNEL "CozmoAnimMain"

namespace {
  bool gShutdown = false;
}

static void Shutdown(int signum)
{
  Anki::Util::DropBreadcrumb(false, nullptr, -1);
  LOG_INFO("CozmoAnimMain.Shutdown", "Shutdown on signal %d", signum);
  gShutdown = true;
}

Anki::Util::Data::DataPlatform* createPlatform(const std::string& persistentPath,
                                               const std::string& cachePath,
                                               const std::string& resourcesPath)
{
  Anki::Util::FileUtils::CreateDirectory(persistentPath);
  Anki::Util::FileUtils::CreateDirectory(cachePath);
  Anki::Util::FileUtils::CreateDirectory(resourcesPath);

  return new Anki::Util::Data::DataPlatform(persistentPath, cachePath, resourcesPath);
}

Anki::Util::Data::DataPlatform* createPlatform()
{
  char config_file_path[PATH_MAX] = { 0 };
  const char* env_config = getenv("VIC_ANIM_CONFIG");
  if (env_config != NULL) {
    strncpy(config_file_path, env_config, sizeof(config_file_path));
  }

  Json::Value config;

  printf("config_file: %s\n", config_file_path);
  if (strlen(config_file_path) > 0) {
    std::string config_file{config_file_path};
    if (!Anki::Util::FileUtils::FileExists(config_file)) {
      fprintf(stderr, "config file not found: %s\n", config_file_path);
    }

    std::string jsonContents = Anki::Util::FileUtils::ReadFile(config_file);
    printf("jsonContents: %s", jsonContents.c_str());
    Json::Reader reader;
    if (!reader.parse(jsonContents, config)) {
      PRINT_STREAM_ERROR("CozmoAnimMain.createPlatform",
        "json configuration parsing error: " << reader.getFormattedErrorMessages());
    }
  }

  std::string persistentPath;
  std::string cachePath;
  std::string resourcesPath;

  if (config.isMember("DataPlatformPersistentPath")) {
    persistentPath = config["DataPlatformPersistentPath"].asCString();
  } else {
    LOG_ERROR("cozmoAnimMain.createPlatform.DataPlatformPersistentPathUndefined", "");
  }

  if (config.isMember("DataPlatformCachePath")) {
    cachePath = config["DataPlatformCachePath"].asCString();
  } else {
    LOG_ERROR("cozmoAnimMain.createPlatform.DataPlatformCachePathUndefined", "");
  }

  if (config.isMember("DataPlatformResourcesPath")) {
    resourcesPath = config["DataPlatformResourcesPath"].asCString();
  } else {
    LOG_ERROR("cozmoAnimMain.createPlatform.DataPlatformResourcesPathUndefined", "");
  }

  return createPlatform(persistentPath, cachePath, resourcesPath);
}


int main(void)
{
  signal(SIGTERM, Shutdown);

  InstallCrashReporter(LOG_PROCNAME);

  // - create and set logger
  auto logger = std::make_unique<Anki::Util::VictorLogger>(LOG_PROCNAME);

  Util::gLoggerProvider = logger.get();
  Util::gEventProvider = logger.get();

  auto dataPlatform = createPlatform();

  // - console filter for logs
  {
    using namespace Anki::Util;
    ChannelFilter* consoleFilter = new ChannelFilter();

    // load file config
    Json::Value consoleFilterConfig;
    const std::string& consoleFilterConfigPath = "config/engine/console_filter_config.json";
    if (!dataPlatform->readAsJson(Anki::Util::Data::Scope::Resources, consoleFilterConfigPath, consoleFilterConfig))
    {
      LOG_ERROR("CozmoAnimMain.main", "Failed to parse json file '%s'", consoleFilterConfigPath.c_str());
    }
  
    // initialize console filter for this platform
    const std::string& platformOS = dataPlatform->GetOSPlatformString();
    const Json::Value& consoleFilterConfigOnPlatform = consoleFilterConfig[platformOS];
    consoleFilter->Initialize(consoleFilterConfigOnPlatform);

    // set filter in the loggers
    std::shared_ptr<const IChannelFilter> filterPtr( consoleFilter );

    Anki::Util::gLoggerProvider->SetFilter(filterPtr);
  }

  // Set up the console vars to load from file, if it exists
  ANKI_CONSOLE_SYSTEM_INIT(dataPlatform->pathToResource(Anki::Util::Data::Scope::Cache, "consoleVarsAnim.ini").c_str());

  // Create and init AnimEngine
  Anim::AnimEngine * animEngine = new Anim::AnimEngine(dataPlatform);

  Result result = animEngine->Init();
  if (RESULT_OK != result) {
    LOG_ERROR("CozmoAnimMain.main.InitFailed", "Unable to initialize (exit %d)", result);
    delete animEngine;
    Util::gLoggerProvider = nullptr;
    Util::gEventProvider = nullptr;
    UninstallCrashReporter();
    sync();
    exit(result);
  }

  using namespace std::chrono;
  using TimeClock = steady_clock;

  const auto runStart = TimeClock::now();
  auto prevTickStart  = runStart;
  auto tickStart      = runStart;

  // Set the target time for the end of the first frame
  auto targetEndFrameTime = runStart + (microseconds)(ANIM_TIME_STEP_US);

  // Loop until shutdown or error
  while (!gShutdown) {

    const duration<double> curTime_s = tickStart - runStart;
    const BaseStationTime_t curTime_ns = Util::numeric_cast<BaseStationTime_t>(Util::SecToNanoSec(curTime_s.count()));

    result = animEngine->Update(curTime_ns);
    if (RESULT_OK != result) {
      LOG_WARNING("CozmoAnimMain.main.UpdateFailed", "Unable to update (result %d)", result);

      // Don't exit with error code so as not to trigger
      // fault code 800 on what is actually a clean shutdown.
      if (result == RESULT_SHUTDOWN) {
        result = RESULT_OK;
      }
      break;
    }

    const auto tickAfterAnimExecution = TimeClock::now();
    const auto remaining_us = duration_cast<microseconds>(targetEndFrameTime - tickAfterAnimExecution);
    const auto tickDuration_us = duration_cast<microseconds>(tickAfterAnimExecution - tickStart);

    tracepoint(anki_ust, vic_anim_loop_duration, tickDuration_us.count());
#if ENABLE_TICK_TIME_WARNINGS
    // Complain if we're going overtime
    if (remaining_us < microseconds(-ANIM_OVERTIME_WARNING_THRESH_US))
    {
      LOG_WARNING("CozmoAnimMain.overtime", "Update() (%dms max) is behind by %.3fms",
                  ANIM_TIME_STEP_MS, (float)(-remaining_us).count() * 0.001f);
    }
#endif
    // We ALWAYS sleep, but if we're overtime, we 'sleep zero' which still
    // allows other threads to run
    static const auto minimumSleepTime_us = microseconds((long)0);
    const auto sleepTime_us = std::max(minimumSleepTime_us, remaining_us);
    std::this_thread::sleep_for(sleepTime_us);

    // Set the target end time for the next frame
    targetEndFrameTime += (microseconds)(ANIM_TIME_STEP_US);

    // See if we've fallen very far behind (this happens e.g. after a 5-second blocking
    // load operation); if so, compensate by catching the target frame end time up somewhat.
    // This is so that we don't spend the next SEVERAL frames catching up.
    const auto timeBehind_us = -remaining_us;
    static const auto kusPerFrame = ((microseconds)(ANIM_TIME_STEP_US)).count();
    static const int kTooFarBehindFramesThreshold = 2;
    static const auto kTooFarBehindThreshold = (microseconds)(kTooFarBehindFramesThreshold * kusPerFrame);
    if (timeBehind_us >= kTooFarBehindThreshold)
    {
      const int framesBehind = (int)(timeBehind_us.count() / kusPerFrame);
      const auto forwardJumpDuration = kusPerFrame * framesBehind;
      targetEndFrameTime += (microseconds)forwardJumpDuration;
#if ENABLE_TICK_TIME_WARNINGS
      LOG_WARNING("CozmoAnimMain.catchup",
                  "Update was too far behind so moving target end frame time forward by an additional %.3fms",
                  (float)(forwardJumpDuration * 0.001f));
#endif
    }
    tickStart = TimeClock::now();

    const auto timeSinceLastTick_us = duration_cast<microseconds>(tickStart - prevTickStart);
    prevTickStart = tickStart;

    const auto sleepTimeActual_us = duration_cast<microseconds>(tickStart - tickAfterAnimExecution);
    animEngine->RegisterTickPerformance(tickDuration_us.count() * 0.001f,
                                        timeSinceLastTick_us.count() * 0.001f,
                                        sleepTime_us.count() * 0.001f,
                                        sleepTimeActual_us.count() * 0.001f);
  }

  LOG_INFO("CozmoAnimMain.main.Shutdown", "Shutting down (exit %d)", result);

  delete animEngine;

  Util::gLoggerProvider = nullptr;
  Util::gEventProvider = nullptr;

  UninstallCrashReporter();
  sync();
  exit(result);
}
