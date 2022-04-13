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


#ifndef __Vector_Engine_PerfMetric_Engine_H__
#define __Vector_Engine_PerfMetric_Engine_H__

#include "util/perfMetric/iPerfMetric.h"
#include "clad/types/behaviorComponent/activeFeatures.h"
#include "util/stats/statsAccumulator.h"

#include <string>


namespace Anki {
namespace Vector {


class PerfMetricEngine : public PerfMetric
{
public:
  explicit PerfMetricEngine(const CozmoContext* context);
  virtual ~PerfMetricEngine();

  virtual void Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService) override final;
  virtual void Update(const float tickDuration_ms,
                      const float tickFrequency_ms,
                      const float sleepDurationIntended_ms,
                      const float sleepDurationActual_ms) override final;

private:

  virtual void InitDumpAccumulators() override final;
  virtual const FrameMetric& UpdateDumpAccumulators(const int frameBufferIndex) override final;
  virtual const FrameMetric& GetBaseFrame(const int frameBufferIndex) override final { return _frameBuffer[frameBufferIndex]; };
  virtual int AppendFrameData(const DumpType dumpType,
                              const int frameBufferIndex,
                              const int dumpBufferOffset,
                              const bool graphableDataOnly) override final;
  virtual int AppendSummaryData(const DumpType dumpType,
                                const int dumpBufferOffset,
                                const int lineIndex) override final;

  // Frame size:  Base struct is 16 bytes; here is 10 words * 4 (40 bytes), plus 32 bytes = 88 bytes
  // x 4000 frames is ~344 KB
  struct FrameMetricEngine : public FrameMetric
  {
    uint32_t _messageCountRobotToEngine;
    uint32_t _messageCountEngineToRobot;
    uint32_t _messageCountGameToEngine;
    uint32_t _messageCountEngineToGame;
    uint32_t _messageCountViz;
    uint32_t _messageCountGatewayToEngine;
    uint32_t _messageCountEngineToGateway;

    float _batteryVoltage;
    uint32_t _cpuFreq_kHz;

    ActiveFeature    _activeFeature;
    static const int kBehaviorStringMaxSize = 32;
    char _behavior[kBehaviorStringMaxSize]; // Some description of what Victor is doing
  };

  FrameMetricEngine*  _frameBuffer = nullptr;
#if ANKI_PERF_METRIC_ENABLED
  const CozmoContext* _context;
#endif

  Util::Stats::StatsAccumulator _accMessageCountRtE;
  Util::Stats::StatsAccumulator _accMessageCountEtR;
  Util::Stats::StatsAccumulator _accMessageCountGtE;
  Util::Stats::StatsAccumulator _accMessageCountEtG;
  Util::Stats::StatsAccumulator _accMessageCountGatewayToE;
  Util::Stats::StatsAccumulator _accMessageCountEToGateway;
  Util::Stats::StatsAccumulator _accMessageCountViz;
  Util::Stats::StatsAccumulator _accBatteryVoltage;
  Util::Stats::StatsAccumulator _accCPUFreq;
};

static const int kNumFramesInBuffer = 1000;
  
} // namespace Vector
} // namespace Anki


#endif // __Vector_Engine_PerfMetric_Engine_H__
