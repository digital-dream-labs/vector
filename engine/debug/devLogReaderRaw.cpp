/**
* File: devLogReaderRaw
*
* Author: Lee Crippen
* Created: 6/22/2016
*
* Description: Functionality for pulling Raw data out of a log file
*
* Copyright: Anki, inc. 2016
*
*/
#include "engine/debug/devLogReaderRaw.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
  
bool DevLogReaderRaw::FillLogData(std::ifstream& fileHandle, LogData& logData_out) const
{
  uint32_t sizeInBytes = 0;
  fileHandle.read(reinterpret_cast<char*>(&sizeInBytes), sizeof(sizeInBytes));
  if (!fileHandle.good())
  {
    return false;
  }
  
  static constexpr uint32_t metaDataSize = sizeof(sizeInBytes) + sizeof(logData_out._timestamp_ms);
  // There is no exact limit on the largest size a message could be, so this is a really a rough sanity check
  static constexpr uint32_t kLargestReasonableDataSize = 4 * 1024;
  
  // Verify the size makes sense
  bool sizeMakesSense = (sizeInBytes > metaDataSize) && (sizeInBytes <= kLargestReasonableDataSize);
  DEV_ASSERT(sizeMakesSense, "DevLogReaderRaw.FillLogData.InvalidSize");
  if (!sizeMakesSense)
  {
    // This indicates there's some problem with the data, so bail on this file
    return false;
  }
  
  // After reading the size try to read in the timestamp
  fileHandle.read(reinterpret_cast<char*>(&logData_out._timestamp_ms), sizeof(logData_out._timestamp_ms));
  if (!fileHandle.good())
  {
    return false;
  }
  
  // Update our size to remove the metadata so we can copy the right number of bytes remaining
  sizeInBytes -= metaDataSize;
  
  logData_out._data.resize(sizeInBytes);
  fileHandle.read(reinterpret_cast<char*>(logData_out._data.data()), sizeInBytes);
  if (!fileHandle.good())
  {
    return false;
  }
  
  return true;
}

} // end namespace Vector
} // end namespace Anki
