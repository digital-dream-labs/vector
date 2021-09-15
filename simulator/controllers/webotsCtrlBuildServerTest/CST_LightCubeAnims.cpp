/**
 * File: CST_LightCubeAnims.cpp
 *
 * Author: Matt Michini
 * Created: 2/16/2018
 *
 * Description: Plays a few test "cube light animations" and verifies that the simulated
 *              cube LED colors change as expected based on the animations' json definitions.
 *
 * Copyright: Anki, inc. 2018
 *
 */

#include <webots/Supervisor.hpp>
#include "simulator/game/cozmoSimTestController.h"
#include "clad/types/ledTypes.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "util/graphEvaluator/graphEvaluator2d.h"

namespace Anki {
namespace Vector {

enum class TestState {
  Init,
  WaitForHeadUp,
  VerifyAllLeds,  // Plays and verifies "testAllLeds" anim
  VerifyOffset,   // Plays and verifies "testOffset" anim
  VerifyRotation, // Plays and verifies "testRotation" anim
  Exit
};

class CST_LightCubeAnims : public CozmoSimTestController
{
public:
  CST_LightCubeAnims();

private:
  s32 UpdateSimInternal() override;
  
  void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;

  void SetupExpectedLedPattern(Util::GraphEvaluator2d& graphEvaluator);
  
  using LEDValue = std::array<double, 3>;
  
  // Helper to grab the RGB LED color of the cube with id _id
  LEDValue GetLedColor(const u16 ledIndex);
  
  // Asserts that the given LED values are all within tolerance
  // of the actual current LED values.
  void AssertLedValues(const std::array<LEDValue, 4> expectedLedVals,
                       const char* errorStr,
                       const double tolerance = kLedValueTol);
  
  // For testRotation, this verifies that the expected pattern, rotated by
  // rotationPhase, is being displayed correctly on the cube LEDs.
  void AssertRotationLedsCorrect(const u16 rotationPhase);
  
  // This pattern describes a curve that certain LEDs in the "testAllLeds"
  // and "testOffset" cube light animations are expected to conform to
  Util::GraphEvaluator2d _expectedLedPattern;
  
  const float kMaxLedValue = 255.f;
  
  // How different the actual LED value is allowed to be compared to
  // expected LED value. Fudge factor, since timing during fades may
  // cause the actual color to be slightly different from expected.
  static constexpr double kLedValueTol = 30.0;
  
  // Offset in seconds used in the "testOffset" animation
  const float kLedOffset_sec = 0.6f;
  
  // Length of the "testRotation" animation
  const float kRotationAnimLength_sec = 0.45f;
  
  // Cube animation triggers for the test animations
  const CubeAnimationTrigger _testAllLedsTrigger  = CubeAnimationTrigger::TestAllLeds;
  const CubeAnimationTrigger _testOffsetTrigger   = CubeAnimationTrigger::TestOffset;
  const CubeAnimationTrigger _testRotationTrigger = CubeAnimationTrigger::TestRotation;
  
  // Keep track of when the animation started (to compute expected color values)
  float _animStartTime_sec = 0.f;
  
  TestState _testState = TestState::Init;
  
  ObjectID _id;
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_LightCubeAnims);

CST_LightCubeAnims::CST_LightCubeAnims() {}
  
s32 CST_LightCubeAnims::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      SetupExpectedLedPattern(_expectedLedPattern);
      
      // Turn off engine-controlled cube lights
      ExternalInterface::EnableLightStates m;
      m.enable = false;
      ExternalInterface::MessageGameToEngine message;
      message.Set_EnableLightStates(m);
      SendMessage(message);
      
      SendMoveHeadToAngle(0.f, 100, 100);
      
      // Request a cube connection
      SendConnectToCube();

      SET_TEST_STATE(WaitForHeadUp);
      break;
    }

    case TestState::WaitForHeadUp:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(15,
                                            _id >= 0,
                                            NEAR(GetRobotHeadAngle_rad(), 0.f, HEAD_ANGLE_TOL)) {
        SendCubeAnimation(_id, _testAllLedsTrigger);
        // Add BS_TIME_STEP_MS because the engine will take one tick to start the anim
        _animStartTime_sec = GetSupervisorTime() + BS_TIME_STEP_MS/1000.f;
        SET_TEST_STATE(VerifyAllLeds);
      }
      break;
    }
      
    case TestState::VerifyAllLeds:
    {
      const float now_sec = GetSupervisorTime();
      const float timeIntoAnimation_sec = now_sec - _animStartTime_sec;
      const double curExpectedLedValue = (double) _expectedLedPattern.EvaluateY(timeIntoAnimation_sec);
      
      // LED0:red, LED1:green, and LED2:blue should all be following the
      // expected pattern, and all others should be zero (see testAllLeds.json)
      AssertLedValues({{
        {{curExpectedLedValue, 0.0, 0.0}},
        {{0.0, curExpectedLedValue, 0.0}},
        {{0.0, 0.0, curExpectedLedValue}},
        {{0.0, 0.0, 0.0}}
      }},
      "CST_LightCubeAnims.VerifyAllLeds.WrongLedColor");
      
      // Transition to next state as soon as we're past the expected curve's x range
      const auto& lastNode = _expectedLedPattern.GetNode(_expectedLedPattern.GetNumNodes()-1);
      if (timeIntoAnimation_sec > lastNode._x) {
        SendStopCubeAnimation(_id, _testAllLedsTrigger);
        SendCubeAnimation(_id, _testOffsetTrigger);
        // Add BS_TIME_STEP_MS because the engine will take one tick to start the anim
        _animStartTime_sec = GetSupervisorTime() + BS_TIME_STEP_MS/1000.f;
        SET_TEST_STATE(VerifyOffset);
      }
      break;
    }

    case TestState::VerifyOffset:
    {
      const float now_sec = GetSupervisorTime();
      const float timeIntoAnimation_sec = now_sec - _animStartTime_sec;

      // LED0:red, LED1:green, and LED2:blue should all be following the
      // expected pattern, but with the configured offset (see testOffset.json)
      const double curExpectedLedValueRed   = (double) _expectedLedPattern.EvaluateY(timeIntoAnimation_sec);
      const double curExpectedLedValueGreen = (double) _expectedLedPattern.EvaluateY(timeIntoAnimation_sec - kLedOffset_sec);
      const double curExpectedLedValueBlue  = (double) _expectedLedPattern.EvaluateY(timeIntoAnimation_sec - 2.f*kLedOffset_sec);
      
      AssertLedValues({{
        {{curExpectedLedValueRed, 0.0, 0.0}},
        {{0.0, curExpectedLedValueGreen, 0.0}},
        {{0.0, 0.0, curExpectedLedValueBlue}},
        {{0.0, 0.0, 0.0}}
      }},
      "CST_LightCubeAnims.VerifyOffset.WrongLedColor");
      
      // Transition to next state as soon as we're past the expected curve's x range
      const auto& lastNode = _expectedLedPattern.GetNode(_expectedLedPattern.GetNumNodes()-1);
      if (timeIntoAnimation_sec > lastNode._x) {
        SendStopCubeAnimation(_id, _testOffsetTrigger);
        SendCubeAnimation(_id, _testRotationTrigger);
        // Add BS_TIME_STEP_MS because the engine will take one tick to start the anim
        _animStartTime_sec = GetSupervisorTime() + BS_TIME_STEP_MS/1000.f;
        SET_TEST_STATE(VerifyRotation);
      }
      break;
    }
      
    case TestState::VerifyRotation:
    {
      const float now_sec = GetSupervisorTime();
      const float timeIntoAnimation_sec = now_sec - _animStartTime_sec;
      const float timeTol_sec = 0.15f;
      
      // Midway through each repetition of the animation, test that the expected colors are correct
      if (NEAR(timeIntoAnimation_sec, 0.5f * kRotationAnimLength_sec, timeTol_sec)) {
        // Should be in the 0th rotation phase (LED0:red, LED1:green, and LED2:blue all on)
        AssertRotationLedsCorrect(0);
      } else if (NEAR(timeIntoAnimation_sec, 1.5f * kRotationAnimLength_sec, timeTol_sec)) {
        // Should be in the first rotation phase (LED1:red, LED2:green, and LED3:blue all on)
        AssertRotationLedsCorrect(1);
      } else if (NEAR(timeIntoAnimation_sec, 2.5f * kRotationAnimLength_sec, timeTol_sec)) {
        AssertRotationLedsCorrect(2);
      } else if (NEAR(timeIntoAnimation_sec, 3.5f * kRotationAnimLength_sec, timeTol_sec)) {
        AssertRotationLedsCorrect(3);
      }
      
      if (timeIntoAnimation_sec >= 4*kRotationAnimLength_sec) {
        SET_TEST_STATE(Exit);
      }
      break;
    }

    case TestState::Exit:
    {
      CST_EXIT();
      break;
    }
  }
  return _result;
}

void CST_LightCubeAnims::SetupExpectedLedPattern(Util::GraphEvaluator2d& graphEvaluator)
{
  // This pattern must match the test animation's json file
  const float period_sec  = 1.2f;
  
  // Create a trapezoid matching the expected light animation LED curve:
  //    ___      255
  //   /   \
  //  /     \
  // /       \___ 0
  // 0  1  2  3  4   (periods)
  graphEvaluator.AddNode(0.f             , 0.f);
  graphEvaluator.AddNode(1.f * period_sec, kMaxLedValue);
  graphEvaluator.AddNode(2.f * period_sec, kMaxLedValue);
  graphEvaluator.AddNode(3.f * period_sec, 0.f);
  graphEvaluator.AddNode(4.f * period_sec, 0.f);
}
                   
void CST_LightCubeAnims::AssertRotationLedsCorrect(const u16 rotationPhase)
{
  // Rotated LED0:red, LED1:green, and LED2:blue should be at full value, and rest
  // should be at 0.
  std::array<LEDValue, 4> expectedLedValues =
  {{
    {{kMaxLedValue, 0.0, 0.0}},
    {{0.0, kMaxLedValue, 0.0}},
    {{0.0, 0.0, kMaxLedValue}},
    {{0.0, 0.0, 0.0}}
  }};
  
  // Rotate the expected value array by rotationPhase (right rotation):
  std::rotate(expectedLedValues.rbegin(),
              expectedLedValues.rbegin() + rotationPhase,
              expectedLedValues.rend());
  
  AssertLedValues(expectedLedValues,
                  "CST_LightCubeAnims.AssertRotationLedsCorrect.WrongLedColor");
}

CST_LightCubeAnims::LEDValue CST_LightCubeAnims::GetLedColor(const u16 ledIndex)
{
  DEV_ASSERT(ledIndex <= 4, "CST_LightCubeAnims.GetLedColor.InvalidIndex");
  
  static const webots::Field* colorField = GetNodeByDefName("cube")->getField("ledColors");
  DEV_ASSERT(colorField != nullptr, "CST_LightCubeAnims.GetLedColor.NullColorField");
  
  const double* colorVec = colorField->getMFVec3f(ledIndex);
  return {{colorVec[0], colorVec[1], colorVec[2]}};
}
  
void CST_LightCubeAnims::AssertLedValues(const std::array<LEDValue, 4> expectedLedVals,
                                         const char* errorStr,
                                         const double tolerance)
{
  for (int ledIndex = 0 ; ledIndex < expectedLedVals.size() ; ledIndex++) {
    for (int colorInd = 0 ; colorInd < 3 ; colorInd++) {
      const auto& expectedLedVal = expectedLedVals[ledIndex][colorInd];
      const auto& actualLedVal = GetLedColor(ledIndex)[colorInd];
      if (!NEAR(expectedLedVal, actualLedVal, tolerance)) {
        PRINT_NAMED_ERROR("CST_LightCubeAnims.AssertLedValues.WrongValue",
                          "Actual LED value of %f is not near expected value of %f (tol %f) for ledIndex %d, colorIndex %d",
                          actualLedVal, expectedLedVal, kLedValueTol, ledIndex, colorInd);
        CST_ASSERT(false && "WrongLedColor", errorStr);
      }
    }
  }
}
  
void CST_LightCubeAnims::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
{
  if(msg.connected) {
    _id = msg.objectID;
  }
}

}  // namespace Vector
}  // namespace Anki
