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


#include "engine/namedColors/namedColors.h"


namespace Anki {
namespace NamedColors {

// Add some BlockWorld-specific named colors:
const ColorRGBA EXECUTED_PATH
  (1.f, 0.0f, 0.0f, 1.0f);    // red
const ColorRGBA PREDOCKPOSE
  (1.f, 0.0f, 0.0f, 0.75f);   // clear red
const ColorRGBA PRERAMPPOSE
  (0.f, 0.0f, 1.0f, 0.75f);   // clear blue
const ColorRGBA SELECTED_OBJECT
  (0.f, 1.0f, 0.0f, 0.75f);   // clear green
const ColorRGBA BLOCK_BOUNDING_QUAD
  (0.f, 0.0f, 1.0f, 0.75f);   // clear blue
const ColorRGBA OBSERVED_QUAD
  (1.f, 0.0f, 0.0f, 0.75f);   // clear red
const ColorRGBA ROBOT_BOUNDING_QUAD
  (0.f, 0.8f, 0.0f, 0.75f);   // clear dark green
const ColorRGBA REPLAN_BLOCK_BOUNDING_QUAD
  (1.f, 0.1f, 1.0f, 0.75f);   // clear magenta
const ColorRGBA LOCALIZATION_OBJECT
  (1.0, 0.0f, 1.0f, 1.0f);    // magenta
const ColorRGBA UNKNOWN_OBJECT
  (0.7f, 0.7f, 0.7f);         // light gray
const ColorRGBA DIRTY_OBJECT
  (0.4f, 0.4f, 0.4f);         // dark gray
  
} // end namespace NamedColors
} // end namespace Anki
