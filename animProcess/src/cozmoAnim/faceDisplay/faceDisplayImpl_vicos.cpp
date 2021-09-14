/**
 * File: faceDisplayImpl_vicos.cpp
 *
 * Author: Kevin Yoon
 * Created: 07/20/2017
 *
 * Description:
 *               Defines interface to simulated face display
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "cozmoAnim/faceDisplay/faceDisplayImpl.h"

#include "anki/cozmo/shared/factory/faultCodes.h"

#include "core/lcd.h"

#include "util/logging/logging.h"

namespace Anki {
namespace Vector {

  FaceDisplayImpl::FaceDisplayImpl()
  {
    int res = lcd_init();
    if(res < 0)
    {
      FaultCode::DisplayFaultCode(FaultCode::DISPLAY_FAILURE);
    }
  }

  FaceDisplayImpl::~FaceDisplayImpl()
  {
    FaceClear();
    lcd_shutdown();
  }
  
  void FaceDisplayImpl::FaceClear()
  {
    lcd_clear_screen();
  }
  
  void FaceDisplayImpl::FaceDraw(const u16* frame)
  {
    lcd_draw_frame2(frame, FACE_DISPLAY_WIDTH*FACE_DISPLAY_HEIGHT*sizeof(u16));
  }
  
  void FaceDisplayImpl::FacePrintf(const char* format, ...)
  {
    // Stub
  }

  void FaceDisplayImpl::SetFaceBrightness(int level)
  {
    lcd_set_brightness(level);
  }

} // namespace Vector
} // namespace Anki

extern "C" void core_common_on_exit(void)
{
  lcd_shutdown();
}
