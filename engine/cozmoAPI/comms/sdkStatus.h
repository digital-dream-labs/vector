/**
 * File: sdkStatus
 *
 * Author: Mark Wesley
 * Created: 08/30/16
 *
 * Description: Status of the SDK connection and usage
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Cozmo_Game_Comms_SdkStatus_H__
#define __Cozmo_Game_Comms_SdkStatus_H__


#include "clad/types/sdkStatusTypes.h"
#include "util/container/circularBuffer.h"
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace Anki {
namespace Vector {

  
class IExternalInterface;

namespace ExternalInterface
{
  class MessageGameToEngine;
  struct UiDeviceConnectionWrongVersion;
  enum class MessageEngineToGameTag : uint8_t;
  enum class MessageGameToEngineTag : uint8_t;
} // namespace ExternalInterface

  
class ISocketComms;
  

class SdkStatus
{
public:
  
  SdkStatus();
  ~SdkStatus() {}
  
  void OnWrongVersion(const ExternalInterface::UiDeviceConnectionWrongVersion& message);

  void SetStatus(SdkStatusType statusType, std::string&& statusText)
  {
    const uint32_t idx = (uint32_t)statusType;
    _sdkStatusStrings[idx] = std::move(statusText);
  }
  
  const std::string& GetStatus(SdkStatusType statusType) const
  {
    const uint32_t idx = (uint32_t)statusType;
    return _sdkStatusStrings[idx];
  }

private:
    
  Util::CircularBuffer<ExternalInterface::MessageGameToEngineTag>  _recentCommands;

  std::string   _connectedSdkBuildVersion;
  std::string   _sdkStatusStrings[SdkStatusTypeNumEntries];
};
  
  
} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Game_Comms_SdkStatus_H__

