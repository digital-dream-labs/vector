/**
 * File: DasToSdkHandler
 *
 * Author: Jason Lipak
 * Created: 09/09/16
 *
 * Description: A handler for SDK messages which will simply send back das json files to the SDK.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Cozmo_Basestation_Debug_DasToSdkHandler_H__
#define __Cozmo_Basestation_Debug_DasToSdkHandler_H__

#include "util/signals/simpleSignal_fwd.h"
#include <vector>

namespace Anki {
namespace Vector {

template <typename Type>
class AnkiEvent;


namespace ExternalInterface {
  class MessageGameToEngine;
}
class IExternalInterface;
  
class DasToSdkHandler
{
//----------------------------------------------------------------------------------------------------------------------------
public:
  void Init( IExternalInterface* externalInterface );
  void HandleEvent(const AnkiEvent<ExternalInterface::MessageGameToEngine>& event);
//----------------------------------------------------------------------------------------------------------------------------
private:
  void SendJsonDasLogsToSdk();
  std::vector<Signal::SmartHandle> _signalHandles;
  IExternalInterface* _externalInterface = nullptr;
};
  

} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Basestation_Debug_DasToSdkHandler_H__

