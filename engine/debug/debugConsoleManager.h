/**
 * File: DebugConsoleManager
 *
 * Author: Molly Jameson
 * Created: 11/17/15
 *
 * Description: A singleton wrapper around the console so that it can use CLAD at
 * the game level instead of util. If you need to specify robotID for a console command it needs to be a function.
 *
 * Copyright: Anki, Inc. 2015
 *
 **/


#ifndef __Cozmo_Basestation_Debug_DebugConsoleManager_H__
#define __Cozmo_Basestation_Debug_DebugConsoleManager_H__

#include "util/signals/simpleSignal_fwd.h"
#include <vector>

namespace Anki {
namespace Vector {


  
template <typename Type>
class AnkiEvent;


namespace ExternalInterface {
  class MessageGameToEngine;
}
  
namespace RobotInterface {
  class MessageHandler;
}
  
class CozmoEngineHostImpl;
class IExternalInterface;
  
  
class DebugConsoleManager
{
//----------------------------------------------------------------------------------------------------------------------------
public:
  void Init( IExternalInterface* externalInterface, RobotInterface::MessageHandler* robotInterface );
  void HandleEvent(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event);
//----------------------------------------------------------------------------------------------------------------------------
private:
  void SendAllDebugConsoleVars();
  std::vector<Signal::SmartHandle> _signalHandles;
  IExternalInterface* _externalInterface = nullptr;
  RobotInterface::MessageHandler* _robotInterface = nullptr;
};
  

} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Basestation_Debug_DebugConsoleManager_H__

