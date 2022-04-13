/**
* File: externalInterface_fwd
*
* Author: raul
* Created: 03/25/16
*
* Description: Forward declaration for common external interface classes.
*
* Copyright: Anki, inc. 2015
*
*/

#ifndef __Anki_Cozmo_Basestation_ExternalInterface_ExternalInterfaceFwd_H__
#define __Anki_Cozmo_Basestation_ExternalInterface_ExternalInterfaceFwd_H__

#include <cstdint>

namespace Anki {
namespace Vector {

namespace ExternalInterface {
class MessageEngineToGame;
class MessageGameToEngine;
enum class MessageEngineToGameTag : uint8_t;
enum class MessageGameToEngineTag : uint8_t;
} // end namespace ExternalInterface

class IExternalInterface;

} // end namespace Vector
} // end namespace Anki

#endif //__Anki_Cozmo_Basestation_ExternalInterface_ExternalInterfaceFwd_H__
