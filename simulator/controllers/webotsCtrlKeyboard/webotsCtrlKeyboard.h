/*
 * File:          webotsCtrlKeyboard.cpp
 * Date:
 * Description:
 * Author:
 * Modifications:
 */

#ifndef __webotsCtrlKeyboard_H_
#define __webotsCtrlKeyboard_H_

#include "simulator/game/uiGameController.h"

namespace Anki {
namespace Vector {

class WebotsKeyboardController : public UiGameController {
public:
  WebotsKeyboardController(s32 step_time_ms);

  // Called before WaitOnKeyboardToConnect (and also before Init), sets up the basics needed for
  // WaitOnKeyboardToConnect, including enabling the keyboard
  void PreInit();
  
  // if the corresponding proto field is set to true, this function will wait until the user presses
  // Shift+enter to return.This can be used to allow unity to connect. If we notice another connection
  // attempt, we will exit the keyboard controller. This is called before Init
  void WaitOnKeyboardToConnect();

protected:
  bool RegisterKeyFcn(int key, int modifier, std::function<void()> fcn, const char* help_msg, std::string display_string = "");
  void ProcessKeystroke();
  void ProcessKeyPressFunction(int key, int modifier);
  
  // === Key press functions ===
  void PrintHelp();
  
  void LogRawProxData();
  void ToggleVizDisplay();
  void TogglePoseMarkerMode();
  void PlayNeedsGetOutAnimIfNeeded();
  void GotoPoseMarker();
  
  void ToggleEngineLightComponent();
  void SearchForNearbyObject();
  void ToggleCliffSensorEnable();
  void ToggleTestBackpackLights();
  void DoCliffAlignToWhite();
  
  void ToggleTrackToFace();
  void ToggleTrackToObject();
  void TrackPet();
  void ExecuteTestPlan();
  
  void ExecuteBehavior();
  void LogCliffSensorData();
  
  void FakeCloudIntent();
  void FakeUserIntent();

  void SetEmotion();
  void TriggerEmotionEvent();
  
  void PickOrPlaceObject();
  void MountSelectedCharger();
  void TeleportOntoCharger();
  void FlipSelectedBlock();
  
  void PopAWheelie();
  void RollObject();
  
  void SetControllerGains();
  
  void ToggleVisionWhileMoving();
  void SetRobotVolume();
  
  void SetActiveObjectLights();
  
  void AlignWithObject();
  void TurnTowardsObject();
  void GotoObject();
  void RequestIMUData();
  
  void AssociateNameWithCurrentFace();
  void TurnTowardsFace();
  void EraseLastObservedFace();
  void ToggleFaceDetection();
  
  void ToggleEyeRendering();
  
  void SetDefaultKeepFaceAliveParams();
  void SetKeepFaceAliveParams();
  
  void RequestSingleImageToGame();
  void ToggleImageStreamingToGame();
  void SaveSingleImage();
  void ToggleImageSaving();
  void ToggleImageAndStateSaving();
  
  void TurnInPlaceCCW();
  void TurnInPlaceCW();
  
  void ExecutePlaypenTest();
  
  void SetFaceDisplayHue();
  void SendRandomProceduralFace();
  
  void PlayAnimation();
  void PlayAnimationGroup();
  void PlayAnimationTrigger();
  
  void RunDebugConsoleFunc();
  void SetDebugConsoleVar();

  void SetRollActionParams();
  void SetCameraSettings();
  void SayText();
  void PlayCubeAnimation();
  
  void TogglePowerMode();

  void TurnTowardsImagePoint();
  void QuitKeyboardController();
  void ToggleLiftPower();
  
  void MoveLiftToLowDock();
  void MoveLiftToHighDock();
  void MoveLiftToCarryHeight();
  void MoveLiftToAngle();
  
  void MoveHeadToLowLimit();
  void MoveHeadToHorizontal();
  void MoveHeadToHighLimit();
  
  void MoveHeadUp();
  void MoveHeadDown();
  void MoveLiftUp();
  void MoveLiftDown();
  
  void DriveForward();
  void DriveBackward();
  void DriveLeft();
  void DriveRight();
  
  void ExecuteRobotTestMode();
  void PressBackButton();
  void TouchBackSensor();

  void CycleConnectionFlowState();

  void ToggleCameraCaptureFormat();
  
  // ==== End of key press functions ====
  
  
  f32 GetLiftSpeed_radps();
  f32 GetLiftAccel_radps2();
  f32 GetLiftDuration_sec();
  f32 GetHeadSpeed_radps();
  f32 GetHeadAccel_radps2();
  f32 GetHeadDuration_sec();
  
  void TestLightCube();
  
  Pose3d GetGoalMarkerPose();
  
  virtual void InitInternal() override;
  virtual s32 UpdateInternal() override;

  virtual void HandleImageChunk(const ImageChunk& msg) override;
  virtual void HandleRobotObservedObject(const ExternalInterface::RobotObservedObject& msg) override;
  virtual void HandleRobotObservedFace(const ExternalInterface::RobotObservedFace& msg) override;
  virtual void HandleRobotObservedPet(const ExternalInterface::RobotObservedPet& msg) override;
  virtual void HandleLoadedKnownFace(const Vision::LoadedKnownFace& msg) override;
  virtual void HandleEngineErrorCode(const ExternalInterface::EngineErrorCodeMessage& msg) override;
  virtual void HandleRobotConnected(const ExternalInterface::RobotConnectionResponse& msg) override;
  
private:

  bool _shouldQuit = false;

  webots::Node* _chargerNode = nullptr;
  
}; // class WebotsKeyboardController
} // namespace Vector
} // namespace Anki

#endif  // __webotsCtrlKeyboard_H_
