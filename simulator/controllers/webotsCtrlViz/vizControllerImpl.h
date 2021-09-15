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
#ifndef __WebotsCtrlViz_VizControllerImpl_H__
#define __WebotsCtrlViz_VizControllerImpl_H__


#include "engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior_fwd.h"
#include "engine/encodedImage.h"
#include "engine/events/ankiEventMgr.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/image.h"

#include "clad/types/cameraParams.h"
#include "clad/vizInterface/messageViz.h"

#include <webots/Supervisor.hpp>
#include <webots/ImageRef.hpp>
#include <webots/Display.hpp>
#include <vector>
#include <map>

namespace Anki {
namespace Vector {

struct CozmoBotVizParams
{
  bool Valid() {return (trans != nullptr) &&
                       (rot != nullptr) &&
                       (liftAngle != nullptr) &&
                       (headAngle != nullptr); }
  
  webots::Field* trans = nullptr;
  webots::Field* rot = nullptr;
  webots::Field* liftAngle = nullptr;
  webots::Field* headAngle = nullptr;
};

// Information about viz objects to draw (e.g. wireframe of cube)
struct VizObjectInfo {
  VizInterface::Object data;
  int webotsNodeId = -1; // Optional webots node identifier for 3D objects that are dynamically added to the scene tree.
};

// Information about viz line segments to draw
struct VizSegmentInfo {
  VizInterface::LineSegment data;
  int webotsNodeId = -1;
};

// Information about viz quads to draw
struct VizQuadInfo {
  VizInterface::Quad data;
  int webotsNodeId = -1;
};
  
// Information about viz paths to draw
struct VizPathSegmentLineInfo {
  VizInterface::AppendPathSegmentLine data;
  int webotsNodeId = -1;
};

struct VizPathSegmentArcInfo {
  VizInterface::AppendPathSegmentArc data;
  int webotsNodeId = -1;
};
  
struct VizPathInfo {
  uint32_t color = 0;
  std::vector<VizPathSegmentLineInfo> lines;
  std::vector<VizPathSegmentArcInfo> arcs;
};
  
// Note: The values of these labels are used to determine the line number
//       at which the corresponding text is displayed in the window.
enum class VizTextLabelType : unsigned int
{
  TEXT_LABEL_POSE = 0,
  TEXT_LABEL_HEAD_LIFT,
  TEXT_LABEL_PITCH,
  TEXT_LABEL_ROLL,
  TEXT_LABEL_ACCEL,
  TEXT_LABEL_GYRO,
  TEXT_LABEL_CLIFF,
  TEXT_LABEL_DIST,
  TEXT_LABEL_SPEEDS,
  TEXT_LABEL_OFF_TREADS_STATE,
  TEXT_LABEL_TOUCH,
  TEXT_LABEL_BATTERY,
  TEXT_LABEL_ANIM,
  TEXT_LABEL_ANIM_TRACK_LOCKS,
  TEXT_LABEL_VID_RATE,
  TEXT_LABEL_STATUS_FLAG,
  TEXT_LABEL_STATUS_FLAG_2,
  TEXT_LABEL_STATUS_FLAG_3,
  TEXT_LABEL_DOCK_ERROR_SIGNAL,
  NUM_TEXT_LABELS
};


class VizControllerImpl
{

public:
  VizControllerImpl(webots::Supervisor& vs);

  void Init();
  void Update();

  // Set whether or not VizController should draw objects in the 3D display
  void EnableDrawingObjects(const bool b) { _drawingObjectsEnabled = b; }
  
  void ProcessMessage(VizInterface::MessageViz&& message);

private:

  void SetRobotPose(CozmoBotVizParams& vizParams, const Pose3d& pose, const f32 headAngle, const f32 liftAngle);

  void DrawText(webots::Display* disp, u32 lineNum, u32 color, const char* text);
  void DrawText(webots::Display* disp, u32 lineNum, const char* text);
  void ProcessVizSetRobotMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizSetLabelMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizDockingErrorSignalMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizVisionMarkerMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCameraQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCameraRectMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCameraLineMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCameraOvalMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCameraTextMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizImageChunkMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizTrackerQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizRobotStateMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizCurrentAnimation(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessCameraParams(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessBehaviorStackDebug(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVisionModeDebug(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessEnabledVisionModes(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessSaveImages(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessSaveState(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessVizSetOriginMessage(const AnkiEvent<VizInterface::MessageViz> &msg);
  
  void ProcessVizMemoryMapMessageBegin(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizMemoryMapMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizMemoryMapMessageEnd(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessVizObjectMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizEraseObjectMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizShowObjectsMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessVizLineSegmentMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizEraseLineSegmentsMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessVizQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizEraseQuadMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void ProcessVizAppendPathSegmentLineMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizAppendPathSegmentArcMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizSetPathColorMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  void ProcessVizErasePathMessage(const AnkiEvent<VizInterface::MessageViz>& msg);
  
  void DisplayBufferedCameraImage(const RobotTimeStamp_t timestamp);
  void DisplayCameraInfo(const RobotTimeStamp_t timestamp);
  
  void EraseVizObjects(const uint32_t lowerBoundId = 0,
                       const uint32_t upperBoundId = std::numeric_limits<uint32_t>::max());
  
  void EraseVizSegments(const std::string& identifier);
  
  void EraseVizQuads(const VizQuadType quadType,
                     const uint32_t quadId);
  
  void EraseVizPath(const uint32_t pathId);
  
  void Draw();
  
  void DrawObjects();
  void DrawLineSegments();
  void DrawQuads();
  void DrawPaths();
  
  void Subscribe(const VizInterface::MessageVizTag& tagType, std::function<void(const AnkiEvent<VizInterface::MessageViz>&)> messageHandler) {
    _eventMgr.SubscribeForever(static_cast<uint32_t>(tagType), messageHandler);
  }

  // Update the visibility of "node" for other nodes like camera and rangefinder
  void SetNodeVisibility(webots::Node* node);

  webots::Supervisor& _vizSupervisor;

  // For displaying nav map in the 3D view
  webots::Display* _navMapDisp;
  
  // For displaying misc debug data
  webots::Display* _disp;

  // For displaying docking data
  webots::Display* _dockDisp;

  // For the behavior stack
  webots::Display* _bsmStackDisp;

  // For displaying active VisionMode data
  webots::Display* _visionModeDisp;

  // For displaying images
  webots::Display* _camDisp;

  // Image reference for display in camDisp
  webots::ImageRef* _camImg = nullptr;

  // The Pose3d of the viz controller with respect to the Webots world
  Pose3d _vizControllerPose;
  
  // CozmoBot for vizualization (when connected to a physical robot)
  CozmoBotVizParams _vizBot;

  // Image message processing
  static const size_t kNumBufferedImages = 10;
  std::array<EncodedImage, kNumBufferedImages> _bufferedImages;
  size_t                                       _imageBufferIndex = 0;
  std::map<RobotTimeStamp_t, size_t>           _encodedImages;
  std::map<RobotTimeStamp_t, u32>              _bufferedSaveCtrs;
  RobotTimeStamp_t _curImageTimestamp = 0;
  ImageSendMode _saveImageMode = ImageSendMode::Off;
  std::string   _savedImagesFolder = "";
  u32           _saveCtr = 0;
  bool          _saveVizImage = false;
  
  // For managing "debug" image displays
  struct DebugImage {
    EncodedImage      encodedImage;
    webots::Display*  imageDisplay;
    webots::ImageRef* imageRef;
    
    DebugImage(webots::Display* display) : imageDisplay(display), imageRef(nullptr) { }
  };
  std::vector<DebugImage> _debugImages;
  
  // Camera info
  Vision::CameraParams _cameraParams;
  
  // Store the nodeID of the camera node inside the simulated robot (if any). This is to be able to make the viz
  // displays and objects invisible to the robot's camera using webots::Node::setVisibility().
  int _cozmoCameraNodeId = -1;
  int _cozmoToFNodeId = -1;
  
  // For saving state
  bool          _saveState = false;
  std::string   _savedStateFolder = "";
  
  AnkiEventMgr<VizInterface::MessageViz> _eventMgr;

  std::string _currAnimName = "";
  u8          _currAnimTag = 0;
  
  std::vector<ExternalInterface::MemoryMapQuadInfoFull> _navMapNodes;
  
  // "Global" switch to enable drawing of objects from this controller
  bool _drawingObjectsEnabled = false;
  
  // Whether or not to draw objects (based on ShowObjects message)
  bool _showObjects = true;
  
  double _lastDrawTime_sec = -1.0;
  
  // Objects to visualize (e.g. cubes, charger, poses, etc.). Map keyed on viz object ID
  std::map<uint32_t, VizObjectInfo> _vizObjects;
  
  // Line segments to visualize. Map keyed on string identifier.
  std::map<std::string, std::vector<VizSegmentInfo>> _vizSegments;
  
  // Quads to visualize. Inner map keyed on QuadID
  std::map<VizQuadType, std::map<uint32_t, VizQuadInfo>> _vizQuads;
  
  // Paths to visualize. Map keyed on path ID
  std::map<uint32_t, VizPathInfo> _vizPaths;
};

} // end namespace Vector
} // end namespace Anki


#endif //__WebotsCtrlViz_VizControllerImpl_H__
