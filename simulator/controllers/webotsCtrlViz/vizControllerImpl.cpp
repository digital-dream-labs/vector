/**
* File: vizControllerImpl
*
* Author: damjan stulic
* Created: 9/15/15
*
* Description: 
*
* Copyright: Anki, inc. 2015
*
*/

#include "vizControllerImpl.h"

#include "simulator/controllers/shared/webotsHelpers.h"
#include "coretech/common/shared/array2d.h"
#include "coretech/common/engine/colorRGBA.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/vision/engine/image.h"
#include "clad/types/animationTypes.h"
#include "clad/vizInterface/messageViz.h"
#include "engine/aiComponent/behaviorComponent/behaviorTypesWrapper.h"
#include "engine/vision/visionModesHelpers.h"
#include "engine/viz/vizTextLabelTypes.h"
#include "util/fileUtils/fileUtils.h"
#include "util/helpers/fullEnumToValueArrayChecker.h"
#include "util/logging/logging.h"
#include <functional>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <webots/Display.hpp>
#include <webots/ImageRef.hpp>
#include <webots/Supervisor.hpp>

#include <iomanip>

namespace Anki {
namespace Vector {


VizControllerImpl::VizControllerImpl(webots::Supervisor& vs)
  : _vizSupervisor(vs)
{
}
  

void VizControllerImpl::Init()
{
  // bind to specific handlers in the robot class
  Subscribe(VizInterface::MessageVizTag::SetRobot,
    std::bind(&VizControllerImpl::ProcessVizSetRobotMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::SetLabel,
    std::bind(&VizControllerImpl::ProcessVizSetLabelMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::DockingErrorSignal,
    std::bind(&VizControllerImpl::ProcessVizDockingErrorSignalMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::VisionMarker,
    std::bind(&VizControllerImpl::ProcessVizVisionMarkerMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraQuad,
    std::bind(&VizControllerImpl::ProcessVizCameraQuadMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraRect,
    std::bind(&VizControllerImpl::ProcessVizCameraRectMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraLine,
    std::bind(&VizControllerImpl::ProcessVizCameraLineMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraOval,
    std::bind(&VizControllerImpl::ProcessVizCameraOvalMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraText,
    std::bind(&VizControllerImpl::ProcessVizCameraTextMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::ImageChunk,
    std::bind(&VizControllerImpl::ProcessVizImageChunkMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::TrackerQuad,
    std::bind(&VizControllerImpl::ProcessVizTrackerQuadMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::RobotStateMessage,
    std::bind(&VizControllerImpl::ProcessVizRobotStateMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CurrentAnimation,
    std::bind(&VizControllerImpl::ProcessVizCurrentAnimation, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::SaveImages,
    std::bind(&VizControllerImpl::ProcessSaveImages, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::SaveState,
    std::bind(&VizControllerImpl::ProcessSaveState, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::CameraParams,
    std::bind(&VizControllerImpl::ProcessCameraParams, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::BehaviorStackDebug,
    std::bind(&VizControllerImpl::ProcessBehaviorStackDebug, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::VisionModeDebug,
    std::bind(&VizControllerImpl::ProcessVisionModeDebug, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::EnabledVisionModes,
    std::bind(&VizControllerImpl::ProcessEnabledVisionModes, this, std::placeholders::_1));

  Subscribe(VizInterface::MessageVizTag::SetVizOrigin,
            std::bind(&VizControllerImpl::ProcessVizSetOriginMessage, this, std::placeholders::_1));
  
  Subscribe(VizInterface::MessageVizTag::MemoryMapMessageVizBegin,
            std::bind(&VizControllerImpl::ProcessVizMemoryMapMessageBegin, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::MemoryMapMessageViz,
            std::bind(&VizControllerImpl::ProcessVizMemoryMapMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::MemoryMapMessageVizEnd,
            std::bind(&VizControllerImpl::ProcessVizMemoryMapMessageEnd, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::Object,
            std::bind(&VizControllerImpl::ProcessVizObjectMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::EraseObject,
            std::bind(&VizControllerImpl::ProcessVizEraseObjectMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::ShowObjects,
            std::bind(&VizControllerImpl::ProcessVizShowObjectsMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::LineSegment,
            std::bind(&VizControllerImpl::ProcessVizLineSegmentMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::EraseLineSegments,
            std::bind(&VizControllerImpl::ProcessVizEraseLineSegmentsMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::Quad,
            std::bind(&VizControllerImpl::ProcessVizQuadMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::EraseQuad,
            std::bind(&VizControllerImpl::ProcessVizEraseQuadMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::AppendPathSegmentLine,
            std::bind(&VizControllerImpl::ProcessVizAppendPathSegmentLineMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::AppendPathSegmentArc,
            std::bind(&VizControllerImpl::ProcessVizAppendPathSegmentArcMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::SetPathColor,
            std::bind(&VizControllerImpl::ProcessVizSetPathColorMessage, this, std::placeholders::_1));
  Subscribe(VizInterface::MessageVizTag::ErasePath,
            std::bind(&VizControllerImpl::ProcessVizErasePathMessage, this, std::placeholders::_1));

  // Get display devices
  _navMapDisp = _vizSupervisor.getDisplay("nav_map");
  _disp = _vizSupervisor.getDisplay("cozmo_viz_display");
  _dockDisp = _vizSupervisor.getDisplay("cozmo_docking_display");
  _bsmStackDisp = _vizSupervisor.getDisplay("victor_behavior_stack_display");
  _visionModeDisp = _vizSupervisor.getDisplay("victor_vision_mode_display");

  // Find all the debug image displays in the proto. Use the first as the camera feed and the rest for debug images.
  {
    webots::Node* vizNode = _vizSupervisor.getSelf();
    webots::Field* numDisplaysField = vizNode->getField("numDebugImageDisplays");
    s32 numDisplays = 1;
    if(numDisplaysField == nullptr)
    {
      PRINT_NAMED_WARNING("VizControllerImpl.Init.MissingNumDebugDisplaysField", "Assuming single display (camera)");
    }
    else 
    {
      numDisplays = numDisplaysField->getSFInt32() + 1; // +1 because this is in addition to the camera
    }
    
    _camDisp = nullptr;

    for(s32 displayCtr = 0; displayCtr < numDisplays; ++displayCtr)
    {
      webots::Display* display = _vizSupervisor.getDisplay("cozmo_debug_image_display" + std::to_string(displayCtr));
      DEV_ASSERT_MSG(display != nullptr, "VizControllerImpl.Init.NullDebugDisplay", "displayCtr=%d", displayCtr);
    
      if(displayCtr==0)
      {
        _camDisp = display;
      }
      else
      {
        _debugImages.emplace_back(display);
      }
    }
    
    DEV_ASSERT(_camDisp != nullptr, "VizControllerImpl.Init.NoCameraDisplay");
    PRINT_NAMED_DEBUG("VizControllerImpl.Init.ImageDisplaysCreated",
                      "Found camera display and %zu debug displays",
                      _debugImages.size()-1);
  }

  _disp->setFont("Lucida Console", 8, true);
  _bsmStackDisp->setFont("Lucida Console", 8, true);
  _visionModeDisp->setFont("Lucida Console", 8, true);
  
  // === Look for CozmoBot in scene tree ===

  // Look for controller-less CozmoBot in children.
  // These will be used as visualization robots.
  auto nodeInfo = WebotsHelpers::GetFirstMatchingSceneTreeNode(_vizSupervisor, "CozmoBot");
  if (nodeInfo.nodePtr == nullptr) {
    // If there's no Vector, look for a Whiskey
    nodeInfo = WebotsHelpers::GetFirstMatchingSceneTreeNode(_vizSupervisor, "WhiskeyBot");
  }

  const auto* nd = nodeInfo.nodePtr;
  if (nd != nullptr) {
    DEV_ASSERT(nodeInfo.type == webots::Node::ROBOT, "VizControllerImpl.Init.CozmoBotNotASupervisor");
    
    // Get the vizMode status
    bool vizMode = false;
    webots::Field* vizModeField = nd->getField("vizMode");
    if (vizModeField) {
      vizMode = vizModeField->getSFBool();
    }
    
    if (vizMode) {
      PRINT_NAMED_INFO("VizControllerImpl.Init.FoundVizRobot",
                       "Found Viz robot with name %s", nodeInfo.typeName.c_str());

      // Find pose fields
      _vizBot.trans = nd->getField("translation");
      _vizBot.rot = nd->getField("rotation");

      // Find lift and head angle fields
      _vizBot.headAngle = nd->getField("headAngle");
      _vizBot.liftAngle = nd->getField("liftAngle");

      DEV_ASSERT_MSG(_vizBot.Valid(),
                     "VizControllerImpl.Init.MissingFields",
                     "Could not find all required fields in CozmoBot supervisor");
    } else if (_drawingObjectsEnabled) {
      // vizMode is false here, meaning that there is an actual simulated robot in the world. If drawing objects is
      // enabled, then we must be able to hide any new objects from the robot's camera. Therefore, we need to be able
      // to access the Camera node so that we can call Node::setVisibility() on each new object. There seems to be no
      // good way to get the underlying node pointer of the camera, so we have to do this somewhat hacky iteration over
      // all of the nodes in the world to find the camera node's ID.
      const int maxNodesToSearch = 10000;
      webots::Node* cameraNode = nullptr;
      webots::Node* tofNode = nullptr;
      for (int i=0 ; i < maxNodesToSearch ; i++) {
        auto* node = _vizSupervisor.getFromId(i);
        if (node != nullptr)
        {
          if(node->getTypeName() == "CozmoCamera")
          {
            cameraNode = node;
          }
          else if(node->getTypeName() == "RangeFinder")
          {
            tofNode = node;
          }
        }

        if(cameraNode != nullptr && tofNode != nullptr)
        {
          break;
        }
      }
      
      DEV_ASSERT(cameraNode != nullptr, "No camera found");
      _cozmoCameraNodeId = cameraNode->getId();

      // A RangeFinder node may or may not exist depending on whether or not the simulated robot
      // is Whiskey or Vector
      if(tofNode != nullptr)
      {
        _cozmoToFNodeId = tofNode->getId();
      }

      SetNodeVisibility(_vizSupervisor.getSelf());
    }
  }
}

void VizControllerImpl::Update()
{
  const double currTime_sec = _vizSupervisor.getTime();
  const double updateRate = _vizSupervisor.getSelf()->getField("drawObjectsRate_sec")->getSFFloat();
  
  if (currTime_sec - _lastDrawTime_sec > updateRate) {
    Draw();
    _lastDrawTime_sec = currTime_sec;
  }
}

void VizControllerImpl::ProcessMessage(VizInterface::MessageViz&& message)
{
  uint32_t type = static_cast<uint32_t>(message.GetTag());
  _eventMgr.Broadcast(AnkiEvent<VizInterface::MessageViz>(
    type, std::move(message)));
}
  
void VizControllerImpl::ProcessSaveImages(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_SaveImages();
  _saveImageMode = payload.mode;
  if(_saveImageMode != ImageSendMode::Off)
  {
    if(payload.path.empty()) {
      _savedImagesFolder = "saved_images";
    } else {
      _savedImagesFolder = payload.path;
    }
    
    if (!_savedImagesFolder.empty() && !Util::FileUtils::CreateDirectory(_savedImagesFolder, false, true)) {
      PRINT_NAMED_WARNING("VizControllerImpl.ProcessSaveImages.CreateDirectoryFailed",
                          "Could not create: %s", _savedImagesFolder.c_str());
    }
    else {
      PRINT_NAMED_INFO("VizControllerImpl.ProcessSaveImages.DirectorySet",
                       "Will save to %s", _savedImagesFolder.c_str());
    }
  }
  else
  {
    PRINT_NAMED_INFO("VizControllerImpl.ProcessSaveImages.DisablingImageSaving",
                     "Disabling image saving");
  }
}
  
  
void VizControllerImpl::ProcessSaveState(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_SaveState();
  _saveState = payload.enabled;
  if(_saveState)
  {
    if(_savedStateFolder.empty()) {
      _savedStateFolder = "saved_state";
    } else {
      _savedStateFolder = payload.path;
    }
  }
}

void VizControllerImpl::SetRobotPose(CozmoBotVizParams& vizParams, const Pose3d& pose, const f32 headAngle, const f32 liftAngle)
{
  // Make sure we haven't tried to set these Webots fields in the current time step
  // (which causes weird behavior due to a Webots R2018a bug with the setSF* functions)
  // This should be removed once the Webots bug is fixed (COZMO-16021)
  static double lastUpdateTime = 0.0;
  const double currTime = _vizSupervisor.getTime();
  if (FLT_NEAR(currTime, lastUpdateTime)) {
    return;
  }
  lastUpdateTime = currTime;
  
  double trans[3] = {0};
  WebotsHelpers::GetWebotsTranslation(pose, trans);
  vizParams.trans->setSFVec3f(trans);
  
  double rot[4] = {0};
  WebotsHelpers::GetWebotsRotation(pose, rot);
  vizParams.rot->setSFRotation(rot);

  vizParams.liftAngle->setSFFloat(liftAngle + 0.199763);  // Adding LIFT_LOW_ANGLE_LIMIT since the model's lift angle does not correspond to robot's lift angle.
  // TODO: Make this less hard-coded.
  vizParams.headAngle->setSFFloat(headAngle);
}


void VizControllerImpl::ProcessVizSetRobotMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  if (_vizBot.Valid()) {
    const auto& payload = msg.GetData().Get_SetRobot();
    
    SetRobotPose(_vizBot,
                 Pose3d(payload.rot_rad,
                        Vec3f(payload.rot_axis_x, payload.rot_axis_y, payload.rot_axis_z),
                        Vec3f(payload.x_trans_m, payload.y_trans_m, payload.z_trans_m)),
                 payload.head_angle,
                 payload.lift_angle);
  }
}

static inline void SetColorHelper(webots::Display* disp, u32 ankiColor)
{
  disp->setColor(ankiColor >> 8);
  
  const uint8_t alpha = (uint8_t)(ankiColor & 0xff);
  if(alpha < 0xff) {
    static const float oneOver255 = 1.f / 255.f;
    disp->setAlpha(oneOver255 * static_cast<f32>(alpha));
  } else {
    disp->setAlpha(1.f); // need to restore alpha to 1.0 in case it was lowered from a previous call
  }
}
  
void VizControllerImpl::DrawText(webots::Display* disp, u32 lineNum, u32 color, const char* text)
{
  if (disp == nullptr) {
    PRINT_NAMED_WARNING("VizControllerImpl.DrawText.NullDisplay", "");
    return;
  }
  
  const int baseXOffset = 8;
  const int baseYOffset = 8;
  const int yLabelStep = 10;  // Line spacing in pixels. Characters are 8x8 pixels in size.

  // Clear line specified by lineNum
  SetColorHelper(disp, NamedColors::BLACK);
  disp->fillRectangle(0, baseYOffset + yLabelStep * lineNum, disp->getWidth(), yLabelStep);

  // Draw text
  SetColorHelper(disp, color);

  std::string str(text);
  if(str.empty()) {
    str = " "; // Avoid webots warnings for empty text
  }
  disp->drawText(str, baseXOffset, baseYOffset + yLabelStep * lineNum);
}

void VizControllerImpl::DrawText(webots::Display* disp, u32 lineNum, const char* text)
{
  DrawText(disp, lineNum, 0xffffff, text);
}
  
void VizControllerImpl::ProcessVizSetLabelMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_SetLabel();
  const u32 lineNum = ((uint32_t)VizTextLabelType::NUM_TEXT_LABELS + payload.labelID);
  DrawText(_disp, lineNum, payload.colorID, payload.text.c_str());
}

void VizControllerImpl::ProcessVizDockingErrorSignalMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  // TODO: This can overlap with text being displayed. Create a dedicated display for it?
  const auto& payload = msg.GetData().Get_DockingErrorSignal();
  // Pixel dimensions of display area
  const int baseXOffset = 8;
  const int baseYOffset = 40;
  const int rectW = 180;
  const int rectH = 180;
  const int halfBlockFaceLength = 20;

  const f32 MM_PER_PIXEL = 2.f;

  // Print values
  char text[111];
  sprintf(text, "ErrSig x:%.1f y:%.1f z:%.1f a:%.2f\n",
          payload.x_dist, payload.y_dist, payload.z_dist, payload.angle);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_DOCK_ERROR_SIGNAL, text);
  _camDisp->setColor(0xff0000);
  _camDisp->drawText(text, 0, 0);


  // Clear the space
  _dockDisp->setColor(0x0);
  _dockDisp->fillRectangle(baseXOffset, baseYOffset, rectW, rectH);

  _dockDisp->setColor(0xffffff);
  _dockDisp->drawRectangle(baseXOffset, baseYOffset, rectW, rectH);

  // Draw robot position
  _dockDisp->drawOval((int)(baseXOffset + 0.5f*rectW), baseYOffset + rectH, 3, 3);


  // Get pixel coordinates of block face center where
  int blockFaceCenterX = (int)(0.5f*rectW - payload.y_dist / MM_PER_PIXEL);
  int blockFaceCenterY = (int)(rectH - payload.x_dist / MM_PER_PIXEL);

  // Check that center is within display area
  if (blockFaceCenterX < halfBlockFaceLength || (blockFaceCenterX > rectW - halfBlockFaceLength) ||
    blockFaceCenterY < halfBlockFaceLength || (blockFaceCenterY > rectH - halfBlockFaceLength) ) {
    return;
  }

  blockFaceCenterX += baseXOffset;
  blockFaceCenterY += baseYOffset;

  // Draw line representing the block face
  int dx = (int)(halfBlockFaceLength * cosf(payload.angle));
  int dy = (int)(-halfBlockFaceLength * sinf(payload.angle));
  _dockDisp->drawLine(blockFaceCenterX + dx, blockFaceCenterY + dy, blockFaceCenterX - dx, blockFaceCenterY - dy);
  _dockDisp->drawOval(blockFaceCenterX, blockFaceCenterY, 2, 2);

}

void VizControllerImpl::ProcessVizVisionMarkerMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_VisionMarker();
  if(payload.verified) {
    _camDisp->setColor(0xff0000);
  } else {
    _camDisp->setColor(0x0000ff);
  }
  _camDisp->drawLine(payload.topLeft_x, payload.topLeft_y, payload.bottomLeft_x, payload.bottomLeft_y);
  _camDisp->drawLine(payload.bottomLeft_x, payload.bottomLeft_y, payload.bottomRight_x, payload.bottomRight_y);
  _camDisp->drawLine(payload.bottomRight_x, payload.bottomRight_y, payload.topRight_x, payload.topRight_y);
  _camDisp->drawLine(payload.topRight_x, payload.topRight_y, payload.topLeft_x, payload.topLeft_y);
}

void VizControllerImpl::ProcessVizCameraQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraQuad();

  SetColorHelper(_camDisp, payload.color);
  _camDisp->drawLine((int)payload.xUpperLeft, (int)payload.yUpperLeft, (int)payload.xLowerLeft, (int)payload.yLowerLeft);
  _camDisp->drawLine((int)payload.xLowerLeft, (int)payload.yLowerLeft, (int)payload.xLowerRight, (int)payload.yLowerRight);
  _camDisp->drawLine((int)payload.xLowerRight, (int)payload.yLowerRight, (int)payload.xUpperRight, (int)payload.yUpperRight);
  
  if(payload.topColor != payload.color)
  {
    SetColorHelper(_camDisp, payload.topColor);
  }
  _camDisp->drawLine((int)payload.xUpperRight, (int)payload.yUpperRight, (int)payload.xUpperLeft, (int)payload.yUpperLeft);
}
  
void VizControllerImpl::ProcessVizCameraRectMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraRect();
  
  SetColorHelper(_camDisp, payload.color);
  if(payload.filled)
  {
    _camDisp->fillRectangle(payload.x, payload.y, payload.width, payload.height);
  }
  else
  {
    _camDisp->drawRectangle(payload.x, payload.y, payload.width, payload.height);
  }
}

void VizControllerImpl::ProcessVizCameraLineMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraLine();
  SetColorHelper(_camDisp, payload.color);
  _camDisp->drawLine((int)payload.xStart, (int)payload.yStart, (int)payload.xEnd, (int)payload.yEnd);
}

void VizControllerImpl::ProcessVizCameraOvalMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraOval();
  SetColorHelper(_camDisp, payload.color);
  _camDisp->drawOval((int)std::round(payload.xCen), (int)std::round(payload.yCen),
    (int)std::round(payload.xRad), (int)std::round(payload.yRad));
}

void VizControllerImpl::ProcessVizCameraTextMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraText();
  if (payload.text.size() > 0){
    // Drop shadow
    SetColorHelper(_camDisp, NamedColors::BLACK);
    _camDisp->drawText(payload.text, (int)payload.x+1, (int)payload.y+1);
    
    // Actual text
    SetColorHelper(_camDisp, payload.color);
    _camDisp->drawText(payload.text, (int)payload.x, (int)payload.y);
  }
}
  
static void DisplayImageHelper(const EncodedImage& encodedImage, webots::ImageRef* &imageRef, webots::Display* display)
{
  // Delete existing image if there is one
  if (imageRef != nullptr) {
    display->imageDelete(imageRef);
  }
  
  Vision::ImageRGB img;
  Result result = encodedImage.DecodeImageRGB(img);
  if(RESULT_OK != result) {
    PRINT_NAMED_WARNING("VizControllerImpl.DisplayImageHelper.DecodeFailed", "t=%d", (TimeStamp_t)encodedImage.GetTimeStamp());
    return;
  }
  
  if(img.IsEmpty()) {
    PRINT_NAMED_WARNING("VizControllerImpl.DisplayImageHelper.EmptyImageDecoded", "t=%d", (TimeStamp_t)encodedImage.GetTimeStamp());
    return;
  }
  
  if(img.GetNumCols() == display->getWidth() && img.GetNumRows() == display->getHeight())
  {
    // Simple case: image already the right size
    imageRef = display->imageNew(img.GetNumCols(), img.GetNumRows(), img.GetDataPointer(), webots::Display::RGB);
  }
  else
  {
    // Resize to fit the display
    // NOTE: making fixed-size data buffer static because resizedImage will change dims each time it's used. 
    static std::vector<u8> buffer(display->getWidth()*display->getHeight()*3);
    Vision::ImageRGB resizedImage(display->getHeight(), display->getWidth(), buffer.data());
    img.ResizeKeepAspectRatio(resizedImage, Vision::ResizeMethod::NearestNeighbor);
    imageRef = display->imageNew(resizedImage.GetNumCols(), resizedImage.GetNumRows(),
                                 resizedImage.GetDataPointer(), webots::Display::RGB);
  }
  
  display->imagePaste(imageRef, 0, 0);
}

void VizControllerImpl::ProcessVizImageChunkMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_ImageChunk();
  
  const s32 displayIndex = payload.displayIndex;
  
  if(displayIndex == 0)
  {
    // Display index 0 (camera feed) is special:
    // - If saving is enabled, we go ahead and save as soon as it is complete
    // - We don't display until we receive a DisplayImage message (see ProcessVizDisplayImageMessage())
    // - We do extra bookkeeping around the save counter so that we can also save the
    //   the visualized image (with any extra viz elements overlaid) when it is complete, and with
    //   a matching filename.
    EncodedImage& encodedImage = _bufferedImages[_imageBufferIndex];
    const bool isImageReady = encodedImage.AddChunk(payload);
    
    if(isImageReady)
    {
      DEV_ASSERT_MSG(payload.frameTimeStamp == encodedImage.GetTimeStamp(),
                     "VizControllerImpl.ProcessVizImageChunkMessage.TimestampMismath",
                     "Payload:%u Image:%u", payload.frameTimeStamp, (TimeStamp_t)encodedImage.GetTimeStamp());
      
      // Add an entry in EncodedImages map for this new image, now that it's complete
      auto result = _encodedImages.emplace(payload.frameTimeStamp, _imageBufferIndex);
      DEV_ASSERT_MSG(result.second, "VizControllerImpl.ProcessVizImageChunkMessage.DuplicateTimestamp",
                     "t=%u", payload.frameTimeStamp);
      DEV_ASSERT_MSG(result.first->second == _imageBufferIndex,
                     "VizControllerImpl.ProcessVizImageChunkMessage.BadInsertion",
                     "Expected index:%zu Got:%zu", _imageBufferIndex, result.first->second);
#     pragma unused(result) // Avoid unused variable error in Release (only used in DEV_ASSERTs)
      
      // Move to next buffered index circularly
      ++_imageBufferIndex;
      if(_imageBufferIndex == _bufferedImages.size())
      {
        _imageBufferIndex = 0;
      }
      
      // Invalidate anything in encodedImages using the index we are about to start adding chunks to (not the one we
      // just completed; i.e. encodedImage != _bufferedImages[_imageBufferIndex] now because we incremented the index!)
      _encodedImages.erase(_bufferedImages[_imageBufferIndex].GetTimeStamp());
      
      const bool saveImage = (_saveImageMode != ImageSendMode::Off);
      
      // Store the mapping for its timestamp to save counter so we can keep saved "viz" images' counters and filenames
      // in sync with these raw images files.
      // Have to do this anytime saveVizImage is enabled (which it could be even while _saveImageMode is Off, thanks
      // to the vision system potentially processing images more slowly than full frame rate) or when it is about to
      // be enabled (when saveImage is true)
      if(saveImage || _saveVizImage)
      {
        _bufferedSaveCtrs[encodedImage.GetTimeStamp()] = _saveCtr;
      }
      
      if(saveImage)
      {
        // Save original image
        std::stringstream origFilename;
        origFilename << "images_" << encodedImage.GetTimeStamp() << "_" << _saveCtr << ".jpg";
        encodedImage.Save(Util::FileUtils::FullFilePath({_savedImagesFolder, origFilename.str()}));
        _saveVizImage = true;
        ++_saveCtr;
        
        if(_saveImageMode == ImageSendMode::SingleShot) {
          _saveImageMode = ImageSendMode::Off;
        }
      }
      
      DisplayBufferedCameraImage(encodedImage.GetTimeStamp());
    }
  }
  else
  {
    // For non-camera (debug) images, just display (and save) immediately. No need to wait for any additional
    // "viz" overlay to be added. Note: debug images are only saved in "Stream" mode (not "SingleShot")
    if(displayIndex < 1 || displayIndex > _debugImages.size())
    {
      PRINT_NAMED_WARNING("VizControllerImpl.ProcessVizImageChunkMessage.InvalidDisplayIndex",
                          "No debug display for index=%d", displayIndex);
    }
    else
    {
      DebugImage& debugImage = _debugImages.at(displayIndex-1);
      const bool isImageReady = debugImage.encodedImage.AddChunk(payload);
      
      if(isImageReady)
      {
        if(ImageSendMode::Stream == _saveImageMode)
        {
          std::stringstream debugFilename;
          debugFilename << "debug" << displayIndex << "_" << debugImage.encodedImage.GetTimeStamp() << ".jpg";
          debugImage.encodedImage.Save(Util::FileUtils::FullFilePath({_savedImagesFolder, debugFilename.str()}));
        }
        
        DisplayImageHelper(debugImage.encodedImage, debugImage.imageRef, debugImage.imageDisplay);
      }
    }
  }
  
}
  
void VizControllerImpl::DisplayBufferedCameraImage(const RobotTimeStamp_t timestamp)
{
  auto encImgIter = _encodedImages.find(timestamp);
  if(encImgIter == _encodedImages.end())
  {
    return;
  }
  
  const EncodedImage& encodedImage = _bufferedImages[encImgIter->second];
  DEV_ASSERT_MSG(timestamp == encodedImage.GetTimeStamp(),
                 "VizControllerImpl.ProcessVizDisplayImage.TimeStampMisMatch",
                 "key=%u vs. encImg=%u", (TimeStamp_t)timestamp, (TimeStamp_t)encodedImage.GetTimeStamp());
  
  if(_saveVizImage && _curImageTimestamp > 0)
  {
    if (!_savedImagesFolder.empty() && !Util::FileUtils::CreateDirectory(_savedImagesFolder, false, true)) {
      PRINT_NAMED_WARNING("VizControllerImpl.CreateDirectory", "Could not create images directory");
    }
    
    auto saveCtrIter = _bufferedSaveCtrs.find(_curImageTimestamp);
    if(saveCtrIter != _bufferedSaveCtrs.end())
    {
      // Save previous image with any viz overlaid before we delete it
      webots::ImageRef* copyImg = _camDisp->imageCopy(0, 0, _camDisp->getWidth(), _camDisp->getHeight());
      std::stringstream vizFilename;
      vizFilename << "viz_images_" << _curImageTimestamp << "_" << saveCtrIter->second << ".png";
      _camDisp->imageSave(copyImg, Util::FileUtils::FullFilePath({_savedImagesFolder, vizFilename.str()}));
      _camDisp->imageDelete(copyImg);
      _saveVizImage = false;
      
      // Remove all saved counters up to and including timestamp we just saved (the assumption is we never
      // go backward, so once we've saved this one, we don't need it or anything that came before it)
      _bufferedSaveCtrs.erase(_bufferedSaveCtrs.begin(), ++saveCtrIter);
      
    }
  }

  DisplayImageHelper(encodedImage, _camImg, _camDisp);
 
  // Store the timestamp for the currently displayed image so we can use it to save
  // that image with the right filename next call
  _curImageTimestamp = timestamp;
  
  DisplayCameraInfo(timestamp);
  
  // Remove all encoded images up to and including the specified timestamp (the assumption is we never
  // go backward, so once we've displayed this one, we don't need it or anything that came before it)
  _encodedImages.erase(_encodedImages.begin(), ++encImgIter);
}

void VizControllerImpl::ProcessCameraParams(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CameraParams();
  _cameraParams = payload.cameraParams;
}

void VizControllerImpl::DisplayCameraInfo(const RobotTimeStamp_t timestamp)
{
  // Print values
  char text[42];
  snprintf(text, sizeof(text), "Exp:%u Gain:%.3f\n", 
           _cameraParams.exposureTime_ms, _cameraParams.gain);
  SetColorHelper(_camDisp, NamedColors::RED);
  _camDisp->drawText(std::to_string((TimeStamp_t)timestamp), 1, _camDisp->getHeight()-9); // display timestamp at lower left
  _camDisp->drawText(text, _camDisp->getWidth()-144, _camDisp->getHeight()-9); //display exposure in bottom right


  snprintf(text, sizeof(text), "AWB:%.3f %.3f %.3f\n", 
           _cameraParams.whiteBalanceGainR, 
           _cameraParams.whiteBalanceGainG, 
           _cameraParams.whiteBalanceGainB);
  SetColorHelper(_camDisp, NamedColors::RED);
  _camDisp->drawText(text, _camDisp->getWidth()-180, _camDisp->getHeight()-18);
}


void VizControllerImpl::ProcessVizTrackerQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_TrackerQuad();
  _camDisp->setColor(0x0000ff);
  _camDisp->drawLine((int)payload.topLeft_x, (int)payload.topLeft_y, (int)payload.topRight_x, (int)payload.topRight_y);
  _camDisp->setColor(0x00ff00);
  _camDisp->drawLine((int)payload.topRight_x, (int)payload.topRight_y, (int)payload.bottomRight_x, (int)payload.bottomRight_y);
  _camDisp->drawLine((int)payload.bottomRight_x, (int)payload.bottomRight_y, (int)payload.bottomLeft_x, (int)payload.bottomLeft_y);
  _camDisp->drawLine((int)payload.bottomLeft_x, (int)payload.bottomLeft_y, (int)payload.topLeft_x, (int)payload.topLeft_y);
}
  
void VizControllerImpl::ProcessVizRobotStateMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_RobotStateMessage();
  char txt[128];

  sprintf(txt, "Pose: %6.1f, %6.1f, ang: %4.1f  [fid: %u, oid: %u]",
    payload.state.pose.x,
    payload.state.pose.y,
    RAD_TO_DEG(payload.state.pose.angle),
    payload.state.pose_frame_id,
    payload.state.pose_origin_id);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_POSE, Anki::NamedColors::GREEN, txt);

  sprintf(txt, "Head: %5.1f deg, Lift: %4.1f mm",
    RAD_TO_DEG(payload.state.headAngle),
    ConvertLiftAngleToLiftHeightMM(payload.state.liftAngle));
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_HEAD_LIFT, Anki::NamedColors::GREEN, txt);

  sprintf(txt, "Pitch: %4.1f deg (IMUHead: %4.1f deg)",
    RAD_TO_DEG(payload.state.pose.pitch_angle),
    RAD_TO_DEG(payload.state.pose.pitch_angle + payload.state.headAngle));
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_PITCH, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "Roll: %4.1f deg",
          RAD_TO_DEG(payload.state.pose.roll_angle));
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_ROLL, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "Acc:  %6.0f %6.0f %6.0f mm/s2  ImuTemp %+6.2f degC",
          payload.state.accel.x,
          payload.state.accel.y,
          payload.state.accel.z,
          payload.imuTemperature_degC);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_ACCEL, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "Gyro: %6.1f %6.1f %6.1f deg/s",
    RAD_TO_DEG(payload.state.gyro.x),
    RAD_TO_DEG(payload.state.gyro.y),
    RAD_TO_DEG(payload.state.gyro.z));
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_GYRO, Anki::NamedColors::GREEN, txt);

  bool cliffDetected = payload.state.cliffDetectedFlags > 0;
  sprintf(txt, "Cliff: {%4u, %4u, %4u, %4u} thresh: {%4u, %4u, %4u, %4u}",
          payload.state.cliffDataRaw[0],
          payload.state.cliffDataRaw[1],
          payload.state.cliffDataRaw[2],
          payload.state.cliffDataRaw[3],
          payload.cliffThresholds[0],
          payload.cliffThresholds[1],
          payload.cliffThresholds[2],
          payload.cliffThresholds[3]);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_CLIFF, cliffDetected ? Anki::NamedColors::RED : Anki::NamedColors::GREEN, txt);

  const auto& proxData = payload.state.proxData;
  sprintf(txt, "Dist: %4u mm, sigStrength: %5.3f, ambient: %5.3f status %s",
          proxData.distance_mm,
          proxData.signalIntensity / proxData.spadCount,
          100.f * proxData.ambientIntensity / proxData.spadCount,
          RangeStatusToString(proxData.rangeStatus));
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_DIST, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "Speed L: %4d  R: %4d mm/s",
    (int)payload.state.lwheel_speed_mmps,
    (int)payload.state.rwheel_speed_mmps);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_SPEEDS, Anki::NamedColors::GREEN, txt);

  const auto currTreadState = payload.offTreadsState;
  const auto nextTreadState = payload.awaitingConfirmationTreadState;
  const bool onTreads = (currTreadState == OffTreadsState::OnTreads);
  sprintf(txt, "OffTreadsState: %s  %s",
          EnumToString(currTreadState),
          (currTreadState != nextTreadState) ? EnumToString(nextTreadState) : "");
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_OFF_TREADS_STATE, onTreads ? Anki::NamedColors::GREEN : Anki::NamedColors::RED, txt);
  
  sprintf(txt, "Touch: %u", 
    payload.state.backpackTouchSensorRaw
  );
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_TOUCH, Anki::NamedColors::GREEN, txt);

  sprintf(txt, "Batt: %2.2fV, %2uC [%c%c]", 
    payload.batteryVolts,
    payload.state.battTemp_C,
    payload.state.status & (uint32_t)RobotStatusFlag::IS_BATTERY_OVERHEATED ? 'H' : ' ',
    payload.state.status & (uint32_t)RobotStatusFlag::IS_BATTERY_DISCONNECTED ? 'D' : ' ');
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_BATTERY, Anki::NamedColors::GREEN, txt);

  sprintf(txt, "Locked: %c%c%c, InUse: %c%c%c",
        (payload.lockedAnimTracks & (u8)AnimTrackFlag::LIFT_TRACK) ? 'L' : ' ',
        (payload.lockedAnimTracks & (u8)AnimTrackFlag::HEAD_TRACK) ? 'H' : ' ',
        (payload.lockedAnimTracks & (u8)AnimTrackFlag::BODY_TRACK) ? 'B' : ' ',
        (payload.animTracksInUse  & (u8)AnimTrackFlag::LIFT_TRACK) ? 'L' : ' ',
        (payload.animTracksInUse  & (u8)AnimTrackFlag::HEAD_TRACK) ? 'H' : ' ',
        (payload.animTracksInUse  & (u8)AnimTrackFlag::BODY_TRACK) ? 'B' : ' ');
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_ANIM_TRACK_LOCKS, Anki::NamedColors::GREEN, txt);


  sprintf(txt, "Video: %.1f Hz   Proc: %.1f Hz",
    1000.f / (f32)payload.videoFramePeriodMs, 1000.f / (f32)payload.imageProcPeriodMs);
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_VID_RATE, Anki::NamedColors::GREEN, txt);

  sprintf(txt, "Status: %5s %5s %6s %4s %4s",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_CARRYING_BLOCK ? "CARRY" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_PICKING_OR_PLACING ? "PAP" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_PICKED_UP ? "PICKUP" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_BEING_HELD ? "HELD" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_FALLING ? "FALL" : "");
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_STATUS_FLAG, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "   %8s %10s %7s %4s",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_CHARGING ? "CHARGING" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_ON_CHARGER ? "ON_CHARGER" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_BUTTON_PRESSED ? "PWR_BTN" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::CALM_POWER_MODE ? "CALM" : "");
  
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_STATUS_FLAG_2, Anki::NamedColors::GREEN, txt);
  
  sprintf(txt, "   %4s %7s %7s %6s",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_PATHING ? "PATH" : "",
    payload.state.status & (uint32_t)RobotStatusFlag::LIFT_IN_POS ? "" : "LIFTING",
    payload.state.status & (uint32_t)RobotStatusFlag::HEAD_IN_POS ? "" : "HEADING",
    payload.state.status & (uint32_t)RobotStatusFlag::IS_MOVING ? "MOVING" : "");
  DrawText(_disp, (u32)VizTextLabelType::TEXT_LABEL_STATUS_FLAG_3, Anki::NamedColors::GREEN, txt);
    
  // Save state to file
  if(_saveState)
  {
    const size_t kMaxPayloadSize = 256;
    if(payload.Size() > kMaxPayloadSize) {
      PRINT_NAMED_WARNING("VizController.ProcessVizRobotStateMessage.PayloadSizeTooLarge",
                          "%zu > %zu", payload.Size(), kMaxPayloadSize);
    } else {
      // Compose line for entire state msg in hex
      char stateMsgLine[2*kMaxPayloadSize + 1];
      memset(stateMsgLine,0,kMaxPayloadSize);
      u8 msgBytes[kMaxPayloadSize];
      payload.Pack(msgBytes, kMaxPayloadSize);
      for (int i=0; i < payload.Size(); i++){
        sprintf(&stateMsgLine[2*i], "%02x", (unsigned char)msgBytes[i]);
      }
      sprintf(&stateMsgLine[payload.Size() * 2],"\n");
      
      FILE *stateFile;
      stateFile = fopen("RobotState.txt", "at");
      fputs(stateMsgLine, stateFile);
      fclose(stateFile);
    }
  } // if(_saveState)
}

void VizControllerImpl::ProcessVizCurrentAnimation(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_CurrentAnimation();
  _currAnimName = payload.animName;
  _currAnimTag = payload.tag;
}


void VizControllerImpl::ProcessBehaviorStackDebug(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  if( _bsmStackDisp == nullptr ) {
    return;
  }

  // Clear the space
  _bsmStackDisp->setColor(0x0);
  _bsmStackDisp->fillRectangle(0, 0, _bsmStackDisp->getWidth(), _bsmStackDisp->getHeight());
  
  const VizInterface::BehaviorStackDebug& debugData = msg.GetData().Get_BehaviorStackDebug();

  for( size_t i=0; i < debugData.debugStrings.size(); ++i ) {
    DrawText(_bsmStackDisp, (u32)i, (u32)Anki::NamedColors::WHITE, debugData.debugStrings[i].c_str());
  }
}

void VizControllerImpl::ProcessVisionModeDebug(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  if( _visionModeDisp == nullptr ) {
    return;
  }

  // Clear the space
  _visionModeDisp->setColor(0x0);
  _visionModeDisp->fillRectangle(0, 0, _visionModeDisp->getWidth(), _visionModeDisp->getHeight());

  const VizInterface::VisionModeDebug& debugData = msg.GetData().Get_VisionModeDebug();

  DrawText(_visionModeDisp, 0, (u32)Anki::NamedColors::WHITE, "Vision Schedule:       Mode:");
  for( size_t i=0; i < debugData.debugStrings.size(); ++i ) {
    // Only show full-blown vision modes, not modifiers (which just piggy back on their Modes' schedules)
    // The convention is that modifiers have an underscore in their name
    if(debugData.debugStrings[i].find("_") == std::string::npos) {
      DrawText(_visionModeDisp, (u32)(i+1), (u32)Anki::NamedColors::GREEN, debugData.debugStrings[i].c_str());
    }
  }

}

static inline void SetColorForMode(const std::vector<VisionMode>& modes, const VisionMode mode, webots::Display* disp)
{
  // If this mode was processed then draw it in white
  if(std::find(modes.begin(), modes.end(), mode) != modes.end())
  {
    disp->setColor(NamedColors::WHITE.As0RGB());
  }
  // Otherwise draw it in gray
  else
  {
    disp->setColor(NamedColors::DARKGRAY.As0RGB());
  }
}
  
static inline void DrawTextHelper(const u32 x, const u32 y, const std::string& str, webots::Display* disp)
{
  if( (x >= disp->getWidth()) || (y >= disp->getHeight()) )
  {
    LOG_WARNING("VizControllerImpl.DrawTextHelper.StringOOB", "'%s': (x,y)=(%d,%d)", str.c_str(), x, y);
  }
  disp->drawText(str, x, y);
}
  
void VizControllerImpl::ProcessEnabledVisionModes(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  if( _disp == nullptr ) {
    return;
  }

  const auto& data = msg.GetData().Get_EnabledVisionModes();

  const u32 kTextWidth = 15;
  const u32 kNumModesPerLine = 4;
  const u32 kCharWidth = 6;
  const u32 kLineHeight = 10;

  _disp->setColor(NamedColors::BLACK.As0RGB());
  const u32 fillY = ((uint32_t)VizTextLabelType::NUM_TEXT_LABELS + (uint32_t)TextLabelType::VISION_MODE + 1)*kLineHeight;
  _disp->fillRectangle(0, fillY, _disp->getWidth(), _disp->getHeight()-fillY);

  // Insert a little divider
  _disp->setColor(NamedColors::DARKGRAY.As0RGB());
  _disp->drawLine(0, fillY-1, _disp->getWidth(), fillY-1);
  
  // x,y position to draw each VisionMode at in the display
  u32 x = 0;
  u32 y = fillY;

  // organize into modes with and without modifiers (one time only)
  static std::map<VisionMode, std::list<std::pair<VisionMode, std::string>>> kModesMap;
  if(kModesMap.empty())
  {
    for(VisionMode m = VisionMode(0); m < VisionMode::Count; m++)
    {
      std::string str(EnumToString(m));
      const auto underscorePos = str.find("_");
      const bool isModifier = (underscorePos != std::string::npos);
      if(isModifier)
      {
        const VisionMode mode = VisionModeFromString(str.substr(0,underscorePos));
        const size_t kMaxModStrLen = 8;
        const std::string modifierStr = str.substr(underscorePos+1, std::min(str.size()-underscorePos, kMaxModStrLen));
        kModesMap[mode].emplace_back(m, modifierStr);
      }
      else
      {
        kModesMap[m]; // Insert empty entry
      }
    }
  }
  
  // Loop over all the modes and draw those _without_ modifiers first, in columns
  u32 index = 0;
  for(auto const& entry : kModesMap)
  {
    if(!entry.second.empty())
    {
      continue;
    }
    
    const VisionMode m = entry.first;
    
    // Left align text with kTextWidth+1 padding of spaces (+1 for space between modes)
    std::stringstream ss;
    ss << std::setw(kTextWidth + 1) << std::left;
    std::string s(EnumToString(m));
    ss << s.substr(0, kTextWidth);
    
    SetColorForMode(data.modes, m, _disp);
    DrawTextHelper(x, y, ss.str(), _disp);

    // Increase x by VisionMode text length + 1 (for spacing)
    x += kCharWidth*(kTextWidth+1);

    // Only draw kNumModesPerLine
    if((index+1) % kNumModesPerLine == 0)
    {
      x = 0;
      y += kLineHeight;
    }
    
    ++index;
  }
  
  // Second loop draws those _with_ modifiers, one mode per line, modifiers grouped into [] after
  x = 0;
  y += kLineHeight+1;
  
  // Insert a little divider between vision modes with and without modifiers
  _disp->setColor(NamedColors::DARKGRAY.As0RGB());
  _disp->drawLine(0, y-1, _disp->getWidth(), y-1);
  
  for(auto const& entry : kModesMap)
  {
    auto const& modifiers = entry.second;
    if(modifiers.empty())
    {
      continue;
    }
    
    // If this mode was processed then draw it in white
    const VisionMode m = entry.first;
    std::string str(EnumToString(m));
    str += "[";
    SetColorForMode(data.modes, m, _disp);
    DrawTextHelper(x, y, str, _disp);
    x += kCharWidth*(str.size());
    
    // Now loop over the modifiers of this mode
    for(const auto& modifier : modifiers)
    {
      SetColorForMode(data.modes, modifier.first, _disp);
      DrawTextHelper(x, y, modifier.second, _disp);
      x += kCharWidth*(modifier.second.size()+1); // +1 for space
    }
    
    SetColorForMode(data.modes, m, _disp);
    DrawTextHelper(x-kCharWidth, y, "]", _disp); // -kCharWidth for trailing space
    
    // Modes with modifiers each get their own line
    y += kLineHeight;
    x = 0;
  }
}
  
void VizControllerImpl::ProcessVizSetOriginMessage(const AnkiEvent<VizInterface::MessageViz> &msg)
{
  const auto& m = msg.GetData().Get_SetVizOrigin();
  
  _vizControllerPose = Pose3d(m.rot_rad,
                              Vec3f(m.rot_axis_x, m.rot_axis_y, m.rot_axis_z),
                              Vec3f(MM_TO_M(m.trans_x_mm), MM_TO_M(m.trans_y_mm), MM_TO_M(m.trans_z_mm)));

  WebotsHelpers::SetNodePose(*_vizSupervisor.getSelf(), _vizControllerPose);
}
  
void VizControllerImpl::ProcessVizMemoryMapMessageBegin(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  _navMapNodes.clear();
  _navMapNodes.reserve(1024); // reserve some memory to avoid re-allocations
}

void VizControllerImpl::ProcessVizMemoryMapMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_MemoryMapMessageViz();
  _navMapNodes.insert(_navMapNodes.end(),
                      payload.quadInfos.begin(),
                      payload.quadInfos.end());
}

void VizControllerImpl::ProcessVizMemoryMapMessageEnd(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  // Render the quad tree
  
  const auto displayWidth = _navMapDisp->getWidth();
  const auto displayHeight = _navMapDisp->getHeight();
  _navMapDisp->setOpacity(1.0);
  
  // Clear display
  _navMapDisp->setAlpha(0.0);
  _navMapDisp->setColor(0);
  _navMapDisp->fillRectangle(0, 0, displayWidth, displayHeight);
  
  // Store the pixel coordinates of the center of the image (for later conversion from x/y to image coordinates)
  const auto displayCenterX = 0.5 * displayWidth;
  const auto displayCenterY = 0.5 * displayHeight;
  
  // Draw each node
  for (const auto& node : _navMapNodes) {
    const auto rgba = node.colorRGBA;
    const int webotsColor = (rgba>>8); // convert RGBA to RGB
    const float webotsAlpha = (rgba & 0xFF) / 255.f; // convert alpha to 0.0 to 1.0
    _navMapDisp->setAlpha(webotsAlpha);
    _navMapDisp->setColor(webotsColor);
    
    // Webots requires the x,y position of the rectangle to be the top left corner, not the center.
    const auto topLeftCornerX = node.centerX_mm - node.edgeLen_mm/2.f;
    const auto topLeftCornerY = node.centerY_mm + node.edgeLen_mm/2.f;
    
    // Convert x,y (with origin in the center of the image) to image coordinates (top left of image is origin)
    auto imageX =  topLeftCornerX + displayCenterX;
    auto imageY = -topLeftCornerY + displayCenterY;
    
    // We subtract 1 from the width/height to leave a 'space' between nodes, which allows us to see the individual
    // quads even if they are the same color.
    int width = node.edgeLen_mm - 1;
    int height = node.edgeLen_mm - 1;
    
    // If the quad would be off the display plane, we still want to draw as much of it as we can
    if (imageX < 0) {
      width -= std::abs(imageX);
      imageX = 0;
    }
    if (imageY < 0) {
      height -= std::abs(imageY);
      imageY = 0;
    }
    
    const bool shouldDraw = (height > 0) && (width > 0);
    
    if (shouldDraw) {
      _navMapDisp->fillRectangle(imageX, imageY, width, height);
    }
  }

}
  
void VizControllerImpl::ProcessVizObjectMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_Object();
  auto& mapEntry = _vizObjects[payload.objectID];
  mapEntry.data = payload;
}

void VizControllerImpl::ProcessVizEraseObjectMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_EraseObject();
  
  uint32_t lowerBoundId = payload.objectID;
  uint32_t upperBoundId = payload.objectID;
  
  if (payload.objectID == (uint32_t)VizConstants::ALL_OBJECT_IDs) {
    lowerBoundId = 0;
    upperBoundId = std::numeric_limits<decltype(upperBoundId)>::max();
  } else if (payload.objectID == (uint32_t)VizConstants::OBJECT_ID_RANGE) {
    lowerBoundId = payload.lower_bound_id;
    upperBoundId = payload.upper_bound_id;
  }
  
  EraseVizObjects(lowerBoundId, upperBoundId);
}
  
void VizControllerImpl::ProcessVizShowObjectsMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_ShowObjects();
  _showObjects = (payload.show != 0);
  
  // Clear all objects if necessary
  if (!_showObjects) {
    EraseVizObjects();
  }
}

void VizControllerImpl::ProcessVizLineSegmentMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_LineSegment();
  
  if (payload.clearPrevious) {
    EraseVizSegments(payload.identifier);
  }
  
  auto& vizSegment = _vizSegments[payload.identifier];
  vizSegment.emplace_back();
  vizSegment.back().data = payload;
}

void VizControllerImpl::ProcessVizEraseLineSegmentsMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_EraseLineSegments();
  EraseVizSegments(payload.identifier);
}
  
void VizControllerImpl::ProcessVizQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_Quad();
  
  auto& vizQuad = _vizQuads[payload.quadType][payload.quadID];
  vizQuad.data = payload;
}

void VizControllerImpl::ProcessVizEraseQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_EraseQuad();
  EraseVizQuads((VizQuadType) payload.quadType, payload.quadID);
}
  
void VizControllerImpl::ProcessVizAppendPathSegmentLineMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_AppendPathSegmentLine();
  
  auto& pathInfo = _vizPaths[payload.pathID];
  pathInfo.lines.emplace_back();
  pathInfo.lines.back().data = payload;
}

void VizControllerImpl::ProcessVizAppendPathSegmentArcMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_AppendPathSegmentArc();

  auto& pathInfo = _vizPaths[payload.pathID];
  pathInfo.arcs.emplace_back();
  pathInfo.arcs.back().data = payload;
}
  
void VizControllerImpl::ProcessVizSetPathColorMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_SetPathColor();
  
  auto it = _vizPaths.find(payload.pathID);
  if (it != _vizPaths.end()) {
    it->second.color = payload.colorID;
  }
}
  
void VizControllerImpl::ProcessVizErasePathMessage(const AnkiEvent<VizInterface::MessageViz>& msg)
{
  const auto& payload = msg.GetData().Get_ErasePath();
  EraseVizPath(payload.pathID);
}
  
void VizControllerImpl::EraseVizObjects(const uint32_t lowerBoundId, const uint32_t upperBoundId)
{
  // Get lower bound iterator
  auto lowerIt = _vizObjects.lower_bound(lowerBoundId);
  if (lowerIt == _vizObjects.end()) {
    return;
  }
  
  // Get upper bound iterator
  auto upperIt = _vizObjects.upper_bound(upperBoundId);
  
  // Erase objects in bounds (but first remove them from the scene tree if necessary)
  std::for_each(lowerIt, upperIt,
                [this](const std::pair<uint32_t, VizObjectInfo>& pair){
                  auto nodeID = pair.second.webotsNodeId;
                  if (nodeID >= 0) {
                    _vizSupervisor.getFromId(nodeID)->remove();
                  }
                });
  _vizObjects.erase(lowerIt, upperIt);
}

void VizControllerImpl::EraseVizSegments(const std::string& identifier)
{
  auto segmentIt = _vizSegments.find(identifier);
  if (segmentIt != _vizSegments.end()) {
    for (auto& segment : segmentIt->second) {
      auto nodeId = segment.webotsNodeId;
      if (nodeId >= 0) {
        _vizSupervisor.getFromId(nodeId)->remove();
      }
    }
    
    _vizSegments.erase(segmentIt);
  }
}
  
void VizControllerImpl::EraseVizQuads(const VizQuadType quadType, const uint32_t quadId)
{
  auto typeIt = _vizQuads.find(quadType);
  if (typeIt != _vizQuads.end()) {
    auto& quadsWithPayloadType = typeIt->second;
    auto quadIt = quadsWithPayloadType.find(quadId);
    if (quadIt != quadsWithPayloadType.end()) {
      auto nodeId = quadIt->second.webotsNodeId;
      if (nodeId >= 0) {
        _vizSupervisor.getFromId(nodeId)->remove();
      }
      quadsWithPayloadType.erase(quadIt);
    }
    
    if (quadsWithPayloadType.empty()) {
      _vizQuads.erase(typeIt);
    }
  }
}

void VizControllerImpl::EraseVizPath(const uint32_t pathId)
{
  auto it = _vizPaths.find(pathId);
  if (it != _vizPaths.end()) {
    for (auto& line : it->second.lines) {
      if (line.webotsNodeId >= 0) {
        _vizSupervisor.getFromId(line.webotsNodeId)->remove();
      }
    }
    for (auto& arc : it->second.arcs) {
      if (arc.webotsNodeId >= 0) {
        _vizSupervisor.getFromId(arc.webotsNodeId)->remove();
      }
    }
    
    _vizPaths.erase(it);
  }
}

void VizControllerImpl::Draw()
{
  const bool shouldDraw = (_drawingObjectsEnabled && _showObjects);
  if (!shouldDraw) {
    return;
  }
  
  DrawObjects();
  DrawLineSegments();
  DrawQuads();
  DrawPaths();
}

void VizControllerImpl::DrawObjects()
{
  using namespace Util::FullEnumToValueArrayChecker;
  constexpr static const FullEnumToValueArray<VizObjectType, const char*, VizObjectType::NUM_VIZ_OBJECT_TYPES> kVizObjectTypeToProtoString {
    {VizObjectType::VIZ_OBJECT_ROBOT,       "PoseMarker {}"},
    {VizObjectType::VIZ_OBJECT_CUBOID,      "WireframeCuboid {}"},
    {VizObjectType::VIZ_OBJECT_CHARGER,     "WireframeCharger {}"},
    {VizObjectType::VIZ_OBJECT_PREDOCKPOSE, "PoseMarker {}"},
    {VizObjectType::VIZ_OBJECT_HUMAN_HEAD,  "HumanHead {}"},
    {VizObjectType::VIZ_OBJECT_TEXT,        "Text {}"},
  };
  
  static_assert( IsSequentialArray(kVizObjectTypeToProtoString),
                "kVizObjectTypeToProtoString array does not define each entry in order, once and only once!");
  
  for (auto& obj : _vizObjects) {
    auto& vizObjectInfo = obj.second;
    const auto& objectType = vizObjectInfo.data.objectTypeID;
    
    // Add a new object to the scene tree if it doesn't exist already
    if (vizObjectInfo.webotsNodeId < 0) {
      const auto& protoStr = kVizObjectTypeToProtoString[Util::EnumToUnderlying(objectType)].Value();
      vizObjectInfo.webotsNodeId = WebotsHelpers::AddSceneTreeNode(_vizSupervisor, protoStr);
    }
    
    // If we don't have a webots node ID at this point, then this is not a drawable object, so just skip it.
    if (vizObjectInfo.webotsNodeId < 0) {
      continue;
    }
    
    auto* nodePtr = _vizSupervisor.getFromId(vizObjectInfo.webotsNodeId);
    const auto& d = vizObjectInfo.data;
    
    // Set translation/rotation/color
    Pose3d pose(DEG_TO_RAD(d.rot_deg),
                Vec3f(d.rot_axis_x, d.rot_axis_y, d.rot_axis_z),
                Vec3f(d.x_trans_m, d.y_trans_m, d.z_trans_m));
    pose.PreComposeWith(_vizControllerPose);
    
    WebotsHelpers::SetNodePose(*nodePtr, pose);
    WebotsHelpers::SetNodeColor(*nodePtr, d.color);
    
    SetNodeVisibility(nodePtr);
    
    // Apply object-specific parameters (if any)
    switch (objectType) {
      case VizObjectType::VIZ_OBJECT_ROBOT:
        // Draw the robot pose marker a bit above the actual position
        nodePtr->getField("zOffset")->setSFFloat(0.080);
        break;
      case VizObjectType::VIZ_OBJECT_CUBOID:
        nodePtr->getField("xSize")->setSFFloat(d.x_size_m);
        nodePtr->getField("ySize")->setSFFloat(d.y_size_m);
        nodePtr->getField("zSize")->setSFFloat(d.z_size_m);
        break;
      case VizObjectType::VIZ_OBJECT_CHARGER:
        nodePtr->getField("platformLength")->setSFFloat(d.x_size_m);
        nodePtr->getField("slopeLength")->setSFFloat(d.objParameters[0] * d.x_size_m);
        nodePtr->getField("width")->setSFFloat(d.y_size_m);
        nodePtr->getField("height")->setSFFloat(d.z_size_m);
        break;
      case VizObjectType::VIZ_OBJECT_PREDOCKPOSE:
        // Draw the pre-dock pose a bit above the actual position
        nodePtr->getField("zOffset")->setSFFloat(0.080);
        break;
      default:
        break;
    }
  }
}
  
void VizControllerImpl::DrawLineSegments()
{
  for (auto& segmentInfo : _vizSegments) {
    for (auto& segment : segmentInfo.second) {
      // Add a new object to the scene tree if it doesn't exist already
      if (segment.webotsNodeId < 0) {
        segment.webotsNodeId = WebotsHelpers::AddSceneTreeNode(_vizSupervisor, "LineSegment {}");
      }
      auto* nodePtr = _vizSupervisor.getFromId(segment.webotsNodeId);
      
      SetNodeVisibility(nodePtr);
      
      WebotsHelpers::SetNodePose(*nodePtr, _vizControllerPose);
      WebotsHelpers::SetNodeColor(*nodePtr, segment.data.color);
      
      double origin[3] = {segment.data.origin[0], segment.data.origin[1], segment.data.origin[2]};
      nodePtr->getField("origin")->setSFVec3f(origin);
      
      double dest[3] = {segment.data.dest[0], segment.data.dest[1], segment.data.dest[2]};
      nodePtr->getField("dest")->setSFVec3f(dest);
    }
  }
}

void VizControllerImpl::DrawQuads()
{
  for (auto& quadTypeMap : _vizQuads) {
    for (auto& quad : quadTypeMap.second) {
      auto& quadInfo = quad.second;
      const auto& data = quadInfo.data;
      
      // Add a new object to the scene tree if it doesn't exist already
      if (quadInfo.webotsNodeId < 0) {
        quadInfo.webotsNodeId = WebotsHelpers::AddSceneTreeNode(_vizSupervisor, "WireframeQuad {}");
      }
      
      auto* nodePtr = _vizSupervisor.getFromId(quadInfo.webotsNodeId);
      
      SetNodeVisibility(nodePtr);
      
      WebotsHelpers::SetNodePose(*nodePtr, _vizControllerPose);
      WebotsHelpers::SetNodeColor(*nodePtr, data.color);
      
      double upperLeft[3] = {data.xUpperLeft, data.yUpperLeft, data.zUpperLeft};
      nodePtr->getField("upperLeft")->setSFVec3f(upperLeft);
      
      double lowerLeft[3] = {data.xLowerLeft, data.yLowerLeft, data.zLowerLeft};
      nodePtr->getField("lowerLeft")->setSFVec3f(lowerLeft);
      
      double lowerRight[3] = {data.xLowerRight, data.yLowerRight, data.zLowerRight};
      nodePtr->getField("lowerRight")->setSFVec3f(lowerRight);
      
      double upperRight[3] = {data.xUpperRight, data.yUpperRight, data.zUpperRight};
      nodePtr->getField("upperRight")->setSFVec3f(upperRight);      
    }
  }
}
  
void VizControllerImpl::DrawPaths()
{
  for (auto& pathInfo : _vizPaths) {
    
    // Draw lines
    for (auto& line: pathInfo.second.lines) {
      auto& data = line.data;
      
      // Add a new object to the scene tree if it doesn't exist already
      if (line.webotsNodeId < 0) {
        line.webotsNodeId = WebotsHelpers::AddSceneTreeNode(_vizSupervisor, "LineSegment {}");
      }
      auto* nodePtr = _vizSupervisor.getFromId(line.webotsNodeId);
      
      SetNodeVisibility(nodePtr);
      
      WebotsHelpers::SetNodePose(*nodePtr, _vizControllerPose);
      WebotsHelpers::SetNodeColor(*nodePtr, pathInfo.second.color);
      
      double origin[3] = {data.x_start_m, data.y_start_m, data.z_start_m};
      nodePtr->getField("origin")->setSFVec3f(origin);
      
      double dest[3] = {data.x_end_m, data.y_end_m, data.z_end_m};
      nodePtr->getField("dest")->setSFVec3f(dest);
    }
    
    // Draw arcs
    for (auto& arc: pathInfo.second.arcs) {
      auto& data = arc.data;
      
      // Add a new object to the scene tree if it doesn't exist already
      if (arc.webotsNodeId < 0) {
        arc.webotsNodeId = WebotsHelpers::AddSceneTreeNode(_vizSupervisor, "CircularArc {}");
      }
      auto* nodePtr = _vizSupervisor.getFromId(arc.webotsNodeId);
      
      SetNodeVisibility(nodePtr);
      
      WebotsHelpers::SetNodePose(*nodePtr, _vizControllerPose);
      WebotsHelpers::SetNodeColor(*nodePtr, pathInfo.second.color);
      
      nodePtr->getField("xOffset")->setSFFloat(data.x_center_m);
      nodePtr->getField("yOffset")->setSFFloat(data.y_center_m);
      
      nodePtr->getField("radius")->setSFFloat(data.radius_m);
      nodePtr->getField("startAngle")->setSFFloat(data.start_rad);
      nodePtr->getField("sweepAngle")->setSFFloat(data.sweep_rad);
    }
  }
}

void VizControllerImpl::SetNodeVisibility(webots::Node* node)
{
  // Hide this node from the robot's camera (if any)
  if (_cozmoCameraNodeId >= 0) {
    auto* cameraNode = _vizSupervisor.getFromId(_cozmoCameraNodeId);
    node->setVisibility(cameraNode, false);
  }

  if(_cozmoToFNodeId >= 0) {
    auto* tofNode = _vizSupervisor.getFromId(_cozmoToFNodeId);
    node->setVisibility(tofNode, false);
  }
}

} // end namespace Vector
} // end namespace Anki
