/*
 * File:          cozmoEngine.cpp
 * Date:          12/23/2014
 *
 * Description:   (See header file.)
 *
 * Author: Andrew Stein / Kevin Yoon
 *
 * Modifications:
 */

#include "coretech/common/engine/opencvThreading.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "engine/ankiEventUtil.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/components/mics/micComponent.h"
#include "engine/components/mics/micDirectionHistory.h"
#include "engine/components/movementComponent.h"
#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/sensors/proxSensorComponent.h"
#include "engine/components/sensors/touchSensorComponent.h"
#include "engine/components/visionComponent.h"
#include "engine/cozmoAPI/comms/uiMessageHandler.h"
#include "engine/cozmoContext.h"
#include "engine/cozmoEngine.h"
#include "engine/debug/cladLoggerProvider.h"
#include "engine/events/ankiEvent.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/factory/factoryTestLogger.h"
#include "engine/perfMetricEngine.h"
#include "engine/robot.h"
#include "engine/robotDataLoader.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/robotManager.h"
#include "engine/robotTest.h"
#include "engine/utils/cozmoExperiments.h"
#include "engine/utils/parsingConstants/parsingConstants.h"
#include "engine/viz/vizManager.h"
#include "osState/wallTime.h"
#include "engine/cozmoAPI/comms/protoMessageHandler.h"
#include "webServerProcess/src/webService.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "osState/osState.h"

#include "clad/externalInterface/messageGameToEngine.h"

#include "platform/common/diagnosticDefines.h"

#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/global/globalDefinitions.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"
#include "util/logging/DAS.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/logging/multiLoggerProvider.h"
#include "util/time/universalTime.h"
#include "util/environment/locale.h"
#include "util/transport/connectionStats.h"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <time.h>

#if USE_DAS
#include <DAS/DAS.h>
#include <DAS/DASPlatform.h>
#endif

#include "engine/animations/animationTransfer.h"

#define LOG_CHANNEL "CozmoEngine"

#define MIN_NUM_FACTORY_TEST_LOGS_FOR_ARCHIVING 100
#define NUM_OPENCV_THREADS 0

// Local state variables
namespace {

  // How often do we attempt connection to robot/anim process?
  constexpr auto connectInterval = std::chrono::seconds(1);

  // When did we last try connecting?
  auto lastConnectAttempt = std::chrono::steady_clock::time_point();

}

namespace Anki {
namespace Vector {


static int GetEngineStatsWebServerImpl(WebService::WebService::Request* request)
{
  auto* cozmoEngine = static_cast<CozmoEngine*>(request->_cbdata);
  if (cozmoEngine->GetEngineState() != EngineState::Running)
  {
    LOG_INFO("CozmoEngine.GetEngineStatsWebServerImpl.NotReady", "GetEngineStatsWebServerImpl called but engine not running");
    return 0;
  }

  const auto robot = cozmoEngine->GetRobot();

  const auto& batteryComponent = robot->GetBatteryComponent();
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << batteryComponent.GetBatteryVolts() << '\n';
  ss << std::fixed << std::setprecision(3) << batteryComponent.GetBatteryVoltsRaw() << '\n';
  ss << std::fixed << std::setprecision(3) << batteryComponent.GetChargerVoltsRaw() << '\n';
  ss << EnumToString(batteryComponent.GetBatteryLevel()) << '\n';
  ss << std::to_string(static_cast<int>(batteryComponent.GetBatteryTemperature_C())) << '\n';
  ss << (batteryComponent.IsCharging() ? "true" : "false") << '\n';
  ss << (batteryComponent.IsOnChargerContacts() ? "true" : "false") << '\n';
  ss << (batteryComponent.IsOnChargerPlatform() ? "true" : "false") << '\n';
  ss << std::to_string(static_cast<int>(batteryComponent.GetTimeAtLevelSec(BatteryLevel::Full))) << '\n';
  ss << std::to_string(static_cast<int>(batteryComponent.GetTimeAtLevelSec(BatteryLevel::Low))) << '\n';

  const auto& robotState = robot->GetRobotState();

  ss << EnumToString(robot->GetOffTreadsState()) << '\n';
  ss << std::fixed << std::setprecision(1) << RAD_TO_DEG(robotState.poseAngle_rad) << '\n';
  ss << std::fixed << std::setprecision(1) << RAD_TO_DEG(robotState.posePitch_rad) << '\n';
  ss << std::fixed << std::setprecision(1) << RAD_TO_DEG(robotState.headAngle_rad) << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.liftHeight_mm << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.leftWheelSpeed_mmps << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.rightWheelSpeed_mmps << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.accel.x << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.accel.y << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.accel.z << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.gyro.x << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.gyro.y << '\n';
  ss << std::fixed << std::setprecision(3) << robotState.gyro.z << '\n';

  const auto& touchSensorComponent = robot->GetTouchSensorComponent();
  ss << touchSensorComponent.GetLatestRawTouchValue() << '\n';

  const auto& cliffSensorComponent = robot->GetCliffSensorComponent();
  const auto& cliffDataRaw = cliffSensorComponent.GetCliffDataRaw();
  ss << cliffDataRaw[0] << '\n';
  ss << cliffDataRaw[1] << '\n';
  ss << cliffDataRaw[2] << '\n';
  ss << cliffDataRaw[3] << '\n';

  ss << cliffSensorComponent.IsWhiteDetected(static_cast<CliffSensor>(0)) << ' '
     << cliffSensorComponent.IsWhiteDetected(static_cast<CliffSensor>(1)) << ' '
     << cliffSensorComponent.IsWhiteDetected(static_cast<CliffSensor>(2)) << ' '
     << cliffSensorComponent.IsWhiteDetected(static_cast<CliffSensor>(3)) << '\n';

  ss << robot->GetProxSensorComponent().GetDebugString() << '\n';

  ss << robotState.carryingObjectID << '\n';
  ss << robotState.carryingObjectOnTopID << '\n';
  ss << robotState.headTrackingObjectID << '\n';
  ss << robotState.localizedToObjectID << '\n';
  ss << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << robotState.status << '\n';
  ss << std::dec;

  const auto& micDirectionHistory = robot->GetMicComponent().GetMicDirectionHistory();
  ss << micDirectionHistory.GetRecentDirection() << '\n';
  ss << micDirectionHistory.GetSelectedDirection() << '\n';

  const auto& visionComp = robot->GetVisionComponent();
  const TimeStamp_t framePeriod_ms = visionComp.GetFramePeriod_ms();
  const TimeStamp_t procPeriod_ms = visionComp.GetProcessingPeriod_ms();
  
  ss << std::fixed << std::setprecision(3) << 1.f / Util::MilliSecToSec((f32)framePeriod_ms) << '\n';
  ss << std::fixed << std::setprecision(3) << 1.f / Util::MilliSecToSec((f32)procPeriod_ms) << '\n';

  request->_result = ss.str();

  return 1;
}


// Note that this can be called at any arbitrary time, from a webservice thread
static int GetEngineStatsWebServerHandler(struct mg_connection *conn, void *cbdata)
{
  // We ignore the query string because overhead is minimal

  auto* cozmoEngine = static_cast<CozmoEngine*>(cbdata);
  if (cozmoEngine->GetEngineState() != EngineState::Running)
  {
    LOG_INFO("CozmoEngine.GetEngineStatsWebServerHandler.NotReady", "GetEngineStatsWebServerHandler called but engine not running");
    return 0;
  }

  auto ws = cozmoEngine->GetRobot()->GetContext()->GetWebService();
  const int returnCode = ws->ProcessRequestExternal(conn, cbdata, GetEngineStatsWebServerImpl);

  return returnCode;
}


CozmoEngine::CozmoEngine(Util::Data::DataPlatform* dataPlatform)
  : _uiMsgHandler(new UiMessageHandler(1))
  , _protoMsgHandler(new ProtoMessageHandler)
  , _context(new CozmoContext(dataPlatform, _uiMsgHandler.get(), _protoMsgHandler.get()))
  , _animationTransferHandler(new AnimationTransfer(_uiMsgHandler.get(), dataPlatform))
{
#if ANKI_CPU_PROFILER_ENABLED
  // Initialize CPU profiler early and put tracing file at known location with no dependencies on other systems
  Anki::Util::CpuProfiler::GetInstance();
  Anki::Util::CpuThreadProfiler::SetChromeTracingFile(
      dataPlatform->pathToResource(Util::Data::Scope::Cache, "vic-engine-tracing.json").c_str());
  Anki::Util::CpuThreadProfiler::SendToWebVizCallback([&](const Json::Value& json) { _context->GetWebService()->SendToWebViz("cpuprofile", json); });
#endif

  DEV_ASSERT(_context->GetExternalInterface() != nullptr, "Cozmo.Engine.ExternalInterface.nullptr");
  if (Anki::Util::gTickTimeProvider == nullptr) {
    Anki::Util::gTickTimeProvider = BaseStationTimer::getInstance();
  }

  // Designate this thread as the one from which the engine can broadcast messages
  _context->SetEngineThread();

  DASMSG(engine_language_locale, "engine.language_locale", "Prints out the language locale of the robot");
  DASMSG_SET(s1, _context->GetLocale()->GetLocaleString().c_str(), "Locale on start up");
  DASMSG_SEND();

  auto helper = MakeAnkiEventUtil(*_context->GetExternalInterface(), *this, _signalHandles);

  using namespace ExternalInterface;
  helper.SubscribeGameToEngine<MessageGameToEngineTag::ImageRequest>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::RedirectViz>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::StartTestMode>();

  auto handler = [this] (const std::vector<Util::AnkiLab::AssignmentDef>& assignments) {
    _context->GetExperiments()->UpdateLabAssignments(assignments);
  };
  _signalHandles.emplace_back(_context->GetExperiments()->GetAnkiLab()
                              .ActiveAssignmentsUpdatedSignal().ScopedSubscribe(handler));

  _debugConsoleManager.Init(_context->GetExternalInterface(), _context->GetRobotManager()->GetMsgHandler());
  _dasToSdkHandler.Init(_context->GetExternalInterface());
  InitUnityLogger();
}

CozmoEngine::~CozmoEngine()
{
  _engineState = EngineState::ShuttingDown;
  _context->GetWebService()->Stop();

  if (Anki::Util::gTickTimeProvider == BaseStationTimer::getInstance()) {
    Anki::Util::gTickTimeProvider = nullptr;
  }
  BaseStationTimer::removeInstance();

  _context->GetVizManager()->Disconnect();
  _context->Shutdown();
}

Result CozmoEngine::Init(const Json::Value& config) {

  if (_isInitialized) {
    LOG_INFO("CozmoEngine.Init.ReInit", "Reinitializing already-initialized CozmoEngineImpl with new config.");
  }

  _isInitialized = false;


  auto * osState = OSState::getInstance();
  DEV_ASSERT(osState != nullptr, "CozmoEngine.Init.InvalidOSState");

  // set cpu frequency to default (in case we left it in a bad state last time)
  osState->SetDesiredCPUFrequency(DesiredCPUFrequency::Automatic);

  // engine checks the temperature of the OS now.
  // The fluctuation in the temperature is not expected to be fast
  // hence the 5 second update period (to prevent excessive file i/o)
  osState->SetUpdatePeriod(5000);
  osState->SendToWebVizCallback([&](const Json::Value& json) { _context->GetWebService()->SendToWebViz("cpu", json); });

  _config = config;

  if(!_config.isMember(AnkiUtil::kP_ADVERTISING_HOST_IP)) {
    PRINT_NAMED_ERROR("CozmoEngine.Init", "No AdvertisingHostIP defined in Json config.");
    return RESULT_FAIL;
  }

  if(!_config.isMember(AnkiUtil::kP_UI_ADVERTISING_PORT)) {
    PRINT_NAMED_ERROR("CozmoEngine.Init", "No UiAdvertisingPort defined in Json config.");
    return RESULT_FAIL;
  }

  Result lastResult = _uiMsgHandler->Init(_context.get(), _config);
  if (RESULT_OK != lastResult)
  {
    PRINT_NAMED_ERROR("CozmoEngine.Init","Error initializing UiMessageHandler");
    return lastResult;
  }

  lastResult = _protoMsgHandler->Init(_context.get(), _config);
  if (RESULT_OK != lastResult)
  {
    PRINT_NAMED_ERROR("CozmoEngine.Init","Error initializing ProtoMessageHandler");
    return lastResult;
  }

  // Disable Viz entirely on shipping builds
  if(ANKI_DEV_CHEATS)
  {
    if (nullptr != _context->GetExternalInterface())
    {
      // Have VizManager subscribe to the events it should care about
      _context->GetVizManager()->SubscribeToEngineEvents(*_context->GetExternalInterface());
    }
  }

  lastResult = InitInternal();
  if(lastResult != RESULT_OK) {
    PRINT_NAMED_ERROR("CozmoEngine.Init", "Failed calling internal init.");
    return lastResult;
  }

  _context->GetDataLoader()->LoadRobotConfigs();

  _context->GetExperiments()->InitExperiments();

  _context->GetRobotManager()->Init(_config);

  // TODO: Specify random seed from config?
  uint32_t seed = 0; // will choose random seed
# ifdef ANKI_PLATFORM_OSX
  {
    seed = 1; // Setting to non-zero value for now for repeatable testing.
  }
# endif
  _context->SetRandomSeed(seed);

  const auto& webService = _context->GetWebService();
  const auto& dataPlatform = _context->GetDataPlatform();

  webService->Start(dataPlatform,
                    _context->GetDataLoader()->GetWebServerEngineConfig());
  webService->RegisterRequestHandler("/getenginestats", GetEngineStatsWebServerHandler, this);

  _context->GetPerfMetric()->Init(dataPlatform, webService);
  _context->GetRobotTest()->Init(dataPlatform, webService);

  LOG_INFO("CozmoEngine.Init.Version", "2");

  SetEngineState(EngineState::LoadingData);

  // DAS Event: "cozmo_engine.init.build_configuration"
  // s_val: Build configuration
  // data: Unused
  Anki::Util::sInfo("cozmo_engine.init.build_configuration", {},
#if defined(NDEBUG)
                    "RELEASE");
#else
                    "DEBUG");
#endif

  _isInitialized = true;

  return RESULT_OK;
}

Result CozmoEngine::Update(const BaseStationTime_t currTime_nanosec)
{
  ANKI_CPU_PROFILE("CozmoEngine::Update");

  if (!_isInitialized) {
    PRINT_NAMED_ERROR("CozmoEngine.Update", "Cannot update CozmoEngine before it is initialized.");
    return RESULT_FAIL;
  }

  if (!_hasRunFirstUpdate) {
    _hasRunFirstUpdate = true;

    // Designate this as the thread from which engine can broadcast messages
    _context->SetEngineThread();

    // Controls OpenCV's built-in multithreading for the calling thread, so we have to do this on the first
    // call to update due to the threading quirk
    Result cvResult = SetNumOpencvThreads(NUM_OPENCV_THREADS, "CozmoEngine.Init");
    if (RESULT_OK != cvResult)
    {
      return cvResult;
    }
  }

  _uiMsgHandler->ResetMessageCounts();
  _protoMsgHandler->ResetMessageCounts();
  _context->GetRobotManager()->GetMsgHandler()->ResetMessageCounts();
  _context->GetVizManager()->ResetMessageCount();

  _context->GetWebService()->Update();

  _context->GetRobotTest()->Update();

  // Handle UI
  if (!_uiWasConnected && _uiMsgHandler->HasDesiredNumUiDevices()) {
    LOG_INFO("CozmoEngine.Update.UIConnected", "UI has connected");

    _updateMoveComponent = true;
    _uiWasConnected = true;
  } else if (_uiWasConnected && !_uiMsgHandler->HasDesiredNumUiDevices()) {
    LOG_INFO("CozmoEngine.Update.UIDisconnected", "UI has disconnected");
    _updateMoveComponent = true;
    _uiWasConnected = false;
  }

  // Enable/disable external motor commands depending on whether we have an external UI connection (i.e.
  // Webots). If we are connected via webots, then we want to allow external motion commands. Else do not
  // allow motor commands. Note: this cannot be done in the state change logic above, since robot is
  // sometimes null when the UI connection is established.
  Robot* robot = GetRobot();
  if (robot != nullptr && _updateMoveComponent) {
    const bool hasUiConnection = _uiMsgHandler->HasDesiredNumUiDevices();
    robot->GetMoveComponent().AllowExternalMovementCommands(hasUiConnection, "ui");
    _updateMoveComponent = false;
  }

  Result lastResult = _uiMsgHandler->Update();
  if (RESULT_OK != lastResult)
  {
    PRINT_NAMED_ERROR("CozmoEngine.Update", "Error updating UIMessageHandler");
    return lastResult;
  }

  lastResult = _protoMsgHandler->Update();
  if (RESULT_OK != lastResult)
  {
    PRINT_NAMED_ERROR("CozmoEngine.Update", "Error updating ProtoMessageHandler");
    return lastResult;
  }

  switch (_engineState)
  {
    case EngineState::Stopped:
    {
      break;
    }
    case EngineState::LoadingData:
    {
      float currentLoadingDone = 0.0f;
      if (_context->GetDataLoader()->DoNonConfigDataLoading(currentLoadingDone))
      {
        SetEngineState(EngineState::ConnectingToRobot);
      }
      break;
    }
    case EngineState::ConnectingToRobot:
    {
      // Is is time to try connecting?
      const auto now = std::chrono::steady_clock::now();
      const auto elapsed = (now - lastConnectAttempt);
      if (elapsed < connectInterval) {
        // Too soon to try connecting
        break;
      }
      lastConnectAttempt = now;

      // Attempt to connect
      Result result = ConnectToRobotProcess();
      if (RESULT_OK != result) {
        //LOG_WARNING("CozmoEngine.Update.ConnectingToRobot", "Unable to connect to robot (result %d)", result);
        break;
      }

      // Now connected
      LOG_INFO("CozmoEngine.Update.ConnectingToRobot", "Now connected to robot");
      SetEngineState(EngineState::Running);
      break;
    }
    case EngineState::Running:
    {
      // Update time
      BaseStationTimer::getInstance()->UpdateTime(currTime_nanosec);

      // Update OSState
      OSState::getInstance()->Update(currTime_nanosec);

      Result result = _context->GetRobotManager()->UpdateRobotConnection();
      if (RESULT_OK != result) {
        LOG_ERROR("CozmoEngine.Update.Running", "Unable to update robot connection (result %d)", result);
        return result;
      }

      // Let the robot manager do whatever it's gotta do to update the
      // robots in the world.
      result = _context->GetRobotManager()->UpdateRobot();
      if(result != RESULT_OK)
      {
        LOG_WARNING("CozmoEngine.Update.UpdateRobotFailed", "Update robot failed with %d", result);
        return result;
      }

      UpdateLatencyInfo();
      break;
    }
    default:
      PRINT_NAMED_ERROR("CozmoEngine.Update.UnexpectedState","Running Update in an unexpected state!");
      break;
  }

  return RESULT_OK;
}

#if REMOTE_CONSOLE_ENABLED
void PrintTimingInfoStats(const ExternalInterface::TimingInfo& timingInfo, const char* name)
{
  PRINT_CH_INFO("UiComms", "CozmoEngine.LatencyStats", "%s: = %f (%f..%f)", name, timingInfo.avgTime_ms, timingInfo.minTime_ms, timingInfo.maxTime_ms);
}

void PrintTimingInfoStats(const ExternalInterface::CurrentTimingInfo& timingInfo, const char* name)
{
  PRINT_CH_INFO("UiComms", "CozmoEngine.LatencyStats", "%s: = %f (%f..%f) (curr: %f)", name, timingInfo.avgTime_ms, timingInfo.minTime_ms, timingInfo.maxTime_ms, timingInfo.currentTime_ms);
}
CONSOLE_VAR(bool, kLogMessageLatencyOnce, "Network.Stats", false);
#endif // REMOTE_CONSOLE_ENABLED


void CozmoEngine::UpdateLatencyInfo()
{
#if REMOTE_CONSOLE_ENABLED
  if (Util::kNetConnStatsUpdate)
  {
    // We only want to send latency info every N ticks
    constexpr int kTickSendFrequency = 10;
    static int currentTickCount = kTickSendFrequency;
    if (0 != currentTickCount)
    {
      currentTickCount--;
      return;
    }
    currentTickCount = kTickSendFrequency;
    
    if (kLogMessageLatencyOnce)
    {
      ExternalInterface::TimingInfo wifiLatency(Util::gNetStat2LatencyAvg, Util::gNetStat4LatencyMin, Util::gNetStat5LatencyMax);
      ExternalInterface::TimingInfo extSendQueueTime(Util::gNetStat7ExtQueuedAvg_ms, Util::gNetStat8ExtQueuedMin_ms, Util::gNetStat9ExtQueuedMax_ms);
      ExternalInterface::TimingInfo sendQueueTime(Util::gNetStatAQueuedAvg_ms, Util::gNetStatBQueuedMin_ms, Util::gNetStatCQueuedMax_ms);
      const Util::Stats::StatsAccumulator& queuedTimes_ms = _context->GetRobotManager()->GetMsgHandler()->GetQueuedTimes_ms();
      ExternalInterface::TimingInfo recvQueueTime(queuedTimes_ms.GetMean(), queuedTimes_ms.GetMin(), queuedTimes_ms.GetMax());

      const Util::Stats::StatsAccumulator& unityLatency = _uiMsgHandler->GetLatencyStats(UiConnectionType::UI);
      ExternalInterface::TimingInfo unityEngineLatency(unityLatency.GetMean(), unityLatency.GetMin(), unityLatency.GetMax());

      PrintTimingInfoStats(wifiLatency,      "wifi");
      PrintTimingInfoStats(extSendQueueTime, "extSendQueue");
      PrintTimingInfoStats(sendQueueTime,    "sendQueue");
      PrintTimingInfoStats(recvQueueTime,    "recvQueue");
      if (unityLatency.GetNumDbl() > 0.0)
      {
        PrintTimingInfoStats(unityEngineLatency, "unity");
      }

      kLogMessageLatencyOnce = false;
    }
  }
#endif // REMOTE_CONSOLE_ENABLED
}

void CozmoEngine::SetEngineState(EngineState newState)
{
  EngineState oldState = _engineState;
  if (oldState == newState)
  {
    return;
  }

  _engineState = newState;

  DASMSG(engine_state, "engine.state", "EngineState has changed")
  DASMSG_SET(s1, EngineStateToString(oldState), "Old EngineState");
  DASMSG_SET(s2, EngineStateToString(newState), "New EngineState");
  DASMSG_SEND()
}

Result CozmoEngine::InitInternal()
{
  // Archive factory test logs
  FactoryTestLogger factoryTestLogger;
  u32 numLogs = factoryTestLogger.GetNumLogs(_context->GetDataPlatform());
  if (numLogs >= MIN_NUM_FACTORY_TEST_LOGS_FOR_ARCHIVING) {
    if (factoryTestLogger.ArchiveLogs(_context->GetDataPlatform())) {
      LOG_INFO("CozmoEngine.InitInternal.ArchivedFactoryLogs", "%d logs archived", numLogs);
    } else {
      PRINT_NAMED_WARNING("CozmoEngine.InitInternal.ArchivedFactoryLogsFailed", "");
    }
  }

  // clear the first update flag
  _hasRunFirstUpdate = false;

  return RESULT_OK;
}

Result CozmoEngine::ConnectToRobotProcess()
{
  const RobotID_t robotID = OSState::getInstance()->GetRobotID();

  auto * robotManager = _context->GetRobotManager();
  if (!robotManager->DoesRobotExist(robotID)) {
    robotManager->AddRobot(robotID);
  }

  auto * msgHandler = robotManager->GetMsgHandler();
  if (!msgHandler->IsConnected(robotID)) {
    Result result = msgHandler->AddRobotConnection(robotID);
    if (RESULT_OK != result) {
      //LOG_WARNING("CozmoEngine.ConnectToRobotProcess", "Unable to connect to robot %d (result %d)", robotID, result);
      return result;
    }
  }

  return RESULT_OK;
}

Robot* CozmoEngine::GetRobot() {
  return _context->GetRobotManager()->GetRobot();
}

template<>
void CozmoEngine::HandleMessage(const ExternalInterface::ImageRequest& msg)
{
  Robot* robot = GetRobot();
  if(robot != nullptr) {
    return robot->GetVisionComponent().EnableImageSending(msg.mode == ImageSendMode::Stream);
  }
}

template<>
void CozmoEngine::HandleMessage(const ExternalInterface::StartTestMode& msg)
{
  Robot* robot = GetRobot();
  if(robot != nullptr) {
    robot->SendRobotMessage<StartControllerTestMode>(msg.p1, msg.p2, msg.p3, msg.mode);
  }
}

void CozmoEngine::InitUnityLogger()
{
#if ANKI_DEV_CHEATS
  if(Anki::Util::gLoggerProvider != nullptr) {
    Anki::Util::MultiLoggerProvider* multiLoggerProvider = dynamic_cast<Anki::Util::MultiLoggerProvider*>(Anki::Util::gLoggerProvider);
    if (multiLoggerProvider != nullptr) {
      const std::vector<Anki::Util::ILoggerProvider*>& loggers = multiLoggerProvider->GetProviders();
      for(int i = 0; i < loggers.size(); ++i) {
        CLADLoggerProvider* unityLoggerProvider = dynamic_cast<CLADLoggerProvider*>(loggers[i]);
        if (unityLoggerProvider != nullptr) {
          unityLoggerProvider->SetExternalInterface(_context->GetExternalInterface());
          break;
        }
      }
    }
  }
#endif //ANKI_DEV_CHEATS
}


template<>
void CozmoEngine::HandleMessage(const ExternalInterface::RedirectViz& msg)
{
  // Disable viz in shipping
  if(ANKI_DEV_CHEATS) {
    const uint8_t* ipBytes = (const uint8_t*)&msg.ipAddr;
    std::ostringstream ss;
    ss << (int)ipBytes[0] << "." << (int)ipBytes[1] << "." << (int)ipBytes[2] << "." << (int)ipBytes[3];
    std::string ipAddr = ss.str();
    LOG_INFO("CozmoEngine.RedirectViz.ipAddr", "%s", ipAddr.c_str());

    _context->GetVizManager()->Disconnect();
    _context->GetVizManager()->Connect(ipAddr.c_str(), (uint16_t)VizConstants::VIZ_SERVER_PORT);
    _context->GetVizManager()->EnableImageSend(true);

    // Erase anything that's still being visualized in case there were leftovers from
    // a previous run?? (We should really be cleaning up after ourselves when
    // we tear down, but it seems like Webots restarts aren't always allowing
    // the cleanup to happen)
    _context->GetVizManager()->EraseAllVizObjects();
  }
}


Util::AnkiLab::AssignmentStatus CozmoEngine::ActivateExperiment(
  const Util::AnkiLab::ActivateExperimentRequest& request, std::string& outVariationKey)
{
  return _context->GetExperiments()->ActivateExperiment(request, outVariationKey);
}

void CozmoEngine::RegisterEngineTickPerformance(const float tickDuration_ms,
                                                const float tickFrequency_ms,
                                                const float sleepDurationIntended_ms,
                                                const float sleepDurationActual_ms) const
{
  // Update the PerfMetric system for end of tick
  _context->GetPerfMetric()->Update(tickDuration_ms, tickFrequency_ms,
                                    sleepDurationIntended_ms, sleepDurationActual_ms);
}

void CozmoEngine::SetEngineThread()
{
  // Context is valid for lifetime of engine
  DEV_ASSERT(_context, "CozmoEngine.SetEngineThread.InvalidContext");
  _context->SetEngineThread();
}

} // namespace Vector
} // namespace Anki
