#ifndef ANKI_SIM_OVERLAY_DISPLAY_H
#define ANKI_SIM_OVERLAY_DISPLAY_H

#include "coretech/common/shared/types.h"

namespace Anki {
  namespace Vector {
    namespace Sim {
      namespace OverlayDisplay {
        
        // Overlaid Text Display IDs:
        typedef enum {
          CURR_EST_POSE,
          CURR_TRUE_POSE,
          TARGET_POSE,
          PATH_ERROR,
          DEBUG_MSG
        } TextID;
        
        void Init(void);
        
        void SetText(TextID id, const char *formatStr, ...);
        
        void UpdateEstimatedPose(const f32 x, const f32 y, const f32 angle);
        
      } //OverlayDisplay
    } // namespace Sim
  } // namespace Vector
} // namespace Anki

#endif // ANKI_SIM_OVERLAY_DISPLAY_H
