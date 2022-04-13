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
#ifndef __Cozmo_Basestation_Debug_DevLogReaderRaw_H_
#define __Cozmo_Basestation_Debug_DevLogReaderRaw_H_

#include "engine/debug/devLogReader.h"

namespace Anki {
namespace Vector {

class DevLogReaderRaw: public DevLogReader {
public:
  using DevLogReader::DevLogReader;
  
protected:
  // Extract next chunk of data out of the current file handle
  // Returns success
  virtual bool FillLogData(std::ifstream& fileHandle, LogData& logData_out) const override;
};

} // end namespace Vector
} // end namespace Anki


#endif //__Cozmo_Basestation_Debug_DevLogReaderRaw_H_
