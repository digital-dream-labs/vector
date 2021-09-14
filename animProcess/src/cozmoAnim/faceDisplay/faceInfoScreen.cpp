/**
* File: faceInfoScreen.cpp
*
* Author: Kevin Yoon
* Created: 03/02/2018
*
* Description: Face screen object for Customer Support Info / Debug screens that
*              defines menu text, menu item selection behavior, and screen timeouts.
*              Except for menu text, which is drawn with DrawMenu() to the given image,
*              other screen content is draw separately and is handled primarily
*              by FaceInfoScreenManager
*
* Copyright: Anki, Inc. 2018
*
*/
#include "anki/cozmo/shared/cozmoConfig.h"
#include "cozmoAnim/faceDisplay/faceInfoScreen.h"
#include "coretech/common/shared/math/rect.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/vision/engine/image.h"

namespace Anki {
namespace Vector {


FaceInfoScreen::FaceInfoScreen(ScreenName name,
                               ScreenName buttonGotoScreen)
:FaceInfoScreen(name, buttonGotoScreen, {})
{
}

  
FaceInfoScreen::FaceInfoScreen(ScreenName name,
                               ScreenName buttonGotoScreen,
                               const std::vector<std::string>& staticText)
: _name(name)
, _buttonScreen(buttonGotoScreen)
, _timeoutScreen(ScreenName::None)
, _timeoutDuration_s(kDefaultScreenTimeoutDuration_s)
, _timeout_s(0.f)
, _menuCursor(0)
, _staticText(staticText)
{

}

void FaceInfoScreen::EnterScreen()
{
  _menuCursor = 0;
  
  // Only set timeout if the timeout screen is different from the current screen.
  if (_timeoutScreen != _name) {
    RestartTimeout();
  }

  if (_enterAction) {
    _enterAction();
  }
}

void FaceInfoScreen::ExitScreen()
{
  _menuCursor = 0;
  
  if (_exitAction) {
    _exitAction();
  }
}

bool FaceInfoScreen::IsTimedOut()
{
  if (_timeout_s > 0.f) {
    const auto currTime_s = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
    return currTime_s > _timeout_s;
  }
  return false;
}
  
void FaceInfoScreen::SetTimeout(f32 seconds, ScreenName gotoScreen)
{
  _timeoutDuration_s = seconds;
  _timeoutScreen = gotoScreen;
}
  
void FaceInfoScreen::RestartTimeout()
{
  if (_timeoutDuration_s > 0.f) {
    const auto currTime_s = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
    _timeout_s = currTime_s + _timeoutDuration_s;
  }
}
  
void FaceInfoScreen::AppendMenuItem(const std::string& text, ScreenName gotoScreen)
{
  AppendMenuItem(text, [gotoScreen](){ return gotoScreen;});
}
  
void FaceInfoScreen::AppendMenuItem(const std::string& text, MenuItemAction action)
{
  _menu.emplace_back(text, action);
}
  
void FaceInfoScreen::DrawMenu(Vision::ImageRGB565& img) const
{
  const ColorRGBA& menuBgColor = NamedColors::BLACK;
  const ColorRGBA& menuItemColor = NamedColors::WHITE;
  const f32 locX = 10;
  const f32 stepY = 11;
  const f32 textScale = 0.4f;

  f32 locY = stepY;
  for (auto& text : _staticText) {
    img.DrawText({0,locY}, text, menuItemColor, textScale);
    locY += stepY;
  }
  
  if (HasMenu()) {
    // Draw menu items (bottom-aligned)
    locY = FACE_DISPLAY_HEIGHT-1;
    for (s32 i = static_cast<s32>(_menu.size()) - 1; i >= 0; --i) {
      
      if (_menuCursor == i) {
        // Draw cursor
        img.DrawText({0,locY}, ">", menuItemColor, textScale);
      } else {
        const Rectangle<f32> rect( 0.f, locY-stepY, FACE_DISPLAY_WIDTH, stepY);
        img.DrawFilledRect(rect, menuBgColor);
      }

      // Draw menu item text
      img.DrawText({locX, locY}, _menu[i].text, menuItemColor, textScale);
      locY -= stepY;
    }
  }
}

bool FaceInfoScreen::HasMenu() const
{
  return !_menu.empty();
}
  
void FaceInfoScreen::MoveMenuCursorUp()
{
  if (HasMenu()) {
    if (_menuCursor == 0) {
      _menuCursor = _menu.size()-1;
    } else {
      --_menuCursor;
    }
  }
}
void FaceInfoScreen::MoveMenuCursorDown()
{
  if (HasMenu()) {
    ++_menuCursor;
    if (_menuCursor == _menu.size()) {
      _menuCursor = 0;
    }
  }
}

ScreenName FaceInfoScreen::ConfirmMenuItemAndGetNextScreen()
{
  DEV_ASSERT(HasMenu(), "FaceInfoScreen.ConfirmMenuItemAndGetNextScreen.MustHaveMenu");
  
  auto& item = _menu[_menuCursor];
  return item.action();
}
  
} // namespace Vector
} // namespace Anki
