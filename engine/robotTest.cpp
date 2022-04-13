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


#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "engine/aiComponent/aiComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviorComponent.h"
#include "engine/aiComponent/behaviorComponent/userIntentComponent.h"
#include "engine/cozmoContext.h"
#include "engine/perfMetricEngine.h"
#include "engine/robot.h"
#include "engine/robotManager.h"
#include "engine/robotTest.h"
#include "osState/osState.h"
#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/fileUtils/fileUtils.h"
#include "util/string/stringUtils.h"

#include "webServerProcess/src/webService.h"

#define LOG_CHANNEL "RobotTest"

namespace Anki {
namespace Vector {

const std::string RobotTest::kInactiveScriptName = "(NONE)";

#if ANKI_ROBOT_TEST_ENABLED

namespace
{
  RobotTest* s_RobotTest = nullptr;

  const char* kScriptCommandsKey = "scriptCommands";
  const char* kCommandKey = "command";
  const char* kParametersKey = "parameters";

  static const int kNumCPUStatLines = 5;

#if REMOTE_CONSOLE_ENABLED

  static const char* kConsoleGroup = "RobotTest";

  void Status(ConsoleFunctionContextRef context)
  {
    std::string response;
    s_RobotTest->ExecuteWebCommandStatus(&response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(Status, kConsoleGroup);

  void RunScript(ConsoleFunctionContextRef context)
  {
    const std::string& scriptName = ConsoleArg_Get_String(context, "scriptName");
    std::string response;
    s_RobotTest->ExecuteWebCommandRun(scriptName, &response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(RunScript, kConsoleGroup, const char* scriptName);

  void StopScript(ConsoleFunctionContextRef context)
  {
    std::string response;
    s_RobotTest->ExecuteWebCommandStop(&response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(StopScript, kConsoleGroup);

  void ListScripts(ConsoleFunctionContextRef context)
  {
    std::string response;
    s_RobotTest->ExecuteWebCommandListScripts(&response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(ListScripts, kConsoleGroup);

  void GetScript(ConsoleFunctionContextRef context)
  {
    const std::string& scriptName = ConsoleArg_Get_String(context, "scriptName");
    std::string response;
    s_RobotTest->ExecuteWebCommandGetScript(scriptName, &response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(GetScript, kConsoleGroup, const char* scriptName);

  void RefreshUploadedScripts(ConsoleFunctionContextRef context)
  {
    std::string response;
    s_RobotTest->ExecuteWebCommandRefreshUploadedScripts(&response);
    context->channel->WriteLog("%s", response.c_str());
  }
  CONSOLE_FUNC(RefreshUploadedScripts, kConsoleGroup);

#endif  // REMOTE_CONSOLE_ENABLED
}


static int RoboTestWebServerImpl(WebService::WebService::Request* request)
{
  auto* robotTest = static_cast<RobotTest*>(request->_cbdata);

  int returnCode = robotTest->ParseWebCommands(request->_param1);
  if (returnCode != 0)
  {
    // If there were no errors, attempt to execute the commands, and output
    // string messages/results so that they can be returned in the web request
    robotTest->ExecuteQueuedWebCommands(&request->_result);
  }

  return returnCode;
}

// Note that this can be called at any arbitrary time, from a webservice thread
static int RobotTestWebServerHandler(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);
  auto* robotTest = static_cast<RobotTest*>(cbdata);

  std::string commands;
  if (info->content_length > 0)
  {
    char buf[info->content_length + 1];
    mg_read(conn, buf, sizeof(buf));
    buf[info->content_length] = 0;
    commands = buf;
  }
  else if (info->query_string)
  {
    commands = info->query_string;
  }

  auto ws = robotTest->GetWebService();
  const int returnCode = ws->ProcessRequestExternal(conn, cbdata, RoboTestWebServerImpl, commands);

  return returnCode;
}


RobotTest::RobotTest(const CozmoContext* context)
  : _context(context)
{
}

RobotTest::~RobotTest()
{
  s_RobotTest = nullptr;
}

void RobotTest::Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService)
{
  s_RobotTest = this;

  _webService = webService;
  _webService->RegisterRequestHandler("/robottest", RobotTestWebServerHandler, this);

  _platform = dataPlatform;
  _uploadedScriptsPath = _platform->pathToResource(Util::Data::Scope::Persistent,
                                                   "robotTestScripts");
  if (!Util::FileUtils::CreateDirectory(_uploadedScriptsPath))
  {
    LOG_ERROR("RobotTest.Init", "Failed to create folder %s", _uploadedScriptsPath.c_str());
    return;
  }

  {
    // Find and load all scripts in the resources folder
    std::string scriptsPath = _platform->pathToResource(Util::Data::Scope::Resources,
                                                        "config/engine/robotTestFramework");
    static const bool kisUploadedScriptsFolder = false;
    LoadScripts(scriptsPath, kisUploadedScriptsFolder);
  }

  {
    // Find and load all uploaded scripts in the persistent folder
    static const bool kisUploadedScriptsFolder = true;
    LoadScripts(_uploadedScriptsPath, kisUploadedScriptsFolder);
  }

  _prevCPUTime.resize(kNumCPUStatLines);
}


// This is called near the start of the tick
void RobotTest::Update()
{
  ANKI_CPU_PROFILE("RobotTest::Update");

  while (_state == RobotTestState::RUNNING)
  {
    const bool commandCompleted = ExecuteScriptCommand(_nextScriptCommand);
    if (commandCompleted)
    {
      _curScriptCommandIndex++;
      FetchNextScriptCommand();
    }
    else
    {
      // If the command is not completed (e.g. waiting for a signal), we're done with this tick
      break;
    }
  }
}


// Parse commands out of the query string, and only if there are no errors,
// add them to the queue
int RobotTest::ParseWebCommands(std::string& queryString)
{
  queryString = Util::StringToLower(queryString);

  std::vector<RobotTestWebCommand> cmds;
  std::string current;

  while (!queryString.empty())
  {
    size_t amp = queryString.find('&');
    if (amp == std::string::npos)
    {
      current = queryString;
      queryString = "";
    }
    else
    {
      current = queryString.substr(0, amp);
      queryString = queryString.substr(amp + 1);
    }

    if (current == "stop")
    {
      RobotTestWebCommand cmd(WebCommandType::STOP);
      cmds.push_back(cmd);
    }
    else if (current == "status")
    {
      RobotTestWebCommand cmd(WebCommandType::STATUS);
      cmds.push_back(cmd);
    }
    else if (current == "listscripts")
    {
      RobotTestWebCommand cmd(WebCommandType::LIST_SCRIPTS);
      cmds.push_back(cmd);
    }
    else if (current == "refreshuploadedscripts")
    {
      RobotTestWebCommand cmd(WebCommandType::REFRESH_UPLOADED_SCRIPTS);
      cmds.push_back(cmd);
    }
    else
    {
      // Commands that have arguments:
      static const std::string cmdKeywordRun("run=");
      static const std::string cmdKeywordGetScript("getscript=");
      
      if (current.substr(0, cmdKeywordRun.size()) == cmdKeywordRun)
      {
        std::string argumentValue = current.substr(cmdKeywordRun.size());
        RobotTestWebCommand cmd(WebCommandType::RUN, argumentValue);
        cmds.push_back(cmd);
      }
      else if (current.substr(0, cmdKeywordGetScript.size()) == cmdKeywordGetScript)
      {
        std::string argumentValue = current.substr(cmdKeywordGetScript.size());
        RobotTestWebCommand cmd(WebCommandType::GET_SCRIPT, argumentValue);
        cmds.push_back(cmd);
      }
      else
      {
        LOG_ERROR("RobotTest.ParseWebCommands", "Error parsing robot test web command: %s", current.c_str());
        return 0;
      }
    }
  }

  // Now that there are no errors, add all parsed commands to queue
  for (auto& cmd : cmds)
  {
    _queuedWebCommands.push(cmd);
  }
  return 1;
}


void RobotTest::ExecuteQueuedWebCommands(std::string* resultStr)
{
  // Execute queued web commands
  while (!_queuedWebCommands.empty())
  {
    const RobotTestWebCommand cmd = _queuedWebCommands.front();
    _queuedWebCommands.pop();
    switch (cmd._webCommand)
    {
      case WebCommandType::RUN:
        ExecuteWebCommandRun(cmd._paramString, resultStr);
        break;
      case WebCommandType::STOP:
        ExecuteWebCommandStop(resultStr);
        break;
      case WebCommandType::STATUS:
        ExecuteWebCommandStatus(resultStr);
        break;
      case WebCommandType::LIST_SCRIPTS:
        ExecuteWebCommandListScripts(resultStr);
        break;
      case WebCommandType::GET_SCRIPT:
        ExecuteWebCommandGetScript(cmd._paramString, resultStr);
        break;
      case WebCommandType::REFRESH_UPLOADED_SCRIPTS:
        ExecuteWebCommandRefreshUploadedScripts(resultStr);
        break;
    }
  }
}


void RobotTest::ExecuteWebCommandRun(const std::string& scriptName, std::string* resultStr)
{
  const bool success = StartScript(scriptName);
  if (success)
  {
    *resultStr += "Started";
  }
  else
  {
    *resultStr += "Failed to start";
  }
  *resultStr += (" running script \"" + scriptName + "\"\n");
}


void RobotTest::ExecuteWebCommandStop(std::string* resultStr)
{
  StopScript();
  *resultStr += "No script running\n";
}


void RobotTest::ExecuteWebCommandStatus(std::string* resultStr)
{
  *resultStr += (_state == RobotTestState::INACTIVE ?
                 "Inactive" : "Running: " + _curScriptName);
  *resultStr += "\n";
}


void RobotTest::ExecuteWebCommandListScripts(std::string* resultStr)
{
  for (const auto& script : _scripts)
  {
    *resultStr += (script.second._wasUploaded ? "Uploaded: " : "Resource: ");
    *resultStr += (script.second._name + "\n");
  }
  *resultStr += (std::to_string(_scripts.size()) + " scripts total\n");
}


void RobotTest::ExecuteWebCommandGetScript(const std::string& scriptName, std::string* resultStr)
{
  const auto it = _scripts.find(scriptName);
  if (it == _scripts.end())
  {
    *resultStr += ("Script '" + scriptName + "' not found");
    return;
  }

  Json::StyledWriter writer;
  std::string stringifiedJSON;
  stringifiedJSON = writer.write(it->second._scriptJson);
  *resultStr += stringifiedJSON;
}


void RobotTest::ExecuteWebCommandRefreshUploadedScripts(std::string* resultStr)
{
  static const bool kisUploadedScriptsFolder = true;
  LoadScripts(_uploadedScriptsPath, kisUploadedScriptsFolder, resultStr);
}


void RobotTest::LoadScripts(const std::string& path, const bool isUploadedScriptsFolder,
                            std::string* resultStr)
{
  static const bool kUseFullPath = true;
  static const bool kRecurse = true;
  auto fileList = Util::FileUtils::FilesInDirectory(path, kUseFullPath, "json", kRecurse);
  int numValidScripts = 0;
  for (const auto& scriptFilePath : fileList)
  {
    Json::Value scriptJson;
    const bool success = _platform->readAsJson(scriptFilePath, scriptJson);
    if (!success)
    {
      LOG_ERROR("RobotTest.LoadScripts.ScriptLoadError",
                "Robot test script file %s failed to parse as JSON",
                scriptFilePath.c_str());
    }
    else
    {
      const bool isValid = ValidateScript(scriptJson);
      if (!isValid)
      {
        LOG_ERROR("RobotTest.LoadScripts.ScriptValidationError",
                  "Robot test script file %s is valid JSON but has one or more errors",
                  scriptFilePath.c_str());
      }
      else
      {
        static const bool kMustHaveExtension = true;
        static const bool kRemoveExtension = true;
        const std::string name = Util::FileUtils::GetFileName(scriptFilePath, kMustHaveExtension, kRemoveExtension);
        ScriptsMap::const_iterator it = _scripts.find(name);
        if (it != _scripts.end() && !isUploadedScriptsFolder)
        {
          LOG_ERROR("RobotTest.LoadScripts.DuplicateScriptName",
                    "Duplicate test script file name %s in resources; ignoring script with duplicate name",
                    scriptFilePath.c_str());
        }
        else
        {
          RobotTestScript script;
          script._name = name;
          script._wasUploaded = isUploadedScriptsFolder;
          script._scriptJson = scriptJson;
          _scripts[name] = script;
          numValidScripts++;
        }
      }
    }
  }

  LOG_INFO("RobotTest.LoadScripts",
           "Successfully loaded and validated %i robot test scripts out of %i found in %s folder",
           numValidScripts, static_cast<int>(fileList.size()),
           isUploadedScriptsFolder ? "persistent" : "resources");

  if (resultStr)
  {
    *resultStr += "Loaded " + std::to_string(numValidScripts) + " valid robot test scripts from persistent folder";
  }
}


bool RobotTest::ValidateScript(const Json::Value& scriptJson)
{
  bool valid = true;

  const auto perfMetric = _context->GetPerfMetric();

  if (!scriptJson.isMember(kScriptCommandsKey))
  {
    LOG_ERROR("RobotTest.ValidateScript", "Script missing 'scriptCommands'");
    return false;
  }

  const auto& commandsJson = scriptJson[kScriptCommandsKey];
  if (!commandsJson.isArray())
  {
    LOG_ERROR("RobotTest.ValidateScript", "'scriptCommands' must be an array");
    return false;
  }

  const auto numCommands = commandsJson.size();
  for (int i = 0; i < numCommands; i++)
  {
    const auto& commandJson = commandsJson[i];
    if (!commandJson.isMember(kCommandKey))
    {
      LOG_ERROR("RobotTest.ValidateScript",
                "Script command at index %i is missing 'command' key", i);
      valid = false;
      continue;
    }
    const auto& commandStr = commandJson[kCommandKey].asString();
    ScriptCommandType cmd;
    const bool success = StringToScriptCommand(commandStr, cmd);
    valid = valid && success;
    if (!success)
    {
      LOG_ERROR("RobotTest.ValidateScript",
                "'%s' at index %i is not a valid script command", commandStr.c_str(), i);
      continue;
    }

    switch (cmd)
    {
      case ScriptCommandType::EXIT:
        break;
      case ScriptCommandType::PERFMETRIC:
        {
          if (!commandJson.isMember(kParametersKey))
          {
            LOG_ERROR("RobotTest.ValidateScript",
                      "'perfMetric' script command at index %i is missing 'parameters' key", i);
            valid = false;
            continue;
          }
          const auto& paramsStr = commandJson[kParametersKey].asString();
          static const bool kQueueForExecution = false;
          const bool success = perfMetric->ParseCommands(paramsStr, kQueueForExecution);
          valid = valid && success;
          if (!success)
          {
            LOG_ERROR("RobotTest.ValidateScript",
                      "Error parsing 'perfMetric' script command parameters at index %i ('%s')",
                      i, paramsStr.c_str());
            continue;
          }
        }
        break;
      case ScriptCommandType::CLOUD_INTENT:
        break;
      case ScriptCommandType::WAIT_CLOUD_INTENT:
        break;
      case ScriptCommandType::WAIT_UNTIL_ENGINE_TICK_COUNT:
        break;
      case ScriptCommandType::WAIT_TICKS:
        break;
      case ScriptCommandType::WAIT_SECONDS:
        break;
      case ScriptCommandType::CPU_START:
        break;
      case ScriptCommandType::CPU_STOP:
        break;
    }
  }

  return valid;
}


bool RobotTest::StartScript(const std::string& scriptName)
{
  if (_state == RobotTestState::RUNNING)
  {
    StopScript();
  }
  const ScriptsMap::const_iterator it = _scripts.find(scriptName);
  if (it == _scripts.end())
  {
    LOG_INFO("RobotTest.StartScript", "Start requested for script %s but script not found",
             scriptName.c_str());
    return false;
  }

  LOG_INFO("RobotTest.StartScript", "Starting script %s", scriptName.c_str());
  _state = RobotTestState::RUNNING;
  _curScriptName = scriptName;
  _curScriptCommandsJson = &it->second._scriptJson[kScriptCommandsKey];
  _curScriptCommandIndex = 0;
  _waitTickCount = 0;
  _waitTimeToExpire = 0.0f;
  _waitingForCloudIntent = false;
  _cpuStartCommandExecuted = false;
  FetchNextScriptCommand();
  return true;
}


void RobotTest::StopScript()
{
  if (_state != RobotTestState::RUNNING)
  {
    LOG_INFO("RobotTest.StopScript", "Stop command given but no script was running");
    return;
  }
  LOG_INFO("RobotTest.StopScript", "Stopping script %s", _curScriptName.c_str());
  _state = RobotTestState::INACTIVE;
  _curScriptName = kInactiveScriptName;
  _curScriptCommandsJson = nullptr;
}


void RobotTest::FetchNextScriptCommand()
{
  if (_curScriptCommandIndex >= _curScriptCommandsJson->size())
  {
    // Script had no instructions, or we're pointing beyond the end of the script
    _nextScriptCommand = ScriptCommandType::EXIT;
  }
  else
  {
    const auto& commandJson = (*_curScriptCommandsJson)[_curScriptCommandIndex];
    const auto& commandStr = commandJson[kCommandKey].asString();
    const bool success = StringToScriptCommand(commandStr, _nextScriptCommand);
    if (!success)
    {
      // This should not happen once we start validating scripts when they are loaded
      LOG_ERROR("RobotText.FetchNextScriptCommand", "Error fetching next script command");
    }
  }
}


bool RobotTest::StringToScriptCommand(const std::string& commandStr, ScriptCommandType& command)
{
  // todo: probably convert the ScriptCommandType to a CLAD enum, so we get the conversion code automatically
  static const std::unordered_map<std::string, RobotTest::ScriptCommandType> stringToEnumMap =
  {
    {"exit", RobotTest::ScriptCommandType::EXIT},
    {"perfMetric", RobotTest::ScriptCommandType::PERFMETRIC},
    {"cloudIntent", RobotTest::ScriptCommandType::CLOUD_INTENT},
    {"waitCloudIntent", RobotTest::ScriptCommandType::WAIT_CLOUD_INTENT},
    {"waitUntilEngineTickCount", RobotTest::ScriptCommandType::WAIT_UNTIL_ENGINE_TICK_COUNT},
    {"waitTicks", RobotTest::ScriptCommandType::WAIT_TICKS},
    {"waitSeconds", RobotTest::ScriptCommandType::WAIT_SECONDS},
    {"cpuStart", RobotTest::ScriptCommandType::CPU_START},
    {"cpuStop", RobotTest::ScriptCommandType::CPU_STOP},
  };

  auto it = stringToEnumMap.find(commandStr);
  if (it == stringToEnumMap.end())
  {
    return false;
  }

  command = it->second;
  return true;
}


bool RobotTest::ExecuteScriptCommand(ScriptCommandType command)
{
  bool commandCompleted = true;

  switch (command)
  {
    case ScriptCommandType::EXIT:
    {
      // If we've gone past the last instruction, or we've
      // reached an exit command, it's time to stop
      StopScript();
      commandCompleted = false;
    }
    break;

    case ScriptCommandType::PERFMETRIC:
    {
      const auto& paramsStr = (*_curScriptCommandsJson)[_curScriptCommandIndex][kParametersKey].asString();
      const auto perfMetric = _context->GetPerfMetric();
      const bool success = perfMetric->ParseCommands(paramsStr);
      if (success)
      {
        perfMetric->ExecuteQueuedCommands();
      }
    }
    break;

    case ScriptCommandType::CLOUD_INTENT:
    {
      Robot* robot = _context->GetRobotManager()->GetRobot();
      auto& uic = robot->GetAIComponent().GetComponent<BehaviorComponent>().GetComponent<UserIntentComponent>();

      const auto& params = (*_curScriptCommandsJson)[_curScriptCommandIndex][kParametersKey];

      Json::StyledWriter writer;
      std::string stringifiedJSON;
      stringifiedJSON = writer.write(params);

      uic.SetCloudIntentPendingFromString(stringifiedJSON);

      _waitingForCloudIntent = true;
    }
    break;

    case ScriptCommandType::WAIT_CLOUD_INTENT:
    {
      if (_waitingForCloudIntent)
      {
        commandCompleted = false;
      }
    }
    break;

    case ScriptCommandType::WAIT_UNTIL_ENGINE_TICK_COUNT:
    {
      const auto curTickCount = BaseStationTimer::getInstance()->GetTickCount();
      if (_waitTickCount <= 0)
      {
        _waitTickCount = (*_curScriptCommandsJson)[_curScriptCommandIndex][kParametersKey].asInt();
      }
      commandCompleted = (curTickCount >= _waitTickCount);
    }
    break;

    case ScriptCommandType::WAIT_TICKS:
    {
      if (_waitTickCount <= 0)
      {
        _waitTickCount = (*_curScriptCommandsJson)[_curScriptCommandIndex][kParametersKey].asInt();
        commandCompleted = (_waitTickCount <= 0);
      }
      else
      {
        if (--_waitTickCount > 0)
        {
          commandCompleted = false;
        }
      }
    }
    break;

    case ScriptCommandType::WAIT_SECONDS:
    {
      const auto curTime = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
      if (_waitTimeToExpire == 0.0f)
      {
        const auto secondsToWait = (*_curScriptCommandsJson)[_curScriptCommandIndex][kParametersKey].asFloat();
        if (secondsToWait > 0.0f)
        {
          commandCompleted = false;
          _waitTimeToExpire = curTime + secondsToWait;
        }
      }
      else
      {
        if (curTime >= _waitTimeToExpire)
        {
          _waitTimeToExpire = 0.0f;
        }
        else
        {
          commandCompleted = false;
        }
      }
    }
    break;

    case ScriptCommandType::CPU_START:
    {
      _cpuStartCommandExecuted = true;
      static const bool kCalculateUsage = false;
      SampleCPU(kCalculateUsage);
    }
    break;

    case ScriptCommandType::CPU_STOP:
    {
      if (!_cpuStartCommandExecuted)
      {
        LOG_ERROR("RobotTest.ExecuteScriptCommand",
                  "Error: cpuStop script command attempted but there has been no cpuStart script command");
        StopScript();
        commandCompleted = false;
        break;
      }
      static const bool kCalculateUsage = true;
      SampleCPU(kCalculateUsage);
    }
    break;
  }

  return commandCompleted;
}


void RobotTest::SampleCPU(const bool calculateUsage)
{
  ANKI_CPU_PROFILE("RobotTest::SampleCPU");

  std::vector<std::string> cpuTimeStatsStrings;
  {
    ANKI_CPU_PROFILE("RobotTest::SampleCPUCallOS");
    // Request CPU time data from the OS; this gets five strings containing time data;
    // one is for overall CPU, and the other four are for each of the four cores
    const auto& osState = OSState::getInstance();
    osState->UpdateCPUTimeStats();
    osState->GetCPUTimeStats(cpuTimeStatsStrings);
    DEV_ASSERT_MSG(cpuTimeStatsStrings.size() >= kNumCPUStatLines, "RobotTest.SampleCPU",
                   "Insufficient number of cpu time stats lines (%i) returned from osState; should be %i",
                   static_cast<int>(cpuTimeStatsStrings.size()), kNumCPUStatLines);
  }

  for (int lineIndex = 0; lineIndex < kNumCPUStatLines; lineIndex++)
  {
    auto& line = cpuTimeStatsStrings[lineIndex];
    static const int kOffsetForCoreIndicator = 3;
    const char coreIndicator = line[kOffsetForCoreIndicator];
    const int infoIndex = (coreIndicator == ' ' ? 0 : (coreIndicator - '0') + 1);

    // Parse out the time values
    static const int kNumCPUTimeValues = 8;
    size_t indexInString = kOffsetForCoreIndicator + 2;  // Skip core indicator char and space char
    int totalTimeCounter = 0;
    std::array<int, kNumCPUTimeValues> times;
    for (int i = 0; i < kNumCPUTimeValues; i++)
    {
      line = line.substr(indexInString);
      const int val = std::stoi(line, &indexInString);
      times[i] = val;
      totalTimeCounter += val;
    }

    // Calculate idle time and used time
    const int idleTimeCounter = times[3] + times[4]; // 'idle' + 'iowait'
    const int usedTimeCounter = totalTimeCounter - idleTimeCounter;

    auto& prevCPUTime = _prevCPUTime[infoIndex];

    if (calculateUsage)
    {
      const int deltaUsedTime  = usedTimeCounter  - prevCPUTime._usedTimeCounter;
      const int deltaTotalTime = totalTimeCounter - prevCPUTime._totalTimeCounter;
      float usedPercent = 0.0f;
      if (deltaTotalTime > 0)
      {
        usedPercent = ((static_cast<float>(deltaUsedTime) * 100.0f) / static_cast<float>(deltaTotalTime));
      }
      LOG_INFO("RobotTest.SampleCPU", "CPU used = %.2f%% (%s)\n", usedPercent,
               lineIndex == 0 ? "Overall" : std::string("Core " + std::to_string(lineIndex - 1)).c_str());
    }

    prevCPUTime._usedTimeCounter = usedTimeCounter;
    prevCPUTime._totalTimeCounter = totalTimeCounter;
  }
}


#else  // ANKI_ROBOT_TEST_ENABLED

RobotTest::RobotTest(const CozmoContext* context) {}

RobotTest::~RobotTest() {}

void RobotTest::Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService)
{
}

void RobotTest::Update()
{
}

#endif  // (else) ANKI_ROBOT_TEST_ENABLED

} // namespace Vector
} // namespace Anki
