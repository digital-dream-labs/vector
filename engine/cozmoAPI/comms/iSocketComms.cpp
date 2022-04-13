/**
 * File: iSocketComms
 *
 * Author: Mark Wesley
 * Created: 05/14/16
 *
 * Description: Interface for any socket-based communications from e.g. Game/SDK to Engine
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#include "engine/cozmoAPI/comms/iSocketComms.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/time/universalTime.h"

namespace Anki {
namespace Vector {

CONSOLE_VAR(bool, kPrintUiMessageLatency, "UiComms", false);
  
const uint32_t kMaxLatencySamples = 20;
const uint32_t kReportFrequency = 10;
const uint32_t kDefaultPingTimeoutForDisconnect_ms = 5000;

ISocketComms::ISocketComms(bool isEnabled)
  : _lastPingTime_ms(0)
  , _pingTimeoutForDisconnect_ms(kDefaultPingTimeoutForDisconnect_ms)
  , _latencyStats(kMaxLatencySamples)
  , _pingCounter(0)
  , _isEnabled(isEnabled)
{
}

  
ISocketComms::~ISocketComms()
{
}


void ISocketComms::Update()
{
  // If a client is connected, artificially set the lastPingTime so that it still 
  // times out if no ping is ever received.
  const uint32_t curTime_ms = static_cast<uint32_t>(Util::Time::UniversalTime::GetCurrentTimeInMilliseconds());
  if (_lastPingTime_ms == 0 && GetNumConnectedDevices() > 0) {
    _lastPingTime_ms = curTime_ms;
  }

  // Check for disconnect because of ping timeout
  if (_pingTimeoutForDisconnect_ms > 0 && _lastPingTime_ms > 0) {
    if (curTime_ms - _lastPingTime_ms > _pingTimeoutForDisconnect_ms) {
      _lastPingTime_ms = 0;
      PRINT_CH_INFO("UiComms", "Update.DisconnectByPingTimeout", "Timeout: %d ms", _pingTimeoutForDisconnect_ms);
      DisconnectAllDevices();

      // Execute callback if specified
      if (_disconnectCb) {
        _disconnectCb();
      }
    }
  }

  UpdateInternal();
}

void ISocketComms::HandlePingResponse(const ExternalInterface::Ping& pingMsg)
{
  const double now_ms = Util::Time::UniversalTime::GetCurrentTimeInMilliseconds();
  const double latency_ms = (now_ms - pingMsg.timeSent_ms);
  _latencyStats.AddStat(latency_ms);
  if (kPrintUiMessageLatency)
  {
    uint32_t numSamples = _latencyStats.GetNumDbl();
    if (numSamples && ((numSamples % kReportFrequency) == 0))
    {
      const double averageLatency = _latencyStats.GetMean();
      const double stdDevLatency  = _latencyStats.GetStd();
    
      const float minLatency = (numSamples > 0) ? _latencyStats.GetMin() : 0.0f;
      const float maxLatency = (numSamples > 0) ? _latencyStats.GetMax() : 0.0f;
    
      PRINT_CH_INFO("UiComms", "UiMessageLatency", "%.2f ms, [%.2f..%.2f], SD= %.2f, %u samples",
                           averageLatency, minLatency, maxLatency, stdDevLatency, numSamples);
    }
  }
  
  _lastPingTime_ms = static_cast<uint32_t>(now_ms);
}

void ISocketComms::SetPingTimeoutForDisconnect(uint32_t ms, DisconnectCallback cb)
{
  PRINT_CH_DEBUG("UiComms", "SetPingTimeoutForDisconnect", "%d ms", ms);
  _pingTimeoutForDisconnect_ms = ms;
  _disconnectCb = cb;
}

  
} // namespace Vector
} // namespace Anki

