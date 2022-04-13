/**
* File: devLogReader
*
* Author: Lee Crippen
* Created: 6/22/2016
*
* Description: Functionality for pulling data out of a log file
*
* Copyright: Anki, inc. 2016
*
*/
#ifndef __Cozmo_Basestation_Debug_DevLogReader_H_
#define __Cozmo_Basestation_Debug_DevLogReader_H_

#include "util/helpers/noncopyable.h"

#include <string>
#include <cstdint>
#include <functional>
#include <vector>
#include <deque>
#include <fstream>

namespace Anki {
namespace Vector {

class DevLogReader: Util::noncopyable {
public:
  DevLogReader(const std::string& directory);
  virtual ~DevLogReader() { }
  
  const std::string& GetDirectoryName() const { return _directory; }
  
  void Init();

  // Move forward in time by number of milliseconds specified. Can trigger callbacks if they have been set
  // Returns whether there is more data in the logs to process
  bool AdvanceTime(uint32_t timestep_ms);
  
  struct LogData
  {
    uint32_t              _timestamp_ms = 0;
    std::vector<uint8_t>  _data;
    
    bool IsValid() const { return !_data.empty(); }
  };
  
  using DataCallback = std::function<void(const LogData&)>;
  void SetDataCallback(DataCallback callback) { _dataCallback = callback; }

  // return the current playback time
  uint32_t GetCurrPlaybackTime() const { return _currTime_ms; }

  // return the last known timestamp in this log. Note that some loggers may return 0 if they can't easily
  // calculate the total time
  uint32_t GetFinalTime() const { return _finalTime_ms; }

  // return the delta between current time and the next message, aka how much time we should advance to
  // (hopefully) see another message. Note that this may return less than the given time (e.g. if it needs to
  // read a new log file)
  uint32_t GetNextMessageTimeDelta_ms() const;

protected:
  virtual bool FillLogData(std::ifstream& fileHandle, LogData& logData_out) const = 0;

  // called on init with a stream to the last file in the log. Function can do what it wants with the file,
  // but must leave the stream in the same state it found it. Should return the final timestamp contained in
  // the log, or 0 if it can't process.
  virtual uint32_t GetFinalTimestamp_ms(std::ifstream& fileHandle) const { return 0; }
  

private:
  std::string             _directory;
  DataCallback            _dataCallback;
  std::deque<std::string> _files;
  uint32_t                _currTime_ms = 0;
  uint32_t                _finalTime_ms = 0;
  std::ifstream           _currentLogFileHandle;
  LogData                 _currentLogData;
  
  void DiscoverLogFiles();
  bool UpdateForCurrentTime(uint32_t time_ms);
  bool ExtractAndCallback(uint32_t time_ms);
  bool CheckTimeAndCallback(uint32_t time_ms);
};

} // end namespace Vector
} // end namespace Anki


#endif //__Cozmo_Basestation_Debug_DevLogReader_H_
