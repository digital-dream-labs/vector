/**
* File: devLogReaderPrint
*
* Author: Lee Crippen
* Created: 6/24/2016
*
* Description: Functionality for pulling Print data out of a log file
*
* Copyright: Anki, inc. 2016
*
*/

#include "engine/debug/devLogReaderPrint.h"
#include "engine/debug/devLogConstants.h"

namespace Anki {
namespace Vector {
  
bool DevLogReaderPrint::FillLogData(std::ifstream& fileHandle, LogData& logData_out) const
{
  // Try to read in the timestamp
  fileHandle >> logData_out._timestamp_ms;
  if (!fileHandle.good())
  {
    return false;
  }
  
  static constexpr auto kMaxLineLength = 1024;
  static char nextLineBuffer[kMaxLineLength];
  
  // get entry which tells us size of line
  std::streamsize lineSize;
  fileHandle >> lineSize;
  
  // reading an extra space and discarding it
  fileHandle.read(nextLineBuffer, 1);
  fileHandle.read(nextLineBuffer, lineSize);
  if (!fileHandle.good())
  {
    return false;
  }
  
  const auto lineLength = fileHandle.gcount();
  
  // We want to include an extra character in the vector for null
  logData_out._data.resize(lineLength + 1);
  
  std::copy(nextLineBuffer, nextLineBuffer + lineLength, reinterpret_cast<char*>(logData_out._data.data()));
  
  // Put in a null character at the end of the line we just copied with the extra space we created above
  logData_out._data[lineLength] = '\0';
  
  return true;
}

uint32_t DevLogReaderPrint::GetFinalTimestamp_ms(std::ifstream& fileHandle) const
{
  if( ! fileHandle.good() ) {
    printf("ERROR: DevLogReaderPrint.GetFinalTimestamp_ms: bad file handle\n");
    return 0;
  }
  
  // save the current pos so we can reset after we are done.
  auto initialPos = fileHandle.tellg();
  
  // go to the end of the file and work backwards until we find a line which starts with at a n-digit (or
  // longer) number
  fileHandle.seekg(-1,std::ios_base::end);

  while( fileHandle.good() ) {

    auto pos = fileHandle.tellg();

    if( fileHandle.get() == '\n' ) {

      if( fileHandle.good() ) {

        // got a newline, see if there is are n digits after it
        bool hasDigit = true;
        for( int i = 0; i < DevLogConstants::kNumLogTimestampDigits; ++i ) {
          if( !isdigit( fileHandle.get() ) ) {
            hasDigit = false;
            break;
          }
          if( !fileHandle.good() ) {
            break;
          }
        }

        if( hasDigit ) {
          uint32_t time_ms = 0;
          fileHandle.seekg(pos);
          fileHandle >> time_ms;
          if( time_ms > 0 ) {
            // reset fileHandle and return the final time
            fileHandle.seekg(initialPos);
            return time_ms;
          }
        }
      } // fileHandle.good()

      // if we didn't return, that means we didn't get a digit, so go back further. Clear any error flags
      // first (in case we hit an EOF along the way)
      fileHandle.clear();
      fileHandle.seekg(pos);
      fileHandle.seekg(-1, std::ios_base::cur);
    }

    // not a newline, move backwards one character (-1 to undo the get, and -1 to move back)
    fileHandle.seekg(-2,std::ios_base::cur);
  }

  // didn't find any valid times, return 0
  return 0;
}


} // end namespace Vector
} // end namespace Anki
