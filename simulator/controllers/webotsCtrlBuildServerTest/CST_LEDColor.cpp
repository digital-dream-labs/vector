#include <webots/Supervisor.hpp>
#include <webots/LED.hpp>
#include "simulator/game/cozmoSimTestController.h"
#include "clad/types/ledTypes.h"
#include "coretech/messaging/shared/UdpServer.h"
#include "clad/types/vizTypes.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"

namespace Anki {
namespace Vector {

bool EqualityCStyleArray(const double* arr1, const double* arr2, int arraySize) {
  for (int i = 0; i < arraySize; i++) {
    if (arr1[i] != arr2[i]) {
      PRINT_NAMED_DEBUG("CST_LEDColor.EqualityCStyleArray",
                        "These two were not equal: arr1 %f, arr2 %f", arr1[i], arr2[i]);
      return false;
    }
  }

  return true;
}

enum class TestState {
  Init,
  WaitForHeadUp,
  SetRGB,
  VerifyLEDColors,
  SetLEDAnimation,
  WaitForMessageToTransmit,
  VerifyLEDAnimation,
  Exit
};

class CST_LEDColor : public CozmoSimTestController
{
public:
  CST_LEDColor();

private:
  uint8_t const * const GetMessage() const;
  s32 UpdateSimInternal() override;
  
  void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;

  TestState _testState = TestState::Init;
  s32 _result = 0;

  int _framesOn = 0;
  int _framesOff = 0;
  bool _onFramesMatched = false;
  bool _offFramesMatched = false;
  enum class LEDAnimationState {
    ON,  // LEDs are in the on state
    OFF  // LEDs are in the off state
  };
  LEDAnimationState _ledState = LEDAnimationState::OFF;
  
  ObjectID _id;
  
  const f32 kHeadLookupAngle_rad = DEG_TO_RAD(10);
  const f32 kHeadAngleTolerance_rad = DEG_TO_RAD(1);
  const int kRed = 255;
  const int kGreen = 255;
  const int kBlue = 255;
  const int kAlpha = 255;
  
  const u32 kRedColor = (kRed << 24) + kAlpha;
  const u32 kGreenColor = (kGreen << 16) + kAlpha;
  const u32 kBlueColor = (kBlue << 8) + kAlpha;
  const u32 kBlackColor = 0x00000000;
  
  // We pick 240 for on/off period here because it is a multiple of BS_TIME_STEP_MS (currently 60ms)
  // which makes it a lot easier to detect the on/off timing correctly
  const u32 kOnPeriod_ms = 240;
  const u32 kOffPeriod_ms = 240;
  const u32 kTransitionOnPeriod_ms = 0;
  const u32 kTransitionOffPeriod_ms = 0;
  const int kOffset_ms = 0;
  const bool kRotate = false;
  // relative x, y are garbage values since MakeRelativeMode = RELATIVE_LED_MODE_OFF; see COZMO-3049
  const f32 kRelativeToX = 0;
  const f32 kRelativeToY = 0;
  const MakeRelativeMode kMakeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_LEDColor);

CST_LEDColor::CST_LEDColor() {}
s32 CST_LEDColor::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      ExternalInterface::EnableLightStates m;
      m.enable = false;
      ExternalInterface::MessageGameToEngine message;
      message.Set_EnableLightStates(m);
      SendMessage(message);
      
      SendMoveHeadToAngle(kHeadLookupAngle_rad, 100, 100);

      // Request a cube connection
      SendConnectToCube();
      
      SET_TEST_STATE(WaitForHeadUp);
      break;
    }

    case TestState::WaitForHeadUp:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(15,
                                            _id >=0,
                                            NEAR(GetRobotHeadAngle_rad(), kHeadLookupAngle_rad,
                                                 kHeadAngleTolerance_rad)) {
        SET_TEST_STATE(SetRGB);
      }
      break;
    }

    case TestState::SetRGB:
    {
      // Set the: front -> red, left->green, back->blue, right->black
      const std::array<u32, 4> onColor = {{kRedColor, kGreenColor, kBlueColor, kBlackColor}};
      const std::array<u32, 4> onPeriod_ms = {{kOnPeriod_ms, kOnPeriod_ms, kOnPeriod_ms, kOnPeriod_ms}};
      const std::array<u32, 4> offPeriod_ms = {{kOffPeriod_ms, kOffPeriod_ms, kOffPeriod_ms, kOffPeriod_ms}};
      const std::array<u32, 4> transitionOnPeriod_ms = {{kTransitionOnPeriod_ms, kTransitionOnPeriod_ms, kTransitionOnPeriod_ms, kTransitionOnPeriod_ms}};
      const std::array<u32, 4> transitionOffPeriod_ms = {{kTransitionOffPeriod_ms, kTransitionOffPeriod_ms, kTransitionOffPeriod_ms, kTransitionOffPeriod_ms}};

      SendSetAllActiveObjectLEDs(_id,
                                 onColor,
                                 onColor,
                                 onPeriod_ms,
                                 offPeriod_ms,
                                 transitionOnPeriod_ms,
                                 transitionOffPeriod_ms,
                                 {{kOffset_ms,kOffset_ms,kOffset_ms,kOffset_ms}},
                                 kRotate,
                                 kRelativeToX,
                                 kRelativeToY,
                                 kMakeRelative);

      SET_TEST_STATE(VerifyLEDColors);
      break;
    }

    case TestState::VerifyLEDColors:
    {
      const webots::Field* colorField = GetNodeByDefName("cube")->getField("ledColors");
      const double kLed0Color[3] = {colorField->getMFVec3f(0)[0], colorField->getMFVec3f(0)[1], colorField->getMFVec3f(0)[2]};  // Back
      const double kLed1Color[3] = {colorField->getMFVec3f(1)[0], colorField->getMFVec3f(1)[1], colorField->getMFVec3f(1)[2]};  // Left
      const double kLed2Color[3] = {colorField->getMFVec3f(2)[0], colorField->getMFVec3f(2)[1], colorField->getMFVec3f(2)[2]};  // Front
      const double kLed3Color[3] = {colorField->getMFVec3f(3)[0], colorField->getMFVec3f(3)[1], colorField->getMFVec3f(3)[2]};  // Right
      
//      PRINT_NAMED_INFO("colorField", "%0llx", (u64)colorField);
//      PRINT_NAMED_INFO("VerifyColors0", "%f %f %f", kLed0Color[0], kLed0Color[1], kLed0Color[2]);
//      PRINT_NAMED_INFO("VerifyColors1", "%f %f %f", kLed1Color[0], kLed1Color[1], kLed1Color[2]);
//      PRINT_NAMED_INFO("VerifyColors2", "%f %f %f", kLed2Color[0], kLed2Color[1], kLed2Color[2]);
//      PRINT_NAMED_INFO("VerifyColors3", "%f %f %f", kLed3Color[0], kLed3Color[1], kLed3Color[2]);

      // We only check if there are any color in each channel at all because there are some post-
      // processing that happens inside engine with the color information sent from game like white
      // balanching which means the LED won't have the exact color that was sent.
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(5,
          kLed0Color[0] != 0,  // red
          kLed0Color[1] == 0,  // green
          kLed0Color[2] == 0,  // blue

          kLed1Color[0] == 0,  // red
          kLed1Color[1] != 0,  // green
          kLed1Color[2] == 0,  // blue

          kLed2Color[0] == 0,  // red
          kLed2Color[1] == 0,  // green
          kLed2Color[2] != 0,  // blue

          kLed3Color[0] == 0,  // red
          kLed3Color[1] == 0,  // green
          kLed3Color[2] == 0)  // blue
      {
        SET_TEST_STATE(SetLEDAnimation);
      }
      break;
    }

    case TestState::SetLEDAnimation:
    {
      SendSetActiveObjectLEDs(_id,
                              kRedColor,
                              kBlackColor,
                              kOnPeriod_ms,
                              kOffPeriod_ms,
                              kTransitionOnPeriod_ms,
                              kTransitionOffPeriod_ms,
                              kOffset_ms,
                              kRotate,
                              kRelativeToX,
                              kRelativeToY,
                              WhichCubeLEDs::ALL,
                              kMakeRelative,
                              true);

      SET_TEST_STATE(WaitForMessageToTransmit);
      break;
    }

    case TestState::WaitForMessageToTransmit:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(HasXSecondsPassedYet(1), 2) {
        SET_TEST_STATE(VerifyLEDAnimation);
      }
      break;
    }

    case TestState::VerifyLEDAnimation:
    {
      // Considered correct if there are correct number of frames in the on period, and the correct
      // number of frames in the off period, back-to-back.

      const webots::Field* colorField = GetNodeByDefName("cube")->getField("ledColors");
      const double kLed0Color[3] = {colorField->getMFVec3f(0)[0], colorField->getMFVec3f(0)[1], colorField->getMFVec3f(0)[2]};  // Back
      const double kLed1Color[3] = {colorField->getMFVec3f(1)[0], colorField->getMFVec3f(1)[1], colorField->getMFVec3f(1)[2]};  // Left
      const double kLed2Color[3] = {colorField->getMFVec3f(2)[0], colorField->getMFVec3f(2)[1], colorField->getMFVec3f(2)[2]};  // Front
      const double kLed3Color[3] = {colorField->getMFVec3f(3)[0], colorField->getMFVec3f(3)[1], colorField->getMFVec3f(3)[2]};  // Right

      DEV_ASSERT_MSG(EqualityCStyleArray(kLed0Color, kLed1Color, 3) &&
                     EqualityCStyleArray(kLed1Color, kLed2Color, 3) &&
                     EqualityCStyleArray(kLed2Color, kLed3Color, 3),
                     "CST_LEDColor.VerifyLEDAnimation",
                     "All the LEDs should have the same color at this stage.");

      if (kOnPeriod_ms % BS_TIME_STEP_MS != 0 || kOffPeriod_ms % BS_TIME_STEP_MS != 0) {
        PRINT_NAMED_WARNING("CST_LEDColor.VerifyLEDAnimation",
                            "If on or off period is not divisible by BS_TIME_STEP_MS it is going to be"
                            "difficult to verify the on or off period because this update loop will"
                            "be out of sync with the on/off frequency.");
      }

      if (kLed0Color[0] == 0 && kLed0Color[1] == 0 && kLed0Color[2] == 0) {
        PRINT_NAMED_DEBUG("CST_LEDColor.VerifyLEDAnimation", "OFF state");
        _ledState = LEDAnimationState::OFF;
        _framesOn = 0;
        _framesOff++;
      } else {
        PRINT_NAMED_DEBUG("CST_LEDColor.VerifyLEDAnimation", "ON state");
        _ledState = LEDAnimationState::ON;
        _framesOff = 0;
        _framesOn++;
      }

      if (_ledState == LEDAnimationState::ON) {
        if (_framesOn == kOnPeriod_ms / BS_TIME_STEP_MS) {
          _onFramesMatched = true;
        } else {
          _onFramesMatched = false;
        }
      } else if (_ledState == LEDAnimationState::OFF) {
        if (_framesOff == kOffPeriod_ms / BS_TIME_STEP_MS) {
          _offFramesMatched = true;
        } else {
          _offFramesMatched = false;
        }
      }

      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(5, _onFramesMatched, _offFramesMatched) {
        SET_TEST_STATE(Exit);
      }
    }
      break;

    case TestState::Exit:
    {
      CST_EXIT();
      break;
    }
  }
  return _result;
}

void CST_LEDColor::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
{
  if(msg.connected)
  {
    _id = msg.objectID;
  }
  
}

}  // namespace Vector
}  // namespace Anki
