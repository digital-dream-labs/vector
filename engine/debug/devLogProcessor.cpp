/**
* File: devLogProcessor
*
* Author: Lee Crippen
* Created: 6/22/2016
*
* Description: Functionality for processing dev logs
*
* Copyright: Anki, inc. 2016
*
*/
#include "engine/debug/devLogProcessor.h"

#include "engine/debug/devLoggingSystem.h"
#include "engine/debug/devLogReaderPrint.h"
#include "engine/debug/devLogReaderRaw.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"

namespace Anki {
namespace Vector {
  
DevLogProcessor::DevLogProcessor(const std::string& directory)
: _directoryName(directory)
{
  // The directory we've been given isn't valid so we're done
  if (!Util::FileUtils::DirectoryExists(_directoryName))
  {
    PRINT_NAMED_ERROR("DevLogProcessor.Constructor.InvalidDirectory", "Directory %s not found", _directoryName.c_str());
    _directoryName = "";
    return;
  }
  
  _vizMessageReader.reset(new DevLogReaderRaw(Util::FileUtils::FullFilePath( {_directoryName, DevLoggingSystem::kEngineToVizName} )));
  _printReader.reset(new DevLogReaderPrint(Util::FileUtils::FullFilePath( {_directoryName, DevLoggingSystem::kPrintName} )));

  _vizMessageReader->Init();
  _printReader->Init();
}

DevLogProcessor::~DevLogProcessor() = default;

bool DevLogProcessor::AdvanceTime(uint32_t time_ms)
{
  bool anyMoreMessages = false;
  if (_vizMessageReader)
  {
    anyMoreMessages |= _vizMessageReader->AdvanceTime(time_ms);
  }
  
  if (_printReader)
  {
    anyMoreMessages |= _printReader->AdvanceTime(time_ms);
  }
  
  return anyMoreMessages;
}

uint32_t DevLogProcessor::GetCurrPlaybackTime() const
{
  if( _vizMessageReader ) {
    return _vizMessageReader->GetCurrPlaybackTime();
  }
  if( _printReader ) {
    return _printReader->GetCurrPlaybackTime();
  }

  return 0;
}

uint32_t DevLogProcessor::GetFinalTime_ms() const
{
  uint32_t finalTime = 0;

  if( _vizMessageReader ) {
    finalTime = std::max(finalTime, _vizMessageReader->GetFinalTime());
  }
  
  if( _printReader ) {
    finalTime = std::max(finalTime, _printReader->GetFinalTime());
  }

  return finalTime;
}

uint32_t DevLogProcessor::GetNextPrintTime_ms() const
{
  if( _printReader ) {
    return _printReader->GetNextMessageTimeDelta_ms();
  }
  else {
    return 0;
  }
}

void DevLogProcessor::SetVizMessageCallback(DevLogReader::DataCallback callback)
{
  if (_vizMessageReader)
  {
    _vizMessageReader->SetDataCallback(callback);
  }
}
  
void DevLogProcessor::SetPrintCallback(DevLogReader::DataCallback callback)
{
  if (_printReader)
  {
    _printReader->SetDataCallback(callback);
  }
}
  
} // end namespace Vector
} // end namespace Anki
