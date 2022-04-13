/**
 * File: messageHelpers.h
 *
 * Author: Brad Neuman
 * Created: 2016-04-18
 *
 * Description: Helpers for message types
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#ifndef __Cozmo_Basestation_MessageHelpers_H__
#define __Cozmo_Basestation_MessageHelpers_H__

#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/robotInterface/messageRobotToEngineTag.h"
#include <string>

namespace Anki {
namespace Vector {

inline const char* MessageTagToString(const ExternalInterface::MessageEngineToGameTag& tag)
{
  return MessageEngineToGameTagToString(tag);
}

inline const char* MessageTagToString(const ExternalInterface::MessageGameToEngineTag& tag)
{
  return MessageGameToEngineTagToString(tag);
}

inline const char* MessageTagToString(const RobotInterface::RobotToEngineTag& tag)
{
  return RobotToEngineTagToString(tag);
}
  
ExternalInterface::MessageEngineToGameTag GetEToGMessageTypeFromString(const char* inString);

}
}

#endif
