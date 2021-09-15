/*
 * File:          webotsCtrlAnim.cpp
 * Date:
 * Description:   Vector animation process for Webots simulation
 * Author:
 * Modifications:
 */

#include "cozmoAnim/animEngine.h"

#include "../shared/ctrlCommonInitialization.h"
#include "anki/cozmo/shared/cozmoConfig.h"

#include "osState/osState.h"

#include "util/logging/channelFilter.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/logging/logging.h"
#include "util/logging/multiFormattedLoggerProvider.h"
#include "util/time/stopWatch.h"

#include <webots/Supervisor.hpp>

#define LOG_CHANNEL    "webotsCtrlAnim"

namespace Anki {
  namespace Vector {
    CONSOLE_VAR_EXTERN(bool, kEnableCladLogger);
  }
}

using namespace Anki;
using namespace Anki::Vector;


// Instantiate supervisor and pass to AndroidHAL
webots::Supervisor animSupervisor;


int main(int argc, char **argv)
{
  // Start with a step so that we can attach to the process here for debugging
  animSupervisor.step(ANIM_TIME_STEP_MS);

  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);

  // create platform.
  // Unfortunately, CozmoAPI does not properly receive a const DataPlatform, and that change
  // is too big of a change, since it involves changing down to the context, so create a non-const platform
  //const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0]);
  Util::Data::DataPlatform dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlAnim");

  // - create and set logger
  Util::IFormattedLoggerProvider* printfLoggerProvider = new Util::PrintfLoggerProvider(Anki::Util::LOG_LEVEL_WARN,
                                                                                        params.colorizeStderrOutput);
  Util::MultiFormattedLoggerProvider loggerProvider({
    printfLoggerProvider
  });
  loggerProvider.SetMinLogLevel(Anki::Util::LOG_LEVEL_DEBUG);
  Util::gLoggerProvider = &loggerProvider;
  Util::sSetGlobal(DPHYS, "0xdeadffff00000001");

  // - console filter for logs
  if (params.filterLog)
  {
    using namespace Anki::Util;
    ChannelFilter* consoleFilter = new ChannelFilter();

    // load file config
    Json::Value consoleFilterConfig;
    const std::string& consoleFilterConfigPath = "config/engine/console_filter_config.json";
    if (!dataPlatform.readAsJson(Util::Data::Scope::Resources, consoleFilterConfigPath, consoleFilterConfig))
    {
      LOG_ERROR("webotsCtrlAnim.main.loadConsoleConfig", "Failed to parse Json file '%s'", consoleFilterConfigPath.c_str());
    }

    // initialize console filter for this platform
    const std::string& platformOS = dataPlatform.GetOSPlatformString();
    const Json::Value& consoleFilterConfigOnPlatform = consoleFilterConfig[platformOS];
    consoleFilter->Initialize(consoleFilterConfigOnPlatform);

    // set filter in the loggers
    std::shared_ptr<const IChannelFilter> filterPtr( consoleFilter );
    printfLoggerProvider->SetFilter(filterPtr);

    // also parse additional info for providers
    printfLoggerProvider->ParseLogLevelSettings(consoleFilterConfigOnPlatform);

  }
  else
  {
    LOG_INFO("webotsCtrlAnim.main.noFilter", "Console will not be filtered due to program args");
  }

  // Set up the console vars to load from file, if it exists
  ANKI_CONSOLE_SYSTEM_INIT("consoleVarsAnim.ini");

  // Initialize the anim engine
  Anim::AnimEngine animEngine(&dataPlatform);
  const auto result = animEngine.Init();
  if (result != RESULT_OK) {
    LOG_ERROR("webotsCtrlAnim.main", "Failed in creation/initialization of AnimEngine");
  }
  else {
    LOG_INFO("webotsCtrlAnim.main", "AnimEngine created and initialized.");

    OSState::getInstance()->SetRobotID(animSupervisor.getSelf()->getField("robotID")->getSFInt32());

    Anki::Util::Time::StopWatch stopWatch("tick");

    //
    // Main Execution loop: step the world forward
    //
    while (animSupervisor.step(ANIM_TIME_STEP_MS) != -1)
    {
      stopWatch.Start();

      const double currTimeNanoseconds = Util::SecToNanoSec(animSupervisor.getTime());
      animEngine.Update(Util::numeric_cast<BaseStationTime_t>(currTimeNanoseconds));

      const float time_ms = Util::numeric_cast<float>(stopWatch.Stop());

      // Record tick performance; this includes a call to PerfMetric.
      // For webots, we 'fake' the sleep time here.  Unlike in Cozmo webots,
      // we don't actually sleep in this loop
      static const float kTargetDuration_ms = Util::numeric_cast<float>(ANIM_TIME_STEP_MS);
      const float animFreq_ms  = std::max(time_ms, kTargetDuration_ms);
      const float sleepTime_ms = std::max(0.0f, kTargetDuration_ms - time_ms);
      const float sleepTimeActual_ms = sleepTime_ms;
      animEngine.RegisterTickPerformance(time_ms,
                                         animFreq_ms,
                                         sleepTime_ms,
                                         sleepTimeActual_ms);
    } // End tick loop
  }

  Util::gLoggerProvider = nullptr;
  return 0;
}
