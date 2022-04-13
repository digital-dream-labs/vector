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
#include "engine/debug/devLogReader.h"

#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
  
DevLogReader::DevLogReader(const std::string& directory)
: _directory(directory)
{
  // The directory we've been given isn't valid so we're done
  if (!Util::FileUtils::DirectoryExists(_directory))
  {
    return;
  }
  
  DiscoverLogFiles();
}

bool DevLogReader::AdvanceTime(uint32_t timestep_ms)
{
  _currTime_ms += timestep_ms;
  return UpdateForCurrentTime(_currTime_ms);
}

void DevLogReader::DiscoverLogFiles()
{
  _files.clear();
  if (Util::FileUtils::DirectoryExists(_directory))
  {
    auto fileList = Util::FileUtils::FilesInDirectory(_directory, true, "log" );
    for (const auto& file : fileList)
    {
      _files.push_back(file);
    }
    
    // Even though files *might* be sorted alphabetically by the readdir call inside FilesInDirectory,
    // we can't rely on it so do it ourselves
    std::sort(_files.begin(), _files.end());
  }
}

void DevLogReader::Init()
{
  if( ! _files.empty() ) {
    _currentLogFileHandle.open(_files.back());
    if (!_currentLogFileHandle.good())
    {
      PRINT_NAMED_ERROR("DevLogReader.Init.FailBitSet",
                        "Fail bit set on opening file %s",
                        _files.back().c_str());
    }

    _finalTime_ms = GetFinalTimestamp_ms(_currentLogFileHandle);

    // if there is only one file, we can leave this one open. Otherwise, we'll need to close it so
    // UpdateForCurrentTime will open the proper file
    if( _files.size() > 1 ) {
      _currentLogFileHandle.close();
    }
  }
}

bool DevLogReader::UpdateForCurrentTime(uint32_t time_ms)
{
  if (!_currentLogFileHandle.is_open())
  {
    if (_files.empty())
    {
      return false;
    }
    
    _currentLogFileHandle.open(_files.front());
    if (!_currentLogFileHandle.good())
    {
      PRINT_NAMED_ERROR("DevLogReader.UpdateForCurrentTime.FailBitSet",
                        "Fail bit set on opening file %s",
                        _files.front().c_str());
    }
  }
  
  while (_currentLogFileHandle.good())
  {
    // Keep extracting messages until Extract says no more!
    while (ExtractAndCallback(time_ms)) { }
    
    // If the log file is still good we decided to stop reading for a different reason (aka next message is for later)
    if (_currentLogFileHandle.good())
    {
      break;
    }
    
    // If this log file is done load up the next
    if (_currentLogFileHandle.eof())
    {
      _currentLogFileHandle.close();
      _files.pop_front();
      
      if (_files.empty())
      {
        // No more log files so we're done
        break;
      }
      
      _currentLogFileHandle.open(_files.front());
    }
    
    if (!_currentLogFileHandle.good())
    {
      PRINT_NAMED_ERROR("DevLogReader.UpdateForCurrentTime.FailBitSet","Fail bit set for file %s", _files.front().c_str());
    }
  }
  
  return !_files.empty();
}

uint32_t DevLogReader::GetNextMessageTimeDelta_ms() const
{
  if( _currentLogData.IsValid() ) {
    return _currentLogData._timestamp_ms - _currTime_ms;
  }
  else {
    // Just return 1 ms to force an update so we get more valid data
    return 1;
  }
}

// Returns true if a data message was extracted and callback called (because timestamp was earlier than passed in time)
bool DevLogReader::ExtractAndCallback(uint32_t time_ms)
{
  // If it's now time to deal with the data we've already been holding on to, do so.
  // (if CheckTimeAndCallback returns false we know it isn't time yet for the data, so return early)
  if (_currentLogData.IsValid() && !CheckTimeAndCallback(time_ms))
  {
    return false;
  }
  
  // We expect the current log data to be empty at this point
  DEV_ASSERT(!_currentLogData.IsValid(), "DevLogReader.ExtractAndCallback.StaleExtractedData");
  
  // Use the subclass implementation to fill out the next log data
  if (!FillLogData(_currentLogFileHandle, _currentLogData))
  {
    // Failure here means some kind of file error (possibly eof)
    _currentLogFileHandle.close();
    _currentLogData = {};
    return false;
  }
  
  // Now that we've retrieved more data, check the time and callback if we can
  return CheckTimeAndCallback(time_ms);
}
  
bool DevLogReader::CheckTimeAndCallback(uint32_t time_ms)
{
  // If we have some valid log data already but its not time yet we're done
  if (_currentLogData._timestamp_ms > time_ms)
  {
    return false;
  }
  
  if (_dataCallback)
  {
    _dataCallback(_currentLogData);
  }
  _currentLogData = {};
  
  return true;
}
} // end namespace Vector
} // end namespace Anki
