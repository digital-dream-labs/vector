

#ifndef VIZ_TEXTLABELTYPES_H
#define VIZ_TEXTLABELTYPES_H

namespace Anki {
namespace Vector {

// This enum is essentially an extension of the VizTextLabelType enum
// in vizControllerImpl.h but these can be used by Engine
enum class TextLabelType: u8 {
  ACTION,
  LOCALIZED_TO,
  WORLD_ORIGIN,
  VISION_MODE,
  BEHAVIOR_STATE,
  ANIMATION_NAME,
  NEEDS_STATE
};

}
}

#endif
