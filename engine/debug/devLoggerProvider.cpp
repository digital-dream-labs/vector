/**
* File: devLoggerProvider
*
* Author: Lee Crippen
* Created: 6/21/2016
*
* Description: Extension of the SaveToFileLoggerProvider to add milliseconds since app start to the front of messages
*
* Copyright: Anki, inc. 2016
*
*/

#include "engine/debug/devLoggerProvider.h"
#include "engine/debug/devLogConstants.h"
#include "engine/debug/devLoggingSystem.h"

#include <sstream>
#include <iomanip>

namespace Anki {
namespace Vector {
  
  
DevLoggerProvider::DevLoggerProvider(Util::Dispatch::Queue* queue, const std::string& baseDirectory, std::size_t maxFileSize)
: Util::SaveToFileLoggerProvider(queue, baseDirectory, maxFileSize)
{
}
  
void DevLoggerProvider::Log(ILoggerProvider::LogLevel logLevel, const std::string& message)
{
  size_t messageSize = message.size();
  
  std::ostringstream modMessageStream;
  modMessageStream << std::setfill('0') << std::setw(DevLogConstants::kNumLogTimestampDigits)
                   << DevLoggingSystem::GetAppRunMilliseconds() << " " << messageSize << " " << message;
  Util::SaveToFileLoggerProvider::Log(logLevel, modMessageStream.str());
}

} // end namespace Vector
} // end namespace Anki
