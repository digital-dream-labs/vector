/**
 * File: cladLoggerProvider.cpp
 *
 * Author: Molly Jameson
 * Created: 12/6/16
 *
 * Description:
 *
 * Copyright: Anki, inc. 2016
 *
 */

#include "engine/debug/cladLoggerProvider.h"

#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"
#include "coretech/messaging/engine/IComms.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "util/console/consoleInterface.h"
#include <limits>

namespace Anki {
namespace Vector {
  
CONSOLE_VAR(bool, kEnableCladLogger, "Logging", true);


LogLevel ILoggerLevelToCladLevel(Anki::Util::ILoggerProvider::LogLevel logLevel)
{
  static_assert((int)Util::ILoggerProvider::LogLevel::LOG_LEVEL_DEBUG == (int)LogLevel::Debug, "Enum mismatch");
  static_assert((int)Util::ILoggerProvider::LogLevel::LOG_LEVEL_INFO == (int)LogLevel::Info, "Enum mismatch");
  static_assert((int)Util::ILoggerProvider::LogLevel::LOG_LEVEL_EVENT == (int)LogLevel::Event, "Enum mismatch");
  static_assert((int)Util::ILoggerProvider::LogLevel::LOG_LEVEL_WARN == (int)LogLevel::Warning, "Enum mismatch");
  static_assert((int)Util::ILoggerProvider::LogLevel::LOG_LEVEL_ERROR == (int)LogLevel::Error, "Enum mismatch");
  static_assert((int)Util::ILoggerProvider::LogLevel::_LOG_LEVEL_COUNT == LogLevelNumEntries, "Enum mismatch");
  
  const LogLevel res = (LogLevel)logLevel;
  return res;
}
  
  
void CLADLoggerProvider::Log(Anki::Util::ILoggerProvider::LogLevel logLevel, const std::string& message)
{
  if( kEnableCladLogger && (_externalInterface != nullptr) )
  {
    ExternalInterface::DebugAppendConsoleLogLine sendMsg(message, ILoggerLevelToCladLevel(logLevel));
    
    // This CLAD string is limited to 2^16-1 (65535) chars, and CLAD messages are limited
    // to Comms::MsgPacket::MAX_SIZE which is even smaller, so clamp the string if necessary.
    // (if we need very long strings, check the devlog instead).
    // kMaxStrLen must be small enough to fit in Comms::MsgPacket::MAX_SIZE
    // (along with additional tag + message overhead)
    const size_t kMaxStrLen = 2000;
    static_assert(kMaxStrLen < std::numeric_limits<uint16_t>::max(), "DebugAppendConsoleLogLine.line limited to u16 length");
    static_assert((kMaxStrLen + sizeof(ExternalInterface::DebugAppendConsoleLogLine)) <= Comms::MsgPacket::MAX_SIZE, "kMaxStrLen too big for packet");
    
    if( message.length() > kMaxStrLen )
    {
      sendMsg.line = message.substr(0,kMaxStrLen);
    }
    _externalInterface->BroadcastDeferred(ExternalInterface::MessageEngineToGame(std::move(sendMsg)));
  }
}

} // end namespace Vector
} // end namespace Anki
