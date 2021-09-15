/*
 * File:          webotsCtrlWebServer.cpp
 * Date:
 * Description:   Cozmo 2.0 web server process for Webots simulation
 * Author:
 * Modifications:
 */

#include "../shared/ctrlCommonInitialization.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/jsonTools.h"

#include "osState/osState.h"

#include "json/json.h"

#include "util/global/globalDefinitions.h"
#include "util/logging/channelFilter.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/logging/logging.h"
#include "util/logging/multiFormattedLoggerProvider.h"

#include "webService.h"

#include <fstream>

#include <webots/Supervisor.hpp>

#define LOG_CHANNEL    "webotsCtrlWebServer"

using namespace Anki;
using namespace Anki::Vector;


// Instantiate supervisor and pass to AndroidHAL
webots::Supervisor webserverSupervisor;


int main(int argc, char **argv)
{
  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);

  // create platform.
  // Unfortunately, CozmoAPI does not properly receive a const DataPlatform, and that change
  // is too big of a change, since it involves changing down to the context, so create a non-const platform
  //const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0]);
  Util::Data::DataPlatform dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlWebServer");

  // Create the OSState singleton now, while we're in the main thread.
  // If we don't, subsequent calls from the webservice threads will
  // create it in the wrong thread and things won't work right
  (void)OSState::getInstance();

  OSState::getInstance()->SetRobotID(webserverSupervisor.getSelf()->getField("robotID")->getSFInt32());

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
  if ( params.filterLog )
  {
    using namespace Anki::Util;
    ChannelFilter* consoleFilter = new ChannelFilter();

    // load file config
    Json::Value consoleFilterConfig;
    const std::string& consoleFilterConfigPath = "config/engine/console_filter_config.json";
    if (!dataPlatform.readAsJson(Util::Data::Scope::Resources, consoleFilterConfigPath, consoleFilterConfig))
    {
      LOG_ERROR("webotsCtrlWebServer.main.loadConsoleConfig", "Failed to parse Json file '%s'", consoleFilterConfigPath.c_str());
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
    LOG_INFO("webotsCtrlWebServer.main.noFilter", "Console will not be filtered due to program args");
  }

  // Start with a step so that we can attach to the process here for debugging
  webserverSupervisor.step(1);  // Just 1 ms step duration

  // Create the standalone web server
  Json::Value wsConfig;
  static const std::string & wsConfigPath = "webserver/webServerConfig_standalone.json";
  const bool success = dataPlatform.readAsJson(Util::Data::Scope::Resources, wsConfigPath, wsConfig);
  if (!success)
  {
    LOG_ERROR("webotsCtrlWebServer.main.WebServerConfigNotFound",
              "Web server config file %s not found or failed to parse",
              wsConfigPath.c_str());
  }
  WebService::WebService cozmoWebServer;
  cozmoWebServer.Start(&dataPlatform, wsConfig);
  LOG_INFO("webotsCtrlWebServer.main", "cozmoWebServer created and initialized");

  //
  // Main Execution loop: step the world forward
  //
  static const u32 kWebotsWebServerTimeStep_ms = 100;
  while (webserverSupervisor.step(kWebotsWebServerTimeStep_ms) != -1)
  {
  }

  Util::gLoggerProvider = nullptr;
  return 0;
}
