/**
* File: miniFaceDisplay.cpp
*
* Author: chapados
* Date:   02/22/2019
*
* Description: Minimal face display functionality to support emergency pairing in a 
*              fault-code situation.
*
* Copyright: Anki, Inc. 2019
**/

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/factory/faultCodes.h"
#include "core/lcd.h"
#include "coretech/vision/engine/image.h"
#include "rescue/pairing_icon_key.h"
#include "opencv2/highgui.hpp"

#include <getopt.h>
#include <inttypes.h>
#include <unordered_map>

namespace Anki {
namespace Vector {

namespace {
  constexpr const char * kSupportURL = "support.ddl.io";
  constexpr const char * kVectorWillRestart = "Vector will restart";

  const f32 kRobotNameScale = 0.6f;
  const std::string kAppURL = "ddl.io/v";
  const ColorRGBA kWhiteColor(0.9f, 0.9f, 0.9f, 1.f);
  const int kTextThickness = 1;
  const int kNormalFont = CV_FONT_NORMAL;
  const bool kDrawTwice = false;
  const int kRobotNameVerticalPosition = 15;
}

extern "C" void core_common_on_exit(void)
{
  lcd_shutdown();
}

void DrawFaultCode(uint16_t fault, bool willRestart)
{
  // Image in which the fault code is drawn
  static Vision::ImageRGB img(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);

  img.FillWith(0);

  // Draw the fault code centered horizontally
  const std::string faultString = std::to_string(fault);
  Vec2f size = Vision::Image::GetTextSize(faultString, 1.5f, kTextThickness);
  img.DrawTextCenteredHorizontally(faultString,
				   kNormalFont,
				   1.5f,
				   2,
				   NamedColors::WHITE,
				   (FACE_DISPLAY_HEIGHT/2 + size.y()/4),
				   false);

  // Draw text centered horizontally and slightly above
  // the bottom of the screen
  const std::string& text = (willRestart ? kVectorWillRestart : kSupportURL);
  const f32 scale = 0.5f;

  size = Vision::Image::GetTextSize(text, scale, kTextThickness);
  img.DrawTextCenteredHorizontally(text, 
            kNormalFont,
            scale,
            kTextThickness,
            NamedColors::WHITE,
            FACE_DISPLAY_HEIGHT - size.y(),
            kDrawTwice);

  Vision::ImageRGB565 img565(img);
  lcd_draw_frame2(reinterpret_cast<u16*>(img565.GetDataPointer()), img565.GetNumRows() * img565.GetNumCols() * sizeof(u16));
}

bool DrawImage(std::string& image_path)
{
  Vision::ImageRGB565 img565;
  if (img565.Load(image_path) != RESULT_OK) {
    return false;
  }

  // Fail if the image isn't the right size
  if (img565.GetNumCols() != FACE_DISPLAY_WIDTH || img565.GetNumRows() != FACE_DISPLAY_HEIGHT) {
    return false;
  }

  lcd_draw_frame2(reinterpret_cast<u16*>(img565.GetDataPointer()), img565.GetNumRows() * img565.GetNumCols() * sizeof(u16));
  return true;
}

// Draws BLE name and url to screen
bool DrawStartPairingScreen(const std::string& robotName)
{
  if(robotName.empty())
  {
    return false;
  }
  
  auto img = Vision::ImageRGBA(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
  img.FillWith(Vision::PixelRGBA(0, 0));

  img.DrawTextCenteredHorizontally(robotName, kNormalFont, kRobotNameScale, kTextThickness, kWhiteColor, kRobotNameVerticalPosition, kDrawTwice);

  cv::Size textSize;
  float scale = 0;
  Vision::Image::MakeTextFillImageWidth(kAppURL, kNormalFont, kTextThickness, img.GetNumCols(), textSize, scale);
  img.DrawTextCenteredHorizontally(kAppURL, kNormalFont, scale, kTextThickness, kWhiteColor, (FACE_DISPLAY_HEIGHT + textSize.height)/2, true);

  Vision::ImageRGB565 img565(img);
  lcd_draw_frame2(reinterpret_cast<u16*>(img565.GetDataPointer()),
                  img565.GetNumRows() * img565.GetNumCols() * sizeof(u16));

  return true;
}

// Draws BLE name, key icon, and BLE pin to screen
void DrawShowPinScreen(const std::string& robotName, const std::string& pin)
{
  if(robotName.empty() || pin.empty())
  {
    return;
  }

  // Somehow read the pairing icon
  Vision::ImageRGB key = Vision::ImageRGB(Vision::Image(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH, pairing_icon_key_gray));

  auto img = Vision::ImageRGBA(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);
  img.FillWith(Vision::PixelRGBA(0, 0));

  Point2f p((FACE_DISPLAY_WIDTH - key.GetNumCols())/2,
            (FACE_DISPLAY_HEIGHT - key.GetNumRows())/2);
  img.DrawSubImage(key, p);

  img.DrawTextCenteredHorizontally(robotName, kNormalFont, kRobotNameScale, kTextThickness, kWhiteColor, kRobotNameVerticalPosition, kDrawTwice);

  img.DrawTextCenteredHorizontally(pin, kNormalFont, 0.8f, kTextThickness, kWhiteColor, FACE_DISPLAY_HEIGHT-5, kDrawTwice);

  Vision::ImageRGB565 img565(img);
  lcd_draw_frame2(reinterpret_cast<u16*>(img565.GetDataPointer()),
                  img565.GetNumRows() * img565.GetNumCols() * sizeof(u16));
}

}
}
