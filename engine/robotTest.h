/**
 * File: robotTest
 *
 * Author: Paul Terry
 * Created: 04/05/2019
 *
 * Description: Engine-based test framework for physical robot
 *
 * Copyright: Anki, Inc. 2019
 *
 **/


#ifndef __Vector_Engine_RobotTest_H__
#define __Vector_Engine_RobotTest_H__

#include "util/global/globalDefinitions.h"
#include "util/string/stringHelpers.h"
#include "json/json.h"

#include <string>
#include <queue>


// To enable PerfMetric in a build, define ANKI_PERF_METRIC_ENABLED as 1
#if !defined(ANKI_ROBOT_TEST_ENABLED)
  #if ANKI_DEV_CHEATS
    #define ANKI_ROBOT_TEST_ENABLED 1
  #else
    #define ANKI_ROBOT_TEST_ENABLED 0
  #endif
#endif


namespace Anki {
namespace Vector {

namespace WebService {
  class WebService;
}


class RobotTest
{
public:

  explicit RobotTest(const CozmoContext* context);
  virtual ~RobotTest();

  void Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService);
  void Update();

  WebService::WebService* GetWebService() { return _webService; }

  int  ParseWebCommands(std::string& queryString);
  void ExecuteQueuedWebCommands(std::string* resultStr = nullptr);

  void ExecuteWebCommandRun(const std::string& scriptName, std::string* resultStr);
  void ExecuteWebCommandStop(std::string* resultStr);
  void ExecuteWebCommandStatus(std::string* resultStr);
  void ExecuteWebCommandListScripts(std::string* resultStr);
  void ExecuteWebCommandGetScript(const std::string& scriptName, std::string* resultStr);
  void ExecuteWebCommandRefreshUploadedScripts(std::string* resultStr);

  void OnCloudIntentCompleted() { _waitingForCloudIntent = false; }

private:

  void LoadScripts(const std::string& path, const bool isUploadedScriptsFolder,
                   std::string* resultStr = nullptr);
  bool ValidateScript(const Json::Value& scriptJson);

  bool StartScript(const std::string& scriptName);
  void StopScript();

  enum class ScriptCommandType
  {
    EXIT,
    PERFMETRIC,
    CLOUD_INTENT,
    WAIT_CLOUD_INTENT,
    WAIT_UNTIL_ENGINE_TICK_COUNT,
    WAIT_TICKS,
    WAIT_SECONDS,
    CPU_START,
    CPU_STOP,
  };
  void FetchNextScriptCommand();
  bool StringToScriptCommand(const std::string& commandStr, ScriptCommandType& command);
  bool ExecuteScriptCommand(ScriptCommandType command);

  void SampleCPU(const bool calculateUsage);

#if ANKI_ROBOT_TEST_ENABLED
  const CozmoContext*       _context;
  Util::Data::DataPlatform* _platform;
#endif
  WebService::WebService*   _webService;
  std::string               _uploadedScriptsPath;

  enum class RobotTestState
  {
    INACTIVE,
    RUNNING,
  };
  RobotTestState            _state = RobotTestState::INACTIVE;
  static const std::string  kInactiveScriptName;
  std::string               _curScriptName = kInactiveScriptName;
  const Json::Value*        _curScriptCommandsJson = nullptr;
  int                       _curScriptCommandIndex = 0;
  ScriptCommandType         _nextScriptCommand = ScriptCommandType::EXIT;
  int                       _waitTickCount = 0;
  float                     _waitTimeToExpire = 0.0f;
  bool                      _waitingForCloudIntent = false;
  bool                      _cpuStartCommandExecuted = false;

  enum class WebCommandType
  {
    RUN,
    STOP,
    STATUS,
    LIST_SCRIPTS,
    GET_SCRIPT,
    REFRESH_UPLOADED_SCRIPTS,
  };

  struct RobotTestWebCommand
  {
    WebCommandType _webCommand;
    std::string    _paramString;

    RobotTestWebCommand(const WebCommandType cmd)
    {
      _webCommand = cmd; _paramString = "";
    }
    RobotTestWebCommand(const WebCommandType cmd, const std::string& strParam)
    {
      _webCommand = cmd; _paramString = strParam;
    }
  };

  std::queue<RobotTestWebCommand> _queuedWebCommands;

  struct RobotTestScript
  {
    std::string _name;
    bool        _wasUploaded = false;       // Was this script uploaded to persistent folder? (Otherwise found in resources)
    Json::Value _scriptJson;
  };

  struct comp
  {
    bool operator() (const std::string& lhs, const std::string& rhs) const
    {
      return Util::stricmp(lhs.c_str(), rhs.c_str()) < 0;
    }
  };

  using ScriptsMap = std::map<std::string, RobotTestScript, comp>;
  ScriptsMap                _scripts;

  struct cpuTime
  {
    int _usedTimeCounter;
    int _totalTimeCounter;
  };

  std::vector<cpuTime>      _prevCPUTime;
};

} // namespace Vector
} // namespace Anki


#endif // __Vector_Engine_RobotTest_H__
