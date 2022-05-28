/**
 * File: google_breakpad.cpp
 *
 * Author: chapados
 * Created: 10/08/2014
 *
 * Description: Google breakpad platform-specific methods
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "google_breakpad.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/logging/DAS.h"

#if (defined(VICOS) && defined(USE_GOOGLE_BREAKPAD))
#include <client/linux/handler/exception_handler.h>
#include <client/linux/handler/minidump_descriptor.h>
#include "util/string/stringUtils.h"

#include <chrono>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#endif

namespace GoogleBreakpad {

#if (defined(VICOS) && defined(USE_GOOGLE_BREAKPAD))
// Google Breakpad setup
namespace {

#define LOG_CHANNEL "GoogleBreakpad"

std::string dumpTag;
std::string dumpName;
std::string dumpPath;
std::string tmpDumpPath;
static int fd = -1;
static google_breakpad::ExceptionHandler* exceptionHandler;

sighandler_t gSavedQuitHandler = nullptr;

constexpr const char* kRobotVersionFile = "/anki/etc/version";
constexpr const char* kDigits = "0123456789";

std::string GetDateTimeString()
{
  using ClockType = std::chrono::system_clock;
  const auto now = ClockType::now();
  const auto numSecs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
  const auto millisLeft = std::chrono::duration_cast<std::chrono::milliseconds>((now - numSecs).time_since_epoch());

  const auto currTime_t = ClockType::to_time_t(now);
  struct tm localTime; // This is local scoped to make it thread safe
  // For some reason this is giving this error message: "__bionic_open_tzdata: couldn't find any tzdata when looking for GMT!"
  localtime_r(&currTime_t, &localTime);

  // Use the old fashioned strftime for thread safety, instead of std::put_time
  char formatTimeBuffer[256];
  strftime(formatTimeBuffer, sizeof(formatTimeBuffer), "%FT%H-%M-%S-", &localTime);

  std::ostringstream stringStream;
  stringStream << formatTimeBuffer << std::setfill('0') << std::setw(3) << millisLeft.count();

  return stringStream.str();
}

//
// Get path to magic crash report directory
//
const std::string & GetDumpDirectory()
{
  static const std::string dump_directory = "/data/data/com.anki.victor/cache/crashDumps";
  return dump_directory;
}

//
// Generate unique dump name for given prefix
//
std::string GetDumpName(const std::string & prefix)
{
  std::string buildVersion;
  std::ifstream ifs(kRobotVersionFile);
  ifs >> buildVersion;
  const size_t lastDigitIndex = buildVersion.find_last_of(kDigits);
  const size_t firstDigitIndex = buildVersion.find_last_not_of(kDigits, lastDigitIndex) + 1;
  const size_t len = lastDigitIndex - firstDigitIndex + 1;
  buildVersion = buildVersion.substr(firstDigitIndex, len);

  return prefix + "-V" + buildVersion + "-" + GetDateTimeString() + ".dmp";
}

//
// Capture recent log messages into given file
//
void DumpLogMessages(const std::string & path)
{
  // If activation socket exists, activate anki-crash-log.service.
  // This allows unprivileged processes (vic-cloud, vic-gateway)
  // to fetch log messages with reading /var/log/messages directly.
  //
  // Anki-crash-log.service is only available on developer builds.
  // Crash reports from a production build will not include log messages.
  //
  static const char * socket = "/run/anki-crash-log";
  if (!Anki::Util::FileUtils::PathExists(socket)) {
    LOG_WARNING("GoogleBreakpad.DumpLogMessages", "Unable to dump log messages");
    return;
  }

  FILE * fp = fopen(socket, "w");
  if (fp == nullptr) {
    LOG_WARNING("GoogleBreakpad.DumpLogMessages", "Unable to open %s", socket);
    return;
  }
  fprintf(fp, "%s\n", path.c_str());
  fclose(fp);
}

bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                  void* context, bool succeeded)
{
  LOG_INFO("GoogleBreakpad.DumpCallback",
           "Dump path: '%s', fd = %d, context = %p, succeeded = %s",
           tmpDumpPath.c_str(), descriptor.fd(), context, succeeded ? "true" : "false");
  if (descriptor.fd() == fd && fd >= 0) {
    (void) close(fd); fd = -1;
  }

  // Report the crash to DAS
  DASMSG(robot_crash, "robot.crash", "Robot service crash");
  DASMSG_SET(s1, dumpTag.c_str(), "Service name");
  DASMSG_SET(s2, dumpName.c_str(), "Crash name");
  DASMSG_SEND_ERROR();

  //
  // Flush logs to file system.  There is some latency in syslog so there's still no
  // guarantee that latest messages will appear in log files. :(
  //
  sync();

  // Move dump file to upload path
  rename(tmpDumpPath.c_str(), dumpPath.c_str());

  // Capture recent log messages
  DumpLogMessages(dumpPath);

  // Return false (not handled) so breakpad will chain to next handler.
  return false;
}

} // anon namespace

static void QuitHandler(int signum)
{
  if (exceptionHandler != nullptr) {
    exceptionHandler->WriteMinidump();
  }
  signal(signum, gSavedQuitHandler);
  raise(signum);
}

void InstallGoogleBreakpad(const char* filenamePrefix)
{
  const std::string & dump_directory = GetDumpDirectory();
  const std::string & dump_name = GetDumpName(filenamePrefix);

  // Save these strings for later
  dumpTag     = filenamePrefix;
  dumpName    = dump_name;
  dumpPath    = dump_directory + "/" + dump_name;
  tmpDumpPath = dumpPath + "~";

  Anki::Util::FileUtils::CreateDirectory(dump_directory);

  fd = open(tmpDumpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0600);
  google_breakpad::MinidumpDescriptor descriptor(fd);
  descriptor.set_sanitize_stacks(true);
  exceptionHandler = new google_breakpad::ExceptionHandler(descriptor, NULL, DumpCallback, NULL, true, -1);

  gSavedQuitHandler = signal(SIGQUIT, QuitHandler);
}

void UnInstallGoogleBreakpad()
{
  signal(SIGQUIT, gSavedQuitHandler);

  delete exceptionHandler;
  exceptionHandler = nullptr;
  if (fd >= 0) {
    (void) close(fd);
    fd = -1;
  }
  if (!tmpDumpPath.empty()) {
    struct stat tmpDumpStat;
    memset(&tmpDumpStat, 0, sizeof(tmpDumpStat));
    int rc = stat(tmpDumpPath.c_str(), &tmpDumpStat);
    if (!rc && !tmpDumpStat.st_size) {
      (void) unlink(tmpDumpPath.c_str());
    }
  }
}

bool WriteMinidump(const std::string & prefix, std::string & out_dump_path)
{
  const std::string & dump_directory = GetDumpDirectory();
  if (dump_directory.empty()) {
    LOG_ERROR("GoogleBreakpad.WriteMinidump", "Unable to get dump directory");
    return false;
  }

  const std::string & dump_name = GetDumpName(prefix);
  if (dump_name.empty()) {
    LOG_ERROR("GoogleBreakpad.WriteMinidump", "Unable to get dump name");
    return false;
  }

  out_dump_path = dump_directory + "/" + dump_name;

  // Create directory, if needed
  Anki::Util::FileUtils::CreateDirectory(dump_directory);

  // Open dump file
  int fd = open(out_dump_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0600);
  if (fd < 0) {
    LOG_ERROR("GoogleBreakpad.WriteMinidump", "Unable to open dump path %s (errno %d)", out_dump_path.c_str(), errno);
    return false;
  }

  // Write the dump
  google_breakpad::MinidumpDescriptor descriptor(fd);
  descriptor.set_sanitize_stacks(true);

  google_breakpad::ExceptionHandler handler(descriptor, nullptr, nullptr, nullptr, false, -1);

  const bool ok = handler.WriteMinidump();
  if (!ok) {
    LOG_ERROR("GoogleBreakpad.WriteMinidump", "Unable to write minidump %s", out_dump_path.c_str());
    close(fd);
    return false;
  }

  // Clean up and we're done
  close(fd);
  return true;

}

#else

void InstallGoogleBreakpad(const char *path) {}
void UnInstallGoogleBreakpad() {}
bool WriteMinidump(const std::string & prefix, std::string & out_dump_path) { return false; }

#endif

} // namespace GoogleBreakpad
