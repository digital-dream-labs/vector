/**
* File: faceInfoScreen.h
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

#ifndef __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreen_H_
#define __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreen_H_

#include "coretech/common/shared/types.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenTypes.h"

#include "util/singleton/dynamicSingleton.h"

#include <functional>
#include <string>
#include <vector>

namespace Anki {

namespace Vision {
  class ImageRGB565;
}

namespace Vector {


class FaceInfoScreen {
public:

  // screenName:       Name of this scren
  // buttonGotoScreen: Name of the screen to go to when backpack button is pressed. (May be overridden in ProcessMenuNavigation())
  FaceInfoScreen(ScreenName screenName, ScreenName buttonGotoScreen);

  // Convenience ctor that allows you to specify simple non-menu text at the top of the screen
  FaceInfoScreen(ScreenName screenName, ScreenName buttonGotoScreen, const std::vector<std::string>& staticText);
  
  // Returns name of current screen
  ScreenName GetName() { return _name; }
  
  // Add menu item that transitions to gotoScreen when selected
  // Ordering of the items is determined by the order in which you call this function
  void AppendMenuItem(const std::string& text, ScreenName gotoScreen);

  // Add menu item that should execute a function and then goto the
  // screen that is returned by that function
  using MenuItemAction = std::function<ScreenName()>;
  void AppendMenuItem(const std::string& text, MenuItemAction action);
  
  // Specify functions to execute when entering or exiting the screen
  using ScreenAction = std::function<void()>;
  void SetEnterScreenAction(ScreenAction action) { _enterAction = action; }
  void SetExitScreenAction(ScreenAction action)  { _exitAction = action;  }

  // Should be called whenever entering/exiting this screen.
  // Resets cursor position, computes timeout, and executes custom enter/exit functions.
  void EnterScreen();
  void ExitScreen();
  
  // Indicates that the screen is timed out and that
  // we should transition to GetTimeoutScreen()
  bool IsTimedOut();
  
  // Specify the timeout duration and the screen to goto when it expires
  void SetTimeout(f32 seconds, ScreenName gotoScreen);
  
  // Restarts the timeout, if there is one
  void RestartTimeout();

  // Returns true if this screen has a menu item that was added via AppendMenuItem
  bool HasMenu() const;
  
  // Moves the menu cursor up/down
  void MoveMenuCursorUp();
  void MoveMenuCursorDown();
  
  // Draws the menu items and cursor onto the given image
  void DrawMenu(Vision::ImageRGB565& img) const;
  
  ScreenName GetButtonGotoScreen() const { return _buttonScreen;  }
  ScreenName GetTimeoutScreen()    const { return _timeoutScreen; }
  
  // Executes the action of the currently selected menu item,
  // if any was specified, and goes to the next screen
  ScreenName ConfirmMenuItemAndGetNextScreen();
  
  
private:
  // Name of this screen object
  ScreenName _name;
  
  // Screen to go to when button pressed
  ScreenName _buttonScreen;
  
  // Screen to go to when timeout expires
  ScreenName _timeoutScreen;

  // The duration of the timeout
  f32 _timeoutDuration_s;
  
  // The time at which the screen should timeout
  // Set when StartScreen() is called
  f32 _timeout_s;
  
  ScreenAction _enterAction = {};
  ScreenAction _exitAction = {};

  struct MenuItem {
    MenuItem(std::string text, MenuItemAction action)
    : text(text)
    , action(action)
    {}
    
    std::string text;
    MenuItemAction action;
  };
  
  std::vector<MenuItem> _menu;
  size_t _menuCursor;
  
  std::vector<std::string> _staticText;
};
  

} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreen_H_
