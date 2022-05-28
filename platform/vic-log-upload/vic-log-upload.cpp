/**
* File: vic-log-upload.cpp
*
* Description: Victor Log Upload application main
*
* Copyright: Anki, inc. 2018
*
*/

#include "platform/robotLogUploader/robotLogDumper.h"
#include "platform/robotLogUploader/robotLogUploader.h"

#include "json/json.h"
#include "platform/victorCrashReports/victorCrashReporter.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"
#include "util/string/stringUtils.h"

#include <list>

#define LOG_PROCNAME "vic-log-upload"
#define LOG_CHANNEL "VicLogUpload"

static void Error(const std::string & s)
{
  fprintf(stderr, "%s: %s\n", LOG_PROCNAME, s.c_str());
}

static void Usage(FILE * f)
{
  fprintf(f, "Usage: %s [-h] file\n", LOG_PROCNAME);
}

//
// Report result to stdout as parsable json struct
//
static void Report(const std::string & status, const std::string & value)
{
  LOG_INFO("VicLogUpload.Report", "result[%s] = %s", status.c_str(), value.c_str());

  Json::Value json;
  json["result"][status] = value;

  Json::StyledWriter writer;
  fprintf(stdout, "%s", writer.write(json).c_str());
}

int main(int argc, const char * argv[])
{
  using namespace Anki;
  using namespace Anki::Util;
  using namespace Anki::Vector;

  // Set up logging
  auto logger = std::make_unique<VictorLogger>(LOG_PROCNAME);
  gLoggerProvider = logger.get();
  gEventProvider = logger.get();

  // Set up crash reporter
  CrashReporter crashReporter(LOG_PROCNAME);

  // Process arguments
  std::list<std::string> paths;
  for (int i = 1; i < argc; ++i) {
    const std::string & arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      Usage(stdout);
      return 0;
    }
    if (arg[0] == '-') {
      Usage(stderr);
      return 1;
    }
    paths.push_back(arg);
  }

  // Validate arguments
  if (paths.size() != 1) {
    Error("Invalid arguments");
    Usage(stderr);
    return 1;
  }

  // Do the thing
  const std::string & path = paths.front();
  RobotLogUploader logUploader;
  std::string url;

  const Result result = logUploader.Upload(path, url);
  if (result != RESULT_OK) {
    LOG_ERROR("VicLogUpload", "Unable to upload file %s (error %d)", path.c_str(), result);
    Report("error", "Unable to upload file");
    return 1;
  }

  Report("success", url);

}
