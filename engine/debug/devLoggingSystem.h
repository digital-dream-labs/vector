/**
* File: devLoggingSystem
*
* Author: Lee Crippen
* Created: 3/30/2016
*
* Description: System for collecting, archiving, and uploading logs useful for debugging during development.
*
* Copyright: Anki, inc. 2016
*
*/
#ifndef __Basestation_Debug_DevLoggingSystem_H_
#define __Basestation_Debug_DevLoggingSystem_H_

#include "json/json.h"
#include "util/export/export.h"
#include "util/helpers/noncopyable.h"
#include "util/logging/rollingFileLogger.h"

#include <memory>
#include <string>
#include <chrono>
#include <vector>

// Forward declarations
namespace Anki {
  namespace Util {
    class ILoggerProvider;
  }
}

namespace Anki {
namespace Vector {
  
using DevLoggingClock = Util::RollingFileLogger::ClockType;

class DevLoggingSystem : Util::noncopyable {
public:
  // Visible methods are accessible outside of cozmo_engine library.
  // These methods provide a minimal interface for configuration and use of the engine's internal log system.
  ANKI_VISIBLE static void CreateInstance(const std::string& loggingBaseDirectory, const std::string& appRunId);
  ANKI_VISIBLE static DevLoggingSystem* GetInstance();
  ANKI_VISIBLE static Anki::Util::ILoggerProvider* GetInstancePrintProvider();
  ANKI_VISIBLE static void DestroyInstance();

  static const DevLoggingClock::time_point& GetAppRunStartTime() { return kAppRunStartTime; }
  static uint32_t GetAppRunMilliseconds();
  
  ~DevLoggingSystem();
  
  template<typename MsgType>
  void LogMessage(const MsgType& message);
  
  const std::string& GetDevLoggingBaseDirectory() const { return _devLoggingBaseDirectory; }
  
  void PrepareForUpload(const std::string& namePrefix) const;
  void DeleteLog(const std::string& archiveFilename) const;
  std::vector<std::string> GetLogFilenamesForUpload() const;

  Util::Dispatch::Queue* GetQueue() { return _queue; }
  
  static Json::Value GetAppRunData(const std::string& appRunFilename);
  static std::string GetAppRunFilename(const std::string& archiveFilename);
  const std::string& GetCurrentAppRunFilename() const;
  void UpdateDeviceId(const std::string&);

  static const std::string kPrintName;
  static const std::string kGameToEngineName;
  static const std::string kEngineToGameName;
  static const std::string kRobotToEngineName;
  static const std::string kEngineToRobogName;
  static const std::string kEngineToVizName;
  static const std::string kAppRunKey;
  static const std::string kDeviceIdKey;
  static const std::string kTimeSinceEpochKey;
  static const std::string kReadyForUploadKey;
  static const std::string kHasBeenUploadedKey;

private:
  static const DevLoggingClock::time_point kAppRunStartTime;
  static const std::string kArchiveExtensionString;
  static const std::string kAppRunExtension;
  static const std::string kWavFileExtension;
  static const std::string kLogFileExtension;

  Util::Dispatch::Queue*                      _queue;
  std::unique_ptr<Util::RollingFileLogger>    _gameToEngineLog;
  std::unique_ptr<Util::RollingFileLogger>    _engineToGameLog;
  std::unique_ptr<Util::RollingFileLogger>    _robotToEngineLog;
  std::unique_ptr<Util::RollingFileLogger>    _engineToRobotLog;
  std::unique_ptr<Util::RollingFileLogger>    _engineToVizLog;

  std::string _allLogsBaseDirectory;
  std::string _devLoggingBaseDirectory;
  std::string _appRunId;
  
  DevLoggingSystem(const std::string& baseDirectory, const std::string& appRunId);
  
  void DeleteFiles(const std::string& baseDirectory, const std::string& extension) const;
  static void CopyFile(const std::string& sourceFile, const std::string& destination);
  void ArchiveDirectories(const std::string& baseDirectory, const std::vector<std::string>& excludeDirectories) const;
  static void ArchiveOneDirectory(const std::string& baseDirectory);

  template<typename MsgType>
  std::string PrepareMessage(const MsgType& message) const;
  
  void CreateAppRunFile(const std::string& appRunTimeString, const std::string& appRunId);
};

} // end namespace Vector
} // end namespace Anki


#endif //__Basestation_Debug_DevLoggingSystem_H_
