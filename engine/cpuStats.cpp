/**
 * File: cpuStats.cpp
 *
 * Author: Kevin Yoon
 * Created: 9/24/2018
 *
 * Description: Gather and record some long-running cpu statistics
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "engine/cpuStats.h"
#include "osState/osState.h"
#include "util/logging/DAS.h"
#include "util/stats/statsAccumulator.h"
#include "coretech/common/engine/utils/timer.h"

namespace Anki {
namespace Vector {

namespace {
  const float kSamplePeriod_sec = 60.f;                // sample every minute. 
                                                       // If you change this, you have to change how _numSamplesAboveReportingThresh is reported
  const float kDasSendPeriod_sec = 60.f * 60.f * 24.f; // send to DAS every 24 hours or shutdown
  const float kReportingTempThresh_degC = 60.f;        // count minutes above this threshold
}

CPUStats::CPUStats()
: _temperatureStats_degC(std::make_unique<Util::Stats::StatsAccumulator>())
{
  // Set _lastDasSendTime_sec to now so that we do not send to DAS right away
  const float now_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
  _lastDasSendTime_sec = now_sec;
}

  
CPUStats::~CPUStats()
{
  LogToDas();
}

  
void CPUStats::Update()
{
  const float now_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
  
  // If it is time, add a sample to the statistics accumulators
  if ((_lastSampleTime_sec == 0.f) ||
      (now_sec - _lastSampleTime_sec > kSamplePeriod_sec)) {
    const auto cpuTemp_degC = OSState::getInstance()->GetTemperature_C();

    // Ignore temp of 0
    if (cpuTemp_degC > 0) {
      *_temperatureStats_degC += cpuTemp_degC;
      
      if (cpuTemp_degC > kReportingTempThresh_degC) {
        ++_numSamplesAboveReportingThresh;
      }
    }

    _lastSampleTime_sec = now_sec;
  }
  
  // If it is time, send to DAS
  if (now_sec - _lastDasSendTime_sec > kDasSendPeriod_sec) {
    LogToDas();
    _lastDasSendTime_sec = now_sec;
  }
}


void CPUStats::LogToDas()
{
  DASMSG(cpu_temperature_stats, "cpu.temperature_stats", "CPU temperature statistics");
  DASMSG_SET(i1, _temperatureStats_degC->GetIntMin(), "Minimum CPU temperature experienced (degC)");
  DASMSG_SET(i2, _temperatureStats_degC->GetIntMax(), "Maximum CPU temperature experienced (degC)");
  DASMSG_SET(i3, _numSamplesAboveReportingThresh, "Time spent above 60C (min)"); //depends on kSamplePeriod_sec==60
  DASMSG_SET(i4, _temperatureStats_degC->GetNum(), "Total number of samples");
  DASMSG_SEND();
    
  _numSamplesAboveReportingThresh = 0;
  _temperatureStats_degC->Clear();
}


} // Vector namespace
} // Anki namespace
