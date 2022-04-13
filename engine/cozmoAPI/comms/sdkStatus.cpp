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


#include "engine/externalInterface/externalInterface.h"
#include "engine/cozmoAPI/comms/sdkStatus.h"
#include "engine/cozmoAPI/comms/iSocketComms.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "util/logging/logging.h"
#include "util/time/universalTime.h"

namespace Anki {
namespace Vector {

SdkStatus::SdkStatus()
  : _recentCommands(10) // TODO Remove
{
}


void SdkStatus::OnWrongVersion(const ExternalInterface::UiDeviceConnectionWrongVersion& message)
{
  Util::sInfoF("robot.sdk_wrong_version", {{DDATA, message.buildVersion.c_str()}}, "");
  _connectedSdkBuildVersion = message.buildVersion;
}

} // namespace Vector
} // namespace Anki
