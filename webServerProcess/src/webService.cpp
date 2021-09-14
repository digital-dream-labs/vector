/**
 * File: webService.cpp
 *
 * Author: richard; adapted for Victor by Paul Terry 01/08/2018
 * Created: 4/17/2017
 *
 * Description: Provides interface to civetweb, an embedded web server
 *
 *
 * Copyright: Anki, Inc. 2017-2018
 *
 **/

#include "webService.h"

#if !defined(ANKI_NO_WEBSERVER_ENABLED)
  #define ANKI_NO_WEBSERVER_ENABLED 0
#endif

#if !ANKI_NO_WEBSERVER_ENABLED

#if USE_DAS
#include "DAS/DAS.h"
#endif

#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "util/logging/logging.h"
#include "util/console/consoleSystem.h"
#include "util/console/consoleChannel.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/dispatchQueue/dispatchQueue.h"
#include "util/global/globalDefinitions.h"
#include "util/helpers/ankiDefines.h"
#include "util/helpers/templateHelpers.h"
#include "util/string/stringUtils.h"

#include "osState/osState.h"

#if defined(ANKI_PLATFORM_ANDROID)
#include <android/log.h>
#endif

#include <cassert>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>

#define LOG_CHANNEL "WebService"

using namespace Anki::Vector;

namespace {
#ifndef SIMULATOR
  bool                     s_WaitingForProcessStatus = false;
  std::vector<std::string> s_ProcessStatuses;
  std::mutex               s_ProcessStatusMutex;
  std::condition_variable  s_ProcessStatusCondition;
#endif
  std::atomic_bool         s_ShuttingDown{false};
}

// Used websockets codes, see websocket RFC pg 29
// http://tools.ietf.org/html/rfc6455#section-5.2
enum {
  WebSocketsTypeText            = 0x1,
  WebSocketsTypeCloseConnection = 0x8
};

// 256KB to accommodate output of animation names
static const size_t kBigBufferSize = 256*1024;

class ExternalOnlyConsoleChannel : public Anki::Util::IConsoleChannel
{
public:

  ExternalOnlyConsoleChannel(char* outText, uint32_t outTextLength)
    : _outText(outText)
    , _outTextLength(outTextLength)
    , _outTextPos(0)
  {
    assert((_outText!=nullptr) && (_outTextLength > 0));

    _tempBuffer = new char[kTempBufferSize];
  }

  virtual ~ExternalOnlyConsoleChannel()
  {
    if (_outText != nullptr)
    {
      // insure out buffer is null terminated

      if (_outTextPos < _outTextLength)
      {
        _outText[_outTextPos] = 0;
      }
      else
      {
        _outText[_outTextLength - 1] = 0;
      }
    }

    Anki::Util::SafeDeleteArray( _tempBuffer );
  }

  bool IsOpen() override { return true; }

  int WriteData(uint8_t *buffer, int len) override
  {
    assert(0); // not implemented (and doesn't seem to ever be called?)
    return len;
  }

  int WriteLogv(const char *format, va_list args) override
  {
    // Print to a temporary buffer first so we can use that for any required logs
    const int printRetVal = vsnprintf(_tempBuffer, kTempBufferSize, format, args);

    if (printRetVal > 0)
    {
      if ((_outText != nullptr) && (_outTextLength > _outTextPos))
      {
        const uint32_t remainingRoom = _outTextLength - _outTextPos;
        // new line is implicit in all log calls
        const int outPrintRetVal = snprintf(&_outText[_outTextPos], remainingRoom, "%s\n", _tempBuffer);
        // note outPrintRetVal can be >remainingRoom (snprintf returns size required, not size used)
        // so this can make _outTextPos > _outTextLength, but this is ok as we only use it when < _outTextLength
        _outTextPos += (outPrintRetVal > 0) ? outPrintRetVal : 0;
      }
    }

    return printRetVal;
  }

  int WriteLog(const char *format, ...) override
  {
    va_list ap;
    va_start(ap, format);
    int result = WriteLogv(format, ap);
    va_end(ap);
    return result;
  }

  bool Flush() override
  {
    // already flushed
    return true;
  }

  void SetTTYLoggingEnabled(bool newVal) override
  {
  }

  bool IsTTYLoggingEnabled() const override
  {
    return true;
  }

  const char* GetChannelName() const override { return nullptr; }
  void SetChannelName(const char* newName) override {}

private:

  static const size_t kTempBufferSize = kBigBufferSize;

  char*     _tempBuffer;
  char*     _outText;
  uint32_t  _outTextLength;
  uint32_t  _outTextPos;
};

static int
LogMessage(const struct mg_connection *conn, const char *message)
{
  LOG_INFO("WebService.LogMessage", "%s", message);
  return 1;
}

static int
LogHandler(struct mg_connection *conn, void *cbdata)
{
#if USE_DAS
  // Stop rolling over logs so they are viewable
  // (otherwise, they get uploaded and then deleted pretty quickly)
  DASDisableNetwork(DASDisableNetworkReason_LogRollover);
#endif

  // pretend we didn't handle it and pass onto the default handler
  return 0;
}


void ExecCommand(const std::vector<std::string>& args)
{
  LOG_INFO("WebService.ExecCommand", "Called with cmd: %s (and %i arguments)",
           args[0].c_str(), (int)(args.size() - 1));

  pid_t pID = fork();
  if (pID == 0) // child
  {
    char* argv_child[args.size() + 1];

    for (size_t i = 0; i < args.size(); i++) {
      argv_child[i] = (char *) malloc(args[i].size() + 1);
      strcpy(argv_child[i], args[i].c_str());
    }
    argv_child[args.size()] = nullptr;

    execv(argv_child[0], argv_child);

    // We'll only get here if execv fails
    for (size_t i = 0 ; i < args.size() + 1 ; ++i) {
      free(argv_child[i]);
    }
    exit(0);
  }
  else if (pID < 0) // fail
  {
    LOG_INFO("Webservice.ExecCommand.FailedFork", "Failed fork!");
  }
  else  // parent
  {
    // We don't wait for child to complete or do anything special
  }
}


static int
ProcessRequest(struct mg_connection *conn, WebService::WebService::RequestType requestType,
               const std::string& param1, const std::string& param2, const std::string& param3 = "",
               bool waitAndSendResponse = true, WebService::WebService::ExternalCallback extCallback = nullptr,
               void* cbdata = nullptr)
{
  if (s_ShuttingDown)
  {
    return 1;
  }

  WebService::WebService::Request* requestPtr = new WebService::WebService::Request(requestType, param1, param2,
                                                                                    param3, extCallback, cbdata);

  struct mg_context *ctx = mg_get_context(conn);
  WebService::WebService* that = static_cast<WebService::WebService*>(mg_get_user_data(ctx));

  that->AddRequest(requestPtr);

  if (waitAndSendResponse)
  {
    // Wait until the main thread processes the request
    using namespace std::chrono;
    static const long long kTimeoutDuration_s = 10;

    {
      std::unique_lock<std::mutex> lk{requestPtr->_readyMutex};
      requestPtr->_readyCondition.wait_for(lk,
                                           seconds(kTimeoutDuration_s),
                                           [requestPtr]{ return requestPtr->_resultReady; });
    }

    if (!requestPtr->_resultReady)
    {
      std::lock_guard<std::mutex> lock(that->_requestMutex);
      requestPtr->_result = "Timed out after " + std::to_string(kTimeoutDuration_s) + " seconds";
    }

    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
              "close\r\n\r\n");
    mg_printf(conn, "%s\n", requestPtr->_result.c_str());

    // Now mark the request as done so the main thread can delete it.
    // if you pass !waitAndSendResponse, you need to manually set this flag
    requestPtr->_done = true;
  }

  return 1;
}

static int
ConsoleVarsUI(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);
  std::string category = ((info->query_string) ? info->query_string : "");
  std::string standalone = "standalone";
  if (category == "embedded")
  {
    category = "";
    standalone = "";
  }

  const int returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleVarsUI, category, standalone);

  return returnCode;
}

static int
ConsoleVarSet(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);

  std::string query;

  if (info->content_length > 0) {
    char buf[info->content_length+1];
    mg_read(conn, buf, sizeof(buf));
    buf[info->content_length] = 0;
    query = buf;
  }
  else if (info->query_string) {
    query = info->query_string;
  }

  const int returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleVarSet, query, "");

  return returnCode;
}

static int
ConsoleVarGet(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);

  std::string key;

  if (info->query_string) {
    if (!strncmp(info->query_string, "key=", 4)) {
      key = std::string(info->query_string+4);
    }
  }

  const int returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleVarGet, key, "");

  return returnCode;
}

static int
ConsoleVarList(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);

  std::string key;

  if (info->query_string) {
    if (!strncmp(info->query_string, "key=", 4)) {
      key = std::string(info->query_string + 4);
    }
  }

  const int returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleVarList, key, "");

  return returnCode;
}

static int
ConsoleFuncList(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);

  std::string key;

  if (info->query_string) {
    if (!strncmp(info->query_string, "key=", 4)) {
      key = std::string(info->query_string + 4);
    }
  }

  const int returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleFuncList, key, "");

  return returnCode;
}

static int
ConsoleFuncCall(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);

  std::string func;
  std::string args;
  if (info->content_length > 0) {
    char buf[info->content_length+1];
    mg_read(conn, buf, sizeof(buf));
    buf[info->content_length] = 0;
    func = buf;
  }
  else if (info->query_string) {
    func = info->query_string;
  }

  int returnCode = 1;

  if (func.substr(0,5) == "func=") {
    size_t amp = func.find('&');
    if (amp == std::string::npos) {
      func = func.substr(5);
    }
    else {
      args = func.substr(amp+6);  // skip over "args="
      func = func.substr(5, amp-5);

      // unescape '+' => ' ',  except if it follows a '\', in which case keep the '+' and drop the '\'

      size_t plus = args.find("+");
      while (plus != std::string::npos) {
        if( (plus > 0) && (args[plus-1] == '\\') ) {
          args.erase(plus-1, 1);
          --plus;
        } else {
          args = args.replace(plus, 1, " ");
        }
        plus = args.find("+", plus+1);
      }

      // unescape '\"' => '"'

      size_t quote = args.find("\\\"");
      while (quote != std::string::npos) {
        args = args.replace(quote, 2, "\"");
        quote = args.find("\\\"", quote+2);
      }
    }

    returnCode = ProcessRequest(conn, WebService::WebService::RequestType::RT_ConsoleFuncCall, func, args);
  }

  return returnCode;
}

static int
ProcessRequestFromQueryString(struct mg_connection *conn, void *cbdata, WebService::WebService::RequestType type)
{
  const mg_request_info* info = mg_get_request_info(conn);
  std::string request;
  if (info->content_length > 0) {
    char buf[info->content_length+1];
    mg_read(conn, buf, sizeof(buf));
    buf[info->content_length] = 0;
    request = buf;
  }
  else if (info->query_string) {
    request = info->query_string;
  }
  int returnCode = ProcessRequest(conn, type, request, "");

  return returnCode;
}

static int
TempAppToEngine(struct mg_connection *conn, void *cbdata)
{
  return ProcessRequestFromQueryString( conn,
                                        cbdata,
                                        WebService::WebService::RequestType::RT_TempAppToEngine );
}

static int
TempEngineToApp(struct mg_connection *conn, void *cbdata)
{
  return ProcessRequestFromQueryString( conn,
                                        cbdata,
                                        WebService::WebService::RequestType::RT_TempEngineToApp );
}

WebService::WebService::Request::Request(RequestType rt,
                                         const std::string& param1,
                                         const std::string& param2,
                                         const std::string& param3,
                                         ExternalCallback extCallback,
                                         void* cbdata)
{
  _requestType = rt;
  _param1 = param1;
  _param2 = param2;
  _param3 = param3;
  _externalCallback = extCallback;
  _cbdata = cbdata,
  _result = "";
  _resultReady = false;
  _done = false;
}
WebService::WebService::Request::Request(RequestType rt, const std::string& param1, const std::string& param2)
  : Request(rt, param1, param2, "", nullptr, nullptr)
{
}

static int
dasinfo(struct mg_connection *conn, void *cbdata)
{
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");

// NOTE:  For some reason, android builds of the webserver are not getting USE_DAS defined properly
#if USE_DAS

  std::string dasString = "DAS: " + std::string(DASGetLogDir()) + " DASDisableNetworkReason:";
  int disabled = DASGetNetworkingDisabled();
  if (disabled & DASDisableNetworkReason_Simulator) {
    dasString += " Simulator";
  }
  if (disabled & DASDisableNetworkReason_UserOptOut) {
    dasString += " UserOptOut";
  }
  if (disabled & DASDisableNetworkReason_Shutdown) {
    dasString += " Shutdown";
  }
  if (disabled & DASDisableNetworkReason_LogRollover) {
    dasString += " LogRollover";
  }
//  if (disabled & DASDisableNetworkReason_Debug) {
//    dasString += " Debug";
//  }
#else
  std::string dasString = "DAS: #undefined for this platform";
#endif

  mg_printf(conn, "%s", dasString.c_str());
  return 1;
}


static int GetInitialConfig(struct mg_connection *conn, void *cbdata)
{
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");

  struct mg_context *ctx = mg_get_context(conn);
  WebService::WebService* that = static_cast<WebService::WebService*>(mg_get_user_data(ctx));

  const std::string& title0    = that->GetConfig()["title0"].asString();
  const std::string& title1    = that->GetConfig()["title1"].asString();
  const std::string& startPage = that->GetConfig()["startPage"].asString();
#ifdef SIMULATOR
  const std::string& webotsSim = "true";
#else
  const std::string& webotsSim = "false";
#endif
  const std::string& allowPerfPage        = that->GetConfig()["allowPerfPage"].asString();
  const std::string& whichWebServer       = std::to_string(that->GetConfig()["whichWebServer"].asInt());
  const std::string& allowConsoleVarsPage = that->GetConfig()["allowConsoleVarsPage"].asString();
  const std::string& allowPerfMetricPage  = that->GetConfig()["allowPerfMetricPage"].asString();
  const int tickBudget_ms                 = that->GetConfig()["tickBudget_ms"].asInt();

  mg_printf(conn, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%d\n", title0.c_str(), title1.c_str(),
            startPage.c_str(), webotsSim.c_str(), allowPerfPage.c_str(),
            whichWebServer.c_str(), allowConsoleVarsPage.c_str(), allowPerfMetricPage.c_str(),
            tickBudget_ms);
  return 1;
}


static int GetMainRobotInfo(struct mg_connection *conn, void *cbdata)
{
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");

  const auto& osState = OSState::getInstance();
  const std::string serialNo       = osState->GetSerialNumberAsString();
  const std::string ip             = osState->GetIPAddress();
  const std::string robotName      = osState->GetRobotName();
  const std::string osBuildVersion = osState->GetOSBuildVersion();
  const std::string sha            = osState->GetBuildSha();
  const std::string MACAddress     = osState->GetMACAddress();
  const std::string SSID           = osState->GetSSID();

  const std::string buildConfig =
#if defined(NDEBUG)
  "RELEASE";
#else
  "DEBUG";
#endif

#ifdef SIMULATOR

  const std::string procVersion = "n/a (webots)";
  const std::string procCmdLine = "n/a (webots)";

#else

  // This is a one-time read of info that won't change during the run,
  // so we don't keep any file streams open
  std::ifstream fs;

  fs.open("/proc/version", std::ifstream::in);
  std::string procVersion;
  std::getline(fs, procVersion);
  fs.close();

  fs.open("/proc/cmdline", std::ifstream::in);
  std::string procCmdLine;
  std::getline(fs, procCmdLine);
  fs.close();

#endif

  mg_printf(conn, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
            serialNo.c_str(), ip.c_str(),
            buildConfig.c_str(),
            procVersion.c_str(), procCmdLine.c_str(),
            robotName.c_str(), osBuildVersion.c_str(), sha.c_str(),
            MACAddress.c_str(), SSID.c_str());
  return 1;
}


static int GetPerfStats(struct mg_connection *conn, void *cbdata)
{
  if (s_ShuttingDown)
  {
    return 1;
  }

  using namespace std::chrono;
  const auto startTime = steady_clock::now();

  enum {
    kStat_CpuFreq,
    kStat_Temperature,
    kStat_Uptime,
    kStat_IdleTime,
    kStat_RealTimeClock,
    kStat_MemoryInfo1,
    kStat_MemoryInfo2,
    kStat_OverallCpu,
    kStat_Cpu0,
    kStat_Cpu1,
    kStat_Cpu2,
    kStat_Cpu3,
    kStat_UserDiskSpace,
    kNumStats
  };

  bool active[kNumStats];

  const mg_request_info* info = mg_get_request_info(conn);
  const std::string boolsString = info->query_string ? info->query_string : "";
  int i = 0;
  for ( ; i < kNumStats && boolsString[i]; i++)
  {
    active[i] = (boolsString[i] == '1');
  }
  // If the string wasn't long enough, make the rest of the flags false
  for ( ; i < kNumStats; i++)
  {
    active[i] = false;
  }

  const auto& osState = OSState::getInstance();

  std::string stat_cpuFreq;
  if (active[kStat_CpuFreq]) {
    // Update cpu freq
    const uint32_t cpuFreq_kHz = osState->GetCPUFreq_kHz();
    stat_cpuFreq = std::to_string(cpuFreq_kHz);
  }

  std::string stat_temperature;
  if (active[kStat_Temperature]) {
    // Update temperature reading (Celsius)
    const uint32_t cpuTemp_C = osState->GetTemperature_C();
    stat_temperature = std::to_string(cpuTemp_C);
  }

  std::string stat_uptime;
  std::string stat_idleTime;
  if (active[kStat_Uptime] || active[kStat_IdleTime]) {
    // Up time/idle time
    float idleTime = 0.0f;
    const float uptime = osState->GetUptimeAndIdleTime(idleTime);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << uptime;
    stat_uptime = ss.str();
    std::stringstream ss2;
    ss2 << std::fixed << std::setprecision(2) << idleTime;
    stat_idleTime = ss2.str();
  }

  std::string stat_rtc;
  if (active[kStat_RealTimeClock]) {
    // Date/time on robot
    const auto now = system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    stat_rtc = ss.str();
  }

  std::string stat_mem1;
  std::string stat_mem2;
  if (active[kStat_MemoryInfo1] || active[kStat_MemoryInfo2]) {
    OSState::MemoryInfo info;
    osState->GetMemoryInfo(info);
    if (active[kStat_MemoryInfo1]) {
      // Memory use 1
      stat_mem1 = std::to_string(info.totalMem_kB) + "," + std::to_string(info.freeMem_kB);
    }
    if (active[kStat_MemoryInfo2]) {
      // Memory use 2
      stat_mem2 = std::to_string(info.totalMem_kB) + "," + std::to_string(info.availMem_kB);
    }
  }

  std::vector<std::string> stat_cpuStat;
  if (active[kStat_OverallCpu] ||
      active[kStat_Cpu0] || active[kStat_Cpu1] ||
      active[kStat_Cpu2] || active[kStat_Cpu3]) {
    // CPU time stats
    osState->GetCPUTimeStats(stat_cpuStat);
  }

  static constexpr size_t kNumCPUTimeStats = 5;
  if (stat_cpuStat.size() < kNumCPUTimeStats) {
    stat_cpuStat.resize(kNumCPUTimeStats);
  }

  std::string stat_userDiskSpace;
  if (active[kStat_UserDiskSpace]) {
#ifdef ANKI_PLATFORM_VICOS
    OSState::DiskInfo info;
    const bool success = osState->GetDiskInfo("/data", info);
    if (success) {
      stat_userDiskSpace = std::to_string(info.total_kB) + "," + std::to_string(info.avail_kB);
    }
    else {
      stat_userDiskSpace = "1,0";
    }
#else
    stat_userDiskSpace = "1,0"; // Not really applicable to webots
#endif
  }

  const auto now = steady_clock::now();
  const auto elapsed_us = duration_cast<microseconds>(now - startTime).count();
  LOG_INFO("WebService.Perf", "GetPerfStats took %lld microseconds to read", elapsed_us);

  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");

  mg_printf(conn, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
            stat_cpuFreq.c_str(),
            stat_temperature.c_str(),
            stat_uptime.c_str(),
            stat_idleTime.c_str(),
            stat_rtc.c_str(),
            stat_mem1.c_str(),
            stat_mem2.c_str());
  mg_printf(conn, "%s\n%s\n%s\n%s\n%s\n%s\n",
            stat_cpuStat[0].c_str(),
            stat_cpuStat[1].c_str(),
            stat_cpuStat[2].c_str(),
            stat_cpuStat[3].c_str(),
            stat_cpuStat[4].c_str(),
            stat_userDiskSpace.c_str());

  return 1;
}


#ifndef SIMULATOR

static int SystemCtl(struct mg_connection *conn, void *cbdata)
{
  using namespace std::chrono;
  const auto startTime = steady_clock::now();

  const mg_request_info* info = mg_get_request_info(conn);
  const std::string query = info->query_string ? info->query_string : "";
  if (query.substr(0, 5) == "proc=")
  {
    const size_t amp = query.find('&');
    if (amp != std::string::npos)
    {
      const std::string action = query.substr(amp + 1);
      const std::string procName = query.substr(5, amp - 5);

      std::vector<std::string> args;
      args.push_back("/bin/systemctl");
      args.push_back(action);
      args.push_back(procName);

      ExecCommand(args);

      const auto now = steady_clock::now();
      const auto elapsed_us = duration_cast<microseconds>(now - startTime).count();
      LOG_INFO("WebService.Systemctl", "SystemCtl took %lld microseconds", elapsed_us);
    }
  }
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");
  mg_printf(conn, "\n");

  return 1;
}


static int GetProcessStatus(struct mg_connection *conn, void *cbdata)
{
  if (s_ShuttingDown)
  {
    return 1;
  }

  std::string resultsString;

  using namespace std::chrono;
  const auto startTime = steady_clock::now();

  const mg_request_info* info = mg_get_request_info(conn);
  const std::string query = info->query_string ? info->query_string : "";
  if (query.substr(0, 5) == "proc=")
  {
    struct mg_context* ctx = mg_get_context(conn);
    Anki::Vector::WebService::WebService* that = static_cast<Anki::Vector::WebService::WebService*>(mg_get_user_data(ctx));

    std::string remainder = query.substr(5);
    std::vector<std::string> args;

    args.push_back("/bin/sh");
    args.push_back("/anki/bin/vic-getprocessstatus.sh");
    args.push_back(that->GetConfig()["port"].asString());

    // Loop and pull out all requested process names, separated by ampersands
    while (!remainder.empty()) {
      const size_t amp = remainder.find('&');
      if (amp != std::string::npos) {
        args.push_back(remainder.substr(0, amp));
        remainder = remainder.substr(amp + 1);
      }
      else {
        args.push_back(remainder);
        break;
      }
    }

    ExecCommand(args);

    static const long long kTimeoutDuration_s = 10;
    {
      std::unique_lock<std::mutex> lk{s_ProcessStatusMutex};
      s_WaitingForProcessStatus = true;
      s_ProcessStatusCondition.wait_for(lk,
                                        seconds(kTimeoutDuration_s),
                                        []{ return !s_WaitingForProcessStatus; });

      if (s_WaitingForProcessStatus)
      {
        LOG_INFO("WebService.GetProcessStatus", "GetProcessStatus timed out after %lld seconds",
                 kTimeoutDuration_s);
      }

      bool firstDone = false;
      for (const auto& result : s_ProcessStatuses) {
        if (firstDone) {
          resultsString += "\n";
        }
        resultsString += result;
        firstDone = true;
      }
    }
  }

  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");
  mg_printf(conn, "%s", resultsString.c_str());

  const auto now = steady_clock::now();
  const auto elapsed_us = duration_cast<microseconds>(now - startTime).count();
  LOG_INFO("WebService.GetProcessStatus.Time", "GetProcessStatus took %lld microseconds", elapsed_us);

  return 1;
}


static int ProcessStatus(struct mg_connection *conn, void *cbdata)
{
  const mg_request_info* info = mg_get_request_info(conn);
  std::string results = info->query_string ? info->query_string : "";

  {
    std::unique_lock<std::mutex> lk{s_ProcessStatusMutex};

    s_ProcessStatuses.clear();
    while (!results.empty()) {
      const size_t amp = results.find('&');
      if (amp != std::string::npos) {
        s_ProcessStatuses.push_back(results.substr(0, amp));
        results = results.substr(amp + 1);
      }
      else {
        s_ProcessStatuses.push_back(results);
        break;
      }
    }

    // Notify the requesting thread that the result is now ready
    s_WaitingForProcessStatus = false;
  }
  s_ProcessStatusCondition.notify_all();

  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
            "close\r\n\r\n");
  mg_printf(conn, "\n");

  return 1;
}

#endif  // #ifndef SIMULATOR


namespace Anki {
namespace Vector {
namespace WebService {

WebService::WebService()
: _ctx(nullptr)
{
}

WebService::~WebService()
{
  Stop();
}

void WebService::Start(Anki::Util::Data::DataPlatform* platform, const Json::Value& config)
{
  if (platform == nullptr) {
    return;
  }
  if (_ctx != nullptr) {
    return;
  }

  _config = config;
  _platform = platform;

  const std::string portNumString = _config["port"].asString();

  const std::string webserverPath = platform->pathToResource(Util::Data::Scope::Resources, "webserver");

  std::string rewrite;
  rewrite += "/persistent=" + platform->pathToResource(Util::Data::Scope::Persistent, "");
  rewrite += ",";
  rewrite += "/resources=" + platform->pathToResource(Util::Data::Scope::Resources, "");
  rewrite += ",";
  rewrite += "/cache=" + platform->pathToResource(Util::Data::Scope::Cache, "");
  rewrite += ",";
  rewrite += "/currentgamelog=" + platform->pathToResource(Util::Data::Scope::CurrentGameLog, "");
#if USE_DAS
  rewrite += ",";
  rewrite += "/daslog=" + std::string(DASGetLogDir());
#endif

//https://ankiinc.atlassian.net/browse/VIC-1554
//  const std::string& passwordFile = platform->pathToResource(Util::Data::Scope::Resources, "webserver/htpasswd");

  const char *options[] = {
    "document_root",
    webserverPath.c_str(),
    "listening_ports",
    portNumString.c_str(),
    "num_threads",
    "4",
    "url_rewrite_patterns",
    rewrite.c_str(),
    "static_file_max_age",
    "0",
//https://ankiinc.atlassian.net/browse/VIC-1554
//    "put_delete_auth_file",
//    passwordFile.c_str(),
//    "authentication_domain",
//    "vector.anki.com",
    "websocket_timeout_ms",
    "3600000", // 1 hour
//https://ankiinc.atlassian.net/browse/VIC-1554
//#if defined(NDEBUG)
//    "global_auth_file",
//    passwordFile.c_str(),
//#endif
    0
  };

  struct mg_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.log_message = LogMessage;

  _ctx = mg_start(&callbacks, this, options);

  mg_set_websocket_handler(_ctx,
                           "/socket",
                           HandleWebSocketsConnect,
                           HandleWebSocketsReady,
                           HandleWebSocketsData,
                           HandleWebSocketsClose,
                           0);

  mg_set_request_handler(_ctx, "/daslog", LogHandler, 0);
  mg_set_request_handler(_ctx, "/consolevars", ConsoleVarsUI, 0);

  mg_set_request_handler(_ctx, "/consolevarset", ConsoleVarSet, 0);
  mg_set_request_handler(_ctx, "/consolevarget", ConsoleVarGet, 0);
  mg_set_request_handler(_ctx, "/consolevarlist", ConsoleVarList, 0);
  mg_set_request_handler(_ctx, "/consolefunclist", ConsoleFuncList, 0);
  mg_set_request_handler(_ctx, "/consolefunccall", ConsoleFuncCall, 0);

  mg_set_request_handler(_ctx, "/dasinfo", dasinfo, 0);
  mg_set_request_handler(_ctx, "/getinitialconfig", GetInitialConfig, 0);
  mg_set_request_handler(_ctx, "/getmainrobotinfo", GetMainRobotInfo, 0);
  mg_set_request_handler(_ctx, "/getperfstats", GetPerfStats, 0);
#ifndef SIMULATOR
  mg_set_request_handler(_ctx, "/systemctl", SystemCtl, 0);
  mg_set_request_handler(_ctx, "/getprocessstatus", GetProcessStatus, 0);
  mg_set_request_handler(_ctx, "/processstatus", ProcessStatus, 0);
#endif


  // todo (VIC-1398): remove
  if( ANKI_DEV_CHEATS ) {
    mg_set_request_handler(_ctx, "/sendAppMessage", TempAppToEngine, 0);
    mg_set_request_handler(_ctx, "/getAppMessages", TempEngineToApp, 0);
  }

  const std::string& consoleVarsTemplate = platform->pathToResource(Util::Data::Scope::Resources, "webserver/consolevarsui.html");
  _consoleVarsUIHTMLTemplate = Anki::Util::StringFromContentsOfFile(consoleVarsTemplate);

  _requests.clear();

  _dispatchQueue = Util::Dispatch::Create("WebsocketSender");
}


// This is called from the main thread
void WebService::Update()
{
  ANKI_CPU_PROFILE("WebService::Update");

  std::lock_guard<std::mutex> lock(_requestMutex);

  // First pass:  Delete any completely-finished requests from the list (and delete the requests themselves)
  size_t destIndex = 0;
  for (size_t srcIndex = 0; srcIndex < _requests.size(); srcIndex++)
  {
    Request* requestPtr = _requests[srcIndex];
    if (requestPtr->_done)
    {
      delete requestPtr;
    }
    else
    {
      if (srcIndex != destIndex)
      {
        _requests[destIndex] = _requests[srcIndex];
      }
      destIndex++;
    }
  }
  _requests.resize(destIndex);

  Anki::Util::ConsoleSystem& consoleSystem = Anki::Util::ConsoleSystem::Instance();

  // Second pass:  Process any requests that haven't been processed yet
  for (const auto requestPtr : _requests)
  {
    if (!requestPtr->_resultReady)
    {
      switch (requestPtr->_requestType)
      {
        case RT_ConsoleVarsUI:
          {
            const bool standalone = (requestPtr->_param2 == "standalone");
            GenerateConsoleVarsUI(requestPtr->_result, requestPtr->_param1, standalone);
          }
          break;
        case RT_ConsoleVarGet:
          {
            const std::string& key = requestPtr->_param1;

            if (!key.empty()) {
              const Anki::Util::IConsoleVariable* consoleVar = consoleSystem.FindVariable(key.c_str());
              if (consoleVar) {
                requestPtr->_result = consoleVar->ToString() + "<br>";
              }
              else {
                requestPtr->_result = "Variable not found<br>";
              }
            }
            else {
              requestPtr->_result = "Key required (name of variable)<br>";
            }
          }
          break;
        case RT_ConsoleVarSet:
          {
            std::string key = requestPtr->_param1;

            while (!key.empty()) {
              if (key.substr(0, 4) == "key=") {
                std::string value;
                std::string remainder;

                size_t amp = key.find('&');
                if (amp != std::string::npos) {
                  value = key.substr(amp+7);
                  key = key.substr(4, amp-4);
                  size_t amp = value.find('&');
                  if (amp != std::string::npos) {
                    remainder = value.substr(amp+1);
                    value = value.substr(0, amp);
                  }
                }

                Anki::Util::IConsoleVariable* consoleVar = consoleSystem.FindVariable(key.c_str());
                if (consoleVar) {
                  if (consoleVar->ParseText(value.c_str() )) {
                    // success
                    LOG_INFO("WebService", "CONSOLE_VAR %s %s", key.c_str(), value.c_str());
                    requestPtr->_result += consoleVar->ToString() + "<br>";
                  }
                  else {
                    requestPtr->_result += "Error setting variable "+key+"="+value+"<br>";
                  }
                }
                else {
                  requestPtr->_result += "Variable not found "+key+"<br>";
                }

                key = remainder;
              } else {
                break;
              }
            }
          }
          break;
        case RT_ConsoleVarList:
          {
            const std::string& key = requestPtr->_param1;
            const auto keyLen = key.length();

            const Anki::Util::ConsoleSystem::VariableDatabase& varDatabase = consoleSystem.GetVariableDatabase();
            for (Anki::Util::ConsoleSystem::VariableDatabase::const_iterator it = varDatabase.begin();
                 it != varDatabase.end(); ++it)
            {
              std::string label = it->second->GetID();
              if (keyLen == 0 || Anki::Util::StringCaseInsensitiveEquals(label.substr(0, keyLen), key))
              {
                requestPtr->_result += label.c_str();
                requestPtr->_result += "<br>\n";
              }
            }
          }
          break;
        case RT_ConsoleFuncList:
          {
            const std::string& key = requestPtr->_param1;
            const auto keyLen = key.length();

            const Anki::Util::ConsoleSystem::FunctionDatabase& funcDatabase = consoleSystem.GetFunctionDatabase();
            for (Anki::Util::ConsoleSystem::FunctionDatabase::const_iterator it = funcDatabase.begin();
                 it != funcDatabase.end(); ++it)
            {
              std::string label = it->second->GetID();
              if (keyLen == 0 || Anki::Util::StringCaseInsensitiveEquals(label.substr(0, keyLen), key))
              {
                requestPtr->_result += label.c_str();
                requestPtr->_result += "<br>\n";
              }
            }
          }
          break;
        case RT_ConsoleFuncCall:
          {
            const std::string& func = requestPtr->_param1;
            const std::string& args = requestPtr->_param2;

            Anki::Util::IConsoleFunction* consoleFunc = consoleSystem.FindFunction(func.c_str());
            if (consoleFunc) {
              char outText[kBigBufferSize + 1] = {0};
              uint32_t outTextLength = sizeof(outText);

              ExternalOnlyConsoleChannel consoleChannel(outText, outTextLength);

              bool success = consoleSystem.ParseConsoleFunctionCall(consoleFunc, args.c_str(), consoleChannel);
              if (success) {
                LOG_INFO("WebService.FuncCallSuccess", "CONSOLE_FUNC %s %s success", func.c_str(), args.c_str());
                requestPtr->_result += outText;
              }
              else {
                LOG_INFO("WebService.FuncCallFailure", "CONSOLE_FUNC %s %s failed %s", func.c_str(), args.c_str(), outText);
                requestPtr->_result += outText;
              }
            }
            else {
              LOG_INFO("WebService.FuncCallNotFound", "CONSOLE_FUNC %s %s not found", func.c_str(), args.c_str());
            }
          }
          break;
        case RT_External:
          {
            // Call out to the external update handler
            DEV_ASSERT(requestPtr->_externalCallback != nullptr, "Expecting valid externalCallback pointer");
            const int returnCode = requestPtr->_externalCallback(requestPtr);
            if (returnCode == 0) {
              LOG_INFO("WebService.Update", "External callback failed");
            }
          }
          break;
        case RT_TempAppToEngine:
          {
            _appToEngineOnData.emit( requestPtr->_param1 );
          }
          break;
        case RT_TempEngineToApp:
          {
            requestPtr->_result = _appToEngineRequestData.emit();
          }
          break;
        case RT_WebsocketOnSubscribe:
        case RT_WebsocketOnData:
          {
            const auto& moduleName = requestPtr->_param1;
            const auto& idxStr = requestPtr->_param2;
            const size_t idx = std::stoi( idxStr );

            auto sendToClient = [idx, moduleName, this](const Json::Value& toSend){
              std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);
              if( (idx < _webSocketConnections.size())
                 && (_webSocketConnections[idx].subscribedModules.count( moduleName ) > 0) )
              {
                Json::Value payload;
                payload["module"] = moduleName;
                payload["data"] = toSend;
                SendToWebSocket( _webSocketConnections[idx].conn, payload );
              }
            };

            if( requestPtr->_requestType == RT_WebsocketOnSubscribe ) {
              auto signalIt = _webVizSubscribedSignals.find( moduleName );
              if( signalIt != _webVizSubscribedSignals.end() ) {
                signalIt->second.emit( sendToClient );
              }
            } else if( requestPtr->_requestType == RT_WebsocketOnData ) {
              const auto& dataStr = requestPtr->_param3;
              Json::Reader reader;
              Json::Value data;

              if( reader.parse(dataStr, data) ) {
                auto signalIt = _webVizDataSignals.find( moduleName );
                if( signalIt != _webVizDataSignals.end() ) {
                  signalIt->second.emit( data, sendToClient );
                }
              }
            }
            requestPtr->_done = true; // no one cares about the result, just cleanup immediately
          }
          break;
      }

      // Notify the requesting thread that the result is now ready
      {
        std::unique_lock<std::mutex> lk{requestPtr->_readyMutex};
        requestPtr->_resultReady = true;
      }
      requestPtr->_readyCondition.notify_all();
    }
  }
}

void WebService::Stop()
{
  s_ShuttingDown = true;
  if (_ctx)
  {
    // Call update to process any pending request(s) and wake up the
    // thread(s) that are waiting for those request(s) to be processed.
    // This will allow the mg_stop call below to not take forever waiting
    // for threads to shut down.
    Update();

#ifndef SIMULATOR
    // Notify any pending thread that's waiting for process status, so that
    // the mg_stop call below will not hang waiting for it
    {
      std::unique_lock<std::mutex> lk{s_ProcessStatusMutex};
      s_ProcessStatuses.clear();
      s_WaitingForProcessStatus = false;
    }
    s_ProcessStatusCondition.notify_all();
#endif

#ifdef VICOS
    // shutdown nicely on the robot but let the OS handle it for the simulator, mg_stop triggers
    // the thread sanitizer and execution stops here, by removing this line in SIMULATOR builds
    // it allows the thread sanitizier to continue to do useful work.
    mg_stop(_ctx);
#endif
    _ctx = nullptr;
  }
}


void WebService::AddRequest(Request* requestPtr)
{
  std::lock_guard<std::mutex> lock(_requestMutex);
  _requests.push_back(requestPtr);
}


void WebService::RegisterRequestHandler(std::string uri, mg_request_handler handler, void* cbdata)
{
  mg_set_request_handler(_ctx, uri.c_str(), handler, cbdata);
}

int WebService::ProcessRequestExternal(struct mg_connection* conn, void* cbdata,
                                       ExternalCallback extCallback, const std::string& param1,
                                       const std::string& param2, const std::string& param3)
{
  // This is a request coming from an 'external' handler that wants WebService
  // to process the request at the end of the tick (in WebService::Update)
  static const bool waitAndSendResponse = true;
  return ProcessRequest(conn, WebService::WebService::RequestType::RT_External,
                        param1, param2, param3, waitAndSendResponse, extCallback,
                        cbdata);
}

static std::string sanitize_tag(const std::string& tag)
{
  std::string sanitizedTag = tag;
  for(int i = 0; i < sanitizedTag.size(); ++i) {
    if (!isalpha(sanitizedTag[i])) {
      sanitizedTag[i] = '_';
    }
  }
  return sanitizedTag;
}

void WebService::GenerateConsoleVarsUI(std::string& page, const std::string& category,
                                       const bool standalone)
{
  ANKI_CPU_PROFILE("GenerateConsoleVarsUI");

  std::string styleSheetIncludes;
  std::string jqueryIncludes;
  std::string style;
  std::string script;
  std::string html;
  std::map<std::string, std::string> category_html;

  const Anki::Util::ConsoleSystem& consoleSystem = Anki::Util::ConsoleSystem::Instance();
  
  if (standalone)
  {
    styleSheetIncludes += "<link rel=\"stylesheet\" href=\"jquery-ui.css\">\n";
    styleSheetIncludes += "<link rel=\"stylesheet\" href=\"style.css\">\n";
    jqueryIncludes += "<script src=\"jquery-1.12.4.js\"></script>\n";
    jqueryIncludes += "<script src=\"jquery-ui.js\"></script>\n";
  }

  // Variables

  const Anki::Util::ConsoleSystem::VariableDatabase& varDatabase = consoleSystem.GetVariableDatabase();
  for (Anki::Util::ConsoleSystem::VariableDatabase::const_iterator it = varDatabase.begin();
       it != varDatabase.end();
       ++it) {
    std::string cat = it->second->GetCategory();
    if (!category.empty() && category != cat) {
      continue;
    }

    std::string label = it->second->GetID();

    if (it->second->IsToggleable()) {
      category_html[cat] += "                <div>\n";
      category_html[cat] += "                    <label for=\""+label+"\">"+label+"</label>\n";
      const std::string checked = (it->second->GetAsInt64()) ? " checked" : "";
      category_html[cat] += "                    <input type=\"checkbox\" name=\""+label+"\" id=\""+label+"\" onclick=\"onCheckboxClickHandler(this)\""+checked+">\n";
      category_html[cat] += "                </div>\n";
      category_html[cat] += "                <br>\n";
    }
    else if (it->second->IsEnumType()) {
      category_html[cat] += "                <div>\n";
      category_html[cat] += "                    <label for=\""+label+"\">"+label+"</label>\n";
      category_html[cat] += "                    <select name=\""+label+"\" id=\""+label+"\" class=\"listbox\">\n";
      const auto& values = it->second->EnumValues();
      s64 currentValue = it->second->GetAsInt64();
      for(const auto& item : values) {
        const std::string selected = (currentValue == 0) ? "selected=\"selected\"" : "";
        --currentValue;
        category_html[cat] += "                        <option "+selected+">"+item+"</option>\n";
      }
      category_html[cat] += "                    </select>\n";
      category_html[cat] += "                </div>\n";
      category_html[cat] += "                <br>\n";
    }
    else {
      char sliderRange[200];
      char inputRange[200];

      if (it->second->IsIntegerType()) {
        if (it->second->IsSignedType()) {
          snprintf(sliderRange, sizeof(sliderRange),
                   "data-value=\"%lld\" data-begin=\"%lld\" data-end=\"%lld\" data-scale=\"1\"",
                   it->second->GetAsInt64(),
                   it->second->GetMinAsInt64(),
                   it->second->GetMaxAsInt64());

          snprintf(inputRange, sizeof(inputRange),
                   "min=\"%lld\" max=\"%lld\"",
                   it->second->GetMinAsInt64(),
                   it->second->GetMaxAsInt64());
        }
        else {
          snprintf(sliderRange, sizeof(sliderRange),
                   "data-value=\"%llu\" data-begin=\"%llu\" data-end=\"%llu\" data-scale=\"1\"",
                   it->second->GetAsUInt64(),
                   it->second->GetMinAsUInt64(),
                   it->second->GetMaxAsUInt64());

          snprintf(inputRange, sizeof(inputRange),
                   "min=\"%llu\" max=\"%llu\"",
                   it->second->GetMinAsUInt64(),
                   it->second->GetMaxAsUInt64());
        }
      }
      else {
        snprintf(sliderRange, sizeof(sliderRange),
                 "data-value=\"%g\" data-begin=\"%g\" data-end=\"%g\" data-scale=\"100.0\"",
                 it->second->GetAsDouble(),
                 it->second->GetMinAsDouble(),
                 it->second->GetMaxAsDouble());

        snprintf(inputRange, sizeof(inputRange),
                 "min=\"%g\" max=\"%g\"",
                 it->second->GetMinAsDouble(),
                 it->second->GetMaxAsDouble());
      }

      category_html[cat] += "                <div>\n";
      category_html[cat] += "                  <label for=\""+label+"_amount\">"+label+":</label>\n";
      category_html[cat] += "                  <div id=\""+label+"\" class=\"slider\" "+sliderRange+" style=\"width: 100px; margin: 0.25em;\"></div>\n";
      category_html[cat] += "                  <input type=\"text\" id=\""+label+"_amount\" class=\"amount\" "+inputRange+" style=\"margin: 0.25em; border:1; font-weight:bold;\">\n";
      category_html[cat] += "                </div><br>\n";
    }
  }

  // Functions

  const Anki::Util::ConsoleSystem::FunctionDatabase& funcDatabase = consoleSystem.GetFunctionDatabase();
  for (Anki::Util::ConsoleSystem::FunctionDatabase::const_iterator it = funcDatabase.begin();
       it != funcDatabase.end();
       ++it) {
    std::string cat = it->second->GetCategory();
    if (!category.empty() && category != cat) {
      continue;
    }

    std::string label = it->second->GetID();
    std::string sig = it->second->GetSignature();

    if (sig.empty()) {
      category_html[cat] += "                <div>\n";
      category_html[cat] += "                  <input type=\"submit\" value=\""+label+"\" class=\"function\">\n";
      category_html[cat] += "                </div><br>\n";
    }
    else {
      category_html[cat] += "                <div>\n";
      category_html[cat] += "                  <a id=\"tt\" title=\"("+sig+")\"><label for=\""+label+"_function\">"+label+":</label></a>\n";
      category_html[cat] += "                  <input type=\"text\" id=\""+label+"_args\" value=\"\" style=\"margin: 0.25em; border:1; font-weight:bold;\">\n";
      category_html[cat] += "                  <input type=\"submit\" id=\""+label+"_function\" value=\"Call\" class=\"function\">\n";
      category_html[cat] += "                </div><br>\n";
    }

  }

  // merge all categories, categories without subcategories

  std::map<std::string, std::string> category_merged_html;
  for (std::map<std::string, std::string>::const_iterator it = category_html.begin();
       it != category_html.end();
       ++it) {
    std::string cat = it->first;
    size_t dot = cat.find(".");
    if (dot == std::string::npos) {
      category_merged_html[cat] += it->second;
    }
  }

  // then, append subcategories within <fieldset></fieldset>
  for (std::map<std::string, std::string>::const_iterator it = category_html.begin();
       it != category_html.end();
       ++it) {
    std::string cat = it->first;
    size_t dot = cat.find(".");
    if (dot != std::string::npos) {
      std::string group = cat.substr(dot+1);
      cat = cat.substr(0, dot);

      category_merged_html[cat] += "                <fieldset>\n";
      category_merged_html[cat] += "                    <legend>"+group+"</legend>\n";
      category_merged_html[cat] += it->second;
      category_merged_html[cat] += "                </fieldset>\n";
    }
  }

  // finally, generate tabs
  html += "<div id=\"tabs\">\n";
  html += "    <ul>\n";
  for (std::map<std::string, std::string>::const_iterator it = category_merged_html.begin();
    it != category_merged_html.end();
    ++it) {
    html += "        <li><a href=\"#tabs-"+sanitize_tag(it->first)+"\">"+it->first+"</a></li>\n";
  }
  html += "    </ul>\n";

  for (std::map<std::string, std::string>::const_iterator it = category_merged_html.begin();
  it != category_merged_html.end();
  ++it) {
    std::string tag = it->first;
    html += "    <div id=\"tabs-"+sanitize_tag(it->first)+"\">\n";
    html += "    " + it->second+"\n";
    html += "    </div>\n";
  }
  html += "</div>\n";

  page = getConsoleVarsTemplate();

  std::string tmp;
  size_t pos;

  tmp = "/* -- generated stylesheet includes -- */";
  pos = page.find(tmp);
  if (pos != std::string::npos) {
    page = page.replace(pos, tmp.length(), styleSheetIncludes);
  }

  tmp = "/* -- generated jquery includes -- */";
  pos = page.find(tmp);
  if (pos != std::string::npos) {
    page = page.replace(pos, tmp.length(), jqueryIncludes);
  }

  tmp = "/* -- generated style -- */";
  pos = page.find(tmp);
  if (pos != std::string::npos) {
    page = page.replace(pos, tmp.length(), style);
  }

  tmp = "// -- generated script --";
  pos = page.find(tmp);
  if (pos != std::string::npos) {
    page = page.replace(pos, tmp.length(), script);
  }

  tmp = "<!-- generated html -->";
  pos = page.find(tmp);
  if (pos != std::string::npos) {
    page = page.replace(pos, tmp.length(), html);
  }
}


void WebService::SendToWebSockets(const std::string& moduleName, const Json::Value& data) const
{
  std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);
  Json::Value payload;
  bool hasAssigned = false; // don't copy payload unless there is >= 1 client for this module
  for( const auto& connData : _webSocketConnections ) {
    if( connData.subscribedModules.find( moduleName ) != connData.subscribedModules.end() ) {
      if( !hasAssigned ) {
        payload["module"] = moduleName;
        payload["data"] = data;
        hasAssigned = true;
      }
      SendToWebSocket(connData.conn, payload);
    }
  }
}

bool WebService::IsWebVizClientSubscribed(const std::string& moduleName) const
{
  std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);
  for( const auto& connData : _webSocketConnections ) {
    if( (connData.subscribedModules.find( moduleName ) != connData.subscribedModules.end())
        || (moduleName.empty() && !connData.subscribedModules.empty()) ) // any module subscribed
    {
      return true;
    }
  }
  return false;
}

int WebService::HandleWebSocketsConnect(const struct mg_connection* conn, void* cbparams)
{
  return 0; // proceed with connection
}

void WebService::HandleWebSocketsReady(struct mg_connection* conn, void* cbparams)
{
  struct mg_context* ctx = mg_get_context(conn);
  Anki::Vector::WebService::WebService* that = static_cast<Anki::Vector::WebService::WebService*>(mg_get_user_data(ctx));
  DEV_ASSERT(that != nullptr, "Expecting valid webservice this pointer");
  that->OnOpenWebSocket( conn );
}

int WebService::HandleWebSocketsData(struct mg_connection* conn, int bits, char* data, size_t dataLen, void* cbparams)
{
  int ret = 1; // keep open

  // lower 4 bits
  const int opcode = bits & 0xF;

  // see websocket RFC §5.2 http://tools.ietf.org/html/rfc6455
  switch (opcode)
  {
    case WebSocketsTypeText:
    {
      if( (dataLen >= 2) && (data[0] == '{') ) {
        struct mg_context* ctx = mg_get_context(conn);
        Anki::Vector::WebService::WebService* that = static_cast<Anki::Vector::WebService::WebService*>(mg_get_user_data(ctx));
        DEV_ASSERT(that != nullptr, "Expecting valid webservice this pointer");

        Json::Reader reader;
        Json::Value payload;
        bool success = reader.parse(data, payload);
        if( success ) {
          that->OnReceiveWebSocket( conn, payload );
        }
      }
    }
      break;

    case WebSocketsTypeCloseConnection:
    {
      // agree to close connection, but don't do anything here until the close event fires
      ret = 0;
    }
      break;

    default:
      break;
  }

  return ret;
}

void WebService::HandleWebSocketsClose(const struct mg_connection* conn, void* cbparams)
{
  struct mg_context* ctx = mg_get_context(conn);
  Anki::Vector::WebService::WebService* that = static_cast<Anki::Vector::WebService::WebService*>(mg_get_user_data(ctx));
  DEV_ASSERT(that != nullptr, "Expecting valid webservice this pointer");
  that->OnCloseWebSocket( conn );
}


// This is always called in the main thread (whether we're sending or receiving)
void WebService::SendToWebSocket(struct mg_connection* conn, const Json::Value& data) const
{
  // Dispatch work onto another thread (note we copy 'data' by value here)
  Util::Dispatch::Async(_dispatchQueue, [conn, data] {
    std::stringstream ss;
    ss << data;
    const std::string& str = ss.str();

    mg_websocket_write(conn, WebSocketsTypeText, str.c_str(), str.size());
  });
}


const std::string& WebService::getConsoleVarsTemplate()
{
  return _consoleVarsUIHTMLTemplate;
}

void WebService::OnOpenWebSocket(struct mg_connection* conn)
{
  ASSERT_NAMED(conn != nullptr, "Can't create connection to n");
  // add a connection to the list that applies to all services
  std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);
  _webSocketConnections.push_back({});
  _webSocketConnections.back().conn = conn;
}

void WebService::OnReceiveWebSocket(struct mg_connection* conn, const Json::Value& data)
{
  std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);

  // find connection
  auto it = std::find_if( _webSocketConnections.begin(), _webSocketConnections.end(), [&conn](const auto& perConnData) {
    return perConnData.conn == conn;
  });

  if( it != _webSocketConnections.end() ) {
    if( !data["type"].isNull() && !data["module"].isNull() ) {
      const std::string& moduleName = data["module"].asString();
      const size_t idx = it - _webSocketConnections.begin();

      if( data["type"].asString() == "subscribe" ) {
        it->subscribedModules.insert( moduleName );

        const bool waitAndSendResponse = false;
        ProcessRequest(conn,
                       WebService::WebService::RequestType::RT_WebsocketOnSubscribe,
                       moduleName,
                       std::to_string(idx),
                       "",
                       waitAndSendResponse);
      }
      else if( data["type"].asString() == "unsubscribe" ) {
        it->subscribedModules.erase( moduleName );
      }
      else if( (data["type"].asString() == "data") && !data["data"].isNull() ) {
        const bool waitAndSendResponse = false;
        std::stringstream ss;
        ss << data["data"];
        ProcessRequest(conn,
                       WebService::WebService::RequestType::RT_WebsocketOnData,
                       moduleName,
                       std::to_string(idx),
                       ss.str(),
                       waitAndSendResponse);
      }

    } else if( !data["keepalive"].isNull() ) {
      Json::Value response;
      response["keepalive"] = true;
      SendToWebSocket( it->conn, response );
    }
  } else {
    std::stringstream ss;
    ss << data;
    LOG_ERROR("Webservice.OnReceiveWebSocket", "No connection for data %s", ss.str().c_str());
  }
}

void WebService::OnCloseWebSocket(const struct mg_connection* conn)
{
  std::lock_guard<std::mutex> lock(s_wsConnectionsMutex);
  // find connection
  auto it = std::find_if( _webSocketConnections.begin(), _webSocketConnections.end(), [&conn](const auto& perConnData) {
    return perConnData.conn == conn;
  });

  // erase it
  auto& data = *it;
  std::swap(data, _webSocketConnections.back());
  _webSocketConnections.pop_back();
}

} // namespace WebService
} // namespace Vector
} // namespace Anki

#else

namespace Anki {
namespace Vector {
namespace WebService {

  WebService::WebService()
  {
  }

  WebService::~WebService()
  {
  }

  void WebService::Start(Anki::Util::Data::DataPlatform* /*platform*/, const Json::Value& /*config*/)
  {
  }

  void WebService::Update()
  {
  }

  void WebService::Stop()
  {
  }

  void WebService::SendToWebSockets(const std::string& /*moduleName*/, const Json::Value& /*data*/) const
  {
  }

  bool WebService::IsWebVizClientSubscribed(const std::string& /*moduleName*/) const
  {
    return false;
  }

  void WebService::RegisterRequestHandler(std::string /*uri*/, mg_request_handler /*handler*/, void* /*cbdata*/)
  {
  }

  int WebService::ProcessRequestExternal(struct mg_connection* /*conn*/, void* /*cbdata*/,
                                         ExternalCallback /*extCallback*/, const std::string& /*param1*/,
                                         const std::string& /*param2*/, const std::string& /*param3*/)
  {
    return 1;
  }

} // namespace WebService
} // namespace Vector
} // namespace Anki

#endif // ANKI_NO_WEBSERVER_ENABLED
