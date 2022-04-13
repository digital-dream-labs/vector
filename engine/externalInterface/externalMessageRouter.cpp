/**
* File: externalMessageRouter.cpp
*
* Author: ross
* Created: may 31 2018
*
* Description: Templates to automatically wrap messages included in the MessageEngineToGame union
*              and the GatewayWrapper protobuf oneof (union) based on external requirements and
*              event organization (the hierarchy in clad and proto files)
 *             TODO: remove clad portions once messages are converted
*
* Copyright: Anki, Inc. 2018
*/

#include "engine/externalInterface/externalMessageRouter.h"
#include "osState/wallTime.h"

namespace Anki {
namespace Vector {

uint32_t ExternalMessageRouter::GetTimestampUTC()
{
  auto time = WallTime::getInstance()->GetApproximateTime();
  return (uint32_t)std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

}
}
