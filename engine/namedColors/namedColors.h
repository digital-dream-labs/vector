/**
* File: namedColors
*
* Author: damjan stulic
* Created: 9/16/15
*
* Description: 
*
* Copyright: Anki, inc. 2015
*
*/

#ifndef __Cozmo_Basestation_NamedColors_Namedcolors_H__
#define __Cozmo_Basestation_NamedColors_Namedcolors_H__

#include "coretech/common/engine/colorRGBA.h"

namespace Anki {
namespace NamedColors {

// Add some BlockWorld-specific named colors to the existing ones in Anki::NamedColors:
extern const ColorRGBA EXECUTED_PATH;
extern const ColorRGBA PREDOCKPOSE;
extern const ColorRGBA PRERAMPPOSE;
extern const ColorRGBA SELECTED_OBJECT;
extern const ColorRGBA BLOCK_BOUNDING_QUAD;
extern const ColorRGBA OBSERVED_QUAD;
extern const ColorRGBA ROBOT_BOUNDING_QUAD;
extern const ColorRGBA REPLAN_BLOCK_BOUNDING_QUAD;
extern const ColorRGBA LOCALIZATION_OBJECT;
extern const ColorRGBA DIRTY_OBJECT;
extern const ColorRGBA UNKNOWN_OBJECT;
  
} // end namespace NamedColors
} // end namespace Anki

#endif //__Cozmo_Basestation_NamedColors_Namedcolors_H__
