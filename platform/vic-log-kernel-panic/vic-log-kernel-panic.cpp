/**
* File: vic-log-kernel-panic.cpp
*
* Description: Victor Log Kernel Panic application main
*
* Copyright: Anki, inc. 2019
*
*/

#include "platform/victorCrashReports/victorCrashReporter.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/DAS.h"
#include "util/logging/logging.h"
#include "util/logging/victorLogger.h"

#define PROCNAME "vic-log-kernel-panic"

void Error(const char * str)
{
  fprintf(stderr, "%s: %s\n", PROCNAME, str);
}

void Usage()
{
  fprintf(stderr, "Usage: %s [--help] file\n", PROCNAME);
}

int main(int argc, const char * argv[])
{

  std::vector<std::string> args;

  // Process command line
  for (int i = 1; i < argc; ++i) {
    const std::string & arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      Usage();
      exit(0);
    }
    args.push_back(arg);
  }

  if (args.size() != 1) {
    Error("Invalid arguments");
    Usage();
    exit(1);
  }

  const std::string & logpath = args[0];

  // Install global logger
  Anki::Util::VictorLogger logger(PROCNAME);
  Anki::Util::gLoggerProvider = &logger;
  Anki::Util::gEventProvider = &logger;

  //
  // Use crash reporter to generate a minidump in crash directory.
  // Path to dump is RETURNED AS OUTPUT.
  //
  std::string dump_path;
  bool ok = Anki::Vector::WriteMinidump(PROCNAME, dump_path);
  if (!ok) {
    LOG_ERROR("VicLogKernelPanic.WriteMinidump", "Failed to write minidump");
    Error("Failed to write minidump");
    exit(1);
  }

  if (dump_path.empty()) {
    LOG_ERROR("VicLogKernelPanic.NoPathToMinidump", "No path to minidump");
    Error("No path to minidump");
    exit(1);
  }

  // Copy output from kernel panic as attachment
  ok = Anki::Util::FileUtils::CopyFile(dump_path+".log", logpath);
  if (!ok) {
    LOG_ERROR("VicLogKernelPanic.CopyFile", "Failed to copy panic log %s", logpath.c_str());
    Error("Failed to copy panic log");
    exit(1);
  }

  // Report the panic to DAS
  DASMSG(robot_kernel_panic, "robot.kernel.panic", "Robot has rebooted after kernel panic");
  DASMSG_SET(s1, logpath, "Path to panic log");
  DASMSG_SET(s2, dump_path, "Path to minidump");
  DASMSG_SEND_ERROR();

  exit(0);
}
