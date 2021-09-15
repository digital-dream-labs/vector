#include "simulator/robot/sim_overlayDisplay.h"

// Webots Includes
#include <webots/Display.hpp>
#include <webots/Supervisor.hpp>

namespace Anki {
  namespace Vector {
    
    namespace Sim {
      extern webots::Supervisor* CozmoBot;
      
      namespace OverlayDisplay {
        
        namespace { // "Private members"
          
          // For Webots Display:
          const f32 OVERLAY_TEXT_SIZE = 0.08f;
          const u32 OVERLAY_TEXT_COLOR = 0xff0000;
          const u16 MAX_TEXT_DISPLAY_LENGTH = 1024;
          
          char displayText_[MAX_TEXT_DISPLAY_LENGTH];
          
          webots::Node* estPose_      = NULL;
          webots::Field* translation_ = NULL;
          webots::Field* rotation_    = NULL;
        }
        
        void Init(void)
        {
          estPose_ = CozmoBot->getFromDef("CozmoBotPose");
          if(estPose_ != NULL) {
            translation_ = estPose_->getField("translation");
            rotation_    = estPose_->getField("rotation");
          }
        }
        
        void SetText(TextID ot_id, const char* formatStr, ...)
        {
          va_list argptr;
          va_start(argptr, formatStr);
          vsnprintf(displayText_, MAX_TEXT_DISPLAY_LENGTH, formatStr, argptr);
          va_end(argptr);
          
          CozmoBot->setLabel(ot_id, displayText_, 0.6f,
                             0.05f + static_cast<f32>(ot_id) * (OVERLAY_TEXT_SIZE/3.f),
                             OVERLAY_TEXT_SIZE, OVERLAY_TEXT_COLOR, 0);
        } // SetText()
        
        void UpdateEstimatedPose(const f32 x, const f32 y, const f32 angle)
        {
          if(translation_ != NULL) {
            const double estTrans[3] = {x, 0, y};
            translation_->setSFVec3f(estTrans);
          }
          
          if(rotation_ != NULL) {
            const double estRot[4] = {0, 1, 0, angle};
            rotation_->setSFRotation(estRot);
          }
        }
        
      } // namespace OverlayDisplay
    } // namespace Sim
  } // namespace Vector
} // namespace Anki


