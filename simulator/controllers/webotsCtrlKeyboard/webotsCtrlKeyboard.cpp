/*
 * File:          webotsCtrlKeyboard.cpp
 * Date:
 * Description:
 * Author:
 * Modifications:
 */

#include "webotsCtrlKeyboard.h"

#include "../shared/ctrlCommonInitialization.h"
#include "coretech/common/engine/colorRGBA.h"
#include "coretech/common/engine/math/pose.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/image.h"
#include "clad/types/actionTypes.h"
#include "clad/types/behaviorComponent/behaviorIDs.h"
#include "clad/types/ledTypes.h"
#include "clad/types/proceduralFaceTypes.h"
#include "engine/block.h"
#include "engine/encodedImage.h"
#include "util/cladHelpers/cladFromJSONHelpers.h"
#include "simulator/controllers/shared/webotsHelpers.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/printByteArray.h"
#include "util/logging/channelFilter.h"
#include "util/logging/printfLoggerProvider.h"
#include "util/math/math.h"
#include "util/random/randomGenerator.h"
#include <fstream>
#include <opencv2/imgproc/imgproc.hpp>
#include <stdio.h>
#include <string.h>
#include <webots/Compass.hpp>
#include <webots/Display.hpp>
#include <webots/GPS.hpp>
#include <webots/ImageRef.hpp>
#include <webots/Keyboard.hpp>

#define LOG_CHANNEL "Keyboard"

namespace Anki {
namespace Vector {


  // Private members:
  namespace {

  static const Transform3d kTeleportToChargerOffset({ M_PI_2_F, Z_AXIS_3D() },
                                                    { 0.0f, 76.696196f, 10.0f });

    std::set<int> lastKeysPressed_;

    s8 _steeringDir = 0;  // -1 = left, 0 = straight, 1 = right
    s8 _throttleDir = 0;  // -1 = reverse, 0 = stop, 1 = forward

    bool _pressBackpackButton = false;
    bool _wasBackpackButtonPressed = false;

    bool _touchBackpackTouchSensor = false;
    bool _wasBackpackTouchSensorTouched = false;

    f32 _commandedLiftSpeed = 0.f;
    f32 _commandedHeadSpeed = 0.f;

    bool _movingHead      = false;
    bool _movingLift      = false;

    bool _wasMovingWheels = false;
    bool _wasMovingHead   = false;
    bool _wasMovingLift   = false;

    s16  lastDrivingCurvature_mm_ = 0;

    webots::Node* root_ = nullptr;

    u8 poseMarkerMode_ = 0;
    Anki::Pose3d prevGoalMarkerPose_;
    webots::Field* poseMarkerDiffuseColor_ = nullptr;
    double poseMarkerColor_[2][3] = { {0.1, 0.8, 0.1} // Goto pose color
      ,{0.8, 0.1, 0.1} // Place object color
    };

    double lastKeyPressTime_;

    PathMotionProfile pathMotionProfile_ = PathMotionProfile();

    // For displaying cozmo's POV:
    webots::Display* uiCamDisplay_ = nullptr;
    webots::ImageRef* img_ = nullptr;

    EncodedImage _encodedImage;

    std::string _drivingStartAnim = "";
    std::string _drivingLoopAnim = "";
    std::string _drivingEndAnim = "";

    struct ObservedImageCentroid {
      Point2f          point;
      RobotTimeStamp_t timestamp;

      template<class MsgType>
      void SetFromMessage(const MsgType& msg)
      {
        point.x() = msg.img_rect.x_topLeft + msg.img_rect.width*0.5f;
        point.y() = msg.img_rect.y_topLeft + msg.img_rect.height*0.5f;
        timestamp = msg.timestamp;
      }

    } _lastObservedImageCentroid;

    std::set<u32> streamingAccelObjIds; // ObjectIDs of objects that are currently streaming accelerometer data.

    ImageSendMode _imageStreamSavingMode = ImageSendMode::Off;

    int  _currKey;
    bool _shiftKeyPressed;
    bool _altKeyPressed;

    bool useApproachAngle;
    f32 approachAngle_rad;

    // Key press mapping
    typedef struct {
      std::function<void()> fcn;
      std::string helpMsg = "";
      std::string displayString = "";
    } KeyPressFcnInfo;
    using ModifierToFcnMap_t = std::map<int, KeyPressFcnInfo>;
    std::map<int, ModifierToFcnMap_t> _keyFcnMap;
    std::vector<int> _keyRegistrationOrder; // For printing help menu in order of insertion

    enum {
      MOD_NONE = 0,
      MOD_SHIFT = webots::Keyboard::SHIFT,
      MOD_ALT = webots::Keyboard::ALT,
      MOD_ALT_SHIFT = webots::Keyboard::ALT | webots::Keyboard::SHIFT,
    };

  } // private namespace

  // ======== Message handler callbacks =======

  // For processing image chunks arriving from robot.
  // Sends complete images to VizManager for visualization (and possible saving).
  void WebotsKeyboardController::HandleImageChunk(const ImageChunk& msg)
  {
    const bool isImageReady = _encodedImage.AddChunk(msg);

    if(isImageReady)
    {
      Vision::ImageRGB img;
      Result result = _encodedImage.DecodeImageRGB(img);
      if(RESULT_OK != result) {
        printf("WARNING: image decode failed");
        return;
      }

      cv::Mat cvImg = img.get_CvMat_();

      const s32 outputColor = 1; // 1 for Green, 2 for Blue

      for(s32 i=0; i<cvImg.rows; ++i) {

        if(i % 2 == 0) {
          cv::Mat img_i = cvImg.row(i);
          img_i.setTo(0);
        } else {
          u8* img_i = cvImg.ptr(i);
          for(s32 j=0; j<cvImg.cols; ++j) {
            img_i[3*j+outputColor] = std::max(std::max(img_i[3*j], img_i[3*j + 1]), img_i[3*j + 2]);

            img_i[3*j+(3-outputColor)] /= 2;
            img_i[3*j] = 0; // kill red channel

            // [Optional] Add a bit of noise
            f32 noise = 20.f*static_cast<f32>(std::rand()) / static_cast<f32>(RAND_MAX) - 0.5f;
            img_i[3*j+outputColor] = static_cast<u8>(std::max(0.f,std::min(255.f,static_cast<f32>(img_i[3*j+outputColor]) + noise)));

          }
        }
      }


      if (uiCamDisplay_ != nullptr) {
        // Delete existing image if there is one.
        if (img_ != nullptr) {
          uiCamDisplay_->imageDelete(img_);
        }
        img_ = uiCamDisplay_->imageNew(cvImg.cols, cvImg.rows, cvImg.data, webots::Display::RGB);
        uiCamDisplay_->imagePaste(img_, 0, 0);
      }

    } // if(isImageReady)

  } // HandleImageChunk()


  void WebotsKeyboardController::HandleRobotObservedObject(const ExternalInterface::RobotObservedObject& msg)
  {
    if (uiCamDisplay_ != nullptr)
    {
      // Draw a rectangle in red with the object ID as text in the center
      uiCamDisplay_->setColor(0x000000);

      //std::string dispStr(ObjectType::GetName(msg.objectType));
      //dispStr += " ";
      //dispStr += std::to_string(msg.objectID);
      std::string dispStr("Type=" + std::string(ObjectTypeToString(msg.objectType)) + "\nID=" + std::to_string(msg.objectID));
      uiCamDisplay_->drawText(dispStr,
                          msg.img_rect.x_topLeft + msg.img_rect.width/4 + 1,
                          msg.img_rect.y_topLeft + msg.img_rect.height/2 + 1);

      uiCamDisplay_->setColor(0xff0000);
      uiCamDisplay_->drawRectangle(msg.img_rect.x_topLeft, msg.img_rect.y_topLeft,
                                   msg.img_rect.width, msg.img_rect.height);
      uiCamDisplay_->drawText(dispStr,
                              msg.img_rect.x_topLeft + msg.img_rect.width/4,
                              msg.img_rect.y_topLeft + msg.img_rect.height/2);
    }
    // Record centroid of observation in image
    _lastObservedImageCentroid.SetFromMessage(msg);
  }

  void WebotsKeyboardController::HandleRobotObservedFace(const ExternalInterface::RobotObservedFace& msg)
  {
    //printf("RECEIVED FACE OBSERVED: faceID %llu\n", msg.faceID);
    // _lastFace = msg;

    // Record centroid of observation in image
    _lastObservedImageCentroid.SetFromMessage(msg);
  }

  void WebotsKeyboardController::HandleRobotObservedPet(const ExternalInterface::RobotObservedPet& msg)
  {
    // Record centroid of observation in image
    _lastObservedImageCentroid.SetFromMessage(msg);
  }

  void WebotsKeyboardController::HandleLoadedKnownFace(const Vision::LoadedKnownFace& msg)
  {
    printf("HandleLoadedKnownFace: '%s' (ID:%d) first enrolled %lld seconds ago, last updated %lld seconds ago, last seen %lld seconds ago\n",
           msg.name.c_str(), msg.faceID, msg.secondsSinceFirstEnrolled, msg.secondsSinceLastUpdated, msg.secondsSinceLastSeen);
  }

  void WebotsKeyboardController::HandleEngineErrorCode(const ExternalInterface::EngineErrorCodeMessage& msg)
  {
    printf("HandleEngineErrorCode: %s\n", EnumToString(msg.errorCode));
  }

  // ============== End of message handlers =================

  void WebotsKeyboardController::PreInit()
  {
    // Make root point to WebotsKeyBoardController node
    root_ = GetSupervisor().getSelf();

    // enable keyboard
    GetSupervisor().getKeyboard()->enable(GetStepTimeMS());
  }

  void WebotsKeyboardController::WaitOnKeyboardToConnect()
  {
    webots::Field* autoConnectField = root_->getField("autoConnect");
    if( autoConnectField == nullptr ) {
      PRINT_NAMED_ERROR("WebotsKeyboardController.MissingField",
                        "missing autoConnect field, assuming we should auto connect");
      return;
    }
    else {
      bool autoConnect = autoConnectField->getSFBool();
      if( autoConnect ) {
        return;
      }
    }

    LOG_INFO("WebotsKeyboardController.WaitForStart", "Press Shift+Enter to start the engine");

    const int EnterKey = 4; // tested experimentally... who knows if this will work on other platforms
    const int ShiftEnterKey = EnterKey | webots::Keyboard::SHIFT;

    bool start = false;
    while( !start && !_shouldQuit ) {
      int key = -1;
      while((key = GetSupervisor().getKeyboard()->getKey()) >= 0 && !_shouldQuit) {
        if(!start && key == ShiftEnterKey) {
          start = true;
          LOG_INFO("WebotsKeyboardController.StartEngine", "Starting our engines....");
        }
      }
      // manually step simulation
      GetSupervisor().step(GetStepTimeMS());
    }
  }

  void WebotsKeyboardController::InitInternal()
  {
    poseMarkerDiffuseColor_ = root_->getField("poseMarkerDiffuseColor");

    const int displayWidth  = root_->getField("uiCamDisplayWidth")->getSFInt32();
    const int displayHeight = root_->getField("uiCamDisplayHeight")->getSFInt32();
    if (displayWidth > 0 && displayHeight > 0)
    {
      uiCamDisplay_ = GetSupervisor().getDisplay("uiCamDisplay");
    }

    _lastObservedImageCentroid.point = {-1.f,-1.f};
  }




  // ======== Start of key press functions =========

  void WebotsKeyboardController::RequestSingleImageToGame()
  {
    PRINT_NAMED_INFO("RequestSingleImage", "");
    SendImageRequest(ImageSendMode::SingleShot);
  }

  void WebotsKeyboardController::ToggleImageStreamingToGame() {

    static ImageSendMode mode = ImageSendMode::Stream;
    if (mode == ImageSendMode::Stream) {
      mode = ImageSendMode::Off;
    } else {
      mode = ImageSendMode::Stream;
    }

    PRINT_NAMED_INFO("ToggleImageStreaming", "Mode: %s", EnumToString(mode));
    SendImageRequest(mode);
  }

  void WebotsKeyboardController::LogRawProxData()
  {
    SendLogProxDataRequest(2000);
  }

  void WebotsKeyboardController::ToggleVizDisplay()
  {
    static bool showObjects = false;
    SendEnableDisplay(showObjects);
    showObjects = !showObjects;
  }

  void WebotsKeyboardController::SaveSingleImage()
  {
    PRINT_NAMED_INFO("SaveSingleImage","");
    SendSaveImages(ImageSendMode::SingleShot);
  }

  void WebotsKeyboardController::ToggleImageSaving()
  {
    // Toggle saving of images to pgm
    if (_imageStreamSavingMode == ImageSendMode::Stream) {
      _imageStreamSavingMode = ImageSendMode::Off;
    } else {
      _imageStreamSavingMode = ImageSendMode::Stream;
    }

    PRINT_NAMED_INFO("ToggleImageSaving", "Mode: %s", EnumToString(_imageStreamSavingMode));
    SendSaveImages(_imageStreamSavingMode);
  }

  void WebotsKeyboardController::ToggleImageAndStateSaving()
  {
    ToggleImageSaving();
    PRINT_NAMED_INFO("ToggleImageAndStateSaving", "");
    SendSaveState(_imageStreamSavingMode != ImageSendMode::Off);
  }


  void WebotsKeyboardController::TogglePoseMarkerMode()
  {
    poseMarkerMode_ = !poseMarkerMode_;
    printf("Pose marker mode: %d\n", poseMarkerMode_);
    poseMarkerDiffuseColor_->setSFColor(poseMarkerColor_[poseMarkerMode_]);
    SendErasePoseMarker();
  }

  void WebotsKeyboardController::GotoPoseMarker()
  {
    if (poseMarkerMode_ == 0) {
      // Execute path to pose

      // the pose of the green-cone marker in the WebotsOrigin frame
      Pose3d goalMarkerPose = GetGoalMarkerPose();
      printf("Going to pose marker at x=%f y=%f angle=%f\n",
             goalMarkerPose.GetTranslation().x(),
             goalMarkerPose.GetTranslation().y(),
             goalMarkerPose.GetRotationAngle<'Z'>().ToFloat());

      // note: Goal is w.r.t. webots origin which may not match
      // engine origin (due to delocalization or drift). This
      // correction makes them match so the robot drives to where
      // you actually see the goal in Webots.
      //
      // pose math below:
      //
      // G = goal marker
      // E = engine
      // W = webots
      // R = robot
      //
      // Pose^E_G = Pose^E_W                 * Pose^W_G
      //          = Pose^E_R *     Pose^R_W  * Pose^W_G
      //          = Pose^E_R * inv(Pose^W_R) * Pose^W_G
      Pose3d markerPose_inEngineFrame = GetRobotPose() *
      GetRobotPoseActual().GetInverse() *
      goalMarkerPose;

      SendExecutePathToPose(markerPose_inEngineFrame, pathMotionProfile_);
      //SendMoveHeadToAngle(-0.26, headSpeed, headAccel);
    } else {
      Pose3d goalMarkerPose = GetGoalMarkerPose();

      // For placeOn and placeOnGround, specify whether or not to use the exactRotation specified
      bool useExactRotation = root_->getField("useExactPlacementRotation")->getSFBool();

      // Indicate whether or not to place object at the exact rotation specified or
      // just use the nearest preActionPose so that it's merely aligned with the specified pose.
      printf("Setting block on ground at rotation %f rads about z-axis (%s)\n", goalMarkerPose.GetRotationAngle<'Z'>().ToFloat(), useExactRotation ? "Using exact rotation" : "Using nearest preActionPose" );

      SendPlaceObjectOnGroundSequence(goalMarkerPose,
                                      pathMotionProfile_,
                                      useExactRotation);
      // Make sure head is tilted down so that it can localize well
      //SendMoveHeadToAngle(-0.26, headSpeed, headAccel);

    }
  }

  void WebotsKeyboardController::ToggleEngineLightComponent()
  {
    ExternalInterface::EnableLightStates msg;
    static bool enableLightComponent = false;
    LOG_INFO("ToggleEngineLightComponent.EnableLightsComponent",
             "EnableLightsComponent: %s",
             enableLightComponent ? "TRUE" : "FALSE");
    msg.enable = enableLightComponent;
    enableLightComponent = !enableLightComponent;

    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_EnableLightStates(msg);
    SendMessage(msgWrapper);
  }

  void WebotsKeyboardController::SearchForNearbyObject()
  {
    ExternalInterface::QueueSingleAction msg;

    using SFNOD = ExternalInterface::SearchForNearbyObjectDefaults;
    ExternalInterface::SearchForNearbyObject searchAction {
      -1,
      Util::numeric_cast<f32>(Util::EnumToUnderlying(SFNOD::BackupDistance_mm)),
      Util::numeric_cast<f32>(Util::EnumToUnderlying(SFNOD::BackupSpeed_mms)),
      Util::numeric_cast<f32>(DEG_TO_RAD(Util::EnumToUnderlying(SFNOD::HeadAngle_deg)))
    };
    msg.action.Set_searchForNearbyObject(std::move(searchAction));

    SendAction(msg);
  }

  void WebotsKeyboardController::ToggleCliffSensorEnable()
  {
    static bool enableCliffSensor = false;

    printf("setting enable cliff sensor to %d\n", enableCliffSensor);
    ExternalInterface::MessageGameToEngine msg;
    msg.Set_EnableCliffSensor(ExternalInterface::EnableCliffSensor{enableCliffSensor});
    SendMessage(msg);

    enableCliffSensor = !enableCliffSensor;
  }

  void WebotsKeyboardController::DoCliffAlignToWhite()
  {
    ExternalInterface::CliffAlignToWhite msg;
    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_CliffAlignToWhite(msg);
    SendMessage(msgWrapper);
  }

  void WebotsKeyboardController::ToggleTestBackpackLights()
  {
    static bool backpackLightsOn = false;

    ExternalInterface::SetBackpackLEDs msg;
    for (u32 i=0; i < (u32) LEDId::NUM_BACKPACK_LEDS; ++i)
    {
      msg.onColor[i] = 0;
      msg.offColor[i] = 0;
      msg.onPeriod_ms[i] = 1000;
      msg.offPeriod_ms[i] = 2000;
      msg.transitionOnPeriod_ms[i] = 500;
      msg.transitionOffPeriod_ms[i] = 500;
      msg.offset[i] = 0;
    }

    if(!backpackLightsOn) {
      // Use red channel to control left and right lights
      msg.onColor[(uint32_t)LEDId::LED_BACKPACK_FRONT] = ::Anki::NamedColors::RED;
      msg.onColor[(uint32_t)LEDId::LED_BACKPACK_MIDDLE] = ::Anki::NamedColors::GREEN;
      msg.onColor[(uint32_t)LEDId::LED_BACKPACK_BACK] = ::Anki::NamedColors::BLUE;
    }

    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_SetBackpackLEDs(msg);
    SendMessage(msgWrapper);

    backpackLightsOn = !backpackLightsOn;
  }


  void WebotsKeyboardController::TrackPet()
  {
    using namespace ExternalInterface;
    TrackToPet trackAction(5.f, Vision::UnknownFaceID, Vision::PetType::Unknown);
    SendMessage(MessageGameToEngine(std::move(trackAction)));
  }

  void WebotsKeyboardController::ToggleTrackToObject()
  {
    static bool trackingObject = false;

    trackingObject = !trackingObject;

    if(trackingObject) {
      const bool headOnly = false;

      printf("Telling robot to track %sto the currently observed object %d\n",
             headOnly ? "its head " : "",
             GetLastObservedObject().id);

      SendTrackToObject(GetLastObservedObject().id, headOnly);
    } else {
      // Disable tracking
      SendTrackToObject(std::numeric_limits<u32>::max());
    }
  }

  void WebotsKeyboardController::ToggleTrackToFace()
  {
    static bool trackingFace = false;

    trackingFace = !trackingFace;

    if(trackingFace) {
      const bool headOnly = false;

      printf("Telling robot to track %sto the currently observed face %d\n",
             headOnly ? "its head " : "",
             (u32)GetLastObservedFaceID());

      SendTrackToFace((u32)GetLastObservedFaceID(), headOnly);
    } else {
      // Disable tracking
      SendTrackToFace(std::numeric_limits<u32>::max());
    }

  }

  void WebotsKeyboardController::ExecuteTestPlan()
  {
    SendExecuteTestPlan(pathMotionProfile_);
  }

  void WebotsKeyboardController::ExecuteBehavior()
  {
    std::string behaviorName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "behaviorName", behaviorName)) {
      return;
    }

    // Ensure that behaviorName is a valid BehaviorID
    BehaviorID behaviorId;
    if (!EnumFromString(behaviorName, behaviorId)) {
      PRINT_NAMED_ERROR("WebotsKeyboardController.ExecuteBehavior.InvalidBehaviorID",
                        "'%s' is not a valid behavior ID",
                        behaviorName.c_str());
      return;
    }

    printf("Selecting behavior by NAME: %s\n", behaviorName.c_str());
    if (behaviorId == BehaviorID::LiftLoadTest) {
      SendMessage(ExternalInterface::MessageGameToEngine(ExternalInterface::SetLiftLoadTestAsActivatable()));
    }
    const int numRuns = root_->getField("numBehaviorRuns")->getSFInt32();
    SendMessage(ExternalInterface::MessageGameToEngine(
                                                       ExternalInterface::ExecuteBehaviorByID(behaviorName, numRuns, true)));
  }

  void WebotsKeyboardController::LogCliffSensorData()
  {
    // Send a request to log raw cliff sensor data
    SendLogCliffDataRequest(2000);
  }


  // shift + alt + H = Fake trigger word detected
  // H = Fake cloud intent w/ string in field
  void WebotsKeyboardController::FakeCloudIntent()
  {
    std::string cloudIntent;
    if (!WebotsHelpers::GetFieldAsString(*root_, "intent", cloudIntent)) {
      return;
    }

    printf("sending cloud intent '%s'\n", cloudIntent.c_str());

    SendMessage(ExternalInterface::MessageGameToEngine(ExternalInterface::FakeCloudIntent(cloudIntent)));
  }

  // shift + H = Fake user intent detected
  void WebotsKeyboardController::FakeUserIntent()
  {
    std::string userIntent;
    if (!WebotsHelpers::GetFieldAsString(*root_, "intent", userIntent)) {
      return;
    }

    printf("sending user intent '%s'\n", userIntent.c_str());

    SendMessage(ExternalInterface::MessageGameToEngine(ExternalInterface::FakeUserIntent(userIntent)));
  }

  void WebotsKeyboardController::SetEmotion()
  {
    std::string emotionName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "emotionName", emotionName)) {
      return;
    }

    webots::Field* emotionValField = root_->getField("emotionVal");
    if (emotionValField == nullptr) {
      printf("ERROR: No emotionValField field found in WebotsKeyboardController.proto\n");
      return;
    }

    float emotionVal = emotionValField->getSFFloat();
    EmotionType emotionType = EmotionTypeFromString(emotionName.c_str());

    SendMessage(ExternalInterface::MessageGameToEngine(
                                                       ExternalInterface::MoodMessage(
                                                                                      ExternalInterface::MoodMessageUnion(
                                                                                                                          ExternalInterface::SetEmotion( emotionType, emotionVal )))));

  }

  void WebotsKeyboardController::TriggerEmotionEvent()
  {
    std::string emotionEvent;
    if (!WebotsHelpers::GetFieldAsString(*root_, "emotionEvent", emotionEvent)) {
      return;
    }

    SendMessage(ExternalInterface::MessageGameToEngine(
                  ExternalInterface::MoodMessage(
                    ExternalInterface::MoodMessageUnion(
                      ExternalInterface::TriggerEmotionEvent(
                        emotionEvent )))));
  }


  void WebotsKeyboardController::PickOrPlaceObject()
  {
    bool usePreDockPose = !_shiftKeyPressed;
    bool placeOnGroundAtOffset = _altKeyPressed;

    f32 placementOffsetX_mm = 0;
    if (placeOnGroundAtOffset) {
      placementOffsetX_mm = root_->getField("placeOnGroundOffsetX_mm")->getSFFloat();
    }

    // Exact rotation to use if useExactRotation == true
    const double* rotVals = root_->getField("exactPlacementRotation")->getSFRotation();
    Rotation3d rot(rotVals[3], {static_cast<f32>(rotVals[0]), static_cast<f32>(rotVals[1]), static_cast<f32>(rotVals[2])} );
    printf("Rotation %f\n", rot.GetAngleAroundZaxis().ToFloat());

    if (GetCarryingObjectID() < 0) {
      // Not carrying anything so pick up!
      SendPickupSelectedObject(pathMotionProfile_,
                               usePreDockPose,
                               useApproachAngle,
                               approachAngle_rad);
    } else {
      if (placeOnGroundAtOffset) {
        SendPlaceRelSelectedObject(pathMotionProfile_,
                                   usePreDockPose,
                                   placementOffsetX_mm,
                                   useApproachAngle,
                                   approachAngle_rad);
      } else {
        SendPlaceOnSelectedObject(pathMotionProfile_,
                                  usePreDockPose,
                                  useApproachAngle,
                                  approachAngle_rad);
      }
    }

  }


  void WebotsKeyboardController::MountSelectedCharger()
  {
    bool useCliffSensorCorrection = !_shiftKeyPressed;

    SendMountSelectedCharger(pathMotionProfile_,
                             useCliffSensorCorrection);
  }

  void WebotsKeyboardController::TeleportOntoCharger()
  {
    if( _chargerNode == nullptr ) {
      // look for charger node
      const char* nodeName = "VictorCharger";
      const auto& chargerNodeInfo = WebotsHelpers::GetFirstMatchingSceneTreeNode(GetSupervisor(), nodeName);
      if( chargerNodeInfo.nodePtr == nullptr ) {
        PRINT_NAMED_WARNING("WebotsKeyboardController.TeleportOntoCharger.NoChargerNode",
                            "can't find node '%s'",
                            nodeName);
        return;
      }

      _chargerNode = chargerNodeInfo.nodePtr;
    }

    const Pose3d chargerPose = GetPose3dOfNode(_chargerNode);
    Pose3d targetPose(kTeleportToChargerOffset, chargerPose);
    const bool transformOK = targetPose.GetWithRespectTo(_webotsOrigin, targetPose);
    if( !transformOK ) {
      PRINT_NAMED_WARNING("WebotsKeyboardController.TeleportOntoCharger.PoseChainError",
                          "Cannot get target pose W.R.T. webots origin");
      return;
    }

    SetActualRobotPose(targetPose);
    SendForceDelocalize();
  }


  void WebotsKeyboardController::PopAWheelie()
  {
    bool usePreDockPose = !_shiftKeyPressed;
    SendPopAWheelie(-1,
                    pathMotionProfile_,
                    usePreDockPose,
                    useApproachAngle,
                    approachAngle_rad);
  }

  void WebotsKeyboardController::RollObject()
  {
    bool usePreDockPose = !_shiftKeyPressed;
    bool doDeepRoll = root_->getField("doDeepRoll")->getSFBool();
    SendRollSelectedObject(pathMotionProfile_,
                           doDeepRoll,
                           usePreDockPose,
                           useApproachAngle,
                           approachAngle_rad);
  }


  void WebotsKeyboardController::SetControllerGains()
  {
    if (root_) {

      if(_shiftKeyPressed) {
        f32 steer_k1 = root_->getField("steerK1")->getSFFloat();
        f32 steer_k2 = root_->getField("steerK2")->getSFFloat();
        f32 steerDistOffsetCap = root_->getField("steerDistOffsetCap_mm")->getSFFloat();
        f32 steerAngOffsetCap = root_->getField("steerAngOffsetCap_rad")->getSFFloat();
        printf("New steering gains: k1 %f, k2 %f, distOffsetCap %f, angOffsetCap %f\n",
               steer_k1, steer_k2, steerDistOffsetCap, steerAngOffsetCap);
        SendControllerGains(ControllerChannel::controller_steering, steer_k1, steer_k2, steerDistOffsetCap, steerAngOffsetCap);

        // Point turn gains
        f32 kp = root_->getField("pointTurnKp")->getSFFloat();
        f32 ki = root_->getField("pointTurnKi")->getSFFloat();
        f32 kd = root_->getField("pointTurnKd")->getSFFloat();
        f32 maxErrorSum = root_->getField("pointTurnMaxErrorSum")->getSFFloat();
        printf("New pointTurn gains: kp=%f ki=%f kd=%f maxErrorSum=%f\n", kp, ki, kd, maxErrorSum);
        SendControllerGains(ControllerChannel::controller_pointTurn, kp, ki, kd, maxErrorSum);

      } else {

        // Wheel gains
        f32 kp = root_->getField("wheelKp")->getSFFloat();
        f32 ki = root_->getField("wheelKi")->getSFFloat();
        f32 kd = 0;
        f32 maxErrorSum = root_->getField("wheelMaxErrorSum")->getSFFloat();
        printf("New wheel gains: kp=%f ki=%f kd=%f\n", kp, ki, maxErrorSum);
        SendControllerGains(ControllerChannel::controller_wheel, kp, ki, kd, maxErrorSum);

        // Head and lift gains
        kp = root_->getField("headKp")->getSFFloat();
        ki = root_->getField("headKi")->getSFFloat();
        kd = root_->getField("headKd")->getSFFloat();
        maxErrorSum = root_->getField("headMaxErrorSum")->getSFFloat();
        printf("New head gains: kp=%f ki=%f kd=%f maxErrorSum=%f\n", kp, ki, kd, maxErrorSum);
        SendControllerGains(ControllerChannel::controller_head, kp, ki, kd, maxErrorSum);

        kp = root_->getField("liftKp")->getSFFloat();
        ki = root_->getField("liftKi")->getSFFloat();
        kd = root_->getField("liftKd")->getSFFloat();
        maxErrorSum = root_->getField("liftMaxErrorSum")->getSFFloat();
        printf("New lift gains: kp=%f ki=%f kd=%f maxErrorSum=%f\n", kp, ki, kd, maxErrorSum);
        SendControllerGains(ControllerChannel::controller_lift, kp, ki, kd, maxErrorSum);
      }
    } else {
      printf("No WebotsKeyboardController was found in world\n");
    }
  }

  void WebotsKeyboardController::ToggleVisionWhileMoving()
  {
    static bool visionWhileMovingEnabled = false;
    visionWhileMovingEnabled = !visionWhileMovingEnabled;
    printf("%s vision while moving.\n", (visionWhileMovingEnabled ? "Enabling" : "Disabling"));
    ExternalInterface::VisionWhileMoving msg;
    msg.enable = visionWhileMovingEnabled;
    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_VisionWhileMoving(msg);
    SendMessage(msgWrapper);
  }

  void WebotsKeyboardController::SetRobotVolume()
  {
    const f32 robotVolume = root_->getField("robotVolume")->getSFFloat();
    printf("Set robot volume to %f\n", robotVolume);
    SendSetRobotVolume(robotVolume);
  }


  void WebotsKeyboardController::SetActiveObjectLights()
  {
    if(_shiftKeyPressed && _altKeyPressed)
    {
      ExternalInterface::SetAllActiveObjectLEDs msg;
      static int jsonMsgCtr = 0;
      Json::Value jsonMsg;
      Json::Reader reader;
      std::string jsonFilename("../webotsCtrlGameEngine/SetBlockLights_" + std::to_string(jsonMsgCtr++) + ".json");
      std::ifstream jsonFile(jsonFilename);

      if(jsonFile.fail()) {
        jsonMsgCtr = 0;
        jsonFilename = "../webotsCtrlGameEngine/SetBlockLights_" + std::to_string(jsonMsgCtr++) + ".json";
        jsonFile.open(jsonFilename);
      }

      printf("Sending message from: %s\n", jsonFilename.c_str());

      reader.parse(jsonFile, jsonMsg);
      jsonFile.close();
      //ExternalInterface::SetActiveObjectLEDs msg(jsonMsg);
      msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
      msg.objectID = jsonMsg["objectID"].asUInt();
      for (u32 iLED = 0; iLED < 4; ++iLED) {
        msg.onColor[iLED]  = jsonMsg["onColor"][iLED].asUInt();
        msg.offColor[iLED]  = jsonMsg["offColor"][iLED].asUInt();
        msg.onPeriod_ms[iLED]  = jsonMsg["onPeriod_ms"][iLED].asUInt();
        msg.offPeriod_ms[iLED]  = jsonMsg["offPeriod_ms"][iLED].asUInt();
        msg.transitionOnPeriod_ms[iLED]  = jsonMsg["transitionOnPeriod_ms"][iLED].asUInt();
        msg.transitionOffPeriod_ms[iLED]  = jsonMsg["transitionOffPeriod_ms"][iLED].asUInt();
      }

      ExternalInterface::MessageGameToEngine msgWrapper;
      msgWrapper.Set_SetAllActiveObjectLEDs(msg);
      SendMessage(msgWrapper);
    }
    else if(GetLastObservedObject().id >= 0 && GetLastObservedObject().isActive)
    {
      // Proof of concept: cycle colors
      const s32 NUM_COLORS = 4;
      const ColorRGBA colorList[NUM_COLORS] = {
        ::Anki::NamedColors::RED, ::Anki::NamedColors::GREEN, ::Anki::NamedColors::BLUE,
        ::Anki::NamedColors::BLACK
      };

      static s32 colorIndex = 0;

      ExternalInterface::SetActiveObjectLEDs msg;
      msg.objectID = GetLastObservedObject().id;
      msg.onPeriod_ms = 250;
      msg.offPeriod_ms = 250;
      msg.transitionOnPeriod_ms = 500;
      msg.transitionOffPeriod_ms = 100;
      msg.turnOffUnspecifiedLEDs = 1;
      msg.offset = 0;
      msg.rotate = false;

      if(_shiftKeyPressed) {
        printf("Updating active block edge\n");
        msg.onColor = ::Anki::NamedColors::RED;
        msg.offColor = ::Anki::NamedColors::BLACK;
        msg.whichLEDs = WhichCubeLEDs::FRONT;
        msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_BY_SIDE;
        msg.relativeToX = GetRobotPose().GetTranslation().x();
        msg.relativeToY = GetRobotPose().GetTranslation().y();

      } else if( _altKeyPressed) {
        static s32 edgeIndex = 0;

        printf("Turning edge %d new color %d (%x)\n",
               edgeIndex, colorIndex, u32(colorList[colorIndex]));

        msg.whichLEDs = (WhichCubeLEDs)(1 << edgeIndex);
        msg.onColor = colorList[colorIndex];
        msg.offColor = 0;
        msg.turnOffUnspecifiedLEDs = 0;
        msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_BY_SIDE;
        msg.relativeToX = GetRobotPose().GetTranslation().x();
        msg.relativeToY = GetRobotPose().GetTranslation().y();

        ++edgeIndex;
        if(edgeIndex == 4) {
          edgeIndex = 0;
          ++colorIndex;
        }

      } else {
        printf("Cycling active block %d color from (%d,%d,%d) to (%d,%d,%d)\n",
               msg.objectID,
               colorList[colorIndex==0 ? NUM_COLORS-1 : colorIndex-1].r(),
               colorList[colorIndex==0 ? NUM_COLORS-1 : colorIndex-1].g(),
               colorList[colorIndex==0 ? NUM_COLORS-1 : colorIndex-1].b(),
               colorList[colorIndex].r(),
               colorList[colorIndex].g(),
               colorList[colorIndex].b());
        msg.onColor = colorList[colorIndex++];
        msg.offColor = ::Anki::NamedColors::BLACK;
        msg.whichLEDs = WhichCubeLEDs::FRONT;
        msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
        msg.turnOffUnspecifiedLEDs = 1;


        /*
         static bool white = false;
         white = !white;
         if (white) {
         ExternalInterface::SetAllActiveObjectLEDs m;
         m.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
         m.objectID = GetLastObservedObject().id;
         for(s32 iLED = 0; iLED<4; ++iLED) {
         m.onColor[iLED]  = ::Anki::NamedColors::WHITE;
         m.offColor[iLED]  = ::Anki::NamedColors::BLACK;
         m.onPeriod_ms[iLED] = 250;
         m.offPeriod_ms[iLED] = 250;
         m.transitionOnPeriod_ms[iLED] = 500;
         m.transitionOffPeriod_ms[iLED] = 100;
         }
         ExternalInterface::MessageGameToEngine msgWrapper;
         msgWrapper.Set_SetAllActiveObjectLEDs(m);
         SendMessage(msgWrapper);
         break;
         } else {
         msg.onColor = ::Anki::NamedColors::RED;
         msg.offColor = ::Anki::NamedColors::BLACK;
         msg.whichLEDs = WhichCubeLEDs::FRONT;
         msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
         msg.turnOffUnspecifiedLEDs = 0;
         ExternalInterface::MessageGameToEngine msgWrapper;
         msgWrapper.Set_SetActiveObjectLEDs(msg);
         SendMessage(msgWrapper);

         msg.onColor = ::Anki::NamedColors::GREEN;
         msg.offColor = ::Anki::NamedColors::BLACK;
         msg.whichLEDs = WhichCubeLEDs::RIGHT;
         msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
         msg.turnOffUnspecifiedLEDs = 0;
         msgWrapper.Set_SetActiveObjectLEDs(msg);
         SendMessage(msgWrapper);

         msg.onColor = ::Anki::NamedColors::BLUE;
         msg.offColor = ::Anki::NamedColors::BLACK;
         msg.whichLEDs = WhichCubeLEDs::BACK;
         msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
         msg.turnOffUnspecifiedLEDs = 0;
         msgWrapper.Set_SetActiveObjectLEDs(msg);
         SendMessage(msgWrapper);

         msg.onColor = ::Anki::NamedColors::YELLOW;
         msg.offColor = ::Anki::NamedColors::BLACK;
         msg.whichLEDs = WhichCubeLEDs::LEFT;
         msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;
         msg.turnOffUnspecifiedLEDs = 0;
         msgWrapper.Set_SetActiveObjectLEDs(msg);
         SendMessage(msgWrapper);
         }
         */

      }

      if(colorIndex == NUM_COLORS) {
        colorIndex = 0;
      }

      ExternalInterface::MessageGameToEngine msgWrapper;
      msgWrapper.Set_SetActiveObjectLEDs(msg);
      SendMessage(msgWrapper);
    }
  }

  void WebotsKeyboardController::AlignWithObject()
  {
    f32 distToMarker = root_->getField("alignWithObjectDistToMarker_mm")->getSFFloat();
    SendAlignWithObject(-1, // tell game to use blockworld's "selected" object
                        distToMarker,
                        pathMotionProfile_,
                        true,
                        useApproachAngle,
                        approachAngle_rad);
  }

  void WebotsKeyboardController::TurnTowardsObject()
  {
    ExternalInterface::TurnTowardsObject msg;
    msg.objectID = std::numeric_limits<u32>::max(); // HACK to tell game to use blockworld's "selected" object
    msg.panTolerance_rad = DEG_TO_RAD(5);
    msg.maxTurnAngle_rad = DEG_TO_RAD(90);
    msg.headTrackWhenDone = 0;

    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_TurnTowardsObject(msg);
    SendMessage(msgWrapper);
  }

  void WebotsKeyboardController::GotoObject()
  {
    SendGotoObject(-1, // tell game to use blockworld's "selected" object
                   sqrtf(2.f)*44.f,
                   pathMotionProfile_);
  }

  void WebotsKeyboardController::RequestIMUData()
  {
    SendIMURequest(2000);
  }


  void WebotsKeyboardController::AssociateNameWithCurrentFace()
  {
    std::string userName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "userName", userName)) {
      return;
    }

    webots::Field* enrollToIDField = root_->getField("enrollToID");
    if(nullptr == enrollToIDField) {
      printf("No 'enrollToID' field!");
      return;
    }

    const s32 enrollToID = enrollToIDField->getSFInt32();

    //                      printf("Assigning name '%s' to ID %d\n", userName.c_str(), GetLastObservedFaceID());
    //                      ExternalInterface::AssignNameToFace assignNameToFace;
    //                      assignNameToFace.faceID = GetLastObservedFaceID();
    //                      assignNameToFace.name   = userName;
    //                      SendMessage(ExternalInterface::MessageGameToEngine(std::move(assignNameToFace)));

    webots::Field* saveFaceField = root_->getField("saveFaceToRobot");
    if( saveFaceField == nullptr ) {
      PRINT_NAMED_ERROR("WebotsKeyboardController.MissingField",
                        "missing saveFaceToRobot field");
      return;
    }

    using namespace ExternalInterface;

    // Set face enrollment settings
    bool saveFaceToRobot = saveFaceField->getSFBool();

    const bool sayName = true;
    const bool useMusic = false;
    const s32 observedID = Vision::UnknownFaceID; // GetLastObservedFaceID();
    printf("Enrolling face ID %d with name '%s'\n", observedID, userName.c_str());
    SetFaceToEnroll setFaceToEnroll(userName, observedID, enrollToID, saveFaceToRobot, sayName, useMusic);
    SendMessage(MessageGameToEngine(std::move(setFaceToEnroll)));

    // todo: currently we send both the SetFaceToEnroll and the cloud intent for meet victor. This
    // will change, but since we don't know what SetFaceToEnroll will be replaced by yet, both messages
    // are being send for now. Eventually there should be one "meet_victor" message and one "I'm changing
    // the name, but don't restart meet victor"
    const std::string json("{\"intent\": \"intent_names_username_extend\", "
                           "\"parameters\": \"{\\\"username\\\": \\\"" + userName + "\\\"}\" }");
    SendMessage(ExternalInterface::MessageGameToEngine(ExternalInterface::FakeCloudIntent(json)));
  }

  void WebotsKeyboardController::TurnTowardsFace()
  {
    int faceID = root_->getField("faceIDToTurnTowards")->getSFInt32();
    if( faceID == 0 ) {
      // turn towards last face
      printf("Turning to last face\n");
      ExternalInterface::TurnTowardsLastFacePose turnTowardsPose; // construct w/ defaults for speed
      turnTowardsPose.panTolerance_rad = DEG_TO_RAD(10);
      turnTowardsPose.maxTurnAngle_rad = M_PI;
      turnTowardsPose.sayName = true;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(turnTowardsPose)));
    }
    else {
      printf("Turning towards face id %d\n", faceID);
      ExternalInterface::TurnTowardsFace turnTowardsFace;
      turnTowardsFace.faceID = faceID;
      turnTowardsFace.panTolerance_rad = DEG_TO_RAD(10);
      turnTowardsFace.maxTurnAngle_rad = M_PI;
      SendMessage(ExternalInterface::MessageGameToEngine(std::move(turnTowardsFace)));
    }
  }

  void WebotsKeyboardController::EraseLastObservedFace()
  {
    using namespace ExternalInterface;
    SendMessage(MessageGameToEngine(EraseEnrolledFaceByID(GetLastObservedFaceID())));
  }

  void WebotsKeyboardController::ToggleFaceDetection()
  {
    static bool isFaceDetectionEnabled = true;
    isFaceDetectionEnabled = !isFaceDetectionEnabled;
    SendEnableVisionMode(VisionMode::Faces, isFaceDetectionEnabled);
  }

  void WebotsKeyboardController::FlipSelectedBlock()
  {
    ExternalInterface::FlipBlock m;
    m.objectID = -1;
    m.motionProf = pathMotionProfile_;
    ExternalInterface::MessageGameToEngine message;
    message.Set_FlipBlock(m);
    SendMessage(message);
  }


  #define GET_POINT_TURN_PARAMS() \
  f32 pointTurnAngle = std::fabs(root_->getField("pointTurnAngle_deg")->getSFFloat()); \
  f32 pointTurnSpeed = std::fabs(root_->getField("pointTurnSpeed_degPerSec")->getSFFloat()); \
  f32 pointTurnAccel = std::fabs(root_->getField("pointTurnAccel_degPerSec2")->getSFFloat()); \

  void WebotsKeyboardController::TurnInPlaceCCW()
  {
    GET_POINT_TURN_PARAMS();
    if(_altKeyPressed) {
      SendTurnInPlaceAtSpeed(DEG_TO_RAD(pointTurnSpeed), DEG_TO_RAD(pointTurnAccel));
    }
    else {
      SendTurnInPlace(DEG_TO_RAD(pointTurnAngle), DEG_TO_RAD(pointTurnSpeed), DEG_TO_RAD(pointTurnAccel));
    }
  }

  void WebotsKeyboardController::TurnInPlaceCW()
  {
    GET_POINT_TURN_PARAMS();
    if(_altKeyPressed) {
      SendTurnInPlaceAtSpeed(DEG_TO_RAD(-pointTurnSpeed), DEG_TO_RAD(pointTurnAccel));
    } else {
      SendTurnInPlace(DEG_TO_RAD(-pointTurnAngle), DEG_TO_RAD(-pointTurnSpeed), DEG_TO_RAD(pointTurnAccel));
    }
  }

  void WebotsKeyboardController::ExecutePlaypenTest()
  {
    SendMessage(ExternalInterface::MessageGameToEngine(
                                                       ExternalInterface::ExecuteBehaviorByID("PlaypenTest", -1, false)));
  }

  void WebotsKeyboardController::SetFaceDisplayHue()
  {
    using namespace ExternalInterface;

    // Set Hue using "FaceHue" parameter of proto
    webots::Field* hueField = root_->getField("faceHue");
    if(hueField == nullptr) {
      printf("ERROR: No faceHue field found in WebotsKeyboardController.proto\n");
      return;
    }
    SendMessage(MessageGameToEngine(SetFaceHue(hueField->getSFFloat())));
  }

  void WebotsKeyboardController::SendRandomProceduralFace()
  {
    // Send a random procedural face
    using Param = ProceduralEyeParameter;
    ExternalInterface::DisplayProceduralFace msg;
    ProceduralFaceParameters& faceParams = msg.faceParams;
    static_assert( std::tuple_size<decltype(faceParams.leftEye)>::value == (size_t)Param::NumParameters,
                  "LeftEye parameter array is the wrong length");
    static_assert( std::tuple_size<decltype(faceParams.rightEye)>::value == (size_t)Param::NumParameters,
                  "RightEye parameter array is the wrong length");

    Util::RandomGenerator rng;

    faceParams.leftEye[static_cast<s32>(Param::UpperInnerRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::UpperInnerRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::LowerInnerRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::LowerInnerRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::UpperOuterRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::UpperOuterRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::LowerOuterRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::LowerOuterRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.leftEye[static_cast<s32>(Param::EyeCenterX)]    = rng.RandIntInRange(-20,20);
    faceParams.leftEye[static_cast<s32>(Param::EyeCenterY)]    = rng.RandIntInRange(-20,20);
    faceParams.leftEye[static_cast<s32>(Param::EyeScaleX)]     = rng.RandDblInRange(0.8f, 1.2f);
    faceParams.leftEye[static_cast<s32>(Param::EyeScaleY)]     = rng.RandDblInRange(0.8f, 1.2f);
    faceParams.leftEye[static_cast<s32>(Param::EyeAngle)]      = 0;//rng.RandIntInRange(-10,10);
    faceParams.leftEye[static_cast<s32>(Param::LowerLidY)]     = rng.RandDblInRange(0., .25);
    faceParams.leftEye[static_cast<s32>(Param::LowerLidAngle)] = rng.RandIntInRange(-20, 20);
    faceParams.leftEye[static_cast<s32>(Param::LowerLidBend)]  = 0;//rng.RandDblInRange(0, 0.2);
    faceParams.leftEye[static_cast<s32>(Param::UpperLidY)]     = rng.RandDblInRange(0., .25);
    faceParams.leftEye[static_cast<s32>(Param::UpperLidAngle)] = rng.RandIntInRange(-20, 20);
    faceParams.leftEye[static_cast<s32>(Param::UpperLidBend)]  = 0;//rng.RandDblInRange(0, 0.2);
    faceParams.leftEye[static_cast<s32>(Param::Lightness)]     = rng.RandDblInRange(0.5f, 1.f);
    faceParams.leftEye[static_cast<s32>(Param::Saturation)]    = rng.RandDblInRange(0.5f, 1.f);
    faceParams.leftEye[static_cast<s32>(Param::GlowSize)]      = rng.RandDblInRange(0.f, 1.f);
    faceParams.leftEye[static_cast<s32>(Param::HotSpotCenterX)]= rng.RandDblInRange(-0.8f, 0.8f);
    faceParams.leftEye[static_cast<s32>(Param::HotSpotCenterY)]= rng.RandDblInRange(-0.8f, 0.8f);

    faceParams.rightEye[static_cast<s32>(Param::UpperInnerRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::UpperInnerRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::LowerInnerRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::LowerInnerRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::UpperOuterRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::UpperOuterRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::LowerOuterRadiusX)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::LowerOuterRadiusY)]   = rng.RandDblInRange(0., 1.);
    faceParams.rightEye[static_cast<s32>(Param::EyeCenterX)]    = rng.RandIntInRange(-20,20);
    faceParams.rightEye[static_cast<s32>(Param::EyeCenterY)]    = rng.RandIntInRange(-20,20);
    faceParams.rightEye[static_cast<s32>(Param::EyeScaleX)]     = rng.RandDblInRange(0.8, 1.2);
    faceParams.rightEye[static_cast<s32>(Param::EyeScaleY)]     = rng.RandDblInRange(0.8, 1.2);
    faceParams.rightEye[static_cast<s32>(Param::EyeAngle)]      = 0;//rng.RandIntInRange(-15,15);
    faceParams.rightEye[static_cast<s32>(Param::LowerLidY)]     = rng.RandDblInRange(0., .25);
    faceParams.rightEye[static_cast<s32>(Param::LowerLidAngle)] = rng.RandIntInRange(-20, 20);
    faceParams.rightEye[static_cast<s32>(Param::LowerLidBend)]  = rng.RandDblInRange(0., 0.2);
    faceParams.rightEye[static_cast<s32>(Param::UpperLidY)]     = rng.RandDblInRange(0., .25);
    faceParams.rightEye[static_cast<s32>(Param::UpperLidAngle)] = rng.RandIntInRange(-20, 20);
    faceParams.rightEye[static_cast<s32>(Param::UpperLidBend)]  = rng.RandDblInRange(0, 0.2);
    faceParams.rightEye[static_cast<s32>(Param::Lightness)]     = rng.RandDblInRange(0.5f, 1.f);
    faceParams.rightEye[static_cast<s32>(Param::Saturation)]    = rng.RandDblInRange(0.5f, 1.f);
    faceParams.rightEye[static_cast<s32>(Param::GlowSize)]      = rng.RandDblInRange(0.f, 0.75f);
    faceParams.rightEye[static_cast<s32>(Param::HotSpotCenterX)]= rng.RandDblInRange(-0.8f, 0.8f);
    faceParams.rightEye[static_cast<s32>(Param::HotSpotCenterY)]= rng.RandDblInRange(-0.8f, 0.8f);

    faceParams.faceAngle_deg = 0; //rng.RandIntInRange(-10, 10);
    faceParams.faceScaleX = 1.f;//rng.RandDblInRange(0.9, 1.1);
    faceParams.faceScaleY = 1.f;//rng.RandDblInRange(0.9, 1.1);
    faceParams.faceCenX  = 0; //rng.RandIntInRange(-5, 5);
    faceParams.faceCenY  = 0; //rng.RandIntInRange(-5, 5);

    SendMessage(ExternalInterface::MessageGameToEngine(std::move(msg)));
  }

  void WebotsKeyboardController::PlayAnimation() {
    // Send whatever animation is specified in the animationToSendName field
    std::string animToSendName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "animationToSendName", animToSendName)) {
      return;
    }

    webots::Field* animNumLoopsField = root_->getField("animationNumLoops");
    u32 animNumLoops = 1;
    if (animNumLoopsField && (animNumLoopsField->getSFInt32() > 0)) {
      animNumLoops = (u32) animNumLoopsField->getSFInt32();
    }

    SendAnimation(animToSendName.c_str(), animNumLoops, true);
  }

  void WebotsKeyboardController::PlayAnimationTrigger()
  {
    // Send whatever animation trigger is specified in the animationToSendName field
    std::string animTriggerName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "animationToSendName", animTriggerName)) {
      return;
    }

    webots::Field* animNumLoopsField = root_->getField("animationNumLoops");
    u32 animNumLoops = 1;
    if (animNumLoopsField && (animNumLoopsField->getSFInt32() > 0)) {
      animNumLoops = (u32) animNumLoopsField->getSFInt32();
    }

    SendAnimationTrigger(animTriggerName.c_str(), animNumLoops, true);
  }

  void WebotsKeyboardController::PlayAnimationGroup()
  {
    // Send whatever animation group is specified in the animationToSendName field
    std::string animGroupName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "animationToSendName", animGroupName)) {
      return;
    }

    webots::Field* animNumLoopsField = root_->getField("animationNumLoops");
    u32 animNumLoops = 1;
    if (animNumLoopsField && (animNumLoopsField->getSFInt32() > 0)) {
      animNumLoops = (u32) animNumLoopsField->getSFInt32();
    }

    SendAnimationGroup(animGroupName.c_str(), animNumLoops, true);
  }

  void WebotsKeyboardController::RunDebugConsoleFunc()
  {
    // call console function
    std::string funcName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "consoleVarName", funcName)) {
      return;
    }

    std::string funcArgs;
    if (!WebotsHelpers::GetFieldAsString(*root_, "consoleVarValue", funcArgs, false)) {
      return;
    }

    printf("Trying to call console func: %s(%s)\n",
           funcName.c_str(),
           funcArgs.c_str());

    using namespace ExternalInterface;
    if (_altKeyPressed) {
      // Alt: Send to Anim process
      SendMessage(MessageGameToEngine(RunAnimDebugConsoleFuncMessage(funcName, funcArgs)));
    }
    else {
      // Normal: Send to Engine process
      SendMessage(MessageGameToEngine(RunDebugConsoleFuncMessage(funcName, funcArgs)));
    }
  }


  void WebotsKeyboardController::SetDebugConsoleVar()
  {
    // Set console variable
    std::string varName;
    if (!WebotsHelpers::GetFieldAsString(*root_, "consoleVarName", varName)) {
      return;
    }

    std::string tryValue;
    if (!WebotsHelpers::GetFieldAsString(*root_, "consoleVarValue", tryValue)) {
      return;
    }

    printf("Trying to set console var '%s' to '%s'\n",
           varName.c_str(), tryValue.c_str());

    using namespace ExternalInterface;
    if(_altKeyPressed)
    {
      // Alt: Send to Anim process
      SendMessage(MessageGameToEngine(SetAnimDebugConsoleVarMessage(varName, tryValue)));
    }
    else
    {
      // Normal: Send to Engine process
      SendMessage(MessageGameToEngine(SetDebugConsoleVarMessage(varName, tryValue)));
    }
  }


  void WebotsKeyboardController::SetRollActionParams()
  {
    SendRollActionParams(root_->getField("rollLiftHeight_mm")->getSFFloat(),
                         root_->getField("rollDriveSpeed_mmps")->getSFFloat(),
                         root_->getField("rollDriveAccel_mmps2")->getSFFloat(),
                         root_->getField("rollDriveDuration_ms")->getSFInt32(),
                         root_->getField("rollBackupDist_mm")->getSFFloat());
  }

  void WebotsKeyboardController::PlayCubeAnimation()
  {
    if(_altKeyPressed) {
      // Send whatever cube animation trigger is specified in the animationToSendName field
      std::string cubeAnimTriggerStr;
      if (!WebotsHelpers::GetFieldAsString(*root_, "animationToSendName", cubeAnimTriggerStr)) {
        return;
      }

      CubeAnimationTrigger cubeAnimTrigger;
      if (!EnumFromString(cubeAnimTriggerStr, cubeAnimTrigger)) {
        LOG_ERROR("WebotsKeyboardController.PlayCubeAnimation.InvalidCubeAnimationTrigger",
                  "ERROR: %s is not a valid CubeAnimationTrigger name",
                  cubeAnimTriggerStr.c_str());
        return;
      }

      SendCubeAnimation(-1, cubeAnimTrigger);
    } else {
      SendCubeAnimation(-1, CubeAnimationTrigger::Flash);
    }
  }

  void WebotsKeyboardController::TogglePowerMode()
  {
    static bool enableCalmPower = true;
    LOG_INFO("WebotsKeyboardController.TogglePowerMode", "Calm: %d", enableCalmPower);
    using namespace ExternalInterface;
    SendMessage(MessageGameToEngine(RunDebugConsoleFuncMessage("EnableCalmPowerMode",
                                                               enableCalmPower ? "true" : "false")));
    enableCalmPower = !enableCalmPower;
  }

  void WebotsKeyboardController::SetCameraSettings()
  {
    ExternalInterface::SetCameraSettings settings;
    settings.exposure_ms = root_->getField("exposure_ms")->getSFFloat();
    settings.gain = root_->getField("gain")->getSFFloat();
    settings.enableAutoExposure = root_->getField("enableAutoExposure")->getSFBool();
    ExternalInterface::MessageGameToEngine message;
    message.Set_SetCameraSettings(settings);
    SendMessage(message);
  }

  void WebotsKeyboardController::SayText()
  {
    ExternalInterface::SayText sayTextMsg;
    if (!WebotsHelpers::GetFieldAsString(*root_, "sayString", sayTextMsg.text)) {
      return;
    }

    // TODO: Add ability to set action style, voice style, duration scalar and pitch from KB controller
    using AudioTtsProcessingStyle = Anki::AudioMetaData::SwitchState::Robot_Vic_External_Processing;
    sayTextMsg.voiceStyle = (_altKeyPressed ?
                             AudioTtsProcessingStyle::Default_Processed :
                             AudioTtsProcessingStyle::Unprocessed);
    sayTextMsg.durationScalar = 1.f;
    sayTextMsg.playEvent = AnimationTrigger::Count;

    printf("Saying '%s' in voice style '%s' w/ duration scalar %f\n",
           sayTextMsg.text.c_str(),
           EnumToString(sayTextMsg.voiceStyle),
           sayTextMsg.durationScalar);
    SendMessage(ExternalInterface::MessageGameToEngine(std::move(sayTextMsg)));
  }

  void WebotsKeyboardController::TurnTowardsImagePoint()
  {
    if(_lastObservedImageCentroid.point.AllGTE(0.f))
    {
      ExternalInterface::TurnTowardsImagePoint msg;
      msg.x = _lastObservedImageCentroid.point.x();
      msg.y = _lastObservedImageCentroid.point.y();
      msg.timestamp = (TimeStamp_t)_lastObservedImageCentroid.timestamp;

      SendMessage(ExternalInterface::MessageGameToEngine(std::move(msg)));
    }
  }

  void WebotsKeyboardController::QuitKeyboardController()
  {
    _shouldQuit = true;
  }

  void WebotsKeyboardController::ToggleLiftPower()
  {
    static bool liftPowerEnable = false;
    SendEnableLiftPower(liftPowerEnable);
    liftPowerEnable = !liftPowerEnable;
  }

  f32 WebotsKeyboardController::GetLiftSpeed_radps()
  {
    f32 liftSpeed = DEG_TO_RAD(root_->getField("liftSpeedDegPerSec")->getSFFloat());
    if (_shiftKeyPressed) {
      liftSpeed *= 0.5;
    }
    return liftSpeed;
  }

  f32 WebotsKeyboardController::GetLiftAccel_radps2()
  {
    return DEG_TO_RAD(root_->getField("liftAccelDegPerSec2")->getSFFloat());
  }

  f32 WebotsKeyboardController::GetLiftDuration_sec()
  {
    return root_->getField("liftDurationSec")->getSFFloat();
  }

  f32 WebotsKeyboardController::GetHeadSpeed_radps()
  {
    f32 headSpeed = DEG_TO_RAD(root_->getField("headSpeedDegPerSec")->getSFFloat());
    if (_shiftKeyPressed) {
      headSpeed *= 0.5;
    }
    return headSpeed;
  }

  f32 WebotsKeyboardController::GetHeadAccel_radps2()
  {
    return DEG_TO_RAD(root_->getField("headAccelDegPerSec2")->getSFFloat());
  }

  f32 WebotsKeyboardController::GetHeadDuration_sec()
  {
    return root_->getField("headDurationSec")->getSFFloat();
  }

  void WebotsKeyboardController::MoveLiftToLowDock()
  {
    SendMoveLiftToHeight(LIFT_HEIGHT_LOWDOCK, GetLiftSpeed_radps(), GetLiftAccel_radps2(), GetLiftDuration_sec());
  }

  void WebotsKeyboardController::MoveLiftToHighDock()
  {
    SendMoveLiftToHeight(LIFT_HEIGHT_HIGHDOCK, GetLiftSpeed_radps(), GetLiftAccel_radps2(), GetLiftDuration_sec());
  }

  void WebotsKeyboardController::MoveLiftToCarryHeight()
  {
    SendMoveLiftToHeight(LIFT_HEIGHT_CARRY, GetLiftSpeed_radps(), GetLiftAccel_radps2(), GetLiftDuration_sec());
  }

  void WebotsKeyboardController::MoveLiftToAngle()
  {
    f32 targetAngle_rad = DEG_TO_RAD(root_->getField("liftTargetAngleDeg")->getSFFloat());
    SendMoveLiftToAngle(targetAngle_rad, GetLiftSpeed_radps(), GetLiftAccel_radps2(), GetLiftDuration_sec());
  }

  void WebotsKeyboardController::MoveHeadToLowLimit()
  {
    SendMoveHeadToAngle(MIN_HEAD_ANGLE, GetHeadSpeed_radps(), GetHeadAccel_radps2(), GetHeadDuration_sec());
  }

  void WebotsKeyboardController::MoveHeadToHorizontal()
  {
    SendMoveHeadToAngle(0, GetHeadSpeed_radps(), GetHeadAccel_radps2(), GetHeadDuration_sec());
  }

  void WebotsKeyboardController::MoveHeadToHighLimit()
  {
    SendMoveHeadToAngle(MAX_HEAD_ANGLE, GetHeadSpeed_radps(), GetHeadAccel_radps2(), GetHeadDuration_sec());
  }

  void WebotsKeyboardController::MoveHeadUp()
  {
    _commandedHeadSpeed += GetHeadSpeed_radps();
    _movingHead = true;
  }

  void WebotsKeyboardController::MoveHeadDown()
  {
    _commandedHeadSpeed -= GetHeadSpeed_radps();
    _movingHead = true;
  }

  void WebotsKeyboardController::MoveLiftUp()
  {
    _commandedLiftSpeed += GetLiftSpeed_radps();
    _movingLift = true;
  }

  void WebotsKeyboardController::MoveLiftDown()
  {
    _commandedLiftSpeed -= GetLiftSpeed_radps();
    _movingLift = true;
  }

  void WebotsKeyboardController::DriveForward()
  {
    ++_throttleDir;
  }

  void WebotsKeyboardController::DriveBackward()
  {
    --_throttleDir;
  }

  void WebotsKeyboardController::DriveLeft()
  {
    --_steeringDir;
  }

  void WebotsKeyboardController::DriveRight()
  {
    ++_steeringDir;
  }

  // Check for test mode (alt + key)
  void WebotsKeyboardController::ExecuteRobotTestMode()
  {
    if (_altKeyPressed) {
      if (_currKey >= '0' && _currKey <= '9') {
        if (_shiftKeyPressed) {
          // Hold shift down too to add 10 to the pressed key
          _currKey += 10;
        }

        TestMode m = TestMode(_currKey - '0');

        // Set parameters for special test cases
        s32 p1 = 0, p2 = 0, p3 = 0;
        switch(m) {
          case TestMode::TM_DIRECT_DRIVE:
            // p1: flags (See DriveTestFlags)
            // p2: wheelPowerStepPercent (only applies if DTF_ENABLE_DIRECT_HAL_TEST is set)
            // p3: wheelSpeed_mmps (only applies if DTF_ENABLE_DIRECT_HAL_TEST is not set)
            //p1 = DTF_ENABLE_DIRECT_HAL_TEST | DTF_ENABLE_CYCLE_SPEEDS_TEST;
            //p2 = 5;
            //p3 = 20;
            p1 = root_->getField("driveTest_flags")->getSFInt32();
            p2 = 10;
            p3 = root_->getField("driveTest_wheel_power")->getSFInt32();
            break;
          case TestMode::TM_LIFT:
            p1 = root_->getField("liftTest_flags")->getSFInt32();
            p2 = root_->getField("liftTest_nodCycleTimeMS")->getSFInt32();  // Nodding cycle time in ms (if LiftTF_NODDING flag is set)
            p3 = root_->getField("liftTest_powerPercent")->getSFInt32();    // Power to run motor at. If 0, cycle through increasing power. Only used during LiftTF_TEST_POWER.
            break;
          case TestMode::TM_HEAD:
            p1 = root_->getField("headTest_flags")->getSFInt32();
            p2 = root_->getField("headTest_nodCycleTimeMS")->getSFInt32();  // Nodding cycle time in ms (if HTF_NODDING flag is set)
            p3 = root_->getField("headTest_powerPercent")->getSFInt32();    // Power to run motor at. If 0, cycle through increasing power. Only used during HTF_TEST_POWER.
            break;
          case TestMode::TM_PLACE_BLOCK_ON_GROUND:
            p1 = 100;  // x_offset_mm
            p2 = -10;  // y_offset_mm
            p3 = 0;    // angle_offset_degrees
            break;
          case TestMode::TM_LIGHTS:
            // p1: flags (See LightTestFlags)
            // p2: The LED channel to activate (applies if LTF_CYCLE_ALL not enabled)
            // p3: The color to set it to (applies if LTF_CYCLE_ALL not enabled)
            p1 = (s32)LightTestFlags::LTF_CYCLE_ALL;
            p2 = (s32)LEDId::LED_BACKPACK_FRONT;
            p3 = (s32)LEDColor::LED_GREEN;
            break;
          case TestMode::TM_STOP_TEST:
            p1 = 100;  // slow speed (mmps)
            p2 = 200;  // fast speed (mmps)
            p3 = 1000; // period (ms)
            break;
          default:
            break;
        }

        printf("Sending test mode %s\n", TestModeToString(m));
        SendStartTestMode(m,p1,p2,p3);
      }
    }
  }

  void WebotsKeyboardController::PressBackButton()
  {
    _pressBackpackButton = true;
  }

  void WebotsKeyboardController::TouchBackSensor()
  {
    _touchBackpackTouchSensor = true;
  }

  void WebotsKeyboardController::CycleConnectionFlowState()
  {
    static u8 status = 0;

    SwitchboardInterface::SetConnectionStatus s;
    s.status = static_cast<SwitchboardInterface::ConnectionStatus>(status);

    status++;
    if(status >= static_cast<u8>(SwitchboardInterface::ConnectionStatus::COUNT))
    {
      status = 0;
    }

    ExternalInterface::MessageGameToEngine message;
    message.Set_SetConnectionStatus(s);
    SendMessage(message);
  }

  // ===== End of key press functions ====

  // Register key press and modifier to a function
  #define REGISTER_KEY_FCN(key, modifier, fcn, help_msg) \
  if (!RegisterKeyFcn(key, modifier, std::bind(&WebotsKeyboardController::fcn, this), help_msg)) { \
    PRINT_NAMED_ERROR("WebotsKeyboardController.RegisterKeyFcn.DuplicateRegistration", "Key: '%c' (0x%x), Modifier: 0x%x, Fcn: %s", key, key, modifier, #fcn); \
  }

  // Register key press and modifier to a function with a special display_string
  // for keys that don't print nicely in ASCII (e.g. 'PageUp')
  #define REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(key, modifier, fcn, help_msg, display_string) \
  if (!RegisterKeyFcn(key, modifier, std::bind(&WebotsKeyboardController::fcn, this), help_msg, display_string)) { \
    PRINT_NAMED_ERROR("WebotsKeyboardController.RegisterKeyFcn.DuplicateRegistration", "Key: '%c' (0x%x), Modifier: 0x%x, Fcn: %s", key, key, modifier, #fcn); \
  }

  // Register key that already requires shift to be pressed.
  // Only MOD_NONE and MOD_ALT are valid modifiers since MOD_SHIFT is already implied.
  #define REGISTER_SHIFTED_KEY_FCN(key, modifier, fcn, help_msg) \
  if (modifier & MOD_SHIFT) { \
    PRINT_NAMED_ERROR("WebotsKeyboardController.RegisterKeyFcn.InvalidModifier", "Can't use shift modifier because it's already implied in key '%c' (0x%x)", key, key); \
  } else if (!RegisterKeyFcn(key, modifier | MOD_SHIFT, std::bind(&WebotsKeyboardController::fcn, this), help_msg)) { \
    PRINT_NAMED_ERROR("WebotsKeyboardController.RegisterKeyFcn.DuplicateRegistration", "Key: '%c' (0x%x), Modifier: 0x%x, Fcn: %s", key, key, modifier, #fcn); \
  }

  WebotsKeyboardController::WebotsKeyboardController(s32 step_time_ms) :
  UiGameController(step_time_ms)
  {
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::UP,    MOD_NONE,   DriveForward,  "Drive forward", "↑");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::UP,    MOD_ALT,    DriveForward,  "Drive forward (turbo speed)", "↑");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::UP,    MOD_SHIFT,  DriveForward,  "Drive forward (half speed)", "↑");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::DOWN,  MOD_NONE,   DriveBackward, "Drive backward", "↓");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::DOWN,  MOD_ALT,    DriveBackward, "Drive backward (turbo speed)", "↓");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::DOWN,  MOD_SHIFT,  DriveBackward, "Drive backward (half speed)", "↓");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::LEFT,  MOD_NONE,   DriveLeft,     "Turn left", "←");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::LEFT,  MOD_ALT,    DriveLeft,     "Turn left (turbo speed)", "←");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::LEFT,  MOD_SHIFT,  DriveLeft,     "Turn left (half speed)", "←");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::RIGHT, MOD_NONE,   DriveRight,    "Turn right", "→");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::RIGHT, MOD_ALT,    DriveRight,    "Turn right (turbo speed)", "→");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::RIGHT, MOD_SHIFT,  DriveRight,    "Turn right (half speed)", "→");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEUP,   MOD_NONE,      , "", "<PageUp>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEUP,   MOD_ALT,       , "", "<PageUp>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEUP,   MOD_SHIFT,     , "", "<PageUp>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEUP,   MOD_ALT_SHIFT, , "", "<PageUp>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEDOWN, MOD_NONE,      , "", "<PageDown>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEDOWN, MOD_ALT,       , "", "<PageDown>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEDOWN, MOD_SHIFT,     , "", "<PageDown>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::PAGEDOWN, MOD_ALT_SHIFT, , "", "<PageDown>");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::HOME,  MOD_NONE, PressBackButton, "Press backpack button", "<Home>");
    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::HOME,  MOD_ALT,  TouchBackSensor, "Touch backpack touch sensor", "<Home>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::HOME,     MOD_SHIFT,       , "", "<Home>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::HOME,     MOD_ALT_SHIFT, , "", "<Home>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::END,      MOD_NONE,      , "", "<End>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::END,      MOD_ALT,       , "", "<End>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::END,      MOD_SHIFT,     , "", "<End>");
//      REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(webots::Keyboard::END,      MOD_ALT_SHIFT, , "", "<End>");

    REGISTER_KEY_FCN('`', MOD_NONE,      CycleVizOrigin,         "Update viz alignment");
    REGISTER_KEY_FCN('1', MOD_NONE,      MoveLiftToLowDock,      "Move lift to low dock height");
    REGISTER_KEY_FCN('1', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 1");
    REGISTER_KEY_FCN('1', MOD_ALT_SHIFT, ExecuteRobotTestMode,   "Start robot test mode 11");
    REGISTER_KEY_FCN('2', MOD_NONE,      MoveLiftToHighDock,     "Move lift to high dock height");
    REGISTER_KEY_FCN('2', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 2");
    REGISTER_KEY_FCN('2', MOD_ALT_SHIFT, ExecuteRobotTestMode,   "Start robot test mode 12");
    REGISTER_KEY_FCN('3', MOD_NONE,      MoveLiftToCarryHeight,  "Move lift to carry height");
    REGISTER_KEY_FCN('3', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 3");
//      REGISTER_KEY_FCN('3', MOD_ALT_SHIFT, ,   "");
    REGISTER_KEY_FCN('4', MOD_NONE,      MoveHeadToLowLimit,     "Move head all the way down");
    REGISTER_KEY_FCN('4', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 4");
//      REGISTER_KEY_FCN('4', MOD_ALT_SHIFT, ,   "");
    REGISTER_KEY_FCN('5', MOD_NONE,      MoveHeadToHorizontal,   "Move head to 0 degrees");
    REGISTER_KEY_FCN('5', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 5");
    REGISTER_KEY_FCN('6', MOD_NONE,      MoveHeadToHighLimit,    "Move head all the way up");
    REGISTER_KEY_FCN('6', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 6");
    REGISTER_KEY_FCN('7', MOD_NONE,      MoveLiftToAngle,        "Move lift to targetAngle_deg");
    REGISTER_KEY_FCN('7', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 7");
//      REGISTER_KEY_FCN('8', MOD_NONE,      , "");
    REGISTER_KEY_FCN('8', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 8");
//      REGISTER_KEY_FCN('9', MOD_NONE,      , "");
    REGISTER_KEY_FCN('9', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 9");
    REGISTER_KEY_FCN('0', MOD_NONE,      TurnTowardsImagePoint,  "Turn towards last observed centroid");
    REGISTER_KEY_FCN('0', MOD_ALT,       ExecuteRobotTestMode,   "Start robot test mode 0");
    REGISTER_KEY_FCN('0', MOD_ALT_SHIFT, ExecuteRobotTestMode,   "Start robot test mode 10");
    REGISTER_KEY_FCN('-', MOD_NONE,      SayText,                "Say string 'sayString' unprocessed");
    REGISTER_KEY_FCN('-', MOD_ALT,       SayText,                "Say string 'sayString' cozmo-processed");
//      REGISTER_KEY_FCN('=', MOD_NONE,      , "");
//      REGISTER_KEY_FCN('=', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('[', MOD_NONE,      , "");
//      REGISTER_KEY_FCN('[', MOD_ALT,       , "");
    REGISTER_KEY_FCN(']', MOD_NONE,      SetDebugConsoleVar,     "Set debug console variable in engine process");
    REGISTER_KEY_FCN(']', MOD_ALT,       SetDebugConsoleVar,     "Set debug console variable in anim process");
//      REGISTER_KEY_FCN('\\', MOD_NONE,      , "");
//      REGISTER_KEY_FCN('\\', MOD_ALT,       , "");
//      REGISTER_KEY_FCN(';', MOD_NONE,      , "");
//      REGISTER_KEY_FCN(';', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('\'', MOD_NONE,      , "");
//      REGISTER_KEY_FCN('\'', MOD_ALT,       , "");
//      REGISTER_KEY_FCN(',', MOD_NONE,      , "");
//      REGISTER_KEY_FCN(',', MOD_ALT,       , "");
    REGISTER_KEY_FCN('.', MOD_NONE,      SendSelectNextObject,   "Select next object");
//      REGISTER_KEY_FCN('.', MOD_ALT,      , "");
    REGISTER_KEY_FCN('/', MOD_NONE,      StartFreeplayMode,      "Start 'freeplay' mode (as if robot was shaken)");
//      REGISTER_KEY_FCN('/', MOD_ALT,       , "");

    REGISTER_SHIFTED_KEY_FCN('~', MOD_NONE, PlayAnimationTrigger,              "Play animation trigger specified in 'animationToSendName'");
    REGISTER_SHIFTED_KEY_FCN('~', MOD_ALT,  PlayAnimationGroup,                "Play animation group specified in 'animationToSendName'");
//      REGISTER_SHIFTED_KEY_FCN('!', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN('!', MOD_ALT, , "");
//      REGISTER_SHIFTED_KEY_FCN('@', MOD_NONE, , "");
    REGISTER_SHIFTED_KEY_FCN('@', MOD_ALT,  ExecutePlaypenTest,                "Execute playpen test");
//      REGISTER_SHIFTED_KEY_FCN('#', MOD_NONE, ,                      "");
//      REGISTER_SHIFTED_KEY_FCN('#', MOD_ALT, , "");
    REGISTER_SHIFTED_KEY_FCN('$', MOD_NONE, SendSaveCalibrationImage,          "Save calibration image");
    REGISTER_SHIFTED_KEY_FCN('$', MOD_ALT,  SendClearCalibrationImages,        "Clear calibration images");
    REGISTER_SHIFTED_KEY_FCN('%', MOD_NONE, SendComputeCameraCalibration,      "Compute camera calibration from calibration images");
//      REGISTER_SHIFTED_KEY_FCN('%', MOD_ALT, , "");
    REGISTER_SHIFTED_KEY_FCN('^', MOD_NONE, PlayAnimation,                     "Plays animation specified in 'animationToSendName'");
//      REGISTER_SHIFTED_KEY_FCN('^', MOD_ALT,  ,                 "");
//      REGISTER_SHIFTED_KEY_FCN('&', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN('&', MOD_ALT,  , "");
    REGISTER_SHIFTED_KEY_FCN('*', MOD_NONE, SendRandomProceduralFace,          "Draws random procedural face");
    REGISTER_SHIFTED_KEY_FCN('*', MOD_ALT,  SetFaceDisplayHue,                 "Sets face hue to 'faceHue'");
//      REGISTER_SHIFTED_KEY_FCN('(', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN('(', MOD_ALT,  , "");
//      REGISTER_SHIFTED_KEY_FCN(')', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN(')', MOD_ALT,  , "");
    REGISTER_SHIFTED_KEY_FCN('_', MOD_NONE, SetCameraSettings,                 "Set camera settings");
//      REGISTER_SHIFTED_KEY_FCN('_', MOD_ALT, , "");
//      REGISTER_SHIFTED_KEY_FCN('+', MOD_NONE,  , "");
    REGISTER_SHIFTED_KEY_FCN('+', MOD_ALT,  TogglePowerMode,                   "Toggle (syscon) power mode");
//      REGISTER_SHIFTED_KEY_FCN('{', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN('{', MOD_ALT,  , "");
    REGISTER_SHIFTED_KEY_FCN('}', MOD_NONE, RunDebugConsoleFunc,               "Run debug console function with args in engine process");
    REGISTER_SHIFTED_KEY_FCN('}', MOD_ALT,  RunDebugConsoleFunc,               "Run debug console function with args in anim process");
//      REGISTER_SHIFTED_KEY_FCN('|', MOD_NONE, , "");
//      REGISTER_SHIFTED_KEY_FCN('|', MOD_ALT, , "");
    REGISTER_SHIFTED_KEY_FCN(':', MOD_NONE, SetRollActionParams,               "Set parameters for roll action");
//      REGISTER_SHIFTED_KEY_FCN(':', MOD_ALT, , "");
    REGISTER_SHIFTED_KEY_FCN('"', MOD_NONE, PlayCubeAnimation,                 "Play 'Flash' cube animation on selected cube");
    REGISTER_SHIFTED_KEY_FCN('"', MOD_ALT,  PlayCubeAnimation,                 "Play cube animation trigger specified in 'animationToSendName' on selected cube");
    REGISTER_SHIFTED_KEY_FCN('<', MOD_NONE, TurnInPlaceCCW,                    "Turn in place CCW by 'pointTurnAngle_deg'");
    REGISTER_SHIFTED_KEY_FCN('<', MOD_ALT,  TurnInPlaceCCW,                    "Turn in place CCW forever");
    REGISTER_SHIFTED_KEY_FCN('>', MOD_NONE, TurnInPlaceCW,                     "Turn in place CW by 'pointTurnAngle_deg'");
    REGISTER_SHIFTED_KEY_FCN('>', MOD_ALT,  TurnInPlaceCCW,                    "Turn in place CW forever");
    REGISTER_SHIFTED_KEY_FCN('?', MOD_NONE, PrintHelp,                         "Print help menu");
//      REGISTER_SHIFTED_KEY_FCN('?', MOD_ALT,  , "");


    REGISTER_KEY_FCN('A', MOD_NONE,      MoveLiftUp,            "Move lift up");
    REGISTER_KEY_FCN('A', MOD_SHIFT,     MoveLiftUp,            "Move lift up (half speed)");
//    REGISTER_KEY_FCN('A', MOD_ALT,       SendReadAnimationFile, "Re-load animations (Not working)");
//      REGISTER_KEY_FCN('A', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('B', MOD_NONE,      SetActiveObjectLights, "Cube lights");
    REGISTER_KEY_FCN('B', MOD_SHIFT,     SetActiveObjectLights, "Cube lights");
    REGISTER_KEY_FCN('B', MOD_ALT,       SetActiveObjectLights, "Cube lights");
    REGISTER_KEY_FCN('B', MOD_ALT_SHIFT, SetActiveObjectLights, "Cube lights");

    REGISTER_KEY_FCN('C', MOD_NONE,      LogCliffSensorData,    "Request cliff sensor log");
    REGISTER_KEY_FCN('C', MOD_SHIFT,     ExecuteBehavior,       "Execute behavior in 'behaviorName'");
    REGISTER_KEY_FCN('C', MOD_ALT,       ToggleCameraCaptureFormat, "Toggle camera capture format between RGB and YUV");
    // REGISTER_KEY_FCN('C', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('D', MOD_NONE,      ToggleVizDisplay,      "Toggle viz display");
    REGISTER_KEY_FCN('D', MOD_SHIFT,     LogRawProxData,        "Request prox sensor log");
//      REGISTER_KEY_FCN('D', MOD_ALT,       , "");
    REGISTER_KEY_FCN('D', MOD_ALT_SHIFT, SendForceDelocalize,   "Force robot delocalization");

    REGISTER_KEY_FCN('E', MOD_NONE,      SaveSingleImage,           "Save single image");
    REGISTER_KEY_FCN('E', MOD_SHIFT,     ToggleImageSaving,         "Toggle image saving (in viz) mode");
    REGISTER_KEY_FCN('E', MOD_ALT_SHIFT, ToggleImageAndStateSaving, "Toggle image and robot state saving (in viz) mode");

    REGISTER_KEY_FCN('F', MOD_NONE,      ToggleFaceDetection,          "Toggle face detection");
    REGISTER_KEY_FCN('F', MOD_SHIFT,     AssociateNameWithCurrentFace, "Assign 'userName' to current face");
    REGISTER_KEY_FCN('F', MOD_ALT,       TurnTowardsFace,              "Turn towards face 'faceIDToTurnTowards' or last face if 0");
    REGISTER_KEY_FCN('F', MOD_ALT_SHIFT, EraseLastObservedFace,        "Erase last observed face");

    REGISTER_KEY_FCN('G', MOD_NONE,      GotoPoseMarker,              "Goto/place object at pose marker");
    REGISTER_KEY_FCN('G', MOD_SHIFT,     TogglePoseMarkerMode,        "Toggle pose marker mode");
//      REGISTER_KEY_FCN('G', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('G', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('H', MOD_NONE,      FakeCloudIntent, "Fake clound intent with contents of 'intent' (either a name or valid json)");
    REGISTER_KEY_FCN('H', MOD_SHIFT,     FakeUserIntent, "Fake user intent with the contents of 'intent'");
//      REGISTER_KEY_FCN('H', MOD_ALT,       , "");
    REGISTER_KEY_FCN('H', MOD_ALT_SHIFT, SendFakeTriggerWordDetect, "Send fake trigger word detect");

    REGISTER_KEY_FCN('I', MOD_NONE,      ToggleImageStreamingToGame, "Toggle image streaming");
//      REGISTER_KEY_FCN('I', MOD_SHIFT,     , "");
//      REGISTER_KEY_FCN('I', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('I', MOD_ALT_SHIFT, , "");

     REGISTER_KEY_FCN('J', MOD_NONE,     CycleConnectionFlowState, "Cycle connection flow states");
//      REGISTER_KEY_FCN('J', MOD_SHIFT,     , "");
//      REGISTER_KEY_FCN('J', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('J', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('K', MOD_NONE,      SetControllerGains, "Set wheel/head/lift gains");
    REGISTER_KEY_FCN('K', MOD_SHIFT,     SetControllerGains, "Set steering and point turn gains");
    //REGISTER_KEY_FCN('K', MOD_ALT,       , "");
    //REGISTER_KEY_FCN('K', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('L', MOD_NONE,      ToggleTestBackpackLights,   "Toggles a test pattern on backpack lights");
    REGISTER_KEY_FCN('L', MOD_SHIFT,     SearchForNearbyObject,      "Search for nearby object");
    REGISTER_KEY_FCN('L', MOD_ALT,       ToggleCliffSensorEnable,    "Toggles cliff sensor enable");
    REGISTER_KEY_FCN('L', MOD_ALT_SHIFT, ToggleEngineLightComponent, "Toggle engine light component");

    REGISTER_KEY_FCN('M', MOD_NONE,      SetEmotion,          "Set 'emotionName' to 'emotionVal'");
    REGISTER_KEY_FCN('M', MOD_SHIFT,     TriggerEmotionEvent, "Trigger 'emotionEvent'");
//    REGISTER_KEY_FCN('M', MOD_ALT,     ,   "");
//    REGISTER_KEY_FCN('M', MOD_ALT_SHIFT, ,  "");

//    REGISTER_KEY_FCN('N', MOD_NONE,      ,  );
//    REGISTER_KEY_FCN('N', MOD_SHIFT,     ,  );
//    REGISTER_KEY_FCN('N', MOD_ALT,       ,  );
//    REGISTER_KEY_FCN('N', MOD_ALT_SHIFT, ,  );

    REGISTER_KEY_FCN('O', MOD_NONE,      RequestIMUData,    "Request IMU data log");
    REGISTER_KEY_FCN('O', MOD_SHIFT,     TurnTowardsObject, "Turn torwards selected object");
    REGISTER_KEY_FCN('O', MOD_ALT,       GotoObject,        "Go to selected object");
    REGISTER_KEY_FCN('O', MOD_ALT_SHIFT, AlignWithObject,   "Align with selected object");

    REGISTER_KEY_FCN('P', MOD_NONE,      PickOrPlaceObject, "Pickup or place on selected object from predock pose");
    REGISTER_KEY_FCN('P', MOD_SHIFT,     PickOrPlaceObject, "Pickup or place on selected object from current pose");
    REGISTER_KEY_FCN('P', MOD_ALT,       PickOrPlaceObject, "Pickup or place relative to selected object at offset 'placeOnGroundOffsetX_mm' from predock pose");
    REGISTER_KEY_FCN('P', MOD_ALT_SHIFT, PickOrPlaceObject, "Pickup or place relative to selected object at offset 'placeOnGroundOffsetX_mm' from current pose");

    REGISTER_KEY_FCN('Q', MOD_NONE,      SendAbortPath,    "Cancel current path");
    REGISTER_KEY_FCN('Q', MOD_SHIFT,     SendAbortAll,     "Cancel everything (paths, animations, docking, etc.)");
    REGISTER_KEY_FCN('Q', MOD_ALT,       SendCancelAction, "Cancel current action");
//      REGISTER_KEY_FCN('Q', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('R', MOD_NONE,      MountSelectedCharger, "Dock to charger using cliff sensor correction");
    REGISTER_KEY_FCN('R', MOD_SHIFT,     MountSelectedCharger, "Dock to charger without using cliff sensor correction");
    REGISTER_KEY_FCN('R', MOD_ALT,       FlipSelectedBlock,    "Flips the selected cube");
    REGISTER_KEY_FCN('R', MOD_ALT_SHIFT, TeleportOntoCharger,  "Teleport the robot onto the charger");

    REGISTER_KEY_FCN('S', MOD_NONE,      MoveHeadUp, "Move head up");
    REGISTER_KEY_FCN('S', MOD_SHIFT,     MoveHeadUp, "Move head up (half speed)");
//    REGISTER_KEY_FCN('S', MOD_ALT,       , "");
    REGISTER_KEY_FCN('S', MOD_ALT_SHIFT, DoCliffAlignToWhite, "If one front sensor is detecting white (> MIN_CLIFF_STOP_ON_WHITE_VAL) then rotate until other front sensor detects it as well.");

    REGISTER_KEY_FCN('T', MOD_NONE,      ExecuteTestPlan,     "Execute test plan");
    REGISTER_KEY_FCN('T', MOD_ALT,       ToggleTrackToFace,   "Track to face");
    REGISTER_KEY_FCN('T', MOD_SHIFT,     ToggleTrackToObject, "Track to object");
    REGISTER_KEY_FCN('T', MOD_ALT_SHIFT, TrackPet,            "Track to pet");

    REGISTER_KEY_FCN('U', MOD_NONE,      RequestSingleImageToGame,   "Requests single image to game");
    REGISTER_KEY_FCN('U', MOD_SHIFT,     ToggleImageStreamingToGame, "Toggle image streaming to game mode");
//      REGISTER_KEY_FCN('U', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('U', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('V', MOD_NONE,      SetRobotVolume,          "Set robot volume to 'robotVolume'");
    REGISTER_KEY_FCN('V', MOD_SHIFT,     ToggleVisionWhileMoving, "Toggle vision-while-moving enable");
//      REGISTER_KEY_FCN('V', MOD_ALT,       , "");
//      REGISTER_KEY_FCN('V', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('W', MOD_NONE,      RollObject,  "Roll selected object from predock pose");
    REGISTER_KEY_FCN('W', MOD_SHIFT,     RollObject,  "Roll selected object without using predock pose");
    REGISTER_KEY_FCN('W', MOD_ALT,       PopAWheelie, "Pop-a-wheelie off of selected object from predock pose");
    REGISTER_KEY_FCN('W', MOD_ALT_SHIFT, PopAWheelie, "Pop-a-wheelie off of selected object without using predock pose");

    REGISTER_KEY_FCN('X', MOD_NONE,      MoveHeadDown, "Move head down");
    REGISTER_KEY_FCN('X', MOD_SHIFT,     MoveHeadDown, "Move head down (half speed)");
//      REGISTER_KEY_FCN('X', MOD_ALT,       , "");
    REGISTER_KEY_FCN('X', MOD_ALT_SHIFT, QuitKeyboardController, "Quit keyboard controller");

// REGISTER_KEY_FCN('Y', MOD_NONE,      ToggleKeepFaceAliveEnable,     "Toggle keep face alive enable"); // TODO:(bn) new way to do this

//    REGISTER_KEY_FCN('Y', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN('Z', MOD_NONE,      MoveLiftDown,    "Move lift down");
    REGISTER_KEY_FCN('Z', MOD_SHIFT,     MoveLiftDown,    "Move lift down (half speed)");
    REGISTER_KEY_FCN('Z', MOD_ALT,       ToggleLiftPower, "Toggle lift power");
//      REGISTER_KEY_FCN('Z', MOD_ALT_SHIFT, , "");

    REGISTER_KEY_FCN_WITH_SPECIAL_DISPLAY_CHAR(' ', MOD_NONE,      SendStopAllMotors, "Stops all motors", "<Space>");

  }

  bool WebotsKeyboardController::RegisterKeyFcn(int key, int modifier, std::function<void()> fcn, const char* help_msg, std::string display_string)
  {
    // Check if already registered
    auto mod_it = _keyFcnMap.find(key);
    if (mod_it != _keyFcnMap.end()) {
      auto fcn_it = mod_it->second.find(modifier);
      if (fcn_it != mod_it->second.end()) {
        return false;
      }
    }

    // Register function
    auto& fcnInfo = _keyFcnMap[key][modifier];
    fcnInfo.fcn = fcn;
    fcnInfo.helpMsg = help_msg;
    if (display_string.empty()) {
      // If one wasn't passed in, then convert the key to char
      fcnInfo.displayString = (char)key;
    } else {
      fcnInfo.displayString = display_string;
    }

    // Insert key, if not already present, in registrationOrder list
    auto it = find(_keyRegistrationOrder.begin(), _keyRegistrationOrder.end(), key);
    if (it == _keyRegistrationOrder.end()) {
      _keyRegistrationOrder.push_back(key);
    }
    return true;
  }

  void WebotsKeyboardController::ProcessKeyPressFunction(int key, int modifier)
  {
    auto mod_it = _keyFcnMap.find(key);
    if (mod_it != _keyFcnMap.end()) {
      auto fcn_it = mod_it->second.find(modifier);
      if (fcn_it != mod_it->second.end()) {
        fcn_it->second.fcn();
        return;
      }
    }
    PRINT_NAMED_WARNING("WebotsKeyboardController.ProcessKeyPressFunction.KeyNotRegistered",
                        "Key: '%c' (0x%x), Modifier: 0x%x", key, key, modifier);
  }


  void WebotsKeyboardController::PrintHelp()
  {
    printf("Keyboard controls\n");
    printf("===============================\n");

    for (const auto key : _keyRegistrationOrder) {
      const auto& mod_map = _keyFcnMap[key];
      for (const auto& fcn_map : mod_map) {

        // Generate modifier string
        std::string modifierString = "";
        int modifierKey = fcn_map.first & MOD_ALT_SHIFT;
        switch (modifierKey) {
          case MOD_SHIFT:
            modifierString = "Shift+";
            break;
          case MOD_ALT:
            modifierString = "Alt+";
            break;
          case MOD_ALT_SHIFT:
            modifierString = "Alt+Shift+";
            break;
          default:
            break;
        }

        std::string keyComboStr = modifierString + std::string("'") + fcn_map.second.displayString.c_str() + std::string("'");
        printf("%17s: %s\n", keyComboStr.c_str(), fcn_map.second.helpMsg.c_str());
      }
    }
  }

  //Check the keyboard keys and issue robot commands
  void WebotsKeyboardController::ProcessKeystroke()
  {
    _steeringDir = 0.f;
    _throttleDir = 0.f;
    _pressBackpackButton = false;
    _touchBackpackTouchSensor = false;

    _commandedLiftSpeed = 0.f;
    _commandedHeadSpeed = 0.f;

    _movingHead = false;
    _movingLift = false;

    root_ = GetSupervisor().getSelf();

    static bool keyboardRestart = false;
    if (keyboardRestart) {
      GetSupervisor().getKeyboard()->disable();
      GetSupervisor().getKeyboard()->enable(BS_TIME_STEP_MS);
      keyboardRestart = false;
    }

    // Get all keys pressed this tic
    std::set<int> keysPressed;
    int key;
    while((key = GetSupervisor().getKeyboard()->getKey()) >= 0) {
      keysPressed.insert(key);
    }

    // If exact same keys were pressed last tic, do nothing.
    if (lastKeysPressed_ == keysPressed) {
      return;
    }
    lastKeysPressed_ = keysPressed;

    for(auto key : keysPressed)
    {
      // Extract modifier key(s)
      const int modifier_key = key & ~webots::Keyboard::KEY;
      _shiftKeyPressed = modifier_key & webots::Keyboard::SHIFT;
      _altKeyPressed = modifier_key & webots::Keyboard::ALT;

      // Set key to its modifier-less self
      key &= webots::Keyboard::KEY;

      lastKeyPressTime_ = GetSupervisor().getTime();

      // Update _currKey for functions that might care
      _currKey = key;

      /*
      // DEBUG: Display modifier key information
      printf("Key = '%c' (%d)", char(key), key);
      if(modifier_key) {
        printf(", with modifier keys: ");
        if(modifier_key & webots::Keyboard::ALT) {
          printf("ALT ");
        }
        if(modifier_key & webots::Keyboard::SHIFT) {
          printf("SHIFT ");
        }
        if(modifier_key & webots::Keyboard::CONTROL) {
          printf("CTRL/CMD ");
        }

      }
      printf("\n");
      */


      // Dock speed
      const f32 dockSpeed_mmps = root_->getField("dockSpeed_mmps")->getSFFloat();
      const f32 dockAccel_mmps2 = root_->getField("dockAccel_mmps2")->getSFFloat();
      const f32 dockDecel_mmps2 = root_->getField("dockDecel_mmps2")->getSFFloat();

      // Path speeds
      const f32 pathSpeed_mmps = root_->getField("pathSpeed_mmps")->getSFFloat();
      const f32 pathAccel_mmps2 = root_->getField("pathAccel_mmps2")->getSFFloat();
      const f32 pathDecel_mmps2 = root_->getField("pathDecel_mmps2")->getSFFloat();
      const f32 pathPointTurnSpeed_radPerSec = root_->getField("pathPointTurnSpeed_radPerSec")->getSFFloat();
      const f32 pathPointTurnAccel_radPerSec2 = root_->getField("pathPointTurnAccel_radPerSec2")->getSFFloat();
      const f32 pathPointTurnDecel_radPerSec2 = root_->getField("pathPointTurnDecel_radPerSec2")->getSFFloat();
      const f32 pathReverseSpeed_mmps = root_->getField("pathReverseSpeed_mmps")->getSFFloat();

      // If any of the pathMotionProfile fields are different than the default values use a custom profile
      if(pathMotionProfile_.speed_mmps != pathSpeed_mmps ||
         pathMotionProfile_.accel_mmps2 != pathAccel_mmps2 ||
         pathMotionProfile_.decel_mmps2 != pathDecel_mmps2 ||
         pathMotionProfile_.pointTurnSpeed_rad_per_sec != pathPointTurnSpeed_radPerSec ||
         pathMotionProfile_.pointTurnAccel_rad_per_sec2 != pathPointTurnAccel_radPerSec2 ||
         pathMotionProfile_.pointTurnDecel_rad_per_sec2 != pathPointTurnDecel_radPerSec2 ||
         pathMotionProfile_.dockSpeed_mmps != dockSpeed_mmps ||
         pathMotionProfile_.dockAccel_mmps2 != dockAccel_mmps2 ||
         pathMotionProfile_.dockDecel_mmps2 != dockDecel_mmps2 ||
         pathMotionProfile_.reverseSpeed_mmps != pathReverseSpeed_mmps)
      {
        pathMotionProfile_.isCustom = true;
      }

      pathMotionProfile_.speed_mmps = pathSpeed_mmps;
      pathMotionProfile_.accel_mmps2 = pathAccel_mmps2;
      pathMotionProfile_.decel_mmps2 = pathDecel_mmps2;
      pathMotionProfile_.pointTurnSpeed_rad_per_sec = pathPointTurnSpeed_radPerSec;
      pathMotionProfile_.pointTurnAccel_rad_per_sec2 = pathPointTurnAccel_radPerSec2;
      pathMotionProfile_.pointTurnDecel_rad_per_sec2 = pathPointTurnDecel_radPerSec2;
      pathMotionProfile_.dockSpeed_mmps = dockSpeed_mmps;
      pathMotionProfile_.dockAccel_mmps2 = dockAccel_mmps2;
      pathMotionProfile_.dockDecel_mmps2 = dockDecel_mmps2;
      pathMotionProfile_.reverseSpeed_mmps = pathReverseSpeed_mmps;

      // For pickup or placeRel, specify whether or not you want to use the
      // given approach angle for pickup, placeRel, or roll actions
      useApproachAngle = root_->getField("useApproachAngle")->getSFBool();
      approachAngle_rad = DEG_TO_RAD(root_->getField("approachAngle_deg")->getSFFloat());

      //printf("keypressed: %d, modifier %d, orig_key %d, prev_key %d\n",
      //       key, modifier_key, key | modifier_key, lastKeyPressed_);

      std::string drivingStartAnim, drivingLoopAnim, drivingEndAnim;
      const bool failOnEmptyString = false;
      WebotsHelpers::GetFieldAsString(*root_, "drivingStartAnim", drivingStartAnim, failOnEmptyString);
      WebotsHelpers::GetFieldAsString(*root_, "drivingLoopAnim" , drivingLoopAnim,  failOnEmptyString);
      WebotsHelpers::GetFieldAsString(*root_, "drivingEndAnim"  , drivingEndAnim,   failOnEmptyString);

      if(_drivingStartAnim.compare(drivingStartAnim) != 0 ||
         _drivingLoopAnim.compare(drivingLoopAnim) != 0 ||
         _drivingEndAnim.compare(drivingEndAnim) != 0)
      {
        _drivingStartAnim = drivingStartAnim;
        _drivingLoopAnim = drivingLoopAnim;
        _drivingEndAnim = drivingEndAnim;

        static const char* kWebotsDrivingLock = "webots_driving_lock";
        // Pop whatever driving animations were being used and push the new ones
        SendRemoveDrivingAnimations(kWebotsDrivingLock);
        SendPushDrivingAnimations(kWebotsDrivingLock,
                                  AnimationTriggerFromString(_drivingStartAnim),
                                  AnimationTriggerFromString(_drivingLoopAnim),
                                  AnimationTriggerFromString(_drivingEndAnim));
      }


      ProcessKeyPressFunction(key, modifier_key);

    } // for(auto key : keysPressed_)

    bool movingWheels = _throttleDir || _steeringDir;

    f32 driveAccel = root_->getField("driveAccel")->getSFFloat();
    bool useDriveArc = root_->getField("useDriveArc")->getSFBool();

    if(movingWheels) {

      f32 leftSpeed = 0.f;
      f32 rightSpeed = 0.f;

      f32 wheelSpeed = root_->getField("driveSpeedNormal")->getSFFloat();
      f32 steeringCurvature = root_->getField("steeringCurvature")->getSFFloat();

      // Use slow motor speeds if SHIFT is pressed
      // Use fast motor speeds if ALT is pressed
      if (_shiftKeyPressed) {
        wheelSpeed = root_->getField("driveSpeedSlow")->getSFFloat();
      } else if(_altKeyPressed) {
        wheelSpeed = root_->getField("driveSpeedTurbo")->getSFFloat();
      }

      // Set wheel speeds based on drive commands
      if (_throttleDir > 0) {
        leftSpeed = wheelSpeed + _steeringDir * wheelSpeed * steeringCurvature;
        rightSpeed = wheelSpeed - _steeringDir * wheelSpeed * steeringCurvature;
      } else if (_throttleDir < 0) {
        leftSpeed = -wheelSpeed - _steeringDir * wheelSpeed * steeringCurvature;
        rightSpeed = -wheelSpeed + _steeringDir * wheelSpeed * steeringCurvature;
      } else {
        leftSpeed = _steeringDir * wheelSpeed;
        rightSpeed = -_steeringDir * wheelSpeed;
      }

      if (useDriveArc) {
        f32 speed = _throttleDir * wheelSpeed;
        s16 curvature = -_steeringDir * 50;
        f32 accel = driveAccel;
        if (_steeringDir == 0) {
          curvature = std::numeric_limits<s16>::max();
        }
        if (_throttleDir == 0) {
          speed = -_steeringDir * wheelSpeed / WHEEL_DIST_HALF_MM;
          curvature = 0.f;
          accel = 3.14f;
        }
        SendDriveArc(speed, accel, curvature);
        lastDrivingCurvature_mm_ = curvature;
      } else {
        SendDriveWheels(leftSpeed, rightSpeed, driveAccel, driveAccel);
      }

      _wasMovingWheels = true;
    } else if(_wasMovingWheels && !movingWheels) {
      // If we just stopped moving the wheels:
      if (useDriveArc) {
        SendDriveArc(0, driveAccel, lastDrivingCurvature_mm_);
      } else {
        SendDriveWheels(0, 0, driveAccel, driveAccel);
      }
      _wasMovingWheels = false;
    }

    // If the last key pressed was a move lift key then stop it.
    if(_movingLift) {
      SendMoveLift(_commandedLiftSpeed);
      _wasMovingLift = true;
    } else if (_wasMovingLift && !_movingLift) {
      // If we just stopped moving the lift:
      SendMoveLift(0);
      _wasMovingLift = false;
    }

    if(_movingHead) {
      SendMoveHead(_commandedHeadSpeed);
      _wasMovingHead = true;
    } else if (_wasMovingHead && !_movingHead) {
      // If we just stopped moving the head:
      SendMoveHead(0);
      _wasMovingHead = false;
    }

    if (_pressBackpackButton && !_wasBackpackButtonPressed) {
      PressBackpackButton(true);
    } else if (!_pressBackpackButton && _wasBackpackButtonPressed) {
      PressBackpackButton(false);
    }
    _wasBackpackButtonPressed = _pressBackpackButton;

    if (_touchBackpackTouchSensor && !_wasBackpackTouchSensorTouched) {
      TouchBackpackTouchSensor(true);
    } else if (!_touchBackpackTouchSensor && _wasBackpackTouchSensorTouched) {
      TouchBackpackTouchSensor(false);
    }
    _wasBackpackTouchSensorTouched = _touchBackpackTouchSensor;

  } // BSKeyboardController::ProcessKeyStroke()


  void WebotsKeyboardController::TestLightCube()
  {
    static std::vector<ColorRGBA> colors = {{
      ::Anki::NamedColors::RED, ::Anki::NamedColors::GREEN, ::Anki::NamedColors::BLUE,
      ::Anki::NamedColors::CYAN, ::Anki::NamedColors::ORANGE, ::Anki::NamedColors::YELLOW
    }};
    static std::vector<WhichCubeLEDs> leds = {{
      WhichCubeLEDs::BACK,
      WhichCubeLEDs::LEFT,
      WhichCubeLEDs::FRONT,
      WhichCubeLEDs::RIGHT
    }};

    static auto colorIter = colors.begin();
    static auto ledIter = leds.begin();
    static s32 counter = 0;

    if(counter++ == 30) {
      counter = 0;

      ExternalInterface::SetActiveObjectLEDs msg;
      msg.objectID = GetLastObservedObject().id;
      msg.onPeriod_ms = 100;
      msg.offPeriod_ms = 100;
      msg.transitionOnPeriod_ms = 50;
      msg.transitionOffPeriod_ms = 50;
      msg.turnOffUnspecifiedLEDs = 1;
      msg.onColor = *colorIter;
      msg.offColor = 0;
      msg.whichLEDs = *ledIter;
      msg.makeRelative = MakeRelativeMode::RELATIVE_LED_MODE_OFF;

      ++ledIter;
      if(ledIter==leds.end()) {
        ledIter = leds.begin();
        ++colorIter;
        if(colorIter == colors.end()) {
          colorIter = colors.begin();
        }
      }

      ExternalInterface::MessageGameToEngine message;
      message.Set_SetActiveObjectLEDs(msg);
      SendMessage(message);
    }
  } // TestLightCube()

  Pose3d WebotsKeyboardController::GetGoalMarkerPose()
  {
    // pose of the goal marker is configured in
    // proto for the controller to be reflected
    // in the pose of the Webots::Node
    return GetPose3dOfNode(root_);
  }

  void WebotsKeyboardController::ToggleCameraCaptureFormat()
  {
    ExternalInterface::SetCameraCaptureFormat msg;
    static bool yuv = true;
    LOG_INFO("ToggleCameraCaptureFormat",
             "Switching to %s",
             yuv ? "YUV" : "RGB");
    msg.format = (yuv ? Vision::ImageEncoding::YUV420sp : Vision::ImageEncoding::RawRGB);
    yuv = !yuv;

    ExternalInterface::MessageGameToEngine msgWrapper;
    msgWrapper.Set_SetCameraCaptureFormat(msg);
    SendMessage(msgWrapper);
  }

  s32 WebotsKeyboardController::UpdateInternal()
  {

    static bool streamStarted = false;
    if (!streamStarted) {
      SendImageRequest(ImageSendMode::Stream);
      streamStarted = true;
    }

    Pose3d goalMarkerPose = GetGoalMarkerPose();

    // Update pose marker if different from last time
    if (!(prevGoalMarkerPose_ == goalMarkerPose)) {
      if (poseMarkerMode_ != 0) {
        // Place object mode
        SendDrawPoseMarker(goalMarkerPose);
      }
      prevGoalMarkerPose_ = goalMarkerPose;
    }


    ProcessKeystroke();

    if( _shouldQuit ) {
      return 1;
    }
    else {
      return 0;
    }
  }

  void WebotsKeyboardController::HandleRobotConnected(const ExternalInterface::RobotConnectionResponse& msg)
  {
    // Things to do on robot connect
    if (root_->getField("startFreeplayModeImmediately")->getSFBool()) {
      StartFreeplayMode();
    }

    SendSetRobotVolume(0);
  }

} // namespace Vector
} // namespace Anki


// =======================================================================

int main(int argc, char **argv)
{
  using namespace Anki;
  using namespace Anki::Vector;

  // parse commands
  WebotsCtrlShared::ParsedCommandLine params = WebotsCtrlShared::ParseCommandLine(argc, argv);
  // create platform
  const Anki::Util::Data::DataPlatform& dataPlatform = WebotsCtrlShared::CreateDataPlatformBS(argv[0], "webotsCtrlKeyboard");
  // initialize logger
  WebotsCtrlShared::DefaultAutoGlobalLogger autoLogger(dataPlatform, params.filterLog, params.colorizeStderrOutput);

  Anki::Vector::WebotsKeyboardController webotsCtrlKeyboard(BS_TIME_STEP_MS);
  webotsCtrlKeyboard.PreInit();
  webotsCtrlKeyboard.WaitOnKeyboardToConnect();

  webotsCtrlKeyboard.Init();
  while (webotsCtrlKeyboard.Update() == 0)
  {
  }

  return 0;
}
