/**
* File: faceDebugDraw.cpp
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

#include "cozmoAnim/alexa/alexa.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/backpackLights/animBackpackLightComponent.h"
#include "cozmoAnim/connectionFlow.h"
#include "cozmoAnim/faceDisplay/faceDisplay.h"
#include "cozmoAnim/faceDisplay/faceInfoScreen.h"
#include "cozmoAnim/faceDisplay/faceInfoScreenManager.h"
#include "cozmoAnim/micData/micDataSystem.h"
#include "cozmoAnim/robotDataLoader.h"

#include "micDataTypes.h"

#include "coretech/common/shared/array2d.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/vision/engine/image.h"
#include "util/console/consoleInterface.h"
#include "util/console/consoleSystem.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/templateHelpers.h"
#include "util/internetUtils/internetUtils.h"
#include "util/logging/DAS.h"

#include "clad/robotInterface/messageRobotToEngine.h"
#include "clad/robotInterface/messageEngineToRobot_sendAnimToRobot_helper.h"
#include "clad/robotInterface/messageRobotToEngine_sendAnimToEngine_helper.h"

#include "json/json.h"
#include "osState/osState.h"
#include "osState/wallTime.h"
#include "opencv2/highgui.hpp"

#include "anki/cozmo/shared/factory/emrHelper.h"
#include "anki/cozmo/shared/factory/faultCodes.h"

#include "webServerProcess/src/webService.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>

#ifndef SIMULATOR
#include <linux/reboot.h>
#include <sys/reboot.h>
#endif


// Log options
#define LOG_CHANNEL    "FaceInfoScreenManager"

// Forces transition to BLE pairing screen on double button press
// without waiting for actual START_PAIRING message from switchboard.
// Mainly useful in sim, where there is currently no switchboard.
#ifdef SIMULATOR
#define FORCE_TRANSITION_TO_PAIRING 1
#else
#define FORCE_TRANSITION_TO_PAIRING 0
#endif

#define ENABLE_SELF_TEST 1

#if !FACTORY_TEST

#endif

namespace Anki {
namespace Vector {

// Default values for text rendering
const Point2f FaceInfoScreenManager::kDefaultTextStartingLoc_pix = {0,10};
const u32 FaceInfoScreenManager::kDefaultTextSpacing_pix = 11;
const f32 FaceInfoScreenManager::kDefaultTextScale = 0.4f;

namespace {
  // Number of tics that a wheel needs to be moving for before it registers
  // as a signal to move the menu cursor
  const u32 kMenuCursorMoveCountThresh = 10;

  const f32 kWheelMotionThresh_mmps = 3.f;

  const f32 kMenuLiftRange_rad = DEG_TO_RAD(45);
  f32 _liftLowestAngle_rad;
  f32 _liftHighestAngle_rad;

  const f32 kMenuHeadRange_rad = DEG_TO_RAD(55);
  f32 _headLowestAngle_rad;
  f32 _headHighestAngle_rad;

  const f32 kMenuAngularTriggerThresh_rad = DEG_TO_RAD(5);

  // Variables for performing connectivity checks in threads
  // and triggering redrawing of screens
  std::atomic<bool> _redrawNetwork{false};
  std::atomic<bool> _testingNetwork{true};
  std::atomic<CloudMic::ConnectionCode> _networkStatus{CloudMic::ConnectionCode::Connectivity};

  // How often connectivity checks are performed while on 
  // Main and Network screens.
  const u32 kIPCheckPeriod_sec = 20;
  
  const f32 kAlexaTimeout_s = 5.0f;

  const char* kAlexaIconSpriteName = "face_alexa_icon";

  // TODO (VIC-11606): don't use timeout for mute
  CONSOLE_VAR_RANGED(f32, kToggleMuteTimeout_s, "FaceInfoScreenManager", 1.2f, 0.001f, 3.0f);
  CONSOLE_VAR_RANGED(f32, kAlexaNotificationTimeout_s, "FaceInfoScreenManager", 2.0f, 0.001f, 3.0f);

  // How long the button needs to be pressed for before it should trigger shutdown animation
  CONSOLE_VAR( u32, kButtonPressDurationForShutdown_ms, "FaceInfoScreenManager", 500 );
#if ANKI_DEV_CHEATS
  // Fake one of several types of button presses. This value will get reset immediately, so to
  // run it again from the web interface, first set it to NoOp
  CONSOLE_VAR_ENUM(int, kFakeButtonPressType, "FaceInfoScreenManager", 0, "NoOp,singlePressDetected,doublePressDetected");
#endif
}


FaceInfoScreenManager::FaceInfoScreenManager()
: _scratchDrawingImg(new Vision::ImageRGB565())
, _wheelMovingForwardsCount(0)
, _wheelMovingBackwardsCount(0)
, _liftTriggerReady(false)
, _headTriggerReady(false)
, _debugInfoScreensUnlocked(false)
, _currScreen(nullptr)
, _webService(nullptr)
{
  _scratchDrawingImg->Allocate(FACE_DISPLAY_HEIGHT, FACE_DISPLAY_WIDTH);

  _calmModeMsgOnNone.enable = false;

  memset(&_customText, 0, sizeof(_customText));
}


void FaceInfoScreenManager::Init(Anim::AnimContext* context, Anim::AnimationStreamer* animStreamer)
{
  DEV_ASSERT(context != nullptr, "FaceInfoScreenManager.Init.NullContext");

  _context = context;
  _animationStreamer = animStreamer;
  
  // allow us to send debug info out to the web server
  _webService = context->GetWebService();

  #define ADD_SCREEN(name, gotoScreen) \
    _screenMap.emplace(std::piecewise_construct, \
                       std::forward_as_tuple(ScreenName::name), \
                       std::forward_as_tuple(ScreenName::name, ScreenName::gotoScreen));

  #define ADD_SCREEN_WITH_TEXT(name, gotoScreen, staticText) \
  { \
    std::vector<std::string> temp = staticText; \
    _screenMap.emplace(std::piecewise_construct, \
                       std::forward_as_tuple(ScreenName::name), \
                       std::forward_as_tuple(ScreenName::name, ScreenName::gotoScreen, temp)); \
  }

  #define ADD_MENU_ITEM(screen, itemText, gotoScreen) \
    GetScreen(ScreenName::screen)->AppendMenuItem(itemText, ScreenName::gotoScreen);

  #define ADD_MENU_ITEM_WITH_ACTION(screen, itemText, action) \
    GetScreen(ScreenName::screen)->AppendMenuItem(itemText, action);

  #define SET_TIMEOUT(screen, timeout_sec, timeoutScreen) \
    GetScreen(ScreenName::screen)->SetTimeout(timeout_sec, ScreenName::timeoutScreen);

  #define DISABLE_TIMEOUT(screen) \
    GetScreen(ScreenName::screen)->SetTimeout(0.f, ScreenName::screen);

  #define SET_ENTER_ACTION(screen, lambda) \
    GetScreen(ScreenName::screen)->SetEnterScreenAction(lambda);

  #define SET_EXIT_ACTION(screen, lambda) \
    GetScreen(ScreenName::screen)->SetExitScreenAction(lambda);

  // =============== Screens ==================
  // Screens we don't want users to have access to
  // * Microphone visualization
  // * Camera
  const bool hideSpecialDebugScreens = (FACTORY_TEST && Factory::GetEMR()->fields.PLAYPEN_PASSED_FLAG) || !ANKI_DEV_CHEATS;  // TODO: Use this line in master
  //const bool hideSpecialDebugScreens = (FACTORY_TEST && Factory::GetEMR()->fields.PLAYPEN_PASSED_FLAG);                        // Use this line in factory branch

  ADD_SCREEN_WITH_TEXT(Recovery, Recovery, {"RECOVERY MODE"});
  ADD_SCREEN(None, None);
  ADD_SCREEN(Pairing, Pairing);
  ADD_SCREEN(FAC, None);
  ADD_SCREEN(CustomText, None);
  ADD_SCREEN(Main, Network);
  ADD_SCREEN_WITH_TEXT(ClearUserData, Main, {"CLEAR USER DATA?"});
  ADD_SCREEN_WITH_TEXT(ClearUserDataFail, Main, {"CLEAR USER DATA FAILED"});
  ADD_SCREEN_WITH_TEXT(Rebooting, Rebooting, {"REBOOTING..."});
  ADD_SCREEN_WITH_TEXT(SelfTest, Main, {"START SELF TEST?"});
  ADD_SCREEN(SelfTestRunning, SelfTestRunning)
  ADD_SCREEN(Network, SensorInfo);
  ADD_SCREEN(SensorInfo, IMUInfo);
  ADD_SCREEN(IMUInfo, MotorInfo);
  ADD_SCREEN(MotorInfo, MicInfo);
  ADD_SCREEN(MirrorMode, MirrorMode);
  ADD_SCREEN(AlexaPairing, AlexaPairing);
  ADD_SCREEN(AlexaPairingSuccess, AlexaPairingSuccess);
  ADD_SCREEN(AlexaPairingFailed, AlexaPairingFailed);
  ADD_SCREEN(AlexaPairingExpired, AlexaPairingExpired);
  ADD_SCREEN(ToggleMute, ToggleMute);
  ADD_SCREEN(AlexaNotification, AlexaNotification);
  
  if (hideSpecialDebugScreens) {
    ADD_SCREEN(MicInfo, Main); // Last screen cycles back to Main
  } else {
    ADD_SCREEN(MicInfo, MicDirectionClock);
  }

  ADD_SCREEN(MicDirectionClock, Camera);
  ADD_SCREEN(CameraMotorTest, Camera);
  
  if(IsWhiskey())
  {
    ADD_SCREEN(Camera, ToF);
    ADD_SCREEN(ToF, Main);    // Last screen cycles back to Main
  }
  else
  {
    ADD_SCREEN(Camera, Main);
  }


  // ========== Screen Customization ========= 
  // Enter/Exit fcns, menu items, timeouts

  // === None screen ===
  auto noneEnterFcn = [this]() {
    // Restore power mode as specified by engine
    SendAnimToRobot(_calmModeMsgOnNone);

    if (FACTORY_TEST) {
      InitConnectionFlow(_animationStreamer);
    }
  };
  auto noneExitFcn = []() {
    // Disable calm mode
    RobotInterface::CalmPowerMode msg;
    msg.enable = false;
    SendAnimToRobot(std::move(msg));
  };
  SET_ENTER_ACTION(None, noneEnterFcn);
  SET_EXIT_ACTION(None, noneExitFcn);

  // === FAC screen ===
  auto facEnterFcn = [this]() {
    DrawFAC();
  };
  SET_ENTER_ACTION(FAC, facEnterFcn);
  DISABLE_TIMEOUT(FAC);


  // === Pairing screen ===
  // Never timeout. Let switchboard handle timeouts.
  DISABLE_TIMEOUT(Pairing);


  // === Custom screen ===
  // Give the customText screen an exit action of resetting its timeout back to default, 
  // since elsewhere we modify it when using it
  auto customTextEnterFcn = [this]() {
    DrawCustomText();
  };
  auto customTextExitFcn = [this]() {
    SET_TIMEOUT(CustomText, kDefaultScreenTimeoutDuration_s, None);
  };
  SET_ENTER_ACTION(CustomText, customTextEnterFcn);
  SET_EXIT_ACTION(CustomText, customTextExitFcn);

  // === Main screen ===
  auto mainEnterFcn = [this]() {
    DrawMain();
  };
  SET_ENTER_ACTION(Main, mainEnterFcn);

  ADD_MENU_ITEM(Main, "EXIT", None);
#if ENABLE_SELF_TEST
  ADD_MENU_ITEM(Main, "RUN SELF TEST", SelfTest);
#endif
  ADD_MENU_ITEM(Main, "CLEAR USER DATA", ClearUserData);

  // === Self test screen ===
  ADD_MENU_ITEM(SelfTest, "EXIT", Main);
  FaceInfoScreen::MenuItemAction confirmSelfTest = [animStreamer, this]() {
    animStreamer->Abort();
    animStreamer->EnableKeepFaceAlive(false, 0);
    _context->GetBackpackLightComponent()->SetSelfTestRunning(true);
    RobotInterface::SendAnimToEngine(RobotInterface::StartSelfTest());
    return ScreenName::SelfTestRunning;
  };
  ADD_MENU_ITEM_WITH_ACTION(SelfTest, "CONFIRM", confirmSelfTest);
  DISABLE_TIMEOUT(SelfTestRunning);
  
  // Clear User Data menu
  FaceInfoScreen::MenuItemAction confirmClearUserData = [this]() {
    // Write this file to indicate that the data partition should be wiped on reboot
    if (!Util::FileUtils::WriteFile("/run/wipe-data", "1")) {
      LOG_WARNING("FaceInfoScreenManager.ClearUserData.Failed", "");
      return ScreenName::ClearUserDataFail;
    }

    // Reboot robot for clearing to take effect
    LOG_INFO("FaceInfoScreenManager.ClearUserData.Rebooting", "");
    this->Reboot();
    return ScreenName::Rebooting;
  };
  ADD_MENU_ITEM(ClearUserData, "EXIT", Main);
  ADD_MENU_ITEM_WITH_ACTION(ClearUserData, "CONFIRM", confirmClearUserData);
  SET_TIMEOUT(ClearUserDataFail, 2.f, Main);


  // === Network screen ===
  auto networkEnterFcn = [this]() {
    DrawNetwork();
  };
  SET_ENTER_ACTION(Network, networkEnterFcn);

  // === Recovery screen ===
  FaceInfoScreen::MenuItemAction rebootAction = [this]() {
    LOG_INFO("FaceInfoScreenManager.Recovery.Rebooting", "");
    this->Reboot();

    return ScreenName::Rebooting;
  };
  ADD_MENU_ITEM_WITH_ACTION(Recovery, "EXIT", rebootAction);
  ADD_MENU_ITEM(Recovery, "CONTINUE", None);
  DISABLE_TIMEOUT(Recovery);

    
  // === Camera screen ===
  FaceInfoScreen::ScreenAction cameraEnterAction = [this]() {
    StreamCameraImages m;
    m.enable = true;
    RobotInterface::SendAnimToEngine(std::move(m));
    _animationStreamer->RedirectFaceImagesToDebugScreen(true);
  };
  auto cameraExitAction = [this]() {
    StreamCameraImages m;
    m.enable = false;
    RobotInterface::SendAnimToEngine(std::move(m));
    _animationStreamer->RedirectFaceImagesToDebugScreen(false);
  };
  SET_ENTER_ACTION(Camera, cameraEnterAction);
  SET_EXIT_ACTION(Camera, cameraExitAction);
  
  // === Mirror Mode ===
  // Engine requests this screen so it is assumed that Engine is already
  // set to send us images
  FaceInfoScreen::ScreenAction mirrorEnterAction = [this]() {
    _animationStreamer->RedirectFaceImagesToDebugScreen(true);
  };
  auto mirrorExitAction = [this]() {
    _animationStreamer->RedirectFaceImagesToDebugScreen(false);
  };
  SET_ENTER_ACTION(MirrorMode, mirrorEnterAction);
  SET_EXIT_ACTION(MirrorMode, mirrorExitAction);
  DISABLE_TIMEOUT(MirrorMode); // Let toggling the associated VisionMode handle turning this on/off
  
  // === AlexaPairing ===
  auto alexaEnterAction = [this]() {
    DrawAlexaFace();
  };
  SET_ENTER_ACTION(AlexaPairing,        alexaEnterAction);
  SET_ENTER_ACTION(AlexaPairingSuccess, alexaEnterAction);
  SET_ENTER_ACTION(AlexaPairingFailed,  alexaEnterAction);
  SET_ENTER_ACTION(AlexaPairingExpired, alexaEnterAction);
  DISABLE_TIMEOUT(AlexaPairing); // let the authorization process handle timeout
  SET_TIMEOUT(AlexaPairingSuccess, kAlexaTimeout_s, None);
  SET_TIMEOUT(AlexaPairingFailed,  kAlexaTimeout_s, None);
  SET_TIMEOUT(AlexaPairingExpired, kAlexaTimeout_s, None);
  
  // === Toggling mute ===
  auto toggleMuteEnterAction = [this]() {
    DrawMuteAnimation();
  };
  SET_ENTER_ACTION(ToggleMute, toggleMuteEnterAction);
  // TODO (VIC-11606): don't use timeout and instead wait for mute anim to end
  SET_TIMEOUT(ToggleMute, kToggleMuteTimeout_s, None);
  
  // === AlexaNotification ===
  auto alexaNotification = [this]() {
    DrawAlexaNotification();
  };
  SET_ENTER_ACTION(AlexaNotification, alexaNotification);
  SET_TIMEOUT(AlexaNotification, kAlexaNotificationTimeout_s, None);
  
  // === Camera Motor Test ===
  // Add menu item to camera screen to start a test mode where the motors run back and forth
  // and camera images are streamed to the face
  ADD_MENU_ITEM(Camera, "TEST MODE", CameraMotorTest);
  SET_TIMEOUT(CameraMotorTest, 300.f, None);

  auto cameraMotorTestExitAction = [cameraExitAction]() {
    cameraExitAction();
    SendAnimToRobot(RobotInterface::StopAllMotors());
  };
  SET_ENTER_ACTION(CameraMotorTest, cameraEnterAction);
  SET_EXIT_ACTION(CameraMotorTest, cameraMotorTestExitAction);

  if(IsWhiskey())
  {
    // ToF screen 
    FaceInfoScreen::ScreenAction enterToFScreen = []() {
                                                    RobotInterface::SendRangeData msg;
                                                    msg.enable = true;
                                                    RobotInterface::SendAnimToEngine(std::move(msg));
                                                  };
    SET_ENTER_ACTION(ToF, enterToFScreen);

    // ToF screen 
    FaceInfoScreen::ScreenAction exitToFScreen = []() {
                                                   RobotInterface::SendRangeData msg;
                                                   msg.enable = false;
                                                   RobotInterface::SendAnimToEngine(std::move(msg));
                                                 };
    SET_EXIT_ACTION(ToF, exitToFScreen);
  }

  
  // Check if we booted in recovery mode
  if (OSState::getInstance()->IsInRecoveryMode()) {
    LOG_WARNING("FaceInfoScreenManager.Init.RecoveryModeFileFound", "Going into recovery mode");
    SetScreen(ScreenName::Recovery);
  } else {
    SetScreen(ScreenName::None);
  }
}

FaceInfoScreen* FaceInfoScreenManager::GetScreen(ScreenName name)
{
  auto it = _screenMap.find(name);
  DEV_ASSERT(it != _screenMap.end(), "FaceInfoScreenManager.GetScreen.ScreenNotFound");

  return &(it->second);
}

void FaceInfoScreenManager::SetNetworkStatus(const CloudMic::ConnectionCode& code)
{
  _networkStatus = code;
  _testingNetwork = false;
  _redrawNetwork = true;
}

bool FaceInfoScreenManager::IsActivelyDrawingToScreen() const
{
  switch(GetCurrScreenName()) {
    case ScreenName::None:
    case ScreenName::Pairing:
    case ScreenName::ToggleMute:
    case ScreenName::AlexaNotification:
    case ScreenName::SelfTestRunning:
      return false;
    default:
      return true;
  }
}

void FaceInfoScreenManager::SetShouldDrawFAC(bool draw)
{
  // TODO(Al): Remove once BC is written to persistent storage and it is easy to revert robots
  // to factory firmware to rerun them through playpen
  if(!FACTORY_TEST)
  {
    return;
  }

  bool changed = (_drawFAC != draw);
  _drawFAC = draw;

  if(changed && GetCurrScreenName() != ScreenName::Recovery)
  {
    if(draw)
    {
      SetScreen(ScreenName::FAC);
    }
    else
    {
      SetScreen(ScreenName::None);
    }
  }
}

// Returns true if the screen is of the type during which the lift should be disabled
// and engine behaviors disabled.
bool FaceInfoScreenManager::IsDebugScreen(ScreenName screen) const
{
  switch(screen) {
    case ScreenName::None:
    case ScreenName::FAC:
    case ScreenName::CustomText:
      return false;
    default:
      return true;
  }
}

void FaceInfoScreenManager::SetScreen(ScreenName screen)
{
  bool prevScreenIsDebug = false;
  bool prevScreenNeedsWait = false;
  bool prevScreenWasMute = false;

  // Call ExitScreen
  // _currScreen may be null on the first call of this function
  if (_currScreen != nullptr) {
    if (screen == GetCurrScreenName()) {
      return;
    }
    _currScreen->ExitScreen();
    prevScreenIsDebug = IsDebugScreen(GetCurrScreenName());
    prevScreenNeedsWait = ScreenNeedsWait(GetCurrScreenName());
    prevScreenWasMute = GetCurrScreenName() == ScreenName::ToggleMute;
  }

  _currScreen = GetScreen(screen);

  // If currScreen is null now, you probably haven't called Init yet!
  DEV_ASSERT(_currScreen != nullptr, "FaceInfoScreenManager.SetScreen.NullCurrScreen");

  // Special handling for FAC screen to takeover None screen
  if(_drawFAC && GetCurrScreenName() == ScreenName::None)
  {
    _currScreen = GetScreen(ScreenName::FAC);
  }

  // Tell engine if the screen changes so behaviors can be appropriately enabled/disabled
  bool currScreenIsDebug = IsDebugScreen(GetCurrScreenName());
  bool currScreenNeedsWait = ScreenNeedsWait(GetCurrScreenName());
  if ((currScreenIsDebug != prevScreenIsDebug) || (currScreenNeedsWait != prevScreenNeedsWait)) {
    DebugScreenMode msg;
    msg.isDebug = currScreenIsDebug;
    msg.needsWait = currScreenNeedsWait;
    // leaving the mute screen via single press may coincide with the start of a wake word trigger, so don't clear it
    msg.fromMute = prevScreenWasMute;
    RobotInterface::SendAnimToEngine(std::move(msg));
  }

#ifndef SIMULATOR
  // Enable/Disable lift
  RobotInterface::EnableMotorPower msg;
  msg.motorID = MotorID::MOTOR_LIFT;
  msg.enable = (!currScreenIsDebug ||
                GetCurrScreenName() == ScreenName::CameraMotorTest ||
                GetCurrScreenName() == ScreenName::SelfTestRunning);
  SendAnimToRobot(std::move(msg));
#endif

  _scratchDrawingImg->FillWith(0);
  DrawScratch();

  LOG_INFO("FaceInfoScreenManager.SetScreen.EnteringScreen", "%hhu", GetCurrScreenName());
  _currScreen->EnterScreen();

  if(!IsAlexaScreen(GetCurrScreenName())) {
    // when exiting alexa screens (say, into pairing), cancel any pending alexa authorization
    auto* alexa = _context->GetAlexa();
    if (alexa != nullptr) {
      alexa->CancelPendingAlexaAuth("LEFT_CODE_SCREEN");
    }
  }

  ResetObservedHeadAndLiftAngles();

  // Clear menu navigation triggers
  _headTriggerReady = false;
  _liftTriggerReady = false;
  _wheelMovingForwardsCount = 0;
  _wheelMovingBackwardsCount = 0;

}

void FaceInfoScreenManager::DrawFAC()
{
  DrawTextOnScreen({"FAC"},
                   NamedColors::BLACK,
                   (Factory::GetEMR()->fields.PLAYPEN_PASSED_FLAG ? 
		    NamedColors::GREEN : NamedColors::RED),
                   { 0, FACE_DISPLAY_HEIGHT-10 },
                   10,
                   3.f);
}

void FaceInfoScreenManager::UpdateFAC()
{
  static bool prevPlaypenPassedFlag = Factory::GetEMR()->fields.PLAYPEN_PASSED_FLAG;
  const bool curPlaypenPassedFlag = Factory::GetEMR()->fields.PLAYPEN_PASSED_FLAG;

  if(curPlaypenPassedFlag != prevPlaypenPassedFlag)
  {
    DrawFAC();
  }
  
  prevPlaypenPassedFlag = curPlaypenPassedFlag;
}
  
void FaceInfoScreenManager::DrawCameraImage(const Vision::ImageRGB565& img)
{
  if (GetCurrScreenName() != ScreenName::Camera &&
      GetCurrScreenName() != ScreenName::CameraMotorTest &&
      GetCurrScreenName() != ScreenName::MirrorMode) {
    return;
  }

  _scratchDrawingImg->SetFromImageRGB565(img);
  DrawScratch();
}

void FaceInfoScreenManager::DrawConfidenceClock(
  const RobotInterface::MicDirection& micData,
  float bufferFullPercent,
  uint32_t secondsRemaining,
  bool triggerRecognized)
{
  // since we're always sending this data to the server, let's compute the max confidence
  // values each time and send the pre-computed values to the web server so that if any
  // of these default values change the server gets them too

  const auto& confList = micData.confidenceList;
  const auto& winningIndex = micData.direction;
  auto maxCurConf = (float)micData.confidence;
  for (int i=0; i<12; ++i)
  {
    if (maxCurConf < confList[i])
    {
      maxCurConf = confList[i];
    }
  }

  // Calculate the current scale for the bars to use, based on filtered and current confidence levels
  constexpr float filteredConfScale = 2.0f;
  constexpr float confMaxDefault = 1000.f;
  static auto filteredConf = confMaxDefault;
  filteredConf = ((0.98f * filteredConf) + (0.02f * maxCurConf));
  auto maxConf = filteredConf * filteredConfScale;
  if (maxConf < maxCurConf)
  {
    maxConf = maxCurConf;
  }
  if (maxConf < confMaxDefault)
  {
    maxConf = confMaxDefault;
  }

  // pre-calc the delay time as well for use in both web server and face debug ...
  const auto maxDelayTime_ms = (float) MicData::kRawAudioPerBuffer_ms;
  const auto delayTime_ms = (int) (maxDelayTime_ms * bufferFullPercent);


  if (nullptr != _webService)
  {
    using namespace std::chrono;

    static const std::string kWebVizModuleName = "micdata";
    if (_webService->IsWebVizClientSubscribed(kWebVizModuleName))
    {
      // if we send this data every tick, we crash the robot;
      // only send the web data every X seconds
      static double nextWebServerUpdateTime = 0.0;
      const double currentTime = BaseStationTimer::getInstance()->GetCurrentTimeInSecondsDouble();
      if (currentTime > nextWebServerUpdateTime)
      {
        nextWebServerUpdateTime = currentTime + 0.1;
        
        Json::Value webData;
        webData["time"] = currentTime;
        webData["confidence"] = micData.confidence;
        webData["activeState"] = micData.activeState;
        // 'direction' is the strongest direction, whereas 'selectedDirection' is what's being used (locked-in)
        webData["direction"] = micData.direction;
        webData["selectedDirection"] = micData.selectedDirection;
        webData["maxConfidence"] = maxConf;
        webData["triggerDetected"] = triggerRecognized;
        webData["delayTime"] = delayTime_ms;
        webData["latestPowerValue"] = (double)micData.latestPowerValue;
        webData["latestNoiseFloor"] = (double)micData.latestNoiseFloor;
        
        Json::Value& directionValues = webData["directions"];
        for ( float confidence : micData.confidenceList )
        {
          directionValues.append(confidence);
        }
        
        // Beat Detection stuff
        Json::Value& beatInfo = webData["beatDetector"];
        const auto& latestBeat = _context->GetMicDataSystem()->GetLatestBeatInfo();
        beatInfo["confidence"] = latestBeat.confidence;
        beatInfo["tempo_bpm"] = latestBeat.tempo_bpm;
        
        _webService->SendToWebViz( kWebVizModuleName, webData );
      }
    }
  }

  if (secondsRemaining > 0)
  {
    const auto drawText = std::string(" ") + std::to_string(secondsRemaining);

    RobotInterface::DrawTextOnScreen drawTextData{};
    drawTextData.drawNow = true;
    drawTextData.textColor.r = NamedColors::WHITE.r();
    drawTextData.textColor.g = NamedColors::WHITE.g();
    drawTextData.textColor.b = NamedColors::WHITE.b();
    drawTextData.bgColor.r = NamedColors::BLACK.r();
    drawTextData.bgColor.g = NamedColors::BLACK.g();
    drawTextData.bgColor.b = NamedColors::BLACK.b();
    std::copy(drawText.c_str(), drawText.c_str() + drawText.length(), &(drawTextData.text[0]));
    drawTextData.text[drawText.length()] = '\0';
    drawTextData.text_length = drawText.length();

    SET_TIMEOUT(CustomText, (1.f + (float)secondsRemaining), None);
    SetCustomText(drawTextData);

    return;
  }

  if (GetCurrScreenName() != ScreenName::MicDirectionClock)
  {
    return;
  }

  DEV_ASSERT(_scratchDrawingImg != nullptr, "FaceInfoScreenManager::DrawConfidenceClock.InvalidScratchImage");
  Vision::ImageRGB565& drawImg = *_scratchDrawingImg;
  const auto& clearColor = NamedColors::BLACK;
  drawImg.FillWith( {clearColor.r(), clearColor.g(), clearColor.b()} );

  const Point2i center_px = { FACE_DISPLAY_WIDTH / 2, FACE_DISPLAY_HEIGHT / 2 };
  constexpr int circleRadius_px = 40;
  constexpr int innerRadius_px = 5;
  constexpr int maxBarLen_px = circleRadius_px - innerRadius_px - 4;
  constexpr int barWidth_px = 3;
  constexpr float angleFactorA = 0.866f; // cos(30 degrees)
  constexpr float angleFactorB = 0.5f; // sin(30 degrees)
  constexpr int innerRadA_px = (int) (angleFactorA * (float)innerRadius_px); // 3
  constexpr int innerRadB_px = (int) (angleFactorB * (float)innerRadius_px); // 2
  constexpr int barWidthA_px = (int) (angleFactorA * (float)barWidth_px * 0.5f); // 1
  constexpr int barWidthB_px = (int) (angleFactorB * (float)barWidth_px * 0.5f); // 0
  constexpr int halfBarWidth_px = (int) ((float)barWidth_px * 0.5f);

  // Multiplying factors (cos/sin) for the clock directions.
  // NOTE: Needs to have the 13th value so the unknown direction dot can display properly
  static const std::array<Point2f, 13> barLenFactor =
  {{
    {0.f, 1.f}, // 12 o'clock - in front of robot so point down
    {-angleFactorB, angleFactorA}, // 1 o'clock
    {-angleFactorA, angleFactorB}, // 2 o'clock
    {-1.f, 0.f}, // 3 o'clock
    {-angleFactorA, -angleFactorB}, // 4 o'clock
    {-angleFactorB, -angleFactorA}, // 5 o'clock
    {0.f, -1.f}, // 6 o'clock - behind robot so point up
    {angleFactorB, -angleFactorA}, // 7 o'clock
    {angleFactorA, -angleFactorB}, // 8 o'clock
    {1.f, 0.f}, // 9 o'clock
    {angleFactorA, angleFactorB}, // 10 o'clock
    {angleFactorB, angleFactorA}, // 11 o'clock
    {0.f, 0.f} // Unknown direction
  }};

  // Precalculated offsets for the center of the base of each of the direction bars,
  // to be added to the center point of the clock
  static const std::array<Point2i, 12> barBaseOffset =
  {{
    {0, innerRadius_px}, // 12 o'clock - in front of robot, point down
    {-innerRadB_px, innerRadA_px}, // 1 o'clock
    {-innerRadA_px, innerRadB_px}, // 2 o'clock
    {-innerRadius_px, 0}, // 3 o'clock
    {-innerRadA_px, -innerRadB_px}, // 4 o'clock
    {-innerRadB_px, -innerRadA_px}, // 5 o'clock
    {0, -innerRadius_px}, // 6 o'clock - behind robot, point up
    {innerRadB_px, -innerRadA_px}, // 7 o'clock
    {innerRadA_px, -innerRadB_px}, // 8 o'clock
    {innerRadius_px, 0}, // 9 o'clock
    {innerRadA_px, innerRadB_px}, // 10 o'clock
    {innerRadB_px, innerRadA_px} // 11 o'clock
  }};

  // Precalculated offsets for the lower left and lower right points of the direction bars
  // (relative to a bar pointing at 12 o'clock on the drawn face, aka 6 o'clock relative to victor).
  static const std::array<std::array<Point2i, 2>, 12> barWidthFactor =
  {{
    {{{halfBarWidth_px, 0}, {-halfBarWidth_px, 0}}},// 12 o'clock -  point down
    {{{barWidthA_px, barWidthB_px}, {-barWidthA_px, -barWidthB_px}}}, // 1 o'clock
    {{{barWidthB_px, barWidthA_px}, {-barWidthB_px, -barWidthA_px}}}, // 2 o'clock
    {{{0, halfBarWidth_px}, {0, -halfBarWidth_px}}}, // 3 o'clock - point left
    {{{-barWidthB_px, barWidthA_px}, {barWidthB_px, -barWidthA_px}}}, // 4 o'clock
    {{{-barWidthA_px, barWidthB_px}, {barWidthA_px, -barWidthB_px}}}, // 5 o'clock
    {{{-halfBarWidth_px, 0}, {halfBarWidth_px, 0}}}, // 6 o'clock - point up
    {{{-barWidthA_px, -barWidthB_px}, {barWidthA_px, barWidthB_px}}}, // 7 o'clock
    {{{-barWidthB_px, -barWidthA_px}, {barWidthB_px, barWidthA_px}}}, // 8 o'clock
    {{{0, -halfBarWidth_px}, {0, halfBarWidth_px}}}, // 9 o'clock - point right
    {{{barWidthB_px, -barWidthA_px}, {-barWidthB_px, barWidthA_px}}}, // 10 o'clock
    {{{barWidthA_px, -barWidthB_px}, {-barWidthA_px, barWidthB_px}}} // 11 o'clock
  }};

  // Draw the outer circle
  drawImg.DrawCircle({(float)center_px.x(), (float)center_px.y()}, NamedColors::BLUE, circleRadius_px, 2);

  // Draw each of the clock directions
  for (int i=0; i<12; ++i)
  {
    const auto baseX = center_px.x() + barBaseOffset[i].x();
    const auto baseY = center_px.y() + barBaseOffset[i].y();
    const auto dirLen = confList[i] / maxConf * (float)maxBarLen_px;
    const auto lenX = (int) (barLenFactor[i].x() * (float)dirLen);
    const auto lenY = (int) (barLenFactor[i].y() * (float)dirLen);

    drawImg.DrawFilledConvexPolygon({
      {baseX + barWidthFactor[i][0].x(), baseY + barWidthFactor[i][0].y()},
      {baseX + barWidthFactor[i][0].x() + lenX, baseY + barWidthFactor[i][0].y() + lenY},
      {baseX + barWidthFactor[i][1].x() + lenX, baseY + barWidthFactor[i][1].y() + lenY},
      {baseX + barWidthFactor[i][1].x(), baseY + barWidthFactor[i][1].y()}
      },
      NamedColors::BLUE);
  }

  // Draw the circle indicating the current dominant direction
  drawImg.DrawFilledCircle({
    (float) (center_px.x() + (int)(barLenFactor[winningIndex].x() * (float)(circleRadius_px + 1.f))),
    (float) (center_px.y() + (int)(barLenFactor[winningIndex].y() * (float)(circleRadius_px + 1.f)))
    }, NamedColors::RED, 5);


  // If we have an active state flag set, draw a blue circle for it
  constexpr int activeCircleRad_px = 10;
  if (micData.activeState != 0)
  {
    drawImg.DrawFilledCircle({
      (float) FACE_DISPLAY_WIDTH - activeCircleRad_px,
      (float) FACE_DISPLAY_HEIGHT - activeCircleRad_px
      }, NamedColors::BLUE, activeCircleRad_px);
  }

  // Display the trigger recognized symbol if needed
  constexpr int triggerDispWidth_px = 15;
  constexpr int triggerDispHeight = 16;
  constexpr int triggerOffsetFromActiveCircle_px = 20;
  if (triggerRecognized)
  {
    drawImg.DrawFilledConvexPolygon({
      {FACE_DISPLAY_WIDTH - triggerDispWidth_px,
        FACE_DISPLAY_HEIGHT - activeCircleRad_px*2 - triggerOffsetFromActiveCircle_px},
      {FACE_DISPLAY_WIDTH - triggerDispWidth_px,
        FACE_DISPLAY_HEIGHT - activeCircleRad_px*2 - triggerOffsetFromActiveCircle_px + triggerDispHeight},
      {FACE_DISPLAY_WIDTH,
        FACE_DISPLAY_HEIGHT - activeCircleRad_px*2 - triggerOffsetFromActiveCircle_px + triggerDispHeight/2}
      },
      NamedColors::GREEN);
  }

  constexpr int endOfBarHeight_px = 20;
  constexpr int endOfBarWidth_px = 5;

  constexpr int buffFullBarHeight_px = endOfBarHeight_px / 2;
  constexpr int buffFullBarWidth_px = 52;
  bufferFullPercent = CLIP(bufferFullPercent, 0.0f, 1.0f);

  // Draw the end-of-bar line
  drawImg.DrawFilledConvexPolygon({
    {buffFullBarWidth_px, FACE_DISPLAY_HEIGHT - endOfBarHeight_px},
    {buffFullBarWidth_px, FACE_DISPLAY_HEIGHT},
    {buffFullBarWidth_px + endOfBarWidth_px, FACE_DISPLAY_HEIGHT},
    {buffFullBarWidth_px + endOfBarWidth_px, FACE_DISPLAY_HEIGHT - endOfBarHeight_px}
    },
    NamedColors::RED);

  // Draw the bar showing the mic data buffer fullness
  drawImg.DrawFilledConvexPolygon({
    {0, FACE_DISPLAY_HEIGHT - endOfBarHeight_px + buffFullBarHeight_px / 2},
    {0, FACE_DISPLAY_HEIGHT - buffFullBarHeight_px / 2},
    {(int) (bufferFullPercent * (float) buffFullBarWidth_px), FACE_DISPLAY_HEIGHT - buffFullBarHeight_px / 2},
    {(int) (bufferFullPercent * (float) buffFullBarWidth_px), FACE_DISPLAY_HEIGHT - endOfBarHeight_px + buffFullBarHeight_px / 2}
    },
    NamedColors::RED);

  const std::string confidenceString = std::to_string(micData.confidence);
  drawImg.DrawText( {0.0f, 10.0f}, confidenceString, NamedColors::WHITE, 0.5f );

  // Also draw the delay time in milliseconds
  // Copied from kRawAudioPerBuffer_ms in micDataProcessor.h
  // and doubled for 2 buffers
  const auto delayStr = std::to_string(delayTime_ms);
  const Point2f textLoc = {0.f, FACE_DISPLAY_HEIGHT - endOfBarHeight_px};
  static const auto textScale = 0.5f;
  _scratchDrawingImg->DrawText(textLoc,
                               delayStr,
                               NamedColors::WHITE,
                               textScale);

  // Draw the debug page number
  DrawScratch();
}

void FaceInfoScreenManager::CheckForButtonEvent(const bool buttonPressed, 
                                                bool& buttonPressedEvent,
                                                bool& buttonReleasedEvent,
                                                bool& singlePressDetected, 
                                                bool& doublePressDetected)
{
  static u32  lastPressTime_ms   = 0;
  static bool singlePressPending = false;
  static bool doublePressPending = false;
  static bool buttonWasPressed   = false;

  // Whether or not the shutdown message was already sent
  static bool shutdownSent       = false;

  buttonPressedEvent  = !buttonWasPressed && buttonPressed;
  buttonReleasedEvent = buttonWasPressed && !buttonPressed;
  buttonWasPressed = buttonPressed;
  singlePressDetected = false;
  doublePressDetected = false;

  // The maximum amount of time allowed between button releases
  // to register as a double press
  static const u32 kDoublePressWindow_ms   = 700;

  const u32  curTime_ms         = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
  const bool mightBeDoublePress = (lastPressTime_ms > 0) && (curTime_ms - lastPressTime_ms < kDoublePressWindow_ms);

  if (buttonPressedEvent) {
    if (mightBeDoublePress) {
      lastPressTime_ms = 0;
      doublePressPending = true;
    } else {
      lastPressTime_ms = curTime_ms;
    }
    singlePressPending = false;
  } else if (buttonReleasedEvent) {
    if (lastPressTime_ms > 0) {
      singlePressPending = true;
    } else if (doublePressPending) {
      doublePressPending = false;
      doublePressDetected = true;
    }
    shutdownSent = false;
  } else if (singlePressPending && !mightBeDoublePress) {
    lastPressTime_ms = 0;
    singlePressPending = false;
    singlePressDetected = true;
  }

  // Check if button was held down long enough for shutdown animation to start
  const bool shouldTriggerShutdown = buttonPressed && 
                                     (lastPressTime_ms > 0) && 
                                     (curTime_ms - lastPressTime_ms > kButtonPressDurationForShutdown_ms) &&
                                     (GetCurrScreenName() == ScreenName::None);
  if (shouldTriggerShutdown && !shutdownSent) {
    LOG_INFO("FaceInfoScreenManager.CheckForButtonEvent.StartShutdownAnim", "");
    RobotInterface::SendAnimToEngine(StartShutdownAnim());
    lastPressTime_ms    = 0;
    singlePressPending  = false;
    singlePressDetected = false;
    doublePressPending  = false;
    doublePressDetected = false;
    shutdownSent        = true;
  }
  
#if ANKI_DEV_CHEATS
  if( kFakeButtonPressType == 1 ) { // single press
    singlePressDetected = true;
    kFakeButtonPressType = 0;
  } else if( kFakeButtonPressType == 2 ) { // double press
    doublePressDetected = true;
    kFakeButtonPressType = 0;
  }
#endif
}

void FaceInfoScreenManager::ResetObservedHeadAndLiftAngles()
{
  _liftLowestAngle_rad = std::numeric_limits<f32>::max();
  _liftHighestAngle_rad = std::numeric_limits<f32>::lowest();

  _headLowestAngle_rad = std::numeric_limits<f32>::max();
  _headHighestAngle_rad = std::numeric_limits<f32>::lowest();
}

void FaceInfoScreenManager::ProcessMenuNavigation(const RobotState& state)
{
  const bool buttonIsPressed = static_cast<bool>(state.status & (uint32_t)RobotStatusFlag::IS_BUTTON_PRESSED);
  bool buttonPressedEvent;
  bool buttonReleasedEvent;
  bool singlePressDetected;
  bool doublePressDetected;
  CheckForButtonEvent(buttonIsPressed, 
                      buttonPressedEvent, 
                      buttonReleasedEvent, 
                      singlePressDetected, 
                      doublePressDetected);

  const bool isOnCharger = static_cast<bool>(state.status & (uint32_t)RobotStatusFlag::IS_ON_CHARGER);

  const ScreenName currScreenName = GetCurrScreenName();

  if (singlePressDetected && _engineLoaded) {
    if (IsAlexaScreen(currScreenName)) {
      // Single press should exit any uncompleted alexa authorization
      Alexa* alexa = _context->GetAlexa();
      if( alexa != nullptr ) {
        alexa->CancelPendingAlexaAuth("BUTTON_PRESS");
      }
      EnableAlexaScreen(ScreenName::None,"","");
    } else if (currScreenName == ScreenName::None) {
      // Fake trigger word on single press
      LOG_INFO("FaceInfoScreenManager.ProcessMenuNavigation.GotSinglePress", "Triggering wake word");
      _context->GetMicDataSystem()->FakeTriggerWordDetection();
    }
  }

  // Check for conditions to enter BLE pairing mode
  if (doublePressDetected && 
      isOnCharger &&
      // Only enter pairing from these three screens which include
      // screens that are normally active during playpen test
      CanEnterPairingFromScreen(currScreenName)) {
    LOG_INFO("FaceInfoScreenManager.ProcessMenuNavigation.GotDoublePress", "Entering pairing");
    RobotInterface::SendAnimToEngine(SwitchboardInterface::EnterPairing());

    if (FORCE_TRANSITION_TO_PAIRING) {
      LOG_WARNING("FaceInfoScreenManager.ProcessMenuNavigation.ForcedPairing",
                  "Remove FORCE_TRANSITION_TO_PAIRING when switchboard is working");
      SetScreen(ScreenName::Pairing);
    }
  }
  else if(doublePressDetected &&
          !isOnCharger && // while user-facing instructions may say "pick up the robot and double press," it's really just off charger
          _engineLoaded &&
          CanEnterPairingFromScreen(currScreenName))
  {
    ToggleMute("DOUBLE_PRESS");
  }

  // Check for button press to go to next debug screen
  if (buttonReleasedEvent) {
    if (_debugInfoScreensUnlocked &&
        (currScreenName != ScreenName::None &&
          currScreenName != ScreenName::FAC &&
          currScreenName != ScreenName::Pairing &&
          currScreenName != ScreenName::Recovery) ) {
      SetScreen(_currScreen->GetButtonGotoScreen());
    }
  }

  // Check for screen timeout
  if (_currScreen->IsTimedOut()) {
    SetScreen(_currScreen->GetTimeoutScreen());
  }


  if (_currScreen->HasMenu()) {

    // Process wheel motion for moving the menu select cursor.
    // NOTE: Due to lack of quadrature encoding on the wheels
    //       when they are not actively powered the reported speed
    //       of the wheels when moved manually have a fixed sign.
    //       Consequently, moving the left wheel in any direction
    //       moves the menu cursor down and moving the right wheel
    //       in any direction moves it up.
    const auto lWheelSpeed = std::fabsf(state.lwheel_speed_mmps);
    const auto rWheelSpeed = std::fabsf(state.rwheel_speed_mmps);
    if (rWheelSpeed > kWheelMotionThresh_mmps) {

      ++_wheelMovingForwardsCount;
      _wheelMovingBackwardsCount = 0;

      if (_wheelMovingForwardsCount == kMenuCursorMoveCountThresh) {
        // Move menu cursor up
        _currScreen->MoveMenuCursorUp();
        DrawScratch();
      }

    } else if (lWheelSpeed > kWheelMotionThresh_mmps) {

      ++_wheelMovingBackwardsCount;
      _wheelMovingForwardsCount = 0;

      if (_wheelMovingBackwardsCount == kMenuCursorMoveCountThresh) {
        // Move menu cursor down
        _currScreen->MoveMenuCursorDown();
        DrawScratch();
      }
    }
    else {
      _wheelMovingForwardsCount = 0;
      _wheelMovingBackwardsCount = 0;
    }
  }

  if (_currScreen->HasMenu() || currScreenName == ScreenName::Pairing) {
    // Process lift motion for confirming current menu selection

    // Update min/max lift angles and the current range observed
    const auto liftAngle = state.liftAngle;
    if (liftAngle > _liftHighestAngle_rad) {
      _liftHighestAngle_rad = liftAngle;
    }
    if (liftAngle < _liftLowestAngle_rad) {
      _liftLowestAngle_rad = liftAngle;
    }
    const float liftRange_rad = _liftHighestAngle_rad - _liftLowestAngle_rad;

    if (!_liftTriggerReady && (liftRange_rad > kMenuLiftRange_rad)) {
      _liftTriggerReady = true;
    } else if (_liftTriggerReady && 
               (Util::Abs(liftAngle - _liftLowestAngle_rad) < kMenuAngularTriggerThresh_rad)) {
      // Menu item confirmed. Go to next screen.
      _liftTriggerReady = false;

      if (_currScreen->HasMenu()) {
        SetScreen(_currScreen->ConfirmMenuItemAndGetNextScreen());
      } else if (GetCurrScreenName() == ScreenName::Pairing) {
        LOG_INFO("FaceInfoScreenManager.ProcessMenuNavigation.ExitPairing", "Going to Customer Service Main from Pairing");
        RobotInterface::SendAnimToEngine(SwitchboardInterface::ExitPairing());
        SetScreen(ScreenName::Main);

        // DAS msg for entering customer care screen
        // Note: The debug info screens will only be reported unlocked here if they 
        //       were unlocked the previous time the customer care screen was entered.
        DASMSG(robot_cc_screen_enter, "robot.cc_screen_enter", "Entered customer care screen");
        DASMSG_SET(i1, _debugInfoScreensUnlocked ? 1 : 0, "Debug info screens unlocked");
        DASMSG_SEND();
      }
    }
  }
  else
  {
    _liftTriggerReady = false;
  }

  // Process head motion for going from Main screen to "hidden" debug info screens
  if (currScreenName == ScreenName::Main) {

    // Update min/max head angles and the current range observed
    const auto headAngle = state.headAngle;
    if (headAngle > _headHighestAngle_rad) {
      _headHighestAngle_rad = headAngle;
    }
    if (headAngle < _headLowestAngle_rad) {
      _headLowestAngle_rad = headAngle;
    }
    const float headRange_rad = _headHighestAngle_rad - _headLowestAngle_rad;

    if (!_headTriggerReady && (headRange_rad > kMenuHeadRange_rad)) {
      _headTriggerReady = true;
    } else if (_headTriggerReady && 
               (Util::Abs(headAngle - _headLowestAngle_rad) < kMenuAngularTriggerThresh_rad)) {
      // Menu item confirmed. Go to first debug info screen
      _headTriggerReady = false;
      _debugInfoScreensUnlocked = true;
      LOG_INFO("FaceInfoScreenManager.ProcessMenuNavigation.DebugScreensUnlocked", "");
      DrawScratch();
    }
  }

}


ScreenName FaceInfoScreenManager::GetCurrScreenName() const
{
  return _currScreen->GetName();
}

void FaceInfoScreenManager::Update(const RobotState& state)
{
  ProcessMenuNavigation(state);

  const auto currScreenName = GetCurrScreenName();

  switch(currScreenName) {
    case ScreenName::Main:
    {
      static float lastTime = 0;
      const float now = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
      if ( (now - lastTime) > kIPCheckPeriod_sec ) {
        lastTime = now;
        DrawMain();
      }
      break; 
    }
    case ScreenName::Network:
    {
      static float lastTime = 0;
      const float now = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
      if (_redrawNetwork) {
        _redrawNetwork = false;
        DrawNetwork();
      } 

      if ( !FACTORY_TEST && ((now - lastTime) > kIPCheckPeriod_sec) ) {
        LOG_INFO("FaceInfoScreenManager.Update.CheckingConnectivity", "");  
        _context->GetMicDataSystem()->RequestConnectionStatus();   
        _testingNetwork = true;  
        lastTime = now;
      }
      break; 
    }
    case ScreenName::SensorInfo:
      DrawSensorInfo(state);
      break;
    case ScreenName::IMUInfo:
      DrawIMUInfo(state);
      break;
    case ScreenName::MotorInfo:
      DrawMotorInfo(state);
      break;
    case ScreenName::CustomText:
      DrawCustomText();
      break;
    case ScreenName::FAC:
      UpdateFAC();
      break;
    case ScreenName::CameraMotorTest:
      UpdateCameraTestMode(state.timestamp);
      break;
    default:
      // Other screens are either updated once when SetScreen() is called
      // or updated by their own draw functions that are called externally
      break;
  }
}

void FaceInfoScreenManager::DrawMain()
{
  auto *osstate = OSState::getInstance();

  std::string esn = osstate->GetSerialNumberAsString();
  if(esn.empty())
  {
    // TODO Remove once DVT2s are phased out
    // ESN is 0 assume this is a DVT2 with a fake birthcertificate
    // so look for serial number in "/proc/cmdline"
    static std::string serialNum = "";
    if(serialNum == "")
    {
      std::ifstream infile("/proc/cmdline");

      std::string line;
      while(std::getline(infile, line))
      {
        static const std::string kProp = "androidboot.serialno=";
        size_t index = line.find(kProp);
        if(index != std::string::npos)
        {
          serialNum = line.substr(index + kProp.length(), 8);
        }
      }
      infile.close();
    }
    esn =  serialNum;
  }

  const std::string serialNo = "ESN: "  + esn;

  const std::string hwVer    = "HW: "   + std::to_string(Factory::GetEMR()->fields.HW_VER);

  const std::string osVer    = "OS: "   + osstate->GetOSBuildVersion() +
                                          (FACTORY_TEST ? " (V4)" : "") +
                                          (osstate->IsInRecoveryMode() ? " U" : "");

  const std::string ssid     = "SSID: " + osstate->GetSSID(true);

#if ANKI_DEV_CHEATS
  const std::string sha      = "SHA: "  + osstate->GetBuildSha();
#endif

  std::string ip             = osstate->GetIPAddress();
  if (ip.empty()) {
    ip = "XXX.XXX.XXX.XXX";
  }

  // ESN/serialNo and the HW version are drawn on the same line with serialNo default left aligned and
  // HW version right aligned.
  ColoredTextLines lines = { { {serialNo}, {hwVer, NamedColors::WHITE, false} },
                             {osVer}, 
                             {ssid}, 
#if FACTORY_TEST
                             {"IP: " + ip},
#else
                             { {"IP: "}, {ip, (osstate->IsValidIPAddress(ip) ? NamedColors::GREEN : NamedColors::RED)} },
#endif
#if ANKI_DEV_CHEATS
			     {sha},
#endif
                           };

  DrawTextOnScreen(lines);
}

void FaceInfoScreenManager::DrawNetwork()
{
  auto osstate = OSState::getInstance();
  const std::string ble      = "BLE ID: " + osstate->GetRobotName();
  const std::string mac      = "MAC: "  + osstate->GetMACAddress();
  const std::string ssid     = "SSID: " + osstate->GetSSID(true);

  std::string ip             = osstate->GetIPAddress();
  if (ip.empty()) {
    ip = "XXX.XXX.XXX.XXX";
  }

  std::tm timeObj;
  char timeFormat[50];
  const bool gotTime = WallTime::getInstance()->GetUTCTime(timeObj);
  
  strftime(timeFormat, 50, "%F %R UTC", &timeObj);
  const std::string currTime = gotTime ? timeFormat : "NO CLOCK";

#if !FACTORY_TEST
  const ColoredText reachable("REACHABLE", NamedColors::GREEN);
  const ColoredText unreachable("UNREACHABLE", NamedColors::RED);

  auto getStatusString = [](const auto& status) {
    switch (status) {
      case CloudMic::ConnectionCode::Available:   { return ColoredText("AVAILABLE",    NamedColors::GREEN); }
      case CloudMic::ConnectionCode::Connectivity:{ return ColoredText("CONNECTIVITY", NamedColors::RED); }
      case CloudMic::ConnectionCode::Tls:         { return ColoredText("TLS",          NamedColors::RED); }
      case CloudMic::ConnectionCode::Auth:        { return ColoredText("AUTH",         NamedColors::RED); }
      case CloudMic::ConnectionCode::Bandwidth:   { return ColoredText("BANDWIDTH",    NamedColors::RED); }
      default:                                    { return ColoredText("CHECKING...",  NamedColors::BLUE); }
    }
  };

#endif

  ColoredTextLines lines = { {ble},
                             {mac},
                             {ssid},
#if FACTORY_TEST
                             {"IP: " + ip},
#else
                            // TODO: re-enable after security team has confirmed showing email is allowed
                            //  { {"EMAIL: "}, {"dummy...@a...com"} },
                             { {"IP: "}, {ip, (osstate->IsValidIPAddress(ip) ? NamedColors::GREEN : NamedColors::RED)} },
                             { },
                             { {currTime} },
                             { {"NETWORK: "}, _testingNetwork ? ColoredText("") : getStatusString(_networkStatus) }
                           };
#endif
  DrawTextOnScreen(lines);
}

void FaceInfoScreenManager::DrawSensorInfo(const RobotState& state)
{
  char temp[32] = "";
  sprintf(temp,
          "SYS: %s",
          _sysconVersion.c_str());
  const std::string syscon = temp;


  sprintf(temp,
          "CLF: %4u %4u %4u %4u",
          state.cliffDataRaw[0],
          state.cliffDataRaw[1],
          state.cliffDataRaw[2],
          state.cliffDataRaw[3]);
  const std::string cliffs = temp;


  std::string prox1, prox2;
  if(!IsWhiskey())
  {
    sprintf(temp,
            "DIST:   %3umm",
            state.proxData.distance_mm);
    prox1 = temp;

    sprintf(temp,
            "        (%2.1f %2.1f %3.f)",
            state.proxData.signalIntensity,
            state.proxData.ambientIntensity,
            state.proxData.spadCount);
    prox2 = temp;
  }

  sprintf(temp,
          "TOUCH: %u",
          state.backpackTouchSensorRaw);
  const std::string touch = temp;

  #define IS_STATUS_FLAG_SET(x) ((state.status & (uint32_t)RobotStatusFlag::x) != 0)
  const bool batteryDisconnected = IS_STATUS_FLAG_SET(IS_BATTERY_DISCONNECTED);
  const bool batteryCharging     = IS_STATUS_FLAG_SET(IS_CHARGING);
  const bool batteryHot          = IS_STATUS_FLAG_SET(IS_BATTERY_OVERHEATED);
  const bool batteryLow          = IS_STATUS_FLAG_SET(IS_BATTERY_LOW);
  const bool shutdownImminent    = IS_STATUS_FLAG_SET(IS_SHUTDOWN_IMMINENT);

  sprintf(temp,
          "BATT:  %0.2fV   %s%s%s%s%s",
          state.batteryVoltage,
          batteryDisconnected ? "D" : " ",
          batteryCharging     ? "C" : " ",
          batteryHot          ? "H" : " ",
          batteryLow          ? "L" : " ",
          shutdownImminent    ? "S" : " ");
  const std::string batt = temp;

  sprintf(temp,
          "CHGR:  %0.2fV",
          state.chargerVoltage);
  const std::string charger = temp;

  sprintf(temp,
          "TEMP:  %uC (H) / %uC (B)",
          OSState::getInstance()->GetTemperature_C(),
          state.battTemp_C);
  const std::string tempC = temp;

  if(IsWhiskey())
  {
    DrawTextOnScreen({cliffs, touch, batt, charger, tempC});
  }
  else
  {
    DrawTextOnScreen({syscon, cliffs, prox1, prox2, touch, batt, charger, tempC});
  }
}

void FaceInfoScreenManager::DrawIMUInfo(const RobotState& state)
{
  char temp[32] = "";
  sprintf(temp,
          "%*.0f %*.2f",
          8,
          state.accel.x,
          8,
          state.gyro.x);
  const std::string accelGyroX = temp;

  sprintf(temp,
          "%*.2f %*.2f",
          8,
          state.accel.y,
          8,
          state.gyro.y);
  const std::string accelGyroY = temp;

  sprintf(temp,
          "%*.2f %*.2f",
          8,
          state.accel.z,
          8,
          state.gyro.z);
  const std::string accelGyroZ = temp;

  DrawTextOnScreen({"ACC        GYRO", accelGyroX, accelGyroY, accelGyroZ});
}

void FaceInfoScreenManager::DrawMotorInfo(const RobotState& state)
{
  char temp[32] = "";
  sprintf(temp, "HEAD:   %3.1f deg", RAD_TO_DEG(state.headAngle));
  const std::string head = temp;

  sprintf(temp, "LIFT:   %3.1f deg", RAD_TO_DEG(state.liftAngle));
  const std::string lift = temp;

  sprintf(temp, "LSPEED: %3.1f mm/s", state.lwheel_speed_mmps);
  const std::string lSpeed = temp;

  sprintf(temp, "RSPEED: %3.1f mm/s", state.rwheel_speed_mmps);
  const std::string rSpeed = temp;

  DrawTextOnScreen({head, lift, lSpeed, rSpeed});
}


void FaceInfoScreenManager::DrawMicInfo(const RobotInterface::MicData& micData)
{
  if(GetCurrScreenName() != ScreenName::MicInfo)
  {
    return;
  }

  //Get the intensity of the first sample in each channel and print them to a debug string.
  //(Should we instead use the max intensity of the first n samples per channel?)
  char temp[32] = "";
  sprintf(temp,
          "%d",
          micData.data[0]);
  const std::string micData0 = temp;

  sprintf(temp,
          "%d",
          micData.data[MicData::kSamplesPerBlockPerChannel]);
  const std::string micData1 = temp;

  sprintf(temp,
          "%d",
          micData.data[MicData::kSamplesPerBlockPerChannel*2]);
  const std::string micData2 = temp;

  sprintf(temp,
          "%d",
          micData.data[MicData::kSamplesPerBlockPerChannel*3]);
  const std::string micData3 = temp;

  DrawTextOnScreen({"MICS", micData0, micData1, micData2, micData3});
}

void FaceInfoScreenManager::SetCustomText(const RobotInterface::DrawTextOnScreen& text)
{
  _customText = text;

  if(text.drawNow)
  {
    SetScreen(ScreenName::CustomText);
  }
}

void FaceInfoScreenManager::DrawCustomText()
{
  DrawTextOnScreen({std::string(_customText.text,
                                _customText.text_length)},
                   ColorRGBA(_customText.textColor.r,
                             _customText.textColor.g,
                             _customText.textColor.b),
                   ColorRGBA(_customText.bgColor.r,
                             _customText.bgColor.g,
                             _customText.bgColor.b),
                   { 0, FACE_DISPLAY_HEIGHT-10 }, 10, 3.f);
}
  
void FaceInfoScreenManager::DrawAlexaFace()
{
  if ( nullptr == _currScreen )
  {
    return;
  }

  static const int        kScreenTop            = 0;
  static const int        kIconToTextSpacing    = 0;
  static const float      kDefaultTextScale     = 0.4f;
  static const ColorRGBA& kTextColor            = NamedColors::WHITE;
  static const int        kTextSpacing          = 14;
  static const int        kTextLineThickness    = 1;

  // draw the alexa icon ...

  Vision::ImageRGBA alexaIcon;
  alexaIcon.Load(_context->GetDataLoader()->GetSpritePaths()->GetAssetPath(kAlexaIconSpriteName));

  const int kIconTop  = kScreenTop;
  const int iconLeft  = ( FACE_DISPLAY_WIDTH - alexaIcon.GetNumCols() )  / 2.0f;
  const Point2f iconTopLeft( iconLeft, kIconTop );

  _scratchDrawingImg->DrawSubImage( Vision::ImageRGB565( alexaIcon ), iconTopLeft );

  // draw the texzt ...
  // todo: localization

  struct TextDataLine
  {
    std::string   text;
    float         scale = 1.0f;
  };
  std::vector<TextDataLine> textVec;
  
  switch ( _currScreen->GetName() )
  {
    case ScreenName::AlexaPairing:
    {
      // we have confirmed it's ok to hardcode this url, but if it's been set for us already, use that
      const std::string& url = _alexaUrl.empty() ? "amazon.com/code" : _alexaUrl;
      textVec.push_back( { "Go to " + url } );
      textVec.push_back( { _alexaCode, 1.5f } );

      break;
    }

    case ScreenName::AlexaPairingSuccess:
    {
      textVec.push_back( { "You're ready to use Alexa." } );
      textVec.push_back( { "Check out the Alexa App" } );
      textVec.push_back( { "for things to try." } );

      break;
    }

    case ScreenName::AlexaPairingExpired:
    {
      textVec.push_back( { "The code has expired." } );
      textVec.push_back( { "Retry to generate" } );
      textVec.push_back( { "a new code." } );

      break;
    }

    case ScreenName::AlexaPairingFailed:
    {
      textVec.push_back( { "Something's gone wrong." } );
      textVec.push_back( { "Please try again." } );

      break;
    }

    default:
    {
      ANKI_VERIFY( false && "Unexpected alexa face", "FaceInfoScreenManager.DrawAlexaFace.Unexpected", "" );
      break;
    }
  }

  // loop through our lines of text and draw them centered on the screen
  int textLocationY = ( kIconTop + alexaIcon.GetNumRows() + kIconToTextSpacing );
  for ( const auto& line : textVec )
  {
    textLocationY += ( kTextSpacing * line.scale );
    _scratchDrawingImg->DrawTextCenteredHorizontally( line.text,
                                                      CV_FONT_NORMAL,
                                                      kDefaultTextScale * line.scale,
                                                      kTextLineThickness,
                                                      kTextColor,
                                                      textLocationY,
                                                      false );
  }

  // This actually draws the scratch image to the screen
  DrawScratch();

  RobotInterface::SetHeadAngle headAction;
  headAction.angle_rad = MAX_HEAD_ANGLE;
  headAction.duration_sec = 1.0;
  headAction.max_speed_rad_per_sec = MAX_HEAD_SPEED_RAD_PER_S;
  headAction.accel_rad_per_sec2 = MAX_HEAD_ACCEL_RAD_PER_S2;
  SendAnimToRobot(std::move(headAction));
}
  
void FaceInfoScreenManager::DrawMuteAnimation()
{
  if( _currScreen == nullptr ) {
    return;
  }
  const bool muted = _context->GetMicDataSystem()->IsMicMuted();
  // The value of muted was set prior to this method call, so indicates a transition _to_ that state,
  // so play the on/off or off/on anim to reflect that
  const std::string animName = muted ? "anim_micstate_micoff_01" : "anim_micstate_micon_01";
  const bool shouldInterrupt = true;
  const bool overrideAllSpritesToEyeColor = true;
  _animationStreamer->SetStreamingAnimation(animName, 0, 1, 0, shouldInterrupt, overrideAllSpritesToEyeColor);
  
}
  
void FaceInfoScreenManager::DrawAlexaNotification()
{
  if( _currScreen == nullptr ) {
    return;
  }

  const std::string animName = "anim_avs_notification_loop_01";
  const bool shouldInterrupt = true;
  _animationStreamer->SetStreamingAnimation(animName, 0, 1, 0, shouldInterrupt);
}

// Draws each element of the textVec on a separate line (spacing determined by textSpacing_pix)
// in textColor with a background of bgColor.
void FaceInfoScreenManager::DrawTextOnScreen(const std::vector<std::string>& textVec,
                                             const ColorRGBA& textColor,
                                             const ColorRGBA& bgColor,
                                             const Point2f& loc,
                                             u32 textSpacing_pix,
                                             f32 textScale)
{
  _scratchDrawingImg->FillWith( {bgColor.r(), bgColor.g(), bgColor.b()} );

  const f32 textLocX = loc.x();
  f32 textLocY = loc.y();
  // TODO: Expose line and location(?) as arguments
  const u8  textLineThickness = 8;

  for(const auto& text : textVec)
  {
    _scratchDrawingImg->DrawText(
      {textLocX, textLocY},
      text.c_str(),
      textColor,
      textScale,
      textLineThickness);

    textLocY += textSpacing_pix;
  }

  DrawScratch();
}

void FaceInfoScreenManager::DrawTextOnScreen(const ColoredTextLines& lines,
                                             const ColorRGBA& bgColor,
                                             const Point2f& loc,
                                             u32 textSpacing_pix,
                                             f32 textScale)
{
  _scratchDrawingImg->FillWith( {bgColor.r(), bgColor.g(), bgColor.b()} );

  const u8  textLineThickness = 8;

  f32 textLocY = loc.y();
  for(const auto& line : lines)
  {
    f32 textOffsetX = loc.x();
    f32 textOffsetXRight = loc.x();
    for(const auto& coloredText : line)
    {
      f32 textLocX = textOffsetX;
      
      auto bbox = Vision::Image::GetTextSize(coloredText.text.c_str(), textScale, textLineThickness);
      if(coloredText.leftAlign)
      {
        textOffsetX += bbox.x();
      }
      else
      {
        // Right align text, need to account for the width of the text as DrawText expects the bottom left corner
        // location
        textLocX = FACE_DISPLAY_WIDTH - bbox.x() - textOffsetXRight;
        textOffsetXRight += bbox.x();
      }
      
      _scratchDrawingImg->DrawText({textLocX, textLocY},
                                   coloredText.text.c_str(),
                                   coloredText.color,
                                   textScale,
                                   textLineThickness);


    }
    textLocY += textSpacing_pix;
  }

  DrawScratch();
}

void FaceInfoScreenManager::DrawToF(const RangeDataDisplay& data)
{
  if(GetCurrScreenName() != ScreenName::ToF)
  {
    return;
  }
  
  Vision::ImageRGB565& img = *_scratchDrawingImg;
  const auto& clearColor = NamedColors::BLACK;
  img.FillWith( {clearColor.r(), clearColor.g(), clearColor.b()} );

  // Draw the range data in a 4x4 grid where each cell is one of the range ROIs
  const u32 gridHeight = FACE_DISPLAY_HEIGHT / 4;
  const u32 gridWidth = FACE_DISPLAY_WIDTH / 4;
  for(const auto& rangeData : data.data)
  {
    int roi = rangeData.roi;
    
    const u32 x = (roi % 4) * gridWidth;
    const u32 y = (roi / 4) * gridHeight;
    const Rectangle<f32> rect(x, y, gridWidth-1, gridHeight-1); // -1 for 1 pixel borders

    // Assuming max range is 1m
    f32 temp = std::max(rangeData.processedRange_mm, 0.000001f); // Prevent divide by zero
    temp = std::min(temp, 1000.f) / 1000.f;

    // Scale color based on distance
    u8 color = 255 * temp;

    u8 status = rangeData.status;

    // Signal quality is signalRate / spadCount
    float tempDiv = (rangeData.spadCount == 0 ? -1 : rangeData.spadCount);
    float signalQuality = (f32)(rangeData.signalRate_mcps / tempDiv);

    // Default background color is green
    // unless this ROI reported an invalid status in which the background
    // is red
    ColorRGBA bg(0, (u8)(255-color), 0);
    if(status != 0)
    {
      bg = ColorRGBA((u8)255, (u8)0, (u8)0);
      color = 255;
    }

    img.DrawFilledRect(rect, bg);

    const float kTextScale = 0.3f;
    const int kTextThickness = 1;
    
    // Draw three things in each cell, distance (top left), status (top right), and signal quality (bottom left)
    Point2f loc(x, y + 8); // Draw text 8 pixels below top cell border
    const u8 textColor = (color > 128 ? 255 : 0); // Make text color opposite of background for readability
    img.DrawText(loc,
                 std::to_string((u32)(rangeData.processedRange_mm)),
                 {textColor, textColor, textColor},
                 kTextScale,
                 false,
                 kTextThickness);

    // Range status is drawn a fixed amount from range distance (close to top right corner of cell)
    const f32 xPos = loc.x() + (u32)(2.75f*(f32)kDefaultTextSpacing_pix);
    img.DrawText({xPos, loc.y()},
                 std::to_string(status),
                 {textColor, textColor, textColor},
                 kTextScale,
                 false,
                 kTextThickness);

    const int yOffset = Vision::Image::GetTextSize(std::to_string((u32)(rangeData.processedRange_mm)),
                                                   kTextScale,
                                                   kTextThickness).y();
    const f32 yPos = loc.y() + yOffset + 1; // +1 for extra spacing between text lines
    char t[8];
    sprintf(t, "%2.1f", signalQuality);
    img.DrawText({loc.x(), yPos},
                 std::string(t),
                 {textColor, textColor, textColor},
                 kTextScale,
                 false,
                 kTextThickness);
  }  
  
  DrawScratch();
}


void FaceInfoScreenManager::EnablePairingScreen(bool enable)
{
  if (enable && GetCurrScreenName() != ScreenName::Pairing) {
    LOG_INFO("FaceInfoScreenManager.EnablePairingScreen.Enable", "");
    SetScreen(ScreenName::Pairing);
  } else if (!enable && GetCurrScreenName() == ScreenName::Pairing) {
    LOG_INFO("FaceInfoScreenManager.EnablePairingScreen.Disable", "");
    // TODO: it's possible that the user entered the app pairing screen during Alexa pairing,
    // in which case the face should return to the Alexa screen when app pairing is complete
    SetScreen(ScreenName::None);
  }
}
  
void FaceInfoScreenManager::EnableAlexaScreen(ScreenName screenName, const std::string& code, const std::string& url)
{
  const bool validNewScreen = IsAlexaScreen(screenName) || (screenName == ScreenName::None);
  if (!ANKI_VERIFY(validNewScreen, "FaceInfoScreenManager.EnableAlexaPairingScreen.Invalid",
                   "Screen %d is invalid", (int)screenName))
  {
    return;
  }
  
  const auto currScreen = GetCurrScreenName();
  const bool isAlexaScreen = IsAlexaScreen(currScreen);
  
  if ((screenName == ScreenName::AlexaPairing) && (GetCurrScreenName() != ScreenName::AlexaPairing)) {
    _alexaCode = code;
    _alexaUrl = url;

    LOG_INFO("FaceInfoScreenManager.EnableAlexaPairingScreen.Code", "");

    DASMSG(pairing_code_displayed, "alexa.pairing_code_displayed", "A code to pair with AVS has been displayed");
    DASMSG_SEND();

    SetScreen(ScreenName::AlexaPairing);
  } else if ((screenName == ScreenName::AlexaPairingSuccess) && (currScreen != ScreenName::AlexaPairingSuccess)) {
    LOG_INFO("FaceInfoScreenManager.EnableAlexaPairingScreen.Success", "");
    SetScreen(ScreenName::AlexaPairingSuccess);
  } else if ((screenName == ScreenName::AlexaPairingFailed) && (currScreen != ScreenName::AlexaPairingFailed)) {
    LOG_INFO("FaceInfoScreenManager.EnableAlexaPairingScreen.Failed", "");
    SetScreen(ScreenName::AlexaPairingFailed);
  } else if ((screenName == ScreenName::AlexaPairingExpired) && (currScreen != ScreenName::AlexaPairingExpired)) {
    LOG_INFO("FaceInfoScreenManager.EnableAlexaPairingScreen.Expired", "");
    SetScreen(ScreenName::AlexaPairingExpired);
  } else if ((screenName == ScreenName::None) && isAlexaScreen) {
    LOG_INFO("FaceInfoScreenManager.EnableAlexaPairingScreen.Done", "");
    SetScreen(ScreenName::None);
  }
}
  
void FaceInfoScreenManager::ToggleMute(const std::string& reason)
{
  _context->GetMicDataSystem()->ToggleMicMute();

  if( _context->GetMicDataSystem()->IsMicMuted() ) {
    DASMSG(microphone_off_message, "robot.microphone_off", "Microphone disabled (muted)");
    DASMSG_SET(s1, reason, "reason (how it was toggled)");
    DASMSG_SEND();
  }
  else {
    DASMSG(microphone_on_message, "robot.microphone_on", "Microphone enabled (unmuted)");
    DASMSG_SET(s1, reason, "reason (how it was toggled)");
    DASMSG_SEND();
  }
  
  if ((_currScreen != nullptr) && (_currScreen->GetName() == ScreenName::ToggleMute)) {
    // abort current animation and restart new one
    DrawMuteAnimation();
    _currScreen->RestartTimeout();
  } else {
    SetScreen(ScreenName::ToggleMute);
  }
}
  
void FaceInfoScreenManager::StartAlexaNotification()
{
  SetScreen(ScreenName::AlexaNotification);
}
  
void FaceInfoScreenManager::EnableMirrorModeScreen(bool enable)
{
  // As long as we're not in a screen that's already doing mirror mode
  // and we are not on the pairing screen
  // don't jump to the mirror mode screen
  if (GetCurrScreenName() != ScreenName::Camera && 
      GetCurrScreenName() != ScreenName::CameraMotorTest &&
      GetCurrScreenName() != ScreenName::Pairing) {  

    if (enable && GetCurrScreenName() != ScreenName::MirrorMode) {
      LOG_INFO("FaceInfoScreenManager.EnableMirrorModeScreen.Enable", "");
      SetScreen(ScreenName::MirrorMode);
    } else if (!enable && GetCurrScreenName() == ScreenName::MirrorMode) {
      LOG_INFO("FaceInfoScreenManager.EnableMirrorModeScreen.Disable", "");
      SetScreen(ScreenName::None);
    }
    
  }
}

void FaceInfoScreenManager::DrawScratch()
{
  _currScreen->DrawMenu(*_scratchDrawingImg);

  // Draw white pixel in top-right corner of main customer support screen
  // to indicate that debug screens are unlocked
  const bool drawDebugScreensEnabledPixel = _debugInfoScreensUnlocked &&
                                            GetCurrScreenName() == ScreenName::Main;
  if (drawDebugScreensEnabledPixel) {
    Rectangle<f32> rect(FACE_DISPLAY_WIDTH - 2, 0, 2, 2);
    _scratchDrawingImg->DrawFilledRect(rect, NamedColors::WHITE);
  }

  FaceDisplay::getInstance()->DrawToFaceDebug(*_scratchDrawingImg);
}

void FaceInfoScreenManager::Reboot()
{
#ifdef SIMULATOR
  LOG_WARNING("FaceInfoScreenManager.Reboot.NotSupportInSimulator", "");
  return;
#else
  // Need to call reboot in forked process for some reason.
  // Otherwise, reboot doesn't actually happen.
  // Also useful for transitioning to "REBOOTING..." screen anyway.
  sync();
  pid_t pid = fork();
  if (pid == 0)
  {
    // child process
    execl("/bin/systemctl", "reboot", 0);  // Graceful reboot
  }
  else if (pid > 0)
  {
    // parent process
    LOG_INFO("FaceInfoScreenManager.Reboot.Rebooting", "");
  }
  else
  {
    // fork failed
    LOG_WARNING("FaceInfoScreenManager.Reboot.Failed", "");
  }

#endif
}

void FaceInfoScreenManager::UpdateCameraTestMode(uint32_t curTime_ms)
{
  const ScreenName curScreen = FaceInfoScreenManager::getInstance()->GetCurrScreenName();
  if(curScreen != ScreenName::CameraMotorTest)
  {
    return;
  }

  // Every alternateTime_ms, while we are in the camera test mode,
  // send alternating motor commands
  static const uint32_t alternateTime_ms = 2000;
  static BaseStationTime_t lastMovement_ms = curTime_ms;
  if(curTime_ms - lastMovement_ms > alternateTime_ms)
  {
    lastMovement_ms = curTime_ms;
    static bool up = false;

    RobotInterface::SetHeadAngle head;
    head.angle_rad = (up ? MAX_HEAD_ANGLE : MIN_HEAD_ANGLE);
    head.duration_sec = alternateTime_ms / 1000.f;
    head.max_speed_rad_per_sec = MAX_HEAD_SPEED_RAD_PER_S;
    head.accel_rad_per_sec2 = MAX_HEAD_ACCEL_RAD_PER_S2;
      
    RobotInterface::SetLiftHeight lift;
    lift.height_mm = (up ? LIFT_HEIGHT_CARRY : 50);
    lift.duration_sec = alternateTime_ms / 1000.f;
    lift.max_speed_rad_per_sec = MAX_LIFT_SPEED_RAD_PER_S;
    lift.accel_rad_per_sec2 = MAX_LIFT_ACCEL_RAD_PER_S2;

    RobotInterface::DriveWheels wheels;
    wheels.lwheel_speed_mmps = (up ? 60 : -60);
    wheels.rwheel_speed_mmps = (up ? 60 : -60);
    wheels.lwheel_accel_mmps2 = MAX_WHEEL_ACCEL_MMPS2;
    wheels.rwheel_accel_mmps2 = MAX_WHEEL_ACCEL_MMPS2;

    SendAnimToRobot(std::move(head));
    SendAnimToRobot(std::move(lift));
    SendAnimToRobot(std::move(wheels));

    up = !up;
  }
}

bool FaceInfoScreenManager::CanEnterPairingFromScreen( const ScreenName& screenName) const
{
  switch (screenName)
  {
    case ScreenName::None:
    case ScreenName::FAC:
    case ScreenName::CustomText:
    case ScreenName::Pairing:
    case ScreenName::MirrorMode:
    case ScreenName::AlexaPairing:
    case ScreenName::AlexaPairingSuccess:
    case ScreenName::AlexaPairingFailed:
    case ScreenName::AlexaPairingExpired:
    case ScreenName::ToggleMute:
    case ScreenName::AlexaNotification:
      return true;
    default:
      return false;
  }
}
  
bool FaceInfoScreenManager::IsAlexaScreen(const ScreenName& screenName) const
{
  switch (screenName) {
    case ScreenName::AlexaPairing:
    case ScreenName::AlexaPairingSuccess:
    case ScreenName::AlexaPairingFailed:
    case ScreenName::AlexaPairingExpired:
      return true;
    default:
      return false;
  }
}
  
bool FaceInfoScreenManager::ScreenNeedsWait(const ScreenName& screenName) const
{
  switch (screenName) {
    case ScreenName::AlexaPairing:
    case ScreenName::AlexaPairingSuccess:
    case ScreenName::AlexaPairingFailed:
    case ScreenName::AlexaPairingExpired:
    case ScreenName::ToggleMute:
    case ScreenName::AlexaNotification:
      return true;
    default:
      return false;
  }
}

void FaceInfoScreenManager::SelfTestEnd(Anim::AnimationStreamer* animStreamer)
{
  const ScreenName curScreen = FaceInfoScreenManager::getInstance()->GetCurrScreenName();
  if(curScreen != ScreenName::SelfTestRunning)
  {
    return;
  }

  animStreamer->EnableKeepFaceAlive(true, 0);
  _context->GetBackpackLightComponent()->SetSelfTestRunning(false);
  
  SetScreen(ScreenName::Main);
}
  
void FaceInfoScreenManager::ExitCCScreen(Anim::AnimationStreamer* animStreamer)
{
  const ScreenName curScreen = FaceInfoScreenManager::getInstance()->GetCurrScreenName();
  if(curScreen == ScreenName::SelfTestRunning)
  {
    animStreamer->EnableKeepFaceAlive(true, 0);
    _context->GetBackpackLightComponent()->SetSelfTestRunning(false);
  }
  
  SetScreen(ScreenName::None);
}

} // namespace Vector
} // namespace Anki
