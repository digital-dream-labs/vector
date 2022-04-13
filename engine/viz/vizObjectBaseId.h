/**
* File: vizObjectBaseId
*
* Author: damjan stulic
* Created: 9/16/15
*
* Description:
*
* Copyright: Anki, inc. 2015
*
*/

#ifndef __Cozmo_Basestation_Viz_VizObjectBaseId_H__
#define __Cozmo_Basestation_Viz_VizObjectBaseId_H__

#include "clad/types/vizTypes.h"
#include <stdint.h>

namespace Anki {
namespace Vector {

// Base IDs for each VizObject type
extern const uint32_t VizObjectBaseID[(int)VizObjectType::NUM_VIZ_OBJECT_TYPES+1];

} // end namespace Vector
} // end namespace Anki

#endif //__Cozmo_Basestation_Viz_VizObjectBaseId_H__
