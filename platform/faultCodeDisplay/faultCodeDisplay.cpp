/**
* File: faultCodeDisplay.cpp
*
* Author: Al Chaussee
* Date:   7/26/2018
*
* Description: Displays the first argument to the screen
*
* Copyright: Anki, Inc. 2018
**/

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/factory/faultCodes.h"
#include "core/lcd.h"
#include "coretech/vision/engine/image.h"

#include "opencv2/highgui.hpp"

#include <getopt.h>
#include <inttypes.h>
#include <unordered_map>

namespace Anki {
namespace Vector {

namespace {
  constexpr const char * kSupportURL = "support.ddl.io";
  constexpr const char * kVectorWillRestart = "Vector will restart";

  // Map of fault codes that map to images that should be drawn instead of the number
  std::unordered_map<uint16_t, std::string> kFaultImageMap = {
    {FaultCode::SHUTDOWN_BATTERY_CRITICAL_TEMP, "/anki/data/assets/cozmo_resources/config/sprites/independentSprites/battery_overheated.png"},
    {FaultCode::SHUTDOWN_BATTERY_CRITICAL_VOLT, "/anki/data/assets/cozmo_resources/config/sprites/independentSprites/battery_low.png"},
  };
}

void DrawFaultCode(uint16_t fault, bool willRestart)
{
  // Image in which the fault code is drawn
  static Vision::ImageRGB img(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);

  img.FillWith(0);

  // Draw the fault code centered horizontally
  const std::string faultString = std::to_string(fault);
  Vec2f size = Vision::Image::GetTextSize(faultString, 1.5,  1);
  img.DrawTextCenteredHorizontally(faultString,
				   CV_FONT_NORMAL,
				   1.5,
				   2,
				   NamedColors::WHITE,
				   (FACE_DISPLAY_HEIGHT/2 + size.y()/4),
				   false);

  // Draw text centered horizontally and slightly above
  // the bottom of the screen
  const std::string & text = (willRestart ? kVectorWillRestart : kSupportURL);
  const int font = CV_FONT_NORMAL;
  const f32 scale = 0.5f;
  const int thickness = 1;
  const auto color = NamedColors::WHITE;
  const bool drawTwice = false;

  size = Vision::Image::GetTextSize(text, scale, thickness);
  img.DrawTextCenteredHorizontally(text, font, scale, thickness, color, FACE_DISPLAY_HEIGHT - size.y(), drawTwice);

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

}
}

extern "C" void core_common_on_exit(void)
{
  // Don't shutdown the lcd in order to keep the fault code displayed
  //lcd_shutdown();
}

void usage(FILE * f)
{
  fprintf(f, "Usage: vic-faultCodeDisplay [-hr] nnn\n");
}

int main(int argc, char * argv[])
{
  using namespace Anki::Vector;

  bool willRestart = false;
  char c;

  // Process any options
  while ((c = getopt (argc, argv, "hr")) != -1)
  {
    switch (c) {
      case 'h':
        usage(stdout);
        return 0;
      case 'r':
        willRestart = true;
        break;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  // Process remaining arguments
  const char * s = nullptr;
  for (int index = optind; index < argc; ++index) {
    if (s != nullptr) {
      usage(stderr);
      return -1;
    }
    s = argv[index];
  }

  // Validate argument
  if (s == nullptr) {
    usage(stderr);
    return -1;
  }

  // Convert first argument from a string to a uint16_t
  auto res = strtoumax(s, nullptr, 10);
  if (res > std::numeric_limits<uint16_t>::max() || res == 0) {
    usage(stderr);
    return -1;
  }

  uint16_t code = (uint16_t)res;

  lcd_init();

  // See if an image or number should be drawn
  bool imageDrawn = false;
  auto faultImageIt = kFaultImageMap.find(code);
  if (faultImageIt != kFaultImageMap.end()) {
    imageDrawn = DrawImage(faultImageIt->second);
  }

  // Draw fault code if no image to draw or DrawImage() failed
  if (!imageDrawn) {
    DrawFaultCode(code, willRestart);
  }

  // Don't shutdown the lcd in order to keep the fault code displayed
  //lcd_shutdown();

  return 0;
}
