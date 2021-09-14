/**
 * File: backpackLightComponentTypes.cpp
 *
 * Author: Al Chaussee
 * Created: 8/6/2018
 *
 * Description: Types related to managing various lights on Cozmo's body.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "clad/types/backpackAnimationTriggers.h"
#include <map>

namespace Anki {
namespace Vector {

bool EnumFromString(const std::string& string, BackpackAnimationTrigger& trigger)
{
  for(int i = 0; i < (int)BackpackAnimationTrigger::Count; ++i)
  {
    BackpackAnimationTrigger i_trigger = static_cast<BackpackAnimationTrigger>(i);
    if(std::strcmp(EnumToString(i_trigger), string.c_str()) == 0)
    {
      trigger = i_trigger;
      return true;
    }
  }
  return false;
}

}
}
