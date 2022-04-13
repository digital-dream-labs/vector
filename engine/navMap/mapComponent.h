/**
 * File: mapComponent.h
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
 
 #ifndef __Anki_Cozmo_MapComponent_H__
 #define __Anki_Cozmo_MapComponent_H__

#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/common/engine/math/polygon_fwd.h"

#include "engine/aiComponent/behaviorComponent/behaviorComponents_fwd.h"
#include "util/entityComponent/iDependencyManagedComponent.h"
#include "engine/overheadEdge.h"
#include "engine/navMap/iNavMap.h"
#include "engine/robotComponents_fwd.h"
#include "engine/engineTimeStamp.h"

#include "util/helpers/noncopyable.h"
#include "util/signals/simpleSignal_fwd.h"

#include <assert.h>
#include <string>
#include <list>
#include <map>

namespace Anki {
namespace Vector {

// Forward declarations
class Robot;
class ObservableObject;
  
class MapComponent : public IDependencyManagedComponent<RobotComponentID>, private Util::noncopyable
{
public: 
  explicit MapComponent();
  ~MapComponent();

  //////
  // IDependencyManagedComponent functions
  //////
  virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override;
  virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {
    dependencies.insert(RobotComponentID::CozmoContextWrapper);
  };
  virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {
    dependencies.insert(RobotComponentID::Vision);
    dependencies.insert(RobotComponentID::BlockWorld);
  };
  virtual void UpdateDependent(const RobotCompMap& dependentComps) override;
  //////
  // end IDependencyManagedComponent functions
  //////
  
  ////////////////////////////////////////////////////////////////////////////////
  // Update and init
  ////////////////////////////////////////////////////////////////////////////////

  
  void UpdateMapOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID);
  
  void UpdateRobotPose();

  void UpdateObjectPose(const ObservableObject& object, const Pose3d* oldPose, PoseState oldPoseState);
  
  // Processes the edges found in the given frame
  Result ProcessVisionOverheadEdges(const OverheadEdgeFrame& frameInfo);
  
  // add obstacles detected from the driving classifier to navMap
  void AddDetectedObstacles(const OverheadEdgeFrame& edgeObstacle);

  ////////////////////////////////////////////////////////////////////////////////
  // Message handling / dispatch
  ////////////////////////////////////////////////////////////////////////////////
  
  // flag all interesting edges in front of the robot (using ground plane ROI) as uncertain, meaning we want
  // the robot to grab new edges since we don't trust the ones we currently have in front of us
  void FlagGroundPlaneROIInterestingEdgesAsUncertain();
  
  // flags any interesting edges in the given quad as not interesting anymore. Quad should be passed wrt current origin
  void FlagQuadAsNotInterestingEdges(const Quad2f& quadWRTOrigin);

  // set the region defined by the given poly with the provided data.
  void InsertData(const Poly2f& polyWRTOrigin, const MemoryMapData& data);
  void InsertData(const MemoryMapTypes::MemoryMapRegion& region, const MemoryMapData& data);
  
  // flags all current interesting edges as too small to give useful information
  void FlagInterestingEdgesAsUseless();
  
  // marks any prox obstacles in a small area in front of the robot as explored
  void FlagProxObstaclesUsingPose();
  
  // moves the frontier of explored prox obstacles into touching unexplored prox ostacles
  bool FlagProxObstaclesTouchingExplored();

  // create a new memory map from current robot frame of reference.
  void CreateLocalizedMemoryMap(PoseOriginID_t worldOriginID);
  
  // Publish navMap to the Viz channel / Web / SDK
  void BroadcastMapToViz(const MemoryMapTypes::MapBroadcastData& mapData) const;
  void BroadcastMapToWeb(const MemoryMapTypes::MapBroadcastData& mapData) const;
  void BroadcastMapToSDK(const MemoryMapTypes::MapBroadcastData& mapData) const;
  
  // clear the space in the memory map between the robot and observed markers for the given object,
  // because if we saw the marker, it means there's nothing between us and the marker.
  // The observed markers are obtained querying the current marker observation time
  void ClearRobotToMarkers(const ObservableObject* object);

  // clear the space between the robot and the line segment defined by points p and q. The base of
  // the region is a line segment of fixed length that is perpendicular to the robot direction
  void ClearRobotToEdge(const Point2f& p, const Point2f& q, const RobotTimeStamp_t t);

  // flag the region as clear of all positive obstacles
  void ClearRegion(const BoundedConvexSet2f& region, const RobotTimeStamp_t t);

  // flag the region as a prox obstacle
  void AddProxData(const BoundedConvexSet2f& region, const MemoryMapData& data);

  // return true of the specified region contains any objects of known collision types
  bool CheckForCollisions(const BoundedConvexSet2f& region) const;
  bool CheckForCollisions(const BoundedConvexSet2f& region, const MemoryMapTypes::NodePredicate& pred) const;

  // returns the accumulated area of cells in mm^2 in the current map that satisfy the predicate (and region, if supplied)
  float GetCollisionArea(const BoundedConvexSet2f& region) const;

  // Remove all prox obstacles from the map.
  // CAUTION: This will entirely remove _all_ information about prox
  // obstacles. This should almost never be necessary. Is this really
  // what you want??
  void RemoveAllProxObstacles();
  
  void SetUseProxObstaclesInPlanning(bool enable);
  bool GetUseProxObstaclesInPlanning() const { return _enableProxCollisions; }
  
  // marks observable object as unobserved
  void MarkObjectUnobserved(const ObservableObject& object);
  
  void SendDASInfoAboutCurrentMap() const { SendDASInfoAboutMap(_currentMapOriginID); }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Accessors
  ////////////////////////////////////////////////////////////////////////////////
  
  std::shared_ptr<const INavMap> GetCurrentMemoryMap() const {return GetCurrentMemoryMapHelper(); }  
  std::shared_ptr<INavMap> GetCurrentMemoryMap()       {return GetCurrentMemoryMapHelper(); }  


  // template for all events we subscribe to
  template<typename T>
  void HandleMessage(const T& msg);
  
private:
  std::shared_ptr<INavMap> GetCurrentMemoryMapHelper() const;
  
  // remove current renders for all maps if any
  void ClearRender();
  
  // update broadcast dirty flags with new changes
  void UpdateBroadcastFlags(bool wasChanged);

  // enable/disable rendering of the memory maps
  void SetRenderEnabled(bool enabled);

  // updates the objects reported in curOrigin that are moving to the relocalizedOrigin by virtue of rejiggering
  void UpdateOriginsOfObjects(PoseOriginID_t curOriginID, PoseOriginID_t relocalizedOriginID);

  // add/remove the given object to/from the memory map
  void AddObservableObject(const ObservableObject& object, const Pose3d& newPose);
  void RemoveObservableObject(const ObservableObject& object, PoseOriginID_t originID);

  // search the navMap and remove any nodes that have exceeded their timeout period
  void TimeoutObjects();
  
  void SendDASInfoAboutMap(const PoseOriginID_t& mapOriginID) const;

  // given a set of vision-detected edge points (projected to the robot frame)
  // determines a refinement on the newest cliff's pose, based on known cliff
  // data.
  // Returns true if such a refinement exists
  bool RefineNewCliffPose(const std::vector<Point2f>& points,
                          MemoryMapTypes::MemoryMapDataConstPtr newCliffNode,
                          const std::vector<MemoryMapTypes::MemoryMapDataConstPtr>& oldCliffNodes,
                          Pose3d& refinedCliffPose) const;

  // helper method to retrieve all unique cliff positions in the current map
  // note: unique because multiple cells in the QT are associated with the same
  //       cliff object, which has one pose (determined at the time it was sensed)
  Result FindSensorDetectedCliffs(std::vector<MemoryMapTypes::MemoryMapDataConstPtr>& cliffNodes) const;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Vision border detection
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // adds edges from the given frame to the world info
  Result AddVisionOverheadEdges(const OverheadEdgeFrame& frameInfo);
    
  // poses we have sent to the memory map for objects we know, in each origin
  struct PoseInMapInfo {
    PoseInMapInfo(const Pose3d& p, bool inMap) : pose(p), isInMap(inMap) {}
    PoseInMapInfo() : pose(), isInMap(false) {}
    Pose3d pose;
    bool isInMap; // if true the pose was sent to the map, if false the pose was removed from the map
  };
  
  struct MapInfo {
    std::shared_ptr<INavMap> map;
    EngineTimeStamp_t activationTime_ms;
    TimeStamp_t activeDuration_ms;
  };
  
  using MapTable                  = std::map<PoseOriginID_t, MapInfo>;
  using OriginToPoseInMapInfo     = std::map<PoseOriginID_t, PoseInMapInfo>;
  using ObjectIdToPosesPerOrigin  = std::map<int, OriginToPoseInMapInfo>;
  using EventHandles              = std::vector<Signal::SmartHandle>;
  
  Robot*                          _robot;
  EventHandles                    _eventHandles;
  MapTable                        _navMaps;
  PoseOriginID_t                  _currentMapOriginID;
  ObjectIdToPosesPerOrigin        _reportedPoses;
  Pose3d                          _reportedRobotPose;
  RobotTimeStamp_t                _nextTimeoutUpdate_ms;

  // use multiple dirty flags to broadcast to different channels in case they have different broadcast rates
  bool                            _vizMessageDirty;
  bool                            _gameMessageDirty;
  bool                            _webMessageDirty;
  
  bool                            _isRenderEnabled;
  float                           _broadcastRate_sec = -1.0f;      // (Negative means don't send)

  // config variable for conditionally enabling/disabling prox obstacles in planning
  bool                            _enableProxCollisions;
};

}
}

#endif
