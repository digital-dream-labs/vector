/**
 * File: mapComponent.cpp
 *
 * Author: Michael Willett
 * Created: 2017-09-11
 *
 * Description: Component for consuming new sensor data and processing the information into
 *              the appropriate map objects
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "engine/navMap/mapComponent.h"

#include "coretech/vision/engine/observableObjectLibrary.h"
#include "coretech/common/engine/math/poseOriginList.h"
#include "coretech/common/engine/math/polygon.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/messaging/engine/IComms.h"

#include "engine/navMap/iNavMap.h"
#include "engine/navMap/navMapFactory.h"
#include "engine/navMap/memoryMap/data/memoryMapData_Cliff.h"
#include "engine/navMap/memoryMap/data/memoryMapData_ProxObstacle.h"
#include "engine/navMap/memoryMap/data/memoryMapData_ObservableObject.h"

#include "engine/ankiEventUtil.h"
#include "engine/block.h"
#include "engine/charger.h"
#include "engine/robot.h"
#include "engine/robotStateHistory.h"
#include "engine/cozmoContext.h"
#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/habitatDetectorComponent.h"
#include "engine/actions/actionContainers.h"
#include "engine/actions/basicActions.h"
#include "engine/aiComponent/aiComponent.h"
#include "engine/aiComponent/aiWhiteboard.h"
#include "engine/blockWorld/blockWorld.h"
#include "engine/vision/groundPlaneROI.h"

#include "util/cpuProfiler/cpuProfiler.h"
#include "util/console/consoleInterface.h"
#include "util/logging/DAS.h"

#include "webServerProcess/src/webService.h"

#include "opencv2/imgproc/imgproc.hpp"

#include <numeric>

#define LOG_CHANNEL "MapComponent"

// Giving this its own local define, in case we want to control it independently of DEV_CHEATS / NDEBUG, etc.
#define ENABLE_DRAWING ANKI_DEV_CHEATS

namespace Anki {
namespace Vector {

// how often we request redrawing maps. Added because I think clad is getting overloaded with the amount of quads
CONSOLE_VAR(float, kMapRenderRate_sec, "MapComponent", 0.25f);

// kObjectRotationChangeToReport_deg: if the rotation of an object changes by this much, memory map will be notified
CONSOLE_VAR(float, kObjectRotationChangeToReport_deg, "MapComponent", 10.0f);
// kObjectPositionChangeToReport_mm: if the position of an object changes by this much, memory map will be notified
CONSOLE_VAR(float, kObjectPositionChangeToReport_mm, "MapComponent", 5.0f);

// kNeverMergeOldMaps: if set to false, we only relocalize if the robot is in the same world origin as the previous map
CONSOLE_VAR(bool, kMergeOldMaps, "MapComponent", false);

// kRobotRotationChangeToReport_deg: if the rotation of the robot changes by this much, memory map will be notified
CONSOLE_VAR(float, kRobotRotationChangeToReport_deg, "MapComponent", 20.0f);
// kRobotPositionChangeToReport_mm: if the position of the robot changes by this much, memory map will be notified
CONSOLE_VAR(float, kRobotPositionChangeToReport_mm, "MapComponent", 8.0f);

CONSOLE_VAR(float, kVisionTimeout_ms, "MapComponent", 120.0f * 1000);
CONSOLE_VAR(float, kUnrecognizedTimeout_ms, "MapComponent", 20.0f * 1000);
CONSOLE_VAR(float, kProxTimeout_ms, "MapComponent", 600.0f * 1000);
CONSOLE_VAR(float, kTimeoutUpdatePeriod_ms, "MapComponent", 5.0f * 1000);
CONSOLE_VAR(float, kCliffTimeout_ms, "MapComponent", 1200.f * 1000); // 20 minutes

// the length and half width of two triangles used in FlagProxObstaclesUsingPose (see method)
CONSOLE_VAR(float, kProxExploredTriangleLength_mm, "MapComponent", 300.0f );
CONSOLE_VAR(float, kProxExploredTriangleHalfWidth_mm, "MapComponent", 50.0f );

CONSOLE_VAR(float, kHoughAngleResolution_deg, "MapComponent.VisualEdgeDetection", 2.0);
CONSOLE_VAR(int,   kHoughAccumThreshold,      "MapComponent.VisualEdgeDetection", 20);
CONSOLE_VAR(float, kHoughMinLineLength_mm,    "MapComponent.VisualEdgeDetection", 40.0);
CONSOLE_VAR(float, kHoughMaxLineGap_mm,       "MapComponent.VisualEdgeDetection", 10.0);
CONSOLE_VAR(float, kEdgeLineLengthToInsert_mm,"MapComponent.VisualEdgeDetection", 200.f);
CONSOLE_VAR(float, kVisionCliffPadding_mm,    "MapComponent.VisualEdgeDetection", 20.f);

CONSOLE_VAR(int,   kMaxPixelsUsedForHoughTransform, "MapComponent.VisualEdgeDetection", 160000); // 400 x 400 max size

namespace {

// return the content type we would set in the memory type for each object type
MemoryMapTypes::EContentType ObjectTypeToMemoryMapContentType(ObjectType type, bool isAdding)
{
  using ContentType = MemoryMapTypes::EContentType;
  ContentType retType = ContentType::Unknown;
  if (IsBlockType(type, false) ||
      IsCustomType(type, false) ||
      IsChargerType(type, false)) {
    retType = isAdding ? ContentType::ObstacleObservable : ContentType::ClearOfObstacle;
  }

  return retType;
}

const char* const kWebVizModuleName = "navmap";

decltype(auto) GetChargerRegion(const Pose3d& poseWRTRoot) 
{
  // grab the cannonical corners and then apply the transformation. If we use `GetBoundingQuadXY`, 
  // we no longer know where the "back" is. Unfortunately, order matters here, and for corners
  // on the ground plane, the order is (from charger.cpp): 
  //    {BackLeft, FrontLeft, FrontRight, BackLeft, ...top corners...}
  //
  //            eBL----------------------eBR
  //             |  \       back       /  |               +x
  //             |    iBL----------iBR    |               ^
  //             |     |            |     |               |
  //             |  l  |            |  r  |               |
  //             |  e  |            |  i  |               +-----> +y               
  //             |  f  |            |  g  |
  //             |  t  |            |  h  |
  //             |     |            |  t  |
  //             |     |            |     |
  //            eFL---iFL          iFR---eFR
  //


  // points for calculating the collision area of a charger, which is different from the physical bounding box
  //    (x := marker normal, y := marker horizontal)
  const Vec3f kInteriorChargerOffsetBR = {-12.f, -12.f, 0.f};
  const Vec3f kInteriorChargerOffsetBL = {-12.f,  12.f, 0.f};
  const Vec3f kInteriorChargerOffsetFL = {  5.f,  12.f, 0.f};
  const Vec3f kInteriorChargerOffsetFR = {  5.f, -12.f, 0.f};
  const Vec3f kExteriorChargerOffsetBR = {  0.f,   0.f, 0.f};
  const Vec3f kExteriorChargerOffsetBL = {  0.f,   0.f, 0.f};
  const Vec3f kExteriorChargerOffsetFL = {  5.f,   0.f, 0.f};
  const Vec3f kExteriorChargerOffsetFR = {  5.f,   0.f, 0.f};

  const std::vector<Point3f>& corners = Charger().GetCanonicalCorners();
  const Point2f exteriorBL = poseWRTRoot * (corners[0] + kExteriorChargerOffsetBL);
  const Point2f exteriorFL = poseWRTRoot * (corners[1] + kExteriorChargerOffsetFL);
  const Point2f exteriorFR = poseWRTRoot * (corners[2] + kExteriorChargerOffsetFR);
  const Point2f exteriorBR = poseWRTRoot * (corners[3] + kExteriorChargerOffsetBR);
  const Point2f interiorBL = poseWRTRoot * (corners[0] + kInteriorChargerOffsetBL);
  const Point2f interiorFL = poseWRTRoot * (corners[1] + kInteriorChargerOffsetFL);
  const Point2f interiorFR = poseWRTRoot * (corners[2] + kInteriorChargerOffsetFR);
  const Point2f interiorBR = poseWRTRoot * (corners[3] + kInteriorChargerOffsetBR);


  // only want to flag the back and sides of the charger, so define each side as a separate trapezoid
  // as seen in the diagram above
  return MakeUnion2f( FastPolygon({ exteriorBL, interiorBL, interiorBR, exteriorBR }),  // back
                      FastPolygon({ exteriorBL, exteriorFL, interiorFL, interiorBL }),  // left
                      FastPolygon({ interiorBR, interiorFR, exteriorFR, exteriorBR }) ); // right

}


decltype(auto) GetHabitatRegion(const Pose3d& poseWRTRoot) 
{
  //
  //                   eB                       
  //                 ╱    ╲
  //               ╱   iB   ╲                 +x 
  //             ╱   ╱ xx ╲   ╲                ^
  //           ╱   ╱   xx   ╲   ╲              |
  //         ╱   ╱            ╲   ╲            |
  //       eL--iL              iR--eR          +-----> +y
  //        ╲   ╲             ╱   ╱        
  //          ╲   ╲         ╱   ╱                       
  //            ╲   ╲     ╱   ╱                        
  //              ╲    iF   ╱                           
  //                ╲     ╱                        
  //                   eF                       
  //                                          
  //
  
  // points for calculating the collision area of a habitat, relative to charger pose
  //    (x := charger marker normal, y := charger marker horizontal)
  const Vec3f kInteriorBack  = {  160.f,    0.f, 0.f };
  const Vec3f kInteriorLeft  = {  -40.f, -200.f, 0.f };
  const Vec3f kInteriorRight = {  -40.f,  200.f, 0.f };
  const Vec3f kInteriorFront = { -260.f,    0.f, 0.f };
  const Vec3f kExteriorBack  = {  210.f,    0.f, 0.f };
  const Vec3f kExteriorLeft  = {  -40.f, -250.f, 0.f };
  const Vec3f kExteriorRight = {  -40.f,  250.f, 0.f };
  const Vec3f kExteriorFront = { -290.f,    0.f, 0.f };

  const Point2f actualInteriorBack  = poseWRTRoot * kInteriorBack;
  const Point2f actualInteriorLeft  = poseWRTRoot * kInteriorLeft;
  const Point2f actualInteriorRight = poseWRTRoot * kInteriorRight;
  const Point2f actualInteriorFront = poseWRTRoot * kInteriorFront;
  const Point2f actualExteriorBack  = poseWRTRoot * kExteriorBack;
  const Point2f actualExteriorLeft  = poseWRTRoot * kExteriorLeft;
  const Point2f actualExteriorRight = poseWRTRoot * kExteriorRight;
  const Point2f actualExteriorFront = poseWRTRoot * kExteriorFront;

  // only want to flag the back and sides of the charger, so define each side as a separate trapezoid
  // as seen in the diagram above
  return MakeUnion2f( 
    FastPolygon({ actualExteriorBack,  actualExteriorLeft,  actualInteriorLeft,  actualInteriorBack  }),  // back-left
    FastPolygon({ actualExteriorBack,  actualExteriorRight, actualInteriorRight, actualInteriorBack  }),  // back-right
    FastPolygon({ actualExteriorFront, actualExteriorLeft,  actualInteriorLeft,  actualInteriorFront }),  // front-left
    FastPolygon({ actualExteriorFront, actualExteriorRight, actualInteriorRight, actualInteriorFront })); // front-right
}

// console var utility for testing out Visual extending cliffs
static Robot* consoleRobot = nullptr;

#if REMOTE_CONSOLE_ENABLED 
void DevProcessOneFrameForVisionEdges( ConsoleFunctionContextRef context )
{
  if(consoleRobot != nullptr) {
    consoleRobot->GetActionList().QueueAction(QueueActionPosition::NOW, new WaitForImagesAction(1, VisionMode::OverheadEdges));
  }
}

// cache the visualized cliff frames to clear on subsequent calls
// to drawing the cliffs
static std::vector<std::string> sCliffFrameIdentifiers;

void DevDrawCliffPoses( ConsoleFunctionContextRef context )
{
  if(consoleRobot != nullptr) {
    auto vizm = consoleRobot->GetContext()->GetVizManager();
    auto& mapc = consoleRobot->GetMapComponent();

    const auto currentMap = mapc.GetCurrentMemoryMap();
    if (!currentMap) {
      return;
    }

    for(const auto& id : sCliffFrameIdentifiers) {
      vizm->EraseSegments(id);
    }
    sCliffFrameIdentifiers.clear();

    MemoryMapTypes::NodePredicate isDropSensorCliff = [](MemoryMapTypes::MemoryMapDataConstPtr nodeData) -> bool {
      const bool isValidCliff = nodeData->type == MemoryMapTypes::EContentType::Cliff;
      return isValidCliff;
    };

    MemoryMapTypes::MemoryMapDataConstList cliffNodes;
    currentMap->FindContentIf(isDropSensorCliff, cliffNodes);

    size_t cliffCount = 0;
    for(auto& node : cliffNodes) {
      Pose3d pose = MemoryMapData::MemoryMapDataCast<MemoryMapData_Cliff>(node)->pose;
      // set z-height above the map when rendering
      Point3f renderPoint(pose.GetTranslation().x(),pose.GetTranslation().y(), 3.f);
      pose.SetTranslation(renderPoint);
      std::string id = "cliff_frame" + std::to_string(cliffCount++);
      vizm->DrawFrameAxes(id, pose);
      sCliffFrameIdentifiers.push_back(id);
    }
  }
}
#endif //REMOTE_CONSOLE_ENABLED
CONSOLE_FUNC( DevProcessOneFrameForVisionEdges, "MapComponent.VisualEdgeDetection" );
CONSOLE_FUNC( DevDrawCliffPoses,                "MapComponent.VisualEdgeDetection");

};

using namespace MemoryMapTypes;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MapComponent::MapComponent()
: IDependencyManagedComponent(this, RobotComponentID::Map)
, _currentMapOriginID(PoseOriginList::UnknownOriginID)
, _nextTimeoutUpdate_ms(0)
, _vizMessageDirty(true)
, _gameMessageDirty(true)
, _webMessageDirty(false) // web must request it
, _isRenderEnabled(false)
, _broadcastRate_sec(-1.0f)
, _enableProxCollisions(true)
{
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MapComponent::~MapComponent()
{
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps)
{
  _robot = robot;
  consoleRobot = robot;
  if(_robot->HasExternalInterface())
  {
    using namespace ExternalInterface;
    IExternalInterface& externalInterface = *_robot->GetExternalInterface();
    auto helper = MakeAnkiEventUtil(externalInterface, *this, _eventHandles);
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetMemoryMapRenderEnabled>();
    helper.SubscribeGameToEngine<MessageGameToEngineTag::SetMemoryMapBroadcastFrequency_sec>();
  }

  if( _robot->GetContext() != nullptr ) {
    auto* webService = _robot->GetContext()->GetWebService();
    if( webService != nullptr ) {
      auto onData = [this](const Json::Value& data, const std::function<void(const Json::Value&)>& sendFunc) {
        _webMessageDirty = true;
      };
      _eventHandles.emplace_back( webService->OnWebVizData( kWebVizModuleName ).ScopedSubscribe( onData ) );
    }
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<>
void MapComponent::HandleMessage(const ExternalInterface::SetMemoryMapRenderEnabled& msg)
{
  SetRenderEnabled(msg.enabled);
};

template<>
void MapComponent::HandleMessage(const ExternalInterface::SetMemoryMapBroadcastFrequency_sec& msg)
{
  _broadcastRate_sec = msg.frequency;
};


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateDependent(const RobotCompMap& dependentComps)
{
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if (currentNavMemoryMap)
  {
    // check for object timeouts in navMap
    TimeoutObjects();

    // Check if we should broadcast changes to navMap to different channels
    const f32 currentTime_s = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();

    const bool shouldSendViz = (ENABLE_DRAWING && _vizMessageDirty && _isRenderEnabled) || _webMessageDirty;
    const bool shouldSendSDK = _gameMessageDirty && (_broadcastRate_sec >= 0.0f);
    
    MemoryMapTypes::MapBroadcastData data;
    if( shouldSendViz || shouldSendSDK ) {
      currentNavMemoryMap->GetBroadcastInfo(data);
    }

    // send viz Messages
    if ( shouldSendViz )
    {
      static f32 nextDrawTime_s = currentTime_s;
      const bool doViz = FLT_LE(nextDrawTime_s, currentTime_s);
      if( doViz ) {
        BroadcastMapToViz(data);


        // Reset the timer but don't accumulate error
        nextDrawTime_s += ((int) (currentTime_s - nextDrawTime_s) / kMapRenderRate_sec + 1) * kMapRenderRate_sec;
        _vizMessageDirty = false;
      }

      if( _webMessageDirty ) {
        BroadcastMapToWeb(data);
        _webMessageDirty = false;
      }
    }

    // send SDK messages
    if ( shouldSendSDK )
    {
      static f32 nextBroadcastTime_s = currentTime_s;
      if (FLT_LE(nextBroadcastTime_s, currentTime_s)) {
        BroadcastMapToSDK(data);

        // Reset the timer but don't accumulate error
        nextBroadcastTime_s += ((int) (currentTime_s - nextBroadcastTime_s) / _broadcastRate_sec + 1) * _broadcastRate_sec;
        _gameMessageDirty = false;
      }
    }
  }

  UpdateRobotPose();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateBroadcastFlags(bool wasChanged)
{
  _vizMessageDirty |= wasChanged;
  _gameMessageDirty |= wasChanged;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateMapOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID)
{
  // oldOrigin is the pointer/id of the map we were just building, and it's going away. It's the current map
  // newOrigin is the pointer/id of the map that is staying, it's the one we rejiggered to, and we haven't changed in a while
  auto oldMapIter = _navMaps.find(oldOriginID);
  auto newMapIter = _navMaps.find(newOriginID);

  const Pose3d& oldOrigin = _robot->GetPoseOriginList().GetOriginByID(oldOriginID);
  const Pose3d& newOrigin = _robot->GetPoseOriginList().GetOriginByID(newOriginID);

  ANKI_VERIFY(oldMapIter != _navMaps.end(), 
              "MemoryMap.UpdateMapOrigins.OldOriginNotFound", 
              "PreviousOrigin could not be found, so nothing will be merged");

  ANKI_VERIFY(oldOriginID == _currentMapOriginID, 
              "MemoryMap.UpdateMapOrigins.BadOrigin", 
              "rejiggering map %d, but currentID = %d",
              oldOriginID, 
              _currentMapOriginID);

  // maps have changed, so make sure to clear all the renders
  ClearRender();

  // before we merge the object information from the memory maps, apply rejiggering also to their reported poses
  UpdateOriginsOfObjects(oldOriginID, newOriginID);
  
  // reset newMap if we somehow lost it
  if (newMapIter == _navMaps.end() || nullptr == newMapIter->second.map) {
    _navMaps[newOriginID].map.reset(NavMapFactory::CreateMemoryMap());
    newMapIter = _navMaps.find(newOriginID);
  }

  _currentMapOriginID = newOriginID;
  newMapIter->second.activationTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();

  // if we had an old map, merge its data into the new map, then delete it
  if (oldMapIter != _navMaps.end()) {
    Pose3d oldWrtNew;
    const bool success = oldOrigin.GetWithRespectTo(newOrigin, oldWrtNew);
    DEV_ASSERT(success, "MapComponent.UpdateMapOrigins.BadOldWrtNull");
    UpdateBroadcastFlags(newMapIter->second.map->Merge(*(oldMapIter->second.map), oldWrtNew));

    newMapIter->second.activeDuration_ms += oldMapIter->second.activeDuration_ms;
    _navMaps.erase( oldOriginID ); // smart pointer will delete memory
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateRobotPose()
{
  ANKI_CPU_PROFILE("MapComponent::UpdateRobotPoseInMemoryMap");

  // grab current robot pose
  DEV_ASSERT(_robot->GetPoseOriginList().GetCurrentOriginID() == _currentMapOriginID,
             "MapComponent.OnRobotPoseChanged.InvalidWorldOrigin");
  const Pose3d& robotPose = _robot->GetPose();
  const Pose3d& robotPoseWrtOrigin = robotPose.GetWithRespectToRoot();

  // check if we have moved far enough that we need to resend
  const Point3f distThreshold(kRobotPositionChangeToReport_mm, kRobotPositionChangeToReport_mm, kRobotPositionChangeToReport_mm);
  const Radians angleThreshold( DEG_TO_RAD(kRobotRotationChangeToReport_deg) );
  const bool isPrevSet = _reportedRobotPose.HasParent();
  const bool isFarFromPrev = !isPrevSet || (!robotPoseWrtOrigin.IsSameAs(_reportedRobotPose, distThreshold, angleThreshold));

  // if we need to add
  const bool addAgain = isFarFromPrev;
  if ( addAgain )
  {
    RobotTimeStamp_t currentTimestamp = _robot->GetLastMsgTimestamp();

    // robot quad relative to cliff sensor positions
    Quad2f robotSensorQuad {
      {kCliffSensorXOffsetFront_mm, +kCliffSensorYOffset_mm},  // up L
      {kCliffSensorXOffsetFront_mm, -kCliffSensorYOffset_mm},  // up R
      {kCliffSensorXOffsetRear_mm,  -kCliffSensorYOffset_mm},  // lo R
      {kCliffSensorXOffsetRear_mm,  +kCliffSensorYOffset_mm}}; // lo L

    ((Pose2d) robotPoseWrtOrigin).ApplyTo(robotSensorQuad, robotSensorQuad);
    InsertData(Poly2f(robotSensorQuad), MemoryMapData(EContentType::ClearOfCliff, currentTimestamp));


    const Quad2f& robotQuad = _robot->GetBoundingQuadXY(robotPoseWrtOrigin);

    // regular clear of obstacle
    InsertData(Poly2f(robotQuad), MemoryMapData(EContentType::ClearOfObstacle, currentTimestamp));

    _robot->GetAIComponent().GetComponent<AIWhiteboard>().ProcessClearQuad(robotQuad);
    // update las reported pose
    _reportedRobotPose = robotPoseWrtOrigin;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::TimeoutObjects()
{
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if (currentNavMemoryMap)
  {
    // check for object timeouts in navMap occasionally
    const RobotTimeStamp_t currentTime = _robot->GetLastMsgTimestamp();
    if( currentTime <= _nextTimeoutUpdate_ms ) {
      return;
    }
    _nextTimeoutUpdate_ms = currentTime + kTimeoutUpdatePeriod_ms;
    
    // ternary to prevent uInt wrapping on subtract
    const RobotTimeStamp_t unrecognizedTooOld = (currentTime <= kUnrecognizedTimeout_ms) ? 0 : currentTime - kUnrecognizedTimeout_ms;
    const RobotTimeStamp_t visionTooOld       = (currentTime <= kVisionTimeout_ms)       ? 0 : currentTime - kVisionTimeout_ms;
    const RobotTimeStamp_t proxTooOld         = (currentTime <= kProxTimeout_ms)         ? 0 : currentTime - kProxTimeout_ms;
    const RobotTimeStamp_t cliffTooOld        = (currentTime <= kCliffTimeout_ms)        ? 0 : currentTime - kCliffTimeout_ms;
    
    NodeTransformFunction timeoutObjects =
      [unrecognizedTooOld, visionTooOld, proxTooOld, cliffTooOld] (MemoryMapDataPtr data) -> MemoryMapDataPtr
      {
        const EContentType nodeType = data->type;
        const RobotTimeStamp_t lastObs = data->GetLastObservedTime();

        if ((EContentType::Cliff                == nodeType && lastObs <= cliffTooOld)        ||
            (EContentType::ObstacleUnrecognized == nodeType && lastObs <= unrecognizedTooOld) ||
            (EContentType::InterestingEdge      == nodeType && lastObs <= visionTooOld)       ||
            (EContentType::NotInterestingEdge   == nodeType && lastObs <= visionTooOld)       ||
            (EContentType::ObstacleProx         == nodeType && lastObs <= proxTooOld))
        {
          return MemoryMapDataPtr();
        }
        return data;
      };

    UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(timeoutObjects));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::FlagGroundPlaneROIInterestingEdgesAsUncertain()
{
  // get quad wrt robot
  const Pose3d& curRobotPose = _robot->GetPose().GetWithRespectToRoot();
  Quad3f groundPlaneWrtRobot;
  curRobotPose.ApplyTo(GroundPlaneROI::GetGroundQuad(), groundPlaneWrtRobot);

  // ask memory map to clear
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  DEV_ASSERT(currentNavMemoryMap, "MapComponent.FlagGroundPlaneROIInterestingEdgesAsUncertain.NullMap");

  NodeTransformFunction transform = [] (MemoryMapDataPtr oldData) -> MemoryMapDataPtr
    {
        if (EContentType::InterestingEdge == oldData->type) {
          return MemoryMapDataPtr();
        }
        return oldData;
    };

  FastPolygon poly(Poly2f((Quad2f)groundPlaneWrtRobot));
  UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(transform, poly));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::FlagQuadAsNotInterestingEdges(const Quad2f& quadWRTOrigin)
{
  InsertData(Poly2f(quadWRTOrigin), MemoryMapData(EContentType::NotInterestingEdge, _robot->GetLastImageTimeStamp()));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::FlagInterestingEdgesAsUseless()
{
  // flag all content as Unknown: ideally we would add a new type (SmallInterestingEdge), so that we know
  // we detected something, but we discarded it because it didn't have enough info; however that increases
  // complexity when raycasting, finding boundaries, readding edges, etc. By flagging Unknown we simply say
  // "there was something here, but we are not sure what it was", which can be good to re-explore the area

  auto currentNavMemoryMap = GetCurrentMemoryMap();
  DEV_ASSERT(currentNavMemoryMap, "MapComponent.FlagInterestingEdgesAsUseless.NullMap");

  NodeTransformFunction transform = [] (MemoryMapDataPtr oldData) -> MemoryMapDataPtr
    {
        if (EContentType::InterestingEdge == oldData->type) {
          return MemoryMapDataPtr();
        }
        return oldData;
    };

  UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(transform));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::FlagProxObstaclesUsingPose()
{
  const auto& pose = _robot->GetPose();

  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if( currentNavMemoryMap == nullptr ) {
    return;
  }

  // construct a triangle, pointing away from the robot, to mimic the
  // coverage of the prox sensor, but scaled differently. Any prox obstacle that it
  // covers will be marked as explored. Since it looks better if the robot comes
  // close to an object in order to mark it as explored, the poly isn't as tall as the
  // prox sensor's reach.

  const auto& rot = pose.GetRotation();
  Vec3f offset1(kProxExploredTriangleLength_mm,  kProxExploredTriangleHalfWidth_mm, 0);
  Vec3f offset2(kProxExploredTriangleLength_mm,  -kProxExploredTriangleHalfWidth_mm, 0);
  const Point2f p1 = pose.GetTranslation();
  const Point2f p2 = (pose.GetTransform() * Transform3d(rot, offset1)).GetTranslation();
  const Point2f p3 = (pose.GetTransform() * Transform3d(rot, offset2)).GetTranslation();
  const FastPolygon triangleExplored({p1, p2, p3});

  // mark any prox obstacle in triangleExplored as explored
  NodeTransformFunction exploredFunc = [](MemoryMapDataPtr data) -> MemoryMapDataPtr {
    if( data->type == EContentType::ObstacleProx ) {
      auto castPtr = MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>( data );
      castPtr->MarkExplored();
    }
    return data;
  };
  const bool addedExplored = currentNavMemoryMap->TransformContent(exploredFunc, triangleExplored);

  UpdateBroadcastFlags( addedExplored );

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MapComponent::FlagProxObstaclesTouchingExplored()
{
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if( currentNavMemoryMap == nullptr ) {
    return false;
  }

  // mark any NOT_EXPLORED ObstacleProx that is touching an EXPLORED ObstacleProx quad as EXPLORED
  NodePredicate innerCheckFunc = [](MemoryMapDataConstPtr inside) {
    if( inside->type == EContentType::ObstacleProx ) {
      auto castInside = MemoryMapData::MemoryMapDataCast<const MemoryMapData_ProxObstacle>( inside );
      return !(castInside->IsExplored());
    } else {
      return false;
    }
  };
  NodePredicate outerCheckFunc = [](MemoryMapDataConstPtr outside) {
    if( outside->type == EContentType::ObstacleProx ) {
      auto castOutside = MemoryMapData::MemoryMapDataCast<const MemoryMapData_ProxObstacle>( outside );
      return (castOutside->IsExplored());
    } else {
      // if it is touching an observable obstacle, also mark it as explored
      return ( outside->type == EContentType::ObstacleObservable );
    }
  };
  MemoryMapData_ProxObstacle toAdd( MemoryMapData_ProxObstacle::EXPLORED, {0.0f, 0.0f, 0.0f}, _robot->GetLastImageTimeStamp());
  const bool changedBorder = currentNavMemoryMap->FillBorder(innerCheckFunc, outerCheckFunc,
                                                             toAdd.Clone());

  UpdateBroadcastFlags( changedBorder );

  return changedBorder;

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::CreateLocalizedMemoryMap(PoseOriginID_t worldOriginID)
{
  DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(worldOriginID),
                 "MapComponent.CreateLocalizedMemoryMap.BadWorldOriginID",
                 "ID:%d", worldOriginID);

  // clear all memory map rendering since we are building a new map
  ClearRender();

  // Since we are going to create a new memory map, check if any of the existing ones have become a zombie
  // This could happen if either the current map never saw a localizable object, or if objects in previous maps
  // have been moved or deactivated, which invalidates them as localizable
  MapTable::iterator iter = _navMaps.begin();
  while ( iter != _navMaps.end() )
  {
    // if we cannot merge old maps, force zombie to be true so we delete it
    const bool isZombie = _robot->GetBlockWorld().IsZombiePoseOrigin( iter->first ) || !kMergeOldMaps;
    if ( isZombie ) {
      LOG_INFO("MapComponent.memory_map.deleting_zombie_map", "%d", worldOriginID );
      // also remove the reported poses in this origin for every object (fixes a leak, and better tracks where objects are)
      for( auto& posesForObjectIt : _reportedPoses ) {
        OriginToPoseInMapInfo& posesPerOriginForObject = posesForObjectIt.second;
        const PoseOriginID_t zombieOriginID = iter->first;
        posesPerOriginForObject.erase( zombieOriginID );
      }
      iter = _navMaps.erase(iter);
    } else {
      LOG_INFO("MapComponent.memory_map.keeping_alive_map", "%d", worldOriginID );
      ++iter;
    }
  }

  // if the origin is null, we would never merge the map, which could leak if a new one was created
  // do not support this by not creating one at all if the origin is null
  if ( PoseOriginList::UnknownOriginID != worldOriginID )
  {
    
    const EngineTimeStamp_t currTimeStamp_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
    if ( _currentMapOriginID != PoseOriginList::UnknownOriginID )
    {
      // _currentMapOriginID might have been deleted as a zombie origin
      auto it = _navMaps.find(_currentMapOriginID);
      if( it != _navMaps.end() ) {
        // increment the time that the previous _currentMapOriginID was active
        it->second.activeDuration_ms += TimeStamp_t(currTimeStamp_ms - it->second.activationTime_ms);
      }
    }
    
    
    // create a new memory map in the given origin
    LOG_INFO("MapComponent.CreateLocalizedMemoryMap", "Setting current origin to %i", worldOriginID);
    INavMap* navMemoryMap = NavMapFactory::CreateMemoryMap();
    MapInfo mapInfo;
    mapInfo.map = std::shared_ptr<INavMap>(navMemoryMap);
    mapInfo.activeDuration_ms = 0;
    mapInfo.activationTime_ms = currTimeStamp_ms;
    
    _navMaps.emplace( worldOriginID, std::move(mapInfo) );
    _currentMapOriginID = worldOriginID;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace {
  // constants for broadcasting maps
  // const float kOffSetPerIdx_mm = -250.0f;
  const size_t kReservedBytes = 1 + 2; // Message overhead for:  Tag, and vector size
  const size_t kMaxBufferSize = Anki::Comms::MsgPacket::MAX_SIZE;
  const size_t kMaxBufferForQuads = kMaxBufferSize - kReservedBytes;
  const size_t kQuadsPerMessage = kMaxBufferForQuads / sizeof(QuadInfoVector::value_type);
  const size_t kFullQuadsPerMessage = kMaxBufferForQuads / sizeof(QuadInfoFullVector::value_type);

  static_assert(kQuadsPerMessage > 0,     "MapComponent.Broadcast.InvalidQuadsPerMessage");
  static_assert(kFullQuadsPerMessage > 0, "MapComponent.Broadcast.InvalidFullQuadsPerMessage");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::BroadcastMapToViz(const MapBroadcastData& mapData) const
{
  using namespace VizInterface;

  // Send the begin message
  _robot->Broadcast(MessageViz(MemoryMapMessageVizBegin(_currentMapOriginID, mapData.mapInfo)));
  // chunk the quad messages
  for(u32 seqNum = 0; seqNum*kFullQuadsPerMessage < mapData.quadInfoFull.size(); seqNum++)
  {
    auto start = seqNum*kFullQuadsPerMessage;
    auto end   = std::min(mapData.quadInfoFull.size(), start + kFullQuadsPerMessage);
    _robot->Broadcast(MessageViz(MemoryMapMessageViz(_currentMapOriginID,
      QuadInfoFullVector(mapData.quadInfoFull.begin() + start, mapData.quadInfoFull.begin() + end))));
  }

  // Send the end message
  _robot->Broadcast(MessageViz(MemoryMapMessageVizEnd(_currentMapOriginID)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::BroadcastMapToWeb(const MapBroadcastData& mapData) const
{
  auto* webService = _robot->GetContext()->GetWebService();
  if( webService == nullptr || !webService->IsWebVizClientSubscribed(kWebVizModuleName)) {
    return;
  }

  // Send the begin message
  {
    Json::Value toWeb;
    toWeb["type"] = "MemoryMapMessageVizBegin";
    toWeb["originId"] = _currentMapOriginID;
    toWeb["mapInfo"] = mapData.mapInfo.GetJSON();
    webService->SendToWebViz(kWebVizModuleName, toWeb);
  }

  // chunk the quad messages
  for(u32 seqNum = 0; seqNum * kQuadsPerMessage < mapData.quadInfo.size(); seqNum++)
  {
    auto start = seqNum * kQuadsPerMessage;
    auto end   = std::min(mapData.quadInfo.size(), start + kQuadsPerMessage);
    Json::Value toWeb;
    toWeb["type"] = "MemoryMapMessageViz";
    toWeb["originId"] = _currentMapOriginID;
    toWeb["seqNum"] = seqNum;
    toWeb["quadInfos"] = Json::arrayValue;
    auto& quadInfo = toWeb["quadInfos"];
    for( auto it = mapData.quadInfo.begin() + start; it != mapData.quadInfo.begin() + end; ++it ) {
      quadInfo.append( it->GetJSON() );
    }
    webService->SendToWebViz(kWebVizModuleName, toWeb);
  }

  // Send the end message
  {
    Json::Value toWeb;
    toWeb["type"] = "MemoryMapMessageVizEnd";
    toWeb["originId"] = _currentMapOriginID;
    auto& robotJson = toWeb["robot"];
    robotJson["x"] = _robot->GetPose().GetTranslation().x();
    robotJson["y"] = _robot->GetPose().GetTranslation().y();
    robotJson["z"] = _robot->GetPose().GetTranslation().z();
    robotJson["qW"] = _robot->GetPose().GetRotation().GetQuaternion().w();
    robotJson["qX"] = _robot->GetPose().GetRotation().GetQuaternion().x();
    robotJson["qY"] = _robot->GetPose().GetRotation().GetQuaternion().y();
    robotJson["qZ"] = _robot->GetPose().GetRotation().GetQuaternion().z();
    webService->SendToWebViz(kWebVizModuleName, toWeb);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::BroadcastMapToSDK(const MemoryMapTypes::MapBroadcastData& mapData) const
{
  using namespace ExternalInterface;

  // Send the begin message
  _robot->Broadcast(MessageEngineToGame(MemoryMapMessageBegin(
      _currentMapOriginID, mapData.mapInfo.rootDepth, mapData.mapInfo.rootSize_mm,
      mapData.mapInfo.rootCenterX, mapData.mapInfo.rootCenterY)
  ));

  // chunk the quad messages
  for(u32 seqNum = 0; seqNum*kQuadsPerMessage < mapData.quadInfo.size(); seqNum++)
  {
    auto start = seqNum*kQuadsPerMessage;
    auto end   = std::min(mapData.quadInfo.size(), start + kQuadsPerMessage);
    _robot->Broadcast(MessageEngineToGame(MemoryMapMessage(
      QuadInfoVector(mapData.quadInfo.begin() + start, mapData.quadInfo.begin() + end))));
  }

  // Send the end message
  _robot->Broadcast(MessageEngineToGame(MemoryMapMessageEnd()));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::SendDASInfoAboutMap(const PoseOriginID_t& mapOriginID) const
{
  if ( mapOriginID != PoseOriginList::UnknownOriginID )
  {
    const auto it = _navMaps.find(mapOriginID);
    if ( ANKI_VERIFY( (it != _navMaps.end()) && (it->second.map != nullptr),
                      "MapComponent.SendDASInfoAboutMap.NotFound",
                      "Could not find orgin %u, or the map is null",
                      mapOriginID ) )
    {
      const float explored_mm2 = 1e6f * it->second.map->GetExploredRegionAreaM2();
      const float collision_mm2 = it->second.map->GetArea( [] (const auto& data) { return data->IsCollisionType(); });
      TimeStamp_t activeDuration_ms = 0;
      if ( mapOriginID == _currentMapOriginID )
      {
        // still active, so need to append the time since activated to the duration
        const EngineTimeStamp_t currTime_ms = BaseStationTimer::getInstance()->GetCurrentTimeStamp();
        const EngineTimeStamp_t& activationTime = it->second.activationTime_ms;
        activeDuration_ms = it->second.activeDuration_ms + TimeStamp_t(currTime_ms - activationTime);
      } else {
        // inactive. just use the cached activation time
        activeDuration_ms = it->second.activeDuration_ms;
      }
      
      DASMSG(robot_delocalized_map_info,
             "robot.delocalized_map_info",
             "When the robot is delocalized, this contains information about the nav map. This occurs when the robot "
             "delocalizes due to being picked up");
      DASMSG_SET(i1, (int64_t)explored_mm2, "Total surface area known (mm2)");
      DASMSG_SET(i2, (int64_t)collision_mm2, "Total surface area that is an obstacle (mm2), a subset of i1");
      DASMSG_SET(i3, (int64_t)activeDuration_ms, "Duration (ms) of the map, perhaps after multiple delocalizations" );
      DASMSG_SEND();
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::ClearRender()
{
  if(ENABLE_DRAWING)
  {
    // set map as dirty
    _vizMessageDirty = true;
    _gameMessageDirty = true;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::SetRenderEnabled(bool enabled)
{
  // if disabling, clear render now. If enabling wait until next render time
  if ( _isRenderEnabled && !enabled ) {
    ClearRender();
  }

  // set new value
  _isRenderEnabled = enabled;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::shared_ptr<INavMap> MapComponent::GetCurrentMemoryMapHelper() const
{
  // current map (if any) must match current robot origin
  const bool validOrigin = (PoseOriginList::UnknownOriginID == _currentMapOriginID) ||
                           (_robot->GetPoseOriginList().GetCurrentOriginID() == _currentMapOriginID);
                             
  ANKI_VERIFY(validOrigin, 
              "MemoryMap.GetCurrentMap.BadOrigin", 
              "robot and mapComponent missmatch. robot: %d. map: %d",
              _robot->GetPoseOriginList().GetCurrentOriginID(), 
              _currentMapOriginID);


  std::shared_ptr<INavMap> curMap = nullptr;
  if ( validOrigin ) {
    auto matchPair = _navMaps.find(_currentMapOriginID);
    if ( matchPair != _navMaps.end() ) {
      curMap = matchPair->second.map;
    } else {
      DEV_ASSERT(false, "MapComponent.GetNavMemoryMap.MissingMap");
    }
  }

  return curMap;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateObjectPose(const ObservableObject& object, const Pose3d* oldPose, PoseState oldPoseState)
{
  // TODO (VIC-13789): Clean this method up (e.g., no need to pass a pointer to oldPose)
  
  const ObjectID& objectID = object.GetID();
  DEV_ASSERT(objectID.IsSet(), "MapComponent.OnObjectPoseChanged.InvalidObjectID");

  const PoseState newPoseState = object.GetPoseState();

  /*
    Three things can happen:
     a) first time we see an object: OldPoseState=!Valid, NewPoseState= Valid
     b) updating an object:          OldPoseState= Valid, NewPoseState= Valid
     c) deleting an object:          OldPoseState= Valid, NewPoseState=!Valid
   */
  const bool oldValid = ObservableObject::IsValidPoseState(oldPoseState);
  const bool newValid = ObservableObject::IsValidPoseState(newPoseState);
  if ( !oldValid && newValid )
  {
    // first time we see the object, add report
    AddObservableObject(object, object.GetPose());
  }
  else if ( oldValid && newValid )
  {
    // updating the pose of an object, decide if we update the report. As an optimization, we don't update
    // it if the poses are close enough
    const int objectIdInt = objectID.GetValue();
    const OriginToPoseInMapInfo& reportedPosesForObject = _reportedPoses[objectIdInt];
    const PoseOrigin& curOrigin = object.GetPose().FindRoot();
    const PoseOriginID_t curOriginID = curOrigin.GetID();
    DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(curOriginID),
                   "MapComponent.OnObjectPoseChanged.ObjectOriginNotInOriginList",
                   "ID:%d", curOriginID);
    const auto poseInNewOriginIter = reportedPosesForObject.find( curOriginID );

    if ( poseInNewOriginIter != reportedPosesForObject.end() )
    {
      // note that for distThreshold, since Z affects whether we add to the memory map, distThreshold should
      // be smaller than the threshold to not report
      DEV_ASSERT(kObjectPositionChangeToReport_mm < object.GetDimInParentFrame<'Z'>()*0.5f,
                "OnObjectPoseChanged.ChangeThresholdTooBig");
      const float distThreshold = kObjectPositionChangeToReport_mm;
      const Radians angleThreshold( DEG_TO_RAD(kObjectRotationChangeToReport_deg) );

      // compare new pose with previous entry and decide if isFarFromPrev
      const PoseInMapInfo& info = poseInNewOriginIter->second;
      const bool isFarFromPrev =
        ( !info.isInMap || (!object.GetPose().IsSameAs(info.pose, Point3f(distThreshold), angleThreshold)));

      // if it is far from previous (or previous was not in the map, remove-add)
      if ( isFarFromPrev ) {
        if (object.IsUnique())
        {
          RemoveObservableObject(object, curOriginID);
        }
        AddObservableObject(object, object.GetPose());
      }
    }
    else
    {
      // did not find an entry in the current origin for this object, add it now
      AddObservableObject(object, object.GetPose());
    }
  }
  else if ( oldValid && !newValid )
  {
    // deleting an object, remove its report using oldOrigin (the origin it was removed from)
    const PoseOriginID_t oldOriginID = oldPose->GetRootID();
    RemoveObservableObject(object, oldOriginID);
  }
  else
  {
    // not possible
    PRINT_NAMED_ERROR("MapComponent.OnObjectPoseChanged.BothStatesAreInvalid",
                      "Object %d changing from Invalid to Invalid", objectID.GetValue());
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::AddObservableObject(const ObservableObject& object, const Pose3d& newPose)
{
  const auto& objectType = object.GetType();
  const MemoryMapTypes::EContentType addType = ObjectTypeToMemoryMapContentType(objectType, true);
  if ( addType == MemoryMapTypes::EContentType::Unknown )
  {
    // this is ok, this object type is not tracked in the memory map
    PRINT_CH_INFO("MapComponent", "MapComponent.AddObservableObject.InvalidAddType",
                  "ObjectType '%s' is not known in memory map",
                  ObjectTypeToString(objectType) );
    return;
  }

  const int objectId = object.GetID().GetValue();

  // find the memory map for the given origin
  const PoseOriginID_t originID = newPose.GetRootID();
  auto memoryMap = GetCurrentMemoryMap();
  if ( memoryMap )
  {
    // in order to properly handle stacks, do not add the quad to the memory map for objects that are not
    // on the floor
    Pose3d objWrtRobot;
    if ( newPose.GetWithRespectTo(_robot->GetPose(), objWrtRobot) )
    {

      const bool isFloating = object.IsPoseTooHigh(objWrtRobot, 1.f, STACKED_HEIGHT_TOL_MM, 0.f);
      if ( isFloating )
      {
        // store in as a reported pose, but set as not in map (the pose value is not relevant)
        _reportedPoses[objectId][originID] = PoseInMapInfo(newPose, false);
      }
      else
      {
        // add to memory map flattened out wrt origin
        Pose3d newPoseWrtOrigin = newPose.GetWithRespectToRoot();
        Poly2f boundingPoly(object.GetBoundingQuadXY(newPoseWrtOrigin));
        if (IsChargerType(objectType, false)) {
          const bool inHabitat = (_robot->GetHabitatDetectorComponent().GetHabitatBeliefState() == HabitatBeliefState::InHabitat);
          MemoryMapData_ObservableObject data(object, boundingPoly, _robot->GetLastImageTimeStamp());
          
          if (inHabitat) {
            const auto region = MakeUnion2f( GetChargerRegion(newPoseWrtOrigin), GetHabitatRegion(newPoseWrtOrigin) );
            InsertData( region, data );
          } else {
            InsertData( GetChargerRegion(newPoseWrtOrigin), data );
          }
        } else if (IsBlockType(objectType, false) ||
                   IsCustomType(objectType, false)) {
          // eventually we will want to store multiple ID's to the node data in the case for multiple blocks
          // however, we have no mechanism for merging data, so for now we just replace with the new id
          MemoryMapData_ObservableObject data(object, boundingPoly, _robot->GetLastImageTimeStamp());
          InsertData(boundingPoly, data);
        } else {
          PRINT_NAMED_WARNING("MapComponent.AddObservableObject.AddedNonObservableType",
                              "AddObservableObject was called to add a non observable object");
          InsertData(boundingPoly, MemoryMapData(addType, _robot->GetLastImageTimeStamp()));
        }

        // store in as a reported pose
        _reportedPoses[objectId][originID] = PoseInMapInfo(newPoseWrtOrigin, true);
      }
    }
    else
    {
      // should not happen, so warn about it
      PRINT_NAMED_WARNING("MapComponent.AddObservableObject.InvalidPose",
                          "Could not get object's new pose wrt robot. Won't add to map");
    }
  }
  else
  {
    // if the map was removed (for zombies), we shouldn't be asking to add an object to it
    PRINT_NAMED_ERROR("MapComponent.AddObservableObject.NoMapForOrigin",
                      "Tried to insert an observable object without creating a map first");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::RemoveObservableObject(const ObservableObject& object, PoseOriginID_t originID)
{
  using namespace MemoryMapTypes;

  const auto& objectType = object.GetType();
  const MemoryMapTypes::EContentType removalType = ObjectTypeToMemoryMapContentType(objectType, false);
  if ( removalType == EContentType::Unknown )
  {
    // this is not ok, this object type can be added but can't be removed from the map
    PRINT_NAMED_WARNING("MapComponent.RemoveObservableObject.InvalidRemovalType",
                        "ObjectType '%s' does not have a removal type in memory map",
                        ObjectTypeToString(objectType) );
    return;
  }

  const ObjectID id = object.GetID();

  // find the memory map for the given origin
  auto matchPair = _navMaps.find(originID);
  if ( matchPair != _navMaps.end() )
  {
    RobotTimeStamp_t timeStamp = _robot->GetLastImageTimeStamp();

    // for Cubes, we can lookup by ID
    auto clearData = MemoryMapData(removalType, timeStamp).Clone();
    NodeTransformFunction transform = [id, &clearData](MemoryMapDataPtr data)
    {
      if (data->type == EContentType::ObstacleObservable)
      {
        // eventually we will want to store multiple ID's to the node data in the case for multiple blocks
        // however, we have no mechanism for merging data, so for now we are just completely replacing
        // the NodeContent if the ID matches.
        if (MemoryMapData::MemoryMapDataCast<const MemoryMapData_ObservableObject>(data)->id == id) {
          return clearData;
        }
      }
      return data;
    };

    UpdateBroadcastFlags(matchPair->second.map->TransformContent(transform));
  } else {
    // if the map was removed (for zombies), we shouldn't be asking to remove an object from it
    DEV_ASSERT(matchPair == _navMaps.end(), "MapComponent.RemoveObservableObject.NoMapForOrigin");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::MarkObjectUnobserved(const ObservableObject& object) { 
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if (currentNavMemoryMap) {
    const ObjectID id = object.GetID();
    PRINT_CH_INFO("MapComponent", "MapComponent.MarkObjectUnobserved", "Marking observable object %d as unobserved", (int) id );

    NodeTransformFunction transform = [id] (MemoryMapDataPtr data) {
      if (data->type == EContentType::ObstacleObservable) {
        auto objectData = MemoryMapData::MemoryMapDataCast<MemoryMapData_ObservableObject>(data);
        if (objectData->id == id) { objectData->MarkUnobserved(); }
      }
      return data;
    };

    UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(transform));
  }
} 

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::UpdateOriginsOfObjects(PoseOriginID_t curOriginID, PoseOriginID_t relocalizedOriginID)
{
  // Origins have changed, and some maps may be merged, so make sure to clear everything
  ClearRender();

  // for every object in the current map, we have a decision to make. We are going to bring that memory map
  // into what is becoming the current one. That means also bringing the last reported pose of every object
  // onto the new map. The current map is obviously more up to date than the map we merge into, since the map
  // we merge into is map we identified a while ago. This means that if an object moved and we now know where
  // it is, the good pose is in the currentMap, not in the mapWeMergeInto. So, for every object in the currentMap
  // we are going to remove their pose from the mapWeMergeInto. This will make the map we merge into gain the new
  // info, at the same time that we remove info known to not be the most accurate

  // for every object in the current origin
  for ( auto& pairIdToPoseInfoByOrigin : _reportedPoses )
  {
    // find object in the world
    const ObservableObject* object = _robot->GetBlockWorld().GetLocatedObjectByID(pairIdToPoseInfoByOrigin.first);
    if ( nullptr == object )
    {
      PRINT_CH_INFO("MapComponent", "MapComponent.UpdateOriginsOfObjects.NotAnObject",
                    "Could not find object ID '%d' in MapComponent updating their quads", pairIdToPoseInfoByOrigin.first );
      continue;
    }

    // find the reported pose for this object in the current origin
    OriginToPoseInMapInfo& poseInfoByOriginForObj = pairIdToPoseInfoByOrigin.second;
    const auto& matchInCurOrigin = poseInfoByOriginForObj.find(curOriginID);
    const bool isObjectReportedInCurrent = (matchInCurOrigin != poseInfoByOriginForObj.end());
    if ( isObjectReportedInCurrent )
    {
      // we have an entry in the current origin. We don't care if `isInMap` is true or false. If it's true
      // it means we have a better position available in this frame, if it's false it means we saw the object
      // in this frame, but somehow it became unknown. If it became unknown, the position it had in the origin
      // we are relocalizing to is old and not to be trusted. This is the reason why we don't erase reported poses,
      // but rather flag them as !isInMap.
      // Additionally we don't have to worry about the container we are iterating changing, since iterators are not
      // affected by changing a boolean, but are if we erased from it.
      RemoveObservableObject(*object, relocalizedOriginID);

      // we are bringing over the current info into the relocalized origin, update the reported pose in the
      // relocalized origin to be that of the newest information
      poseInfoByOriginForObj[relocalizedOriginID].isInMap = matchInCurOrigin->second.isInMap;
      if ( matchInCurOrigin->second.isInMap ) {
        // bring over the pose if it's in map (otherwise we don't care about the pose)
        // when we bring it, flatten out to the relocalized origin
        DEV_ASSERT(_robot->GetPoseOriginList().GetOriginByID(relocalizedOriginID).HasSameRootAs(matchInCurOrigin->second.pose),
                   "MapComponent.UpdateOriginsOfObjects.PoseDidNotHookGrandpa");
        poseInfoByOriginForObj[relocalizedOriginID].pose = matchInCurOrigin->second.pose.GetWithRespectToRoot();
      }
      // also, erase the current origin from the reported poses of this object, since we will never use it after this
      // Note this should not alter the iterators we are using for _reportedPoses
      poseInfoByOriginForObj.erase(curOriginID);
    }
    else
    {
      // we don't have this object in the current memory map. The info from this object if at all is in the previous
      // origin (then one we are relocalizing to), or another origin not related to these two, do nothing in those
      // cases
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::ClearRobotToMarkers(const ObservableObject* object)
{
  // the newPose should be directly in the robot's origin
  DEV_ASSERT(object->GetPose().IsChildOf(_robot->GetWorldOrigin()),
             "MapComponent.ClearRobotToMarkers.ObservedObjectParentNotRobotOrigin");

  // get the markers we have seen from this object
  std::vector<const Vision::KnownMarker*> observedMarkers;
  object->GetObservedMarkers(observedMarkers);

  // only clear to the markers, since for custom object types, the object might be significantly larger than the marker
  for ( const auto& observedMarkerIt : observedMarkers )
  {
    // NOTE: (mrw) We are making assumptions here that the marker is both normal to the map plane, and is oriented
    //       to a 90° angle (up/down/left/right). Additionally, this will clear all the way to the marker, so even
    //       if the object's physical properties extend in front of the marker, we might be overwritting that region
    //       with `ClearOfObstacle` state. This is particularly noticeable for the charger, but at the time of writing
    //       this, it is not interfering with any docking behavior.
    const Quad3f& markerCorners = observedMarkerIt->Get3dCorners(observedMarkerIt->GetPose().GetWithRespectToRoot());

    // grab the lowest two points
    const Point3f* p1 = &markerCorners[(Quad::CornerName) 0];
    const Point3f* p2 = &markerCorners[(Quad::CornerName) 1];

    for (Quad::CornerName i = (Quad::CornerName) 1; i < Quad::NumCorners; ++i) {
      if ( FLT_LT(markerCorners[i].z(), p1->z()) ) {
        p2 = p1;
        p1 = &markerCorners[i];
      } else if ( FLT_LT(markerCorners[i].z(), p2->z()) ) {
        p2 = &markerCorners[i];
      }
    }

    ClearRobotToEdge(*p1, *p2, _robot->GetLastImageTimeStamp());
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::ClearRobotToEdge(const Point2f& p, const Point2f& q, const RobotTimeStamp_t t)
{
  auto currentMap = GetCurrentMemoryMap();
  if (currentMap)
  {
    // NOTE: (MRW) currently using robot pose center, though to be correct we should use the center of the 
    //       sensor pose. For now this should be good enough.
    const static float kHalfClearWidth_mm = 1.5f;
    Vec3f rayOffset1(0,  kHalfClearWidth_mm, 0);
    Vec3f rayOffset2(0, -kHalfClearWidth_mm, 0);
    Rotation3d rot = Rotation3d(0.f, Z_AXIS_3D());
    const Point2f r1 = (_robot->GetPose().GetTransform() * Transform3d(rot, rayOffset1)).GetTranslation();
    const Point2f r2 = (_robot->GetPose().GetTransform() * Transform3d(rot, rayOffset2)).GetTranslation();
    FastPolygon quad = ConvexPolygon::ConvexHull({p, q, r1, r2});

    ClearRegion(quad, t);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::ClearRegion(const BoundedConvexSet2f& region, const RobotTimeStamp_t t)
{
  auto currentMap = GetCurrentMemoryMap();
  if (currentMap)
  {
    MemoryMapDataPtr clearData = MemoryMapData(INavMap::EContentType::ClearOfObstacle, t).Clone();
    NodeTransformFunction trfm = [&clearData] (MemoryMapDataPtr currentData) {
      if (currentData->type == EContentType::ObstacleProx) {
        auto castPtr = MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>( currentData );
        castPtr->MarkClear();
        return (castPtr->IsConfirmedClear()) ? clearData : currentData;
      } else if ( currentData->CanOverrideSelfWithContent(clearData) ) {
        return clearData;
      } else {
        return currentData;
      }
    };
    UpdateBroadcastFlags(currentMap->Insert(region, trfm));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::AddProxData(const BoundedConvexSet2f& region, const MemoryMapData& data)
{
  auto currentMap = GetCurrentMemoryMap();
  if (currentMap)
  {
    // Make sure we enable collision types before inserting
    auto newData = MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>(data.Clone());
    newData->SetCollidable(_enableProxCollisions);

    NodeTransformFunction trfm = [&newData] (MemoryMapDataPtr currentData) -> MemoryMapDataPtr {

      if (currentData->type == EContentType::ObstacleProx) {
        auto castPtr = MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>( currentData );
        castPtr->MarkObserved();
        if (castPtr->IsExplored()) { 
          newData->MarkExplored(); 
        }
        return currentData;
      } else if ( currentData->CanOverrideSelfWithContent(newData) ) {
        return newData;
      } else {
        return currentData;
      }
    };
    UpdateBroadcastFlags(currentMap->Insert(region, trfm));
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::RemoveAllProxObstacles()
{
  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if (currentNavMemoryMap) {
    NodeTransformFunction proxObstacles =
    [] (MemoryMapDataPtr data) -> MemoryMapDataPtr {
      const EContentType nodeType = data->type;
      if (EContentType::ObstacleProx == nodeType) {
        return MemoryMapDataPtr();
      }
      return data;
    };
    
    UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(proxObstacles));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::SetUseProxObstaclesInPlanning(bool enable)
{
  _enableProxCollisions = enable;

  auto currentNavMemoryMap = GetCurrentMemoryMap();
  if (currentNavMemoryMap) {
    PRINT_CH_INFO("MapComponent", "MapComponent.SetUseProxObstaclesInPlanning", "Setting prox obstacles as %s collidable", enable ? "" : "NOT" );
    NodeTransformFunction enableProx = [enable] (MemoryMapDataPtr data) {
      if (EContentType::ObstacleProx == data->type) {
        MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>(data)->SetCollidable(enable);
      }
      return data;
    };
    
    UpdateBroadcastFlags(currentNavMemoryMap->TransformContent(enableProx
    ));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::InsertData(const Poly2f& polyWRTOrigin, const MemoryMapData& data)
{
  return InsertData( MemoryMapTypes::MemoryMapRegion( FastPolygon(polyWRTOrigin) ), data );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::InsertData(const MemoryMapTypes::MemoryMapRegion& region, const MemoryMapData& data)
{
  auto currentMap = GetCurrentMemoryMap();
  if (currentMap)
  {
    UpdateBroadcastFlags(currentMap->Insert(region, data));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MapComponent::CheckForCollisions(const BoundedConvexSet2f& region) const
{
  const auto currentMap = GetCurrentMemoryMap();
  if (currentMap) {
    return currentMap->AnyOf( region, [] (const auto& data) { return data->IsCollisionType(); });
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MapComponent::CheckForCollisions(const BoundedConvexSet2f& region, const MemoryMapTypes::NodePredicate& pred) const
{
  const auto currentMap = GetCurrentMemoryMap();
  if (currentMap) {
    return currentMap->AnyOf( region, pred );
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float MapComponent::GetCollisionArea(const BoundedConvexSet2f& region) const
{
  const auto currentMap = GetCurrentMemoryMap();
  if (currentMap) {
    return currentMap->GetArea( [] (const auto& data) { return data->IsCollisionType(); }, region);
  }
  return 0.f;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result MapComponent::ProcessVisionOverheadEdges(const OverheadEdgeFrame& frameInfo)
{
  Result ret = RESULT_OK;
  if ( frameInfo.groundPlaneValid ) {
    if ( !frameInfo.chains.GetVector().empty() ) {
      ret = AddVisionOverheadEdges(frameInfo);
    } else {
      // we expect lack of borders to be reported as !isBorder chains
      DEV_ASSERT(false, "ProcessVisionOverheadEdges.ValidPlaneWithNoChains");
    }
  } else {
    // ground plane was invalid (atm we don't use this). It's probably only useful if we are debug-rendering
    // the ground plane
    _robot->GetContext()->GetVizManager()->EraseSegments("MapComponent.AddVisionOverheadEdges");
  }
  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MapComponent::AddDetectedObstacles(const OverheadEdgeFrame& edgeObstacles)
{
  // TODO: Do something different with these vs. "interesting" overhead edges?
  if( edgeObstacles.groundPlaneValid && !edgeObstacles.chains.GetVector().empty() )
  {
    AddVisionOverheadEdges(edgeObstacles);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result MapComponent::FindSensorDetectedCliffs(std::vector<MemoryMapTypes::MemoryMapDataConstPtr>& cliffNodes) const
{
  cliffNodes.clear();
  const auto currentMap = GetCurrentMemoryMap();
  if (!currentMap) {
    return RESULT_FAIL;
  }

  NodePredicate isDropSensorCliff = [](MemoryMapDataConstPtr nodeData) -> bool {
    const bool isValidCliff = nodeData->type == EContentType::Cliff;
    if(isValidCliff) {
      auto nCliff = MemoryMapData::MemoryMapDataCast<MemoryMapData_Cliff>(nodeData);
      if(nCliff->isFromCliffSensor) {
        return true;
      }
    }
    return false;
  };

  MemoryMapDataConstList cliffNodeSet;
  currentMap->FindContentIf(isDropSensorCliff, cliffNodeSet);

  std::for_each(cliffNodeSet.begin(), cliffNodeSet.end(),
    [&] (const auto& dataPtr) { 
      cliffNodes.push_back(dataPtr); 
    }
  );

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result MapComponent::AddVisionOverheadEdges(const OverheadEdgeFrame& frameInfo) 
{
  const auto currentMap = GetCurrentMemoryMap();
  if (!currentMap) {
    return RESULT_OK;
  }

  // the robot may be moving while we are importing edges
  // take care to get the historical pose estimate for the
  // image timestamp, and use that to project the points on
  // to the ground plane
  HistRobotState histState;
  RobotTimeStamp_t histTimestamp;
  const bool useInterp = true;
  const auto& res = _robot->GetStateHistory()->ComputeStateAt(frameInfo.timestamp, histTimestamp, histState, useInterp);
  if (res != RESULT_OK) {
    PRINT_NAMED_WARNING("MapComponent.AddVisionOverheadEdges.NoHistoricalPose",
                        "Could not retrieve historical pose for timestamp %u",
                        (TimeStamp_t)frameInfo.timestamp);
    return RESULT_FAIL;
  }
  const Pose2d& robotPose = histState.GetPose();

  std::vector<MemoryMapDataConstPtr> cliffNodes;
  auto result = FindSensorDetectedCliffs(cliffNodes);
  if(result != RESULT_OK) {
    PRINT_CH_INFO("MapComponent","MapComponent.AddVisionOverheadEdges.UnableToRetreiveCliffCenters","");
    return result;
  }

  /*
                                                  +--------+
    z                                             |        |
                                                  |        |
    ^                                             |        |
    |                                             |        |
    |                                             |        |   (projection)
    |                                    ---------+. Obst  |        +
    +------> x     +--------+           /         | ...    |        |
                   |        | --(ray)---          |    ... |        |
                   |  Robot |                     |       ...       |
                   |        +------(prox)-------->|        | ...    |
                   +--------+                     +--------+    ... v
                 +--------------------------------X---------+      .X
                                 Ground

  Above is an illustrative case where we want to discard the edge
  detection seen by the robot because of an obstruction.

  the robot senses an obstacle with:
  (1) the prox sensor, which creates a navmap cell which is occupied
  (2) the camera, by detecting an edge-feature on the surface of the obstacle
  
  By assuming the edge-feature is on the ground-plane, we obtain the
  projection point behind the obstacle. If we draw a ray from Robot->Projection
  then it will most likely intersect the obstacle cell detected by the prox.
  This allows us to discard the edge-feature as a "not-a-cliff-edge".
  */


  NodePredicate isCollisionType = [] (const auto& data) { return (data->type == EContentType::ObstacleProx) ||
                                                                 (data->type == EContentType::ObstacleObservable) ||
                                                                 (data->type == EContentType::ObstacleUnrecognized); };

  std::vector<Point2f> validPoints;
  std::vector<Point2f> imagePoints;
  for( const auto& chain : frameInfo.chains.GetVector() )
  {
    if(!chain.isBorder) {
      // edge detection code returns connected points which are:
      // - contiguous within some distance threshold
      // - share the same "isBorder" value
      //
      // the isBorder flag means that the edge detector reached 
      // the end of the ground plane without detecting a border, 
      // so we should ignore this chain
      continue;
    }
    for( const auto& imagePt : chain.points) {
      Point2f imagePtOnGround = robotPose * imagePt.position;
      imagePoints.push_back(std::move(imagePtOnGround));
    }
  }
  std::vector<bool> collisionCheckResults = currentMap->AnyOf(robotPose.GetTranslation(), imagePoints, isCollisionType);

  validPoints.reserve(imagePoints.size());
  for(int i=0; i<imagePoints.size(); ++i) {
    if(!collisionCheckResults[i]) {
      validPoints.push_back(imagePoints[i]);
    }
  }

  if(validPoints.size() >= kHoughAccumThreshold && cliffNodes.size() > 0) {
    // find the newly created cliff, and the old cliffs
    // TODO(agm) currently we set the newest cliff as the "target" cliff to extend
    //           it would be nice if we could decide on the cliff to extend more intelligently
    //           based on what we are currently observing. This assumes that the edge
    //           processing is always called in response to discovering a new cliff from the
    //           drop sensor.
    auto newestNodeIter = std::max_element(cliffNodes.cbegin(), cliffNodes.cend(), [&](const auto& lhs, const auto& rhs) {
      return lhs->GetLastObservedTime() < rhs->GetLastObservedTime();
    });
    std::vector<MemoryMapDataConstPtr> oldCliffNodes;
    for(auto iter = cliffNodes.cbegin(); iter!=cliffNodes.cend(); ++iter) {
      if(iter != newestNodeIter) {
        oldCliffNodes.push_back(*iter);
      }
    }
    if(newestNodeIter != cliffNodes.cend()) {
      auto newCliffNode = MemoryMapData::MemoryMapDataCast<const MemoryMapData_Cliff>(*newestNodeIter);
      Pose3d refinedCliffPose;
      const bool result = RefineNewCliffPose(validPoints, newCliffNode, oldCliffNodes, refinedCliffPose);
      if(result) {
        auto ptr = std::const_pointer_cast<MemoryMapData_Cliff>(newCliffNode.GetSharedPtr());
        ptr->pose = refinedCliffPose; // directly edit the pose of the cliff

        // data node contain visually-seen cliff information
        MemoryMapData_Cliff cliffDataVis(refinedCliffPose, frameInfo.timestamp);
        cliffDataVis.isFromVision = true;
        const auto& cliffDataVisPtr = cliffDataVis.Clone();

        // special transform function to insert visual cliffs, without overwriting
        // sensor-detected cliffs (merges both sources of info the same node)
        NodeTransformFunction transformVisionCliffs = [&cliffDataVisPtr] (MemoryMapDataPtr currNode) -> MemoryMapDataPtr {
          if(currNode->type == EContentType::Cliff) {
            // a node can be from the cliff sensor AND from vision
            auto currCliff = MemoryMapData::MemoryMapDataCast<MemoryMapData_Cliff>(currNode);
            if(currCliff->isFromCliffSensor && !currCliff->isFromVision) {
              currCliff->isFromVision = true;
              // already modified the current node, no need to clone it
              return currNode;
            }
          } else if(currNode->CanOverrideSelfWithContent(cliffDataVisPtr)) {
            // every other type of node is handled here
            return cliffDataVisPtr;
          }
          return currNode;
        };

        const Pose2d& refinedCliffPose2d = refinedCliffPose;
        currentMap->Insert(FastPolygon({
          refinedCliffPose2d * Point2f(-kVisionCliffPadding_mm,  kEdgeLineLengthToInsert_mm),
          refinedCliffPose2d * Point2f(-kVisionCliffPadding_mm, -kEdgeLineLengthToInsert_mm),
          refinedCliffPose2d * Point2f(0.f, -kEdgeLineLengthToInsert_mm),
          refinedCliffPose2d * Point2f(0.f,  kEdgeLineLengthToInsert_mm),
        }), transformVisionCliffs); 
      }
    }
  } else {
    PRINT_CH_INFO("MapComponent",
                  "MapComponent.AddVisionOverheadEdges.InvalidCliffOrPointsCount",
                  "numCliffs=%zd numPoints=%zd",validPoints.size(), cliffNodes.size());
  }
  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MapComponent::RefineNewCliffPose(const std::vector<Point2f>& points,
                                      MemoryMapTypes::MemoryMapDataConstPtr newCliffNode,
                                      const std::vector<MemoryMapTypes::MemoryMapDataConstPtr>& oldCliffNodes,
                                      Pose3d& refinedCliffPose) const
{
  // using a hough transform on a binary image constructed from
  // the input edge-feature points, this method returns the best
  // edge from a computed list of candidate edges
  const Pose3d& newCliffPose = MemoryMapData::MemoryMapDataCast<const MemoryMapData_Cliff>(newCliffNode)->pose;
  const Point2f& newCliffCenter = newCliffPose.GetTranslation();
  std::vector<Point2f> oldCliffCenters;
  std::transform( oldCliffNodes.cbegin(), 
                  oldCliffNodes.cend(), 
                  std::back_inserter(oldCliffCenters), 
                  [](MemoryMapTypes::MemoryMapDataConstPtr iter) -> Point2f { 
                    return MemoryMapData::MemoryMapDataCast<const MemoryMapData_Cliff>(iter)->pose.GetTranslation();
                  });

  DEV_ASSERT(std::all_of( oldCliffNodes.cbegin(), 
                          oldCliffNodes.cend(), 
                          [](const MemoryMapDataConstPtr& ptr){ return ptr->type == EContentType::Cliff; }),
                          "MapComponent.RefineNewCliffPose.MemoryMapDataTypesNotCliff");
  DEV_ASSERT(points.size() > 1, "MapComponent.RefineNewCliffPose.NotEnoughPointsToExtractLineFrom");

  // get the image extents given a set of edge-feature points
  auto xRange = std::minmax_element(points.begin(), points.end(), [](auto& a, auto& b) { return a.x() < b.x(); } );
  auto yRange = std::minmax_element(points.begin(), points.end(), [](auto& a, auto& b) { return a.y() < b.y(); } );
  const f32& xMin = xRange.first->x();
  const f32& yMin = yRange.first->y();
  const f32& xMax = xRange.second->x();
  const f32& yMax = yRange.second->y();

  // NOTE:
  // we assume 1mm = 1pixel in the binary image
  // for the purposes of creating an image that we
  // can run the hough transform on

  int rows = std::ceil(yMax-yMin);
  int cols = std::ceil(xMax-xMin);
  if(rows == 0 || cols == 0) {
    PRINT_NAMED_WARNING("MapComponent.RefineNewCliffPose.BinaryImageHasZeroRowCol","");
    return false;
  }

  if( rows*cols > kMaxPixelsUsedForHoughTransform ) {
    PRINT_NAMED_WARNING("MapComponent.RefineNewCliffPose.BinaryImageTooLarge","dims=(%d,%d)",rows,cols);
    return false;
  }

  // binary image containing the edge-feature points
  cv::Mat binImg = cv::Mat::zeros(rows, cols, CV_8UC1);
  std::for_each(points.begin(), points.end(), [&](const Point2f& point) {
    int i = std::floor(point.y() - yMin);
    int j = std::floor(point.x() - xMin);
    binImg.at<uint8_t>(i,j) = 255;
  });

  // 5 degrees resolution for the angle
  // => if we set this too low, we might get tonnes of lines which
  //    we will waste time iterating over and evaluating for best fit
  //
  // threshold is the number of required votes for a line to be detected
  // => set to 20, which is the minimum number of points needed. This is
  //    arbitrarily set low because the number of lines returned is
  //    reduced by other constants below, or by process of elimination
  //    in later stages when trying to find the "best" line relative to cliffs
  //
  // minLineLength is the number of points needed to compose a line
  // => set to 40mm since we'll usually see 60mm length lines if we are
  //    looking at a real edge. Requiring 2/3rds of the points is to
  //    ensure we get strongly detected candidates.
  //
  // maxLineGap is the largest width between two points to be in the same line
  // => 10mm gap between edge-feature points is used to discard highly fragmented
  //    edge detections (e.g. highly irregular patterned textures)
  std::vector<cv::Vec4i> linesInImg;
  cv::HoughLinesP(binImg, linesInImg, 1, 
                  DEG_TO_RAD(kHoughAngleResolution_deg), 
                  kHoughAccumThreshold, 
                  kHoughMinLineLength_mm, 
                  kHoughMaxLineGap_mm);

  if(linesInImg.size() == 0) {
    PRINT_CH_INFO("MapComponent","MapComponent.RefineNewCliffPose.NoLinesFoundInBinaryImage","%zd count of edge points", points.size());
    return false;
  }

  // helper lambda to transform the result of the Hough transform
  // cv::HoughLinesP returns line segments as 2 points on the line,
  // located on the extreme ends of the detection.
  const auto toCartesian = [&](const cv::Vec4i& seg) -> std::pair<Point2f, Point2f> {
    auto x1 = seg[0] + xMin;
    auto y1 = seg[1] + yMin;
    auto x2 = seg[2] + xMin;
    auto y2 = seg[3] + yMin;
    
    return {
      {x1, y1},
      {x2, y2}
    };
  };

  std::vector<std::pair<Point2f, Point2f> > linesInCartes;
  linesInCartes.reserve(linesInImg.size());
  std::for_each(linesInImg.begin(), linesInImg.end(), [&](const cv::Vec4i& lineInImg) {
    linesInCartes.push_back(toCartesian(lineInImg));
  });

  // helper lambda -- perpendicular distance Squared to a line from point
  const auto& perpDistSqToLine = [](const std::pair<Point2f, Point2f>& endPoints, const Point2f& testPoint) -> float {
    auto& x1 = endPoints.first.x();
    auto& y1 = endPoints.first.y();
    auto& x2 = endPoints.second.x();
    auto& y2 = endPoints.second.y();
    auto& x0 = testPoint.x();
    auto& y0 = testPoint.y();
    return pow( (y2-y1)*x0 - (x2-x1)*y0 + x2*y1 - y2*x1 , 2) / ( pow(y2-y1,2) + pow(x2-x1,2) );
  };

  // minimum perpendicular distance from a cliff to the hough-line
  // in order to consider this hough line as passing through the cliff
  // within 2cm radius = 400mm^2
  const float kMaxDistSqToCliff_mm2 = 400.0f; 

  // determine the best line to insert into the navmap as a newly detected edge
  // the best line is the highest scoring line based on:
  // + total number of cliffs it passes "near enough" (within 2cm) => numerator
  // + closest line to the cliff center
  // this is captured in the scoring formula:
  //
  //  SCORE = NUM_NEAR_CLIFFS / DIST_TO_NEAREST_CLIFF^2
  size_t lineIdx = linesInCartes.size();
  float maxScore = 0.f;
  for(size_t i=0; i<linesInCartes.size(); ++i) {
    float distSqToNewCliff = perpDistSqToLine(linesInCartes[i], newCliffCenter);
    if(distSqToNewCliff > kMaxDistSqToCliff_mm2) {
      continue;
    }

    // count number of old cliffs in "agreement" with this hough-line candidate
    size_t numNearOldCliffs = 0;
    for(size_t j=0; j<oldCliffCenters.size(); ++j) {
      float distSq = perpDistSqToLine(linesInCartes[i], oldCliffCenters[j]);
      if(distSq < kMaxDistSqToCliff_mm2) {
        numNearOldCliffs++;
      }
    }

    float score = (numNearOldCliffs+1) / distSqToNewCliff;
    if(score > maxScore) {
      lineIdx = i;
      maxScore = score;
    }
  }

  if(lineIdx < linesInCartes.size()) {
    auto& pp = linesInCartes[lineIdx]; // point pair
    auto& p1 = pp.first;

    // compute the corrected pose of the cliff:
    // we want to translate the cliff center pose to lie on the detected edge line
    // and reorient the pose s.t. y-axis is along the edge, x-axis points in the direction of "air"
    /*
                  x
          y      /
          :\    /
          : \  /
          :  \/
          :   :
          :   :
          :   :
          :   :
    ------.---.---------- edge
          y'  o'

    start by projecting the origin of the cliff frame
    and the head of the y-axis onto the detected edge

    the vector o'y' is the new y-axis, and the origin
    of the new cliff frame is o'. The new x-axis is
    found by taking the cross product of z with y'
    and thus the new pose is derived by finding the
    angle vector o'x' makes with the world-frame x-axis
    
    */
    Point2f lineUnitVec{ pp.second - pp.first };
    lineUnitVec.MakeUnitLength();
    auto projCliffCenter = p1 + lineUnitVec * Anki::DotProduct(lineUnitVec, newCliffCenter - p1);
    auto projYAxis = p1 + lineUnitVec * Anki::DotProduct(lineUnitVec, (Pose2d)newCliffPose * Y_AXIS_2D() - p1); // on the edgeLine
    auto correctedYAxis = projYAxis - projCliffCenter;
    auto correctedXAxis = Point2f(correctedYAxis.y(), -correctedYAxis.x()); // y^ cross z^ = x^
    Radians cliffAngleWrtWorld = std::atan2(correctedXAxis.y(), correctedXAxis.x());
    refinedCliffPose = Pose3d(cliffAngleWrtWorld, Z_AXIS_3D(), Point3f(projCliffCenter.x(), projCliffCenter.y(), 0.f));
    refinedCliffPose.SetParent(_robot->GetWorldOrigin());
    return true;
  } else {
    PRINT_CH_INFO("MapComponent","MapComponent.RefineNewCliffPose.NoAcceptableLinesFound","%zd candidate lines", linesInCartes.size());
  }
  return false;
}

}
}
