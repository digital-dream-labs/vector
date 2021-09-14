/**
 * File: backpackLightComponentTypes.h
 *
 * Author: Lee Crippen
 * Created: 2/13/2017
 *
 * Description: Types related to managing various lights on Cozmo's body.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_Basestation_Components_BackpackLightComponentTypes_H__
#define __Anki_Cozmo_Basestation_Components_BackpackLightComponentTypes_H__

#include "clad/types/backpackAnimationTriggers.h"
#include <list>
#include <map>
#include <memory>

namespace Anki {
namespace Vector {

// Some of our Util functions use clad features that are only generated for regular Cpp
// not CppLite such as EnumFromString so we need to write our own
bool EnumFromString(const std::string& string, BackpackAnimationTrigger& trigger);
  
namespace Anim {
struct BackpackLightData;
class BackpackLightComponent;

using BackpackLightSourceType = int32_t;
  
// Public facing types that can be used when setting a backpack light configuration
// Should anything within animation process need to start setting backpack lights
// a new source should be added here and in GetLightSourcePriority()
enum class BackpackLightSource : BackpackLightSourceType
{    
  Count
};

using BackpackLightDataRef     = std::shared_ptr<BackpackLightData>;
using BackpackLightDataRefWeak = std::weak_ptr<BackpackLightData>;

using BackpackLightList = std::list<BackpackLightDataRef>;
using BackpackLightMap  = std::map<BackpackLightSourceType, BackpackLightList>;

class BackpackLightDataLocator
{
public:
  bool IsValid() const { return !_dataPtr.expired(); }
  
private:
  friend class BackpackLightComponent;
  
  BackpackLightMap::iterator          _mapIter;
  BackpackLightList::iterator         _listIter;
  BackpackLightDataRefWeak            _dataPtr;
  
}; // class LightDataLocator

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_Basestation_Components_BackpackLightComponentTypes_H__
