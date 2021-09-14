/**
* File: faceInfoScreenManager.h
*
* Author: Lee Crippen
* Created: 12/19/2017
*
* Description: Handles navigation and drawing of the Customer Support Menu / Debug info screens.
*
* Usage: Add drawing functionality as needed from various components. 
*        Add a corresponding ScreenName in faceInfoScreenTypes.h.
*        In the new drawing functionality, return early if the ScreenName does not match appropriately.
*
* Copyright: Anki, Inc. 2017
*
*/

#ifndef __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreenManager_H_
#define __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreenManager_H_

#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/common/shared/types.h"
#include "coretech/common/engine/colorRGBA.h"
#include "coretech/common/shared/math/point_fwd.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenTypes.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "clad/cloud/mic.h"
#include "clad/types/tofDisplayTypes.h"

#include "util/singleton/dynamicSingleton.h"

#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Anki {

namespace Vision {
  class ImageRGB565;
}

namespace Vector {

namespace Anim {
  class AnimContext;
  class AnimationStreamer;
}
class FaceInfoScreen;
  
namespace RobotInterface {
  struct MicData;
  struct MicDirection;
}

namespace WebService {
  class WebService;
}
  
  
class FaceInfoScreenManager : public Util::DynamicSingleton<FaceInfoScreenManager> {
  
  ANKIUTIL_FRIEND_SINGLETON(FaceInfoScreenManager); // Allows base class singleton access
  
public:
  FaceInfoScreenManager();

  void Init(Anim::AnimContext* context, Anim::AnimationStreamer* animStreamer);
  void Update(const RobotState& state);
  
  // Debug drawing is expected from only one thread
  ScreenName GetCurrScreenName() const;

  // Returns true if the current screen is one that
  // FaceInfoScreenManager is responsible for drawing to.
  // Otherwise, it's assumed that someone else is doing it.
  bool IsActivelyDrawingToScreen() const;

  void SetShouldDrawFAC(bool draw);
  void SetCustomText(const RobotInterface::DrawTextOnScreen& text);  
  void SetNetworkStatus(const CloudMic::ConnectionCode& code);

  // When BLE pairing mode is enabled/disabled, this screen should
  // be called so that the physical inputs (head, lift, button) wheels
  // are handled appropriately. 
  // The FaceInfoScreenManager otherwise does nothing since drawing on 
  // screen is handled by ConnectionFlow when in pairing mode.
  void EnablePairingScreen(bool enable);
  
  // When enabled, switches to a screen showing the alexa pairing code, and optionally the URL,
  // depending on how the auth process originated (app or voice command)
  void EnableAlexaScreen(ScreenName screenName, const std::string& code, const std::string& url);

  // turn mute on or off (reason sent to DAS)
  void ToggleMute(const std::string& reason);
  
  void StartAlexaNotification();

  // When enabled, switches to a special camera screen used to show
  // the vision system's "mirror mode", which displays the camera feed
  // and detections live on the robot's face. 
  void EnableMirrorModeScreen(bool enable);
  
  // Begin drawing functionality
  // These functions update the screen only if they are relevant to the current screen
  void DrawConfidenceClock(const RobotInterface::MicDirection& micData,
                           float bufferFullPercent,
                           uint32_t secondsRemaining,
                           bool triggerRecognized);
  void DrawMicInfo(const RobotInterface::MicData& micData);
  void DrawCameraImage(const Vision::ImageRGB565& img);

  void DrawToF(const RangeDataDisplay& data);
  
  // Sets the power mode message to send when returning to none screen
  void SetCalmPowerModeOnReturnToNone(const RobotInterface::CalmPowerMode& msg) { _calmModeMsgOnNone = msg; }

  void SelfTestEnd(Anim::AnimationStreamer* animStreamer);

  // Note when the engine has finished loading for internal use
  void OnEngineLoaded() {_engineLoaded = true;}

  void SetSysconVersion(const std::string& version) { _sysconVersion = version; }
  
  // Forcibly exit any screen
  void ExitCCScreen(Anim::AnimationStreamer* animStreamer);

private:
  const Anim::AnimContext* _context = nullptr;
  
  std::unique_ptr<Vision::ImageRGB565> _scratchDrawingImg;

  bool IsDebugScreen(ScreenName screen) const;

  // Sets the current screen
  void SetScreen(ScreenName screen);
  
  // Gets the current screen
  FaceInfoScreen* GetScreen(ScreenName name);
  
  // Resets the lift and head angles observed thus far.
  // Called everytime the screen changes.
  void ResetObservedHeadAndLiftAngles();

  // Detects various button events
  // Beyond return pressed and released events it also detects when a single button press
  // is detected vs. a double button press. Note that a doublePressDetected does not 
  // coincide with two singlePressDetected's.
  void CheckForButtonEvent(const bool buttonPressed, 
                           bool& buttonPressedEvent,
                           bool& buttonReleasedEvent,
                           bool& singlePressDetected, 
                           bool& doublePressDetected);
  
  // Returns true if screenName is one of the screens that allow the user to enter pairing when
  // double pressing the backpack and on the charger
  bool CanEnterPairingFromScreen( const ScreenName& screenName) const;
  
  // Returns true if screenName is an Alexa screen
  bool IsAlexaScreen(const ScreenName& screenName) const;
  
  // Returns true if screenName is a screen that should cause the behavior system to Wait.
  // Note that Pairing is handled another way, so is not included here.
  bool ScreenNeedsWait(const ScreenName& screenName) const;

  // Process wheel, head, lift, button motion for menu navigation
  void ProcessMenuNavigation(const RobotState& state);
  u32 _wheelMovingForwardsCount;
  u32 _wheelMovingBackwardsCount;
  bool _liftTriggerReady;
  bool _headTriggerReady;
  
  // Flag indicating when debug screens have been unlocked
  bool _debugInfoScreensUnlocked;

  // Power mode to set when returning to None screen
  RobotInterface::CalmPowerMode _calmModeMsgOnNone;
  
  // Map of all screen names to their associated screen objects
  std::unordered_map<ScreenName, FaceInfoScreen> _screenMap;
  FaceInfoScreen* _currScreen;

  // Internal draw functions that
  void DrawFAC();
  void DrawMain();
  void DrawNetwork();
  void DrawSensorInfo(const RobotState& state);
  void DrawIMUInfo(const RobotState& state);
  void DrawMotorInfo(const RobotState& state);
  void DrawCustomText();
  void DrawAlexaFace();
  void DrawMuteAnimation();
  void DrawAlexaNotification();
  
  // Draw the _scratchDrawingImg to the face
  void DrawScratch();

  // Updates the FAC screen if needed
  void UpdateFAC();

  void UpdateCameraTestMode(uint32_t curTime_ms);
  
  static const Point2f kDefaultTextStartingLoc_pix;
  static const u32 kDefaultTextSpacing_pix;
  static const f32 kDefaultTextScale;

  // Helper methods for drawing debug data to face
  void DrawTextOnScreen(const std::vector<std::string>& textVec, 
                        const ColorRGBA& textColor = NamedColors::WHITE,
                        const ColorRGBA& bgColor = NamedColors::BLACK,
                        const Point2f& loc = kDefaultTextStartingLoc_pix,
                        u32 textSpacing_pix = kDefaultTextSpacing_pix,
                        f32 textScale = kDefaultTextScale);

  struct ColoredText {
    ColoredText(const std::string& text,
                const ColorRGBA& color = NamedColors::WHITE,
                bool leftAlign = true)
    : text(text)
    , color(color)
    , leftAlign(leftAlign)
    {}

    const std::string text;
    const ColorRGBA color;
    const bool leftAlign;
  };

  using ColoredTextLines = std::vector<std::vector<ColoredText> >;
  void DrawTextOnScreen(const ColoredTextLines& lines, 
                        const ColorRGBA& bgColor = NamedColors::BLACK,
                        const Point2f& loc = kDefaultTextStartingLoc_pix,
                        u32 textSpacing_pix = kDefaultTextSpacing_pix,
                        f32 textScale = kDefaultTextScale);

  RobotInterface::DrawTextOnScreen _customText;
  WebService::WebService* _webService;
  
  Anim::AnimationStreamer* _animationStreamer = nullptr;
  
  std::string _alexaCode;
  std::string _alexaUrl;
  
  bool _drawFAC = false;
  bool _engineLoaded = false;

  std::string _sysconVersion = "";
  
  // Reboot Linux
  void Reboot();

};

} // namespace Vector
} // namespace Anki

#endif // __AnimProcess_CozmoAnim_FaceDisplay_FaceInfoScreenManager_H_
