/**
 * File: actionDefinitions.h
 *
 * Author: Brad Neuman
 * Created: 2017-05-16
 *
 * Description: Common definitions related to actions
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Cozmo_Basestation_Actions_ActionDefinitions_H__
#define __Cozmo_Basestation_Actions_ActionDefinitions_H__

#include <functional>


namespace Anki {
namespace Vector {

namespace ExternalInterface {
struct RobotCompletedAction;
}

using ActionEndedCallback = std::function<void(const ExternalInterface::RobotCompletedAction&)>;

using ActionEndedCallbackID = int;

}
}



#endif
