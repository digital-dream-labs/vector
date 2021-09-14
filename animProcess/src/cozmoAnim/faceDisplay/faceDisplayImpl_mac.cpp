/**
 * File: faceDisplayImpl_mac.cpp
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
#include "coretech/vision/engine/colorPixelTypes.h"
#include "util/logging/logging.h"

#include <webots/Supervisor.hpp>
#include <webots/Display.hpp>


#ifndef SIMULATOR
#error SIMULATOR should be defined by any target using faceDisplay_mac.cpp
#endif

extern webots::Supervisor animSupervisor;

namespace Anki {
namespace Vector {
  
namespace { // "Private members"

  // Face display
  webots::Display* face_;
  
  // Face 'image' to send to webots each frame
  u32 faceImg_[FACE_DISPLAY_WIDTH*FACE_DISPLAY_HEIGHT] = {0};
  
} // "private" namespace


#pragma mark --- Simulated Hardware Method Implementations ---
  
  FaceDisplayImpl::FaceDisplayImpl()
  {
    // Did you remember to call SetSupervisor()?
//      DEV_ASSERT(animSupervisor != nullptr, "animSupervisor.NullWebotsSupervisor");
    
    // Is the step time defined in the world file >= than the robot time? It should be!
//      DEV_ASSERT(TIME_STEP >= animSupervisor->getBasicTimeStep(), "animSupervisor.UnexpectedTimeStep");

    // Face display
    face_ = animSupervisor.getDisplay("face_display");
    assert(face_->getWidth() == FACE_DISPLAY_WIDTH);
    assert(face_->getHeight() == FACE_DISPLAY_HEIGHT);
    face_->setFont("Lucida Console", 8, true);
    FaceClear();
  }

  FaceDisplayImpl::~FaceDisplayImpl() = default;

  void FaceDisplayImpl::FaceClear()
  {
    face_->setColor(0);
    face_->fillRectangle(0,0, FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT);
  }
  
  void FaceDisplayImpl::FaceDraw(const u16* frame)
  {
    // Convert an RGB565 color into a 32-bit BGRA color image (i.e. 0xBBGGRRAA) which webots expects
    u32* imgPtr = &faceImg_[0];
    for (u32 i = 0; i < FACE_DISPLAY_WIDTH*FACE_DISPLAY_HEIGHT; ++i) {
      Vision::PixelRGB565 rgb565(*frame);
      *imgPtr++ = rgb565.ToBGRA32();
      ++frame;
    }

    // Send the entire image to webots (by using the webots::Display 'clipboard' functionality),
    // paste it from the 'clipboard' to the main display, then delete it.
    // (see https://www.cyberbotics.com/doc/reference/display#display-functions)
    auto imgRef = face_->imageNew(FACE_DISPLAY_WIDTH, FACE_DISPLAY_HEIGHT, faceImg_, webots::Display::ARGB);
    face_->imagePaste(imgRef, 0, 0);
    face_->imageDelete(imgRef);
  }
  
  void FaceDisplayImpl::FacePrintf(const char* format, ...)
  {
    // TODO: Smartly insert line breaks?

    face_->setColor(0xf0ff);
   
    #define MAX_FACE_DISPLAY_CHAR_LENGTH 30
    char line[MAX_FACE_DISPLAY_CHAR_LENGTH];
    
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(line, MAX_FACE_DISPLAY_CHAR_LENGTH, format, argptr);
    va_end(argptr);
    
    face_->drawText(std::string(line), 0, 0);
  }

  void FaceDisplayImpl::SetFaceBrightness(int level)
  {
    // not supported for mac
  }

} // namespace Vector
} // namespace Anki
