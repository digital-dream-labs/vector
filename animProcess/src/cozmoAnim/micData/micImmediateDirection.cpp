/**
* File: micImmediateDirection.cpp
*
* Author: Lee Crippen
* Created: 11/09/2017
*
* Description: Holds onto immediate mic direction data with simple circular buffer
*
* Copyright: Anki, Inc. 2017
*
*/
  
#include "cozmoAnim/micData/micImmediateDirection.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
namespace MicData {

MicImmediateDirection::MicImmediateDirection()
{
  // Fill the historical array with the unknown direction to start
  const auto initialDirection = kDirectionUnknown;
  MicDirectionData initialData{};
  initialData.winningDirection = initialDirection;
  _micDirectionBuffer.fill(initialData);

  // The full count will decrease and go away as other directions come in that are not "unknown"
  _micDirectionsCount[initialDirection] = kMicDirectionBufferLen;
}

void MicImmediateDirection::AddDirectionSample(const MicDirectionData& newSample)
{
  std::lock_guard<std::mutex> lock(_mutex);
  // Update our index to the next oldest sample
  _micDirectionBufferIndex = (_micDirectionBufferIndex + 1) % kMicDirectionBufferLen;

  // Decrement the count for the direction of the existing oldest sample we'll be replacing
  auto& directionDataEntry = _micDirectionBuffer[_micDirectionBufferIndex];
  auto& countRef = _micDirectionsCount[directionDataEntry.winningDirection];
  if (ANKI_VERIFY(
    countRef > 0,
    "MicImmediateDirection.AddDirectionSample",
    "Trying to replace a direction sample in index %d but count is 0",
    directionDataEntry.winningDirection))
  {
    --countRef;
  }

  // Replace the data in the oldest sample with our new sample and update the count on that direction
  directionDataEntry = newSample;
  ++_micDirectionsCount[directionDataEntry.winningDirection];
}

DirectionIndex MicImmediateDirection::GetDominantDirection() const
{
  std::lock_guard<std::mutex> lock(_mutex);
  // Loop through our stored direction counts and pick the direction with the higest count
  // Does not currently consider confidence level
  auto bestIndex = kDirectionUnknown;
  uint32_t bestCount = 0;
  // Ignore kLastIndex (unknown), since that is accumulated whenever the robot moves
  for (auto i = kFirstIndex; i < kLastIndex; ++i)
  {
    if (_micDirectionsCount[i] > bestCount)
    {
      bestCount = _micDirectionsCount[i];
      bestIndex = i;
    }
  }

  return bestIndex;
}

MicDirectionData MicImmediateDirection::GetLatestSample() const
{
  std::lock_guard<std::mutex> lock(_mutex);
  return _micDirectionBuffer[_micDirectionBufferIndex];
}

} // namespace MicData
} // namespace Vector
} // namespace Anki
