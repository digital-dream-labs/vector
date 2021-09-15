/**
 * File: ctrlCommonInitialization.h
 *
 * Author: raul
 * Date:  07/08/16
 *
 * Description: A few functions that all webots controllers share to initialize. Originally created to refactor
 * the way we set filters for logging.
 *
 * Copyright: Anki, Inc. 2016
**/
#include "ctrlCommonInitialization.h"

#include "util/helpers/templateHelpers.h"
#include "util/logging/channelFilter.h"
#include "util/logging/logging.h"
#include "util/logging/printfLoggerProvider.h"

#include "anki/cozmo/shared/factory/emrHelper.h"

#include <cstdio>
#include <string>

const static std::string kRootDirectory = "../../../";
const static std::string kBuildPath = "_build/mac/Debug/playbackLogs/";


namespace Anki {
namespace WebotsCtrlShared {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ParsedCommandLine ParseCommandLine(int argc, char** argv)
{
  ParsedCommandLine ret;

  if ( argc > 1 )
  {
    const std::string kFilterParam = "--applyLogFilter";
    const std::string kColorizeParam = "--colorizeStderrOutput";
    const std::string kWhiskeyParam = "--whiskey";
    for( int i=1; i<argc; ++i) {
      if ( kFilterParam == argv[i] ) {
        ret.filterLog = true;
      } else if ( kColorizeParam == argv[i] ) {
        ret.colorizeStderrOutput = true;
      } else if ( kWhiskeyParam == argv[i] ) {
        Vector::Factory::SetWhiskey(true);
      }
    }
  }

  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Anki::Util::Data::DataPlatform CreateDataPlatformBS(const std::string& runningPath, const std::string& platformID)
{
  #if defined(_WIN32) || defined(WIN32)
    size_t pos = runningPath.rfind('\\');
  #else
    size_t pos = runningPath.rfind('/');
  #endif

  // Get the path
  const std::string path = runningPath.substr(0,pos+1);
  const std::string outputPath = path + kRootDirectory + kBuildPath + platformID;

  const std::string resourcePath = path + "resources";
  const std::string persistentPath = path + "persistent";
  const std::string& cachePath = outputPath;
  Anki::Util::Data::DataPlatform dataPlatform(persistentPath, cachePath, resourcePath);

  return dataPlatform;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Anki::Util::Data::DataPlatform CreateDataPlatformTest(const std::string& runningPath, const std::string& platformID)
{
  #if defined(_WIN32) || defined(WIN32)
    size_t pos = runningPath.rfind('\\');
  #else
    size_t pos = runningPath.rfind('/');
  #endif

  // Get the path
  const std::string path = runningPath.substr(0,pos+1);
  const std::string outputPath = path + kRootDirectory + kBuildPath + platformID;

  const std::string resourcePath = path + "temp";
  const std::string persistentPath = path + "temp";
  const std::string& cachePath = outputPath;
  Anki::Util::Data::DataPlatform dataPlatform(persistentPath, cachePath, resourcePath);

  return dataPlatform;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// AutoGlobalLogger
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AutoGlobalLogger::AutoGlobalLogger(Util::IFormattedLoggerProvider* provider,
  const Util::Data::DataPlatform& dataPlatform,
  bool loadLoggerFilter)
: _provider(provider)
{
  Initialize(provider, dataPlatform, loadLoggerFilter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
AutoGlobalLogger::~AutoGlobalLogger()
{
  // clear global
  if ( _provider == Anki::Util::gLoggerProvider ) {
    Anki::Util::gLoggerProvider = nullptr;
  }

  // now delete the provider
  Util::SafeDelete(_provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void AutoGlobalLogger::Initialize(Util::IFormattedLoggerProvider* loggerProvider,
  const Util::Data::DataPlatform& dataPlatform,
  bool loadLoggerFilter)
{
  Anki::Util::gLoggerProvider = loggerProvider;

  // if we have to set the logger filter
  if ( loadLoggerFilter )
  {
    using namespace Anki::Util;
    ChannelFilter* consoleFilter = new ChannelFilter();

    // load file config
    Json::Value consoleFilterConfig;
    const std::string& consoleFilterConfigPath = "config/engine/console_filter_config.json";
    if (!dataPlatform.readAsJson(Anki::Util::Data::Scope::Resources, consoleFilterConfigPath, consoleFilterConfig))
    {
      PRINT_NAMED_ERROR("AutoGlobalLogger.Initialize", "Failed to parse Json file '%s'", consoleFilterConfigPath.c_str());
    }

    // initialize console filter for this platform
    const std::string& platformOS = dataPlatform.GetOSPlatformString();
    const Json::Value& consoleFilterConfigOnPlatform = consoleFilterConfig[platformOS];
    consoleFilter->Initialize(consoleFilterConfigOnPlatform);

    // set filter in the loggers
    std::shared_ptr<const IChannelFilter> filterPtr( consoleFilter );
    loggerProvider->SetFilter(filterPtr);

    // also parse info for providers
    loggerProvider->ParseLogLevelSettings(consoleFilterConfigOnPlatform);
  }
  else
  {
    PRINT_CH_INFO("LOG", "AutoGlobalLogger.Initialize", "Console will not be filtered due to program args");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
DefaultAutoGlobalLogger::DefaultAutoGlobalLogger(const Util::Data::DataPlatform& dataPlatform, bool loadLoggerFilter, bool colorizeStderrOutput) :
AutoGlobalLogger(new Util::PrintfLoggerProvider(), dataPlatform, loadLoggerFilter)
{
  // assert in case the logger created (passed to base constructor) is not a printfLogger, since we
  // do a static_cast instead of dynamic_cast in shipping for performance.
  DEV_ASSERT(dynamic_cast<Util::PrintfLoggerProvider*>(_provider), "DefaultAutoGlobalLogger.DefaultLoggerIsNotPrintf");
  Util::PrintfLoggerProvider* provider = static_cast<Util::PrintfLoggerProvider*>(_provider);
  provider->SetMinToStderrLevel(Anki::Util::LOG_LEVEL_WARN);
  provider->SetColorizeStderrOutput(colorizeStderrOutput);
}

}; // namespace
}; // namespace
