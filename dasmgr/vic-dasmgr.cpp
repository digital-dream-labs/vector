/**
* File: vic-dasmgr.cpp
*
* Description: Victor DAS Manager service app
*
* Copyright: Anki, inc. 2018
*
*/
#include "dasManager.h"
#include "dasConfig.h"

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "json/json.h"
#include "platform/victorCrashReports/victorCrashReporter.h"
#include "util/logging/logging.h"
#include "util/logging/DAS.h"
#include "util/logging/victorLogger.h"

#include <signal.h>
#include <stdlib.h>

using DataPlatform = Anki::Util::Data::DataPlatform;
using DASConfig = Anki::Vector::DASConfig;

#define LOG_PROCNAME "vic-dasmgr"
#define LOG_CHANNEL  LOG_PROCNAME

namespace
{
  constexpr const char DEFAULT_PLATFORM_CONFIG[] = "/anki/etc/config/platform_config.json";

  bool gShutdown = false;
}

void Shutdown(int signum)
{
  gShutdown = true;
  // Call android log print instead of LOG_INFO so log channel is not prepended
  // to the message, need "@@" to be at the beginning of the message so it is parsed
  // as a termination event by dasManager
  __android_log_print(ANDROID_LOG_INFO, "vic-dasmgr", "@@Shutdown on signal %d\n", signum);
}

static std::unique_ptr<DataPlatform> GetDataPlatform()
{
  std::string path = DEFAULT_PLATFORM_CONFIG;
  const char * cp = getenv("VIC_DASMGR_PLATFORM_CONFIG");
  if (cp != nullptr) {
    path = cp;
  }
  return DataPlatform::GetDataPlatform(path);
}

static std::unique_ptr<DASConfig> GetDASConfig(const DataPlatform & dataPlatform)
{
  const std::string & path = dataPlatform.GetResourcePath("config/DASConfig.json");
  return DASConfig::GetDASConfig(path);
}

int main(int argc, const char * argv[])
{
  // Set up crash reporter
  Anki::Vector::InstallCrashReporter(LOG_PROCNAME);

  // Set up logging
  auto logger = std::make_unique<Anki::Util::VictorLogger>(LOG_PROCNAME);
  Anki::Util::gLoggerProvider = logger.get();
  Anki::Util::gEventProvider = logger.get();

  // Set up signal handler
  signal(SIGTERM, Shutdown);

  // Say hello
  LOG_DEBUG("main.hello", "Hello world");

  auto dataPlatform = GetDataPlatform();
  if (!dataPlatform) {
    LOG_ERROR("main.InvalidDataPlatform", "Unable to get data platform");
    Anki::Util::gLoggerProvider = nullptr;
    Anki::Util::gEventProvider = nullptr;
    exit(1);
  }

  auto dasConfig = GetDASConfig(*dataPlatform);
  if (!dasConfig) {
    LOG_ERROR("main.InvalidDASConfig", "Unable to get DAS configuration");
    Anki::Util::gLoggerProvider = nullptr;
    Anki::Util::gEventProvider = nullptr;
    exit(1);
  }

  // Process log records until shutdown or error
  Anki::Vector::DASManager dasManager(*dasConfig);

  const int status = dasManager.Run(gShutdown);

  // Say goodbye & we're done
  LOG_DEBUG("main.goodbye", "Goodbye world (exit %d)", status);

  Anki::Util::gLoggerProvider = nullptr;
  Anki::Util::gEventProvider = nullptr;

  Anki::Vector::UninstallCrashReporter();

  exit(status);

}
