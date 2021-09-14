/**
* File: micImmediateDirection.h
*
* Author: Lee Crippen
* Created: 11/09/2017
*
* Description: Holds onto immediate mic direction data with simple circular buffer
*
* Copyright: Anki, Inc. 2017
*
*/

#ifndef __AnimProcess_CozmoAnim_MicImmediateDirection_H_
#define __AnimProcess_CozmoAnim_MicImmediateDirection_H_

#include "micDataTypes.h"

#include <array>
#include <cstdint>
#include <mutex>

namespace Anki {
namespace Vector {
namespace MicData {

class MicImmediateDirection
{
public:
  MicImmediateDirection();
  void AddDirectionSample(const MicDirectionData& newSample);
  DirectionIndex GetDominantDirection() const;
  MicDirectionData GetLatestSample() const;

private:
  static constexpr uint32_t kMicDirectionBuffer_ms = 700 + kTriggerOverlapSize_ms;
  static constexpr uint32_t kMicDirectionBufferLen = kMicDirectionBuffer_ms / kTimePerChunk_ms;

  std::array<MicDirectionData, kMicDirectionBufferLen> _micDirectionBuffer{};
  uint32_t _micDirectionBufferIndex = 0;
  std::array<uint32_t, kNumDirections> _micDirectionsCount{};
  mutable std::mutex _mutex;
};

} // namespace MicData
} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_MicImmediateDirection_H_
