/**
 * File: cpuStats.h
 *
 * Author: Kevin Yoon
 * Created: 9/24/2018
 *
 * Description: Gather and record some long-running cpu statistics
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __Engine_CPUStats_H__
#define __Engine_CPUStats_H__

#include <limits>
#include <memory>

namespace Anki {
namespace Util{
namespace Stats{
  class StatsAccumulator;
}
}
namespace Vector {

class CPUStats
{
public:
  CPUStats();
  ~CPUStats();
  
  void Update();
  
private:
  // Write a DAS event with the current statistics.
  // Note: This will clear the stats accumulator(s) when called.
  void LogToDas();
  
  std::unique_ptr<Util::Stats::StatsAccumulator> _temperatureStats_degC;
  uint32_t _numSamplesAboveReportingThresh = 0;
  
  float _lastSampleTime_sec = 0.f;
  float _lastDasSendTime_sec = 0.f;
};
  

} // Vector namespace
} // Anki namespace

#endif // __Engine_Components_BatteryStats_H__
