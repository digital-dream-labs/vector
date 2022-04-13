/**
 * File: perfMetricEngine
 *
 * Author: Paul Terry
 * Created: 11/16/2018
 *
 * Description: Lightweight performance metric recording: for vic-engine
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "engine/aiComponent/aiComponent.h"
#include "engine/aiComponent/behaviorComponent/activeFeatureComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviorComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviorSystemManager.h"
#include "engine/cozmoContext.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/externalInterface/gatewayInterface.h"
#include "engine/perfMetricEngine.h"
#include "engine/robot.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/robotManager.h"
#include "osState/osState.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/perfMetric/perfMetricImpl.h"
#include "util/stats/statsAccumulator.h"


#define LOG_CHANNEL "PerfMetric"

namespace Anki {
namespace Vector {



PerfMetricEngine::PerfMetricEngine(const CozmoContext* context)
  : _frameBuffer(nullptr)
#if ANKI_PERF_METRIC_ENABLED
  , _context(context)
#endif
{
  _headingLine1 = "                     Engine   Engine    Sleep    Sleep     Over      RtE   EtR   GtE   EtG  GWtE  EtGW   Viz  Battery    CPU";
  _headingLine2 = "                   Duration     Freq Intended   Actual    Sleep    Count Count Count Count Count Count Count  Voltage   Freq";
  _headingLine2Extra = "  Active Feature/Behavior";
  _headingLine1CSV = ",,Engine,Engine,Sleep,Sleep,Over,RtE,EtR,GtE,EtG,GWtE,EtGW,Viz,Battery,CPU";
  _headingLine2CSV = ",,Duration,Freq,Intended,Actual,Sleep,Count,Count,Count,Count,Count,Count,Count,Voltage,Freq";
  _headingLine2ExtraCSV = ",Active Feature,Behavior";
}

PerfMetricEngine::~PerfMetricEngine()
{
#if ANKI_PERF_METRIC_ENABLED
  OnShutdown();

  delete[] _frameBuffer;
#endif
}

void PerfMetricEngine::Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService)
{
#if ANKI_PERF_METRIC_ENABLED
  _frameBuffer = new FrameMetricEngine[kNumFramesInBuffer];

  _fileNameSuffix = "Eng";

  InitInternal(dataPlatform, webService);
#endif
}


// This is called at the end of the tick
void PerfMetricEngine::Update(const float tickDuration_ms,
                              const float tickFrequency_ms,
                              const float sleepDurationIntended_ms,
                              const float sleepDurationActual_ms)
{
#if ANKI_PERF_METRIC_ENABLED
  ANKI_CPU_PROFILE("PerfMetricEngine::Update");

  ExecuteQueuedCommands();

  if (_isRecording)
  {
    FrameMetricEngine& frame = _frameBuffer[_nextFrameIndex];

    if (_bufferFilled)
    {
      // Move the 'first frame start time' up by the frame's time we're about to overwrite
      _firstFrameTime = IncrementFrameTime(_firstFrameTime, frame._tickTotal_ms);
    }

    frame._tickExecution_ms     = tickDuration_ms;
    frame._tickTotal_ms         = tickFrequency_ms;
    frame._tickSleepIntended_ms = sleepDurationIntended_ms;
    frame._tickSleepActual_ms   = sleepDurationActual_ms;

    const auto msgHandler = _context->GetRobotManager()->GetMsgHandler();
    frame._messageCountRobotToEngine = msgHandler->GetMessageCountRtE();
    frame._messageCountEngineToRobot = msgHandler->GetMessageCountEtR();

    const auto UIMsgHandler = _context->GetExternalInterface();
    frame._messageCountGatewayToEngine = UIMsgHandler->GetMessageCountGtE();
    frame._messageCountEngineToGame = UIMsgHandler->GetMessageCountEtG();

    const auto vizManager = _context->GetVizManager();
    frame._messageCountViz = vizManager->GetMessageCountViz();

    const auto gateway = _context->GetGatewayInterface();
    frame._messageCountGatewayToEngine = gateway->GetMessageCountIncoming();
    frame._messageCountEngineToGateway = gateway->GetMessageCountOutgoing();

    Robot* robot = _context->GetRobotManager()->GetRobot();

    frame._batteryVoltage = robot == nullptr ? 0.0f : robot->GetBatteryComponent().GetBatteryVolts();

    const auto& osState = OSState::getInstance();
    frame._cpuFreq_kHz = osState->GetCPUFreq_kHz();

    if (robot != nullptr)
    {
      const auto& bc = robot->GetAIComponent().GetComponent<BehaviorComponent>();
      const auto& afc = bc.GetComponent<ActiveFeatureComponent>();
      frame._activeFeature = afc.GetActiveFeature();
      const auto& bsm = bc.GetComponent<BehaviorSystemManager>();
      strncpy(frame._behavior, bsm.GetTopBehaviorDebugLabel().c_str(), sizeof(frame._behavior));
      frame._behavior[FrameMetricEngine::kBehaviorStringMaxSize - 1] = '\0'; // Ensure string is null terminated
    }
    else
    {
      frame._activeFeature = ActiveFeature::NoFeature;
      frame._behavior[0] = '\0';
    }

    if (++_nextFrameIndex >= kNumFramesInBuffer)
    {
      _nextFrameIndex = 0;
      _bufferFilled = true;
    }
  }

  UpdateWaitMode();
#endif
}


void PerfMetricEngine::InitDumpAccumulators()
{
  _accMessageCountRtE.Clear();
  _accMessageCountEtR.Clear();
  _accMessageCountGtE.Clear();
  _accMessageCountEtG.Clear();
  _accMessageCountGatewayToE.Clear();
  _accMessageCountEToGateway.Clear();
  _accMessageCountViz.Clear();
  _accBatteryVoltage.Clear();
  _accCPUFreq.Clear();
}


const PerfMetric::FrameMetric& PerfMetricEngine::UpdateDumpAccumulators(const int frameBufferIndex)
{
  const FrameMetricEngine& frame = _frameBuffer[frameBufferIndex];
  _accMessageCountRtE        += frame._messageCountRobotToEngine;
  _accMessageCountEtR        += frame._messageCountEngineToRobot;
  _accMessageCountGtE        += frame._messageCountGameToEngine;
  _accMessageCountEtG        += frame._messageCountEngineToGame;
  _accMessageCountGatewayToE += frame._messageCountGatewayToEngine;
  _accMessageCountEToGateway += frame._messageCountEngineToGateway;
  _accMessageCountViz        += frame._messageCountViz;
  _accBatteryVoltage         += frame._batteryVoltage;
  _accCPUFreq                += frame._cpuFreq_kHz;

  return _frameBuffer[frameBufferIndex];  // Return the base class data
}


int PerfMetricEngine::AppendFrameData(const DumpType dumpType,
                                      const int frameBufferIndex,
                                      const int dumpBufferOffset,
                                      const bool graphableDataOnly)
{
  const FrameMetricEngine& frame = _frameBuffer[frameBufferIndex];

  if (graphableDataOnly)
  {
    // For the auto-update graph, we omit activity and behavior strings
#define ENGINE_LINE_DATA_VARS_GRAPHABLE_ONLY \
    frame._messageCountRobotToEngine, frame._messageCountEngineToRobot,\
    frame._messageCountGameToEngine, frame._messageCountEngineToGame,\
    frame._messageCountGatewayToEngine, frame._messageCountEngineToGateway,\
    frame._messageCountViz,\
    frame._batteryVoltage, frame._cpuFreq_kHz

    static const char* kFormatLine = "    %5i %5i %5i %5i %5i %5i %5i %8.3f %6i\n";
    static const char* kFormatLineCSV = ",%i,%i,%i,%i,%i,%i,%i,%.3f,%i\n";

    return snprintf(&_dumpBuffer[dumpBufferOffset], kSizeDumpBuffer - dumpBufferOffset,
                    dumpType == DT_FILE_CSV ? kFormatLineCSV : kFormatLine,
                    ENGINE_LINE_DATA_VARS_GRAPHABLE_ONLY);
  }
  else
  {
#define ENGINE_LINE_DATA_VARS \
    frame._messageCountRobotToEngine, frame._messageCountEngineToRobot,\
    frame._messageCountGameToEngine, frame._messageCountEngineToGame,\
    frame._messageCountGatewayToEngine, frame._messageCountEngineToGateway,\
    frame._messageCountViz,\
    frame._batteryVoltage, frame._cpuFreq_kHz, EnumToString(frame._activeFeature),\
    frame._behavior

    static const char* kFormatLine = "    %5i %5i %5i %5i %5i %5i %5i %8.3f %6i  %s  %s\n";
    static const char* kFormatLineCSV = ",%i,%i,%i,%i,%i,%i,%i,%.3f,%i,%s,%s\n";
    
    return snprintf(&_dumpBuffer[dumpBufferOffset], kSizeDumpBuffer - dumpBufferOffset,
                    dumpType == DT_FILE_CSV ? kFormatLineCSV : kFormatLine,
                    ENGINE_LINE_DATA_VARS);
  }
}


int PerfMetricEngine::AppendSummaryData(const DumpType dumpType,
                                        const int dumpBufferOffset,
                                        const int lineIndex)
{
  DEV_ASSERT_MSG(lineIndex < kNumLinesInSummary, "PerfMetricEngine.AppendSummaryData",
                 "lineIndex %d out of range", lineIndex);

#define ENGINE_SUMMARY_LINE_VARS(StatCall)\
  _accMessageCountRtE.StatCall(), _accMessageCountEtR.StatCall(),\
  _accMessageCountGtE.StatCall(), _accMessageCountEtG.StatCall(),\
  _accMessageCountGatewayToE.StatCall(), _accMessageCountEToGateway.StatCall(),\
  _accMessageCountViz.StatCall(),\
  _accBatteryVoltage.StatCall(), _accCPUFreq.StatCall()

  static const char* kFormatLine = "    %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %8.3f %6.0f\n";
  static const char* kFormatLineCSV = ",%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.0f\n";

#define APPEND_SUMMARY_LINE(StatCall)\
  lenOut = snprintf(&_dumpBuffer[dumpBufferOffset], kSizeDumpBuffer - dumpBufferOffset,\
                    dumpType == DT_FILE_CSV ? kFormatLineCSV : kFormatLine,\
                    ENGINE_SUMMARY_LINE_VARS(StatCall));

  int lenOut = 0;
  switch (lineIndex)
  {
    case 0:   APPEND_SUMMARY_LINE(GetMin);   break;
    case 1:   APPEND_SUMMARY_LINE(GetMax);   break;
    case 2:   APPEND_SUMMARY_LINE(GetMean);  break;
    case 3:   APPEND_SUMMARY_LINE(GetStd);   break;
  }
  return lenOut;
}


} // namespace Vector
} // namespace Anki
