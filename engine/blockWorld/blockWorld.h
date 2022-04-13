/**
 * File: blockWorld.h
 *
 * Author: Andrew Stein (andrew)
 * Created: 10/1/2013
 *
 * Description: Defines a container for tracking the state of all objects in Cozmo's world.
 *
 * Copyright: Anki, Inc. 2013
 *
 **/

#ifndef ANKI_COZMO_BLOCKWORLD_H
#define ANKI_COZMO_BLOCKWORLD_H

#include "engine/aiComponent/behaviorComponent/behaviorComponents_fwd.h"

#include "util/entityComponent/iDependencyManagedComponent.h"
#include "engine/robotComponents_fwd.h"

#include "clad/types/objectTypes.h"

#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/observableObjectLibrary_fwd.h"

#include "util/signals/simpleSignal_fwd.h"

#include <map>
#include <vector>

namespace Anki {
namespace Vector {
  
// Forward declarations:
class Robot;
class Block;
class BlockWorldFilter;
class IExternalInterface;
class ObservableObject;

namespace ExternalInterface {
  struct RobotDeletedLocatedObject;
  struct RobotObservedObject;
}

// BlockWorld is updated at the robot component level, same as BehaviorComponent
// Therefore BCComponents (which are managed by BehaviorComponent) can't declare dependencies on BlockWorld
// since when it's Init/Update relative to BehaviorComponent must be declared by BehaviorComponent explicitly,
// not by individual components within BehaviorComponent
class BlockWorld : public UnreliableComponent<BCComponentID>,
                   public IDependencyManagedComponent<RobotComponentID>
{
  using ActiveID = s32;  // TODO: Change this to u32 and use 0 as invalid
  using FactoryID = std::string;
  
public:
  using ObservableObjectLibrary = Vision::ObservableObjectLibrary<ObservableObject>;
  
  BlockWorld();
  
  ~BlockWorld();

  //////
  // IDependencyManagedComponent functions
  //////
  virtual void InitDependent(Robot* robot, const RobotCompMap& dependentComps) override final;
  virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {
    dependencies.insert(RobotComponentID::CozmoContextWrapper);
  };
  virtual void UpdateDependent(const RobotCompMap& dependentComps) override final;
  virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {
    dependencies.insert(RobotComponentID::CubeComms);
    dependencies.insert(RobotComponentID::Vision);
  };

  // Prevent hiding function warnings by exposing the (valid) unreliable component methods
  using UnreliableComponent<BCComponentID>::InitDependent;
  using UnreliableComponent<BCComponentID>::GetInitDependencies;
  using UnreliableComponent<BCComponentID>::UpdateDependent;
  using UnreliableComponent<BCComponentID>::GetUpdateDependencies;
  //////
  // end IDependencyManagedComponent functions
  //////

  
  // Update the BlockWorld's state by processing all queued ObservedMarkers
  // and updating robot's and objects' poses from them.
  Result UpdateObservedMarkers(const std::list<Vision::ObservedMarker>& observedMarkers);
  
  ObjectID CreateFixedCustomObject(const Pose3d& p, const f32 xSize_mm, const f32 ySize_mm, const f32 zSize_mm);

  // Defines an object that could be observed later.
  // Does not add an instance of this object to the existing objects in the world.
  // Instead, provides the definition of an object that could be instantiated based on observations.
  Result DefineObject(std::unique_ptr<const ObservableObject>&& object);

  // Creates and adds an active object of the appropriate type based on factoryID to the connected objects container.
  // Note there is no information about pose, so no instance of this object in the current origin is updated.
  // However, if an object of the same type is found as an unconnected object, the objectID is inherited, and
  // the unconnected instances (in origins) become linked to this connected object instance.
  // It returns the new or inherited objectID on success, or invalid objectID if it fails.
  ObjectID AddConnectedBlock(const ActiveID& activeID,
                                    const FactoryID& factoryID,
                                    const ObjectType& objectType);
  // Removes connected object from the connected objects container. Returns matching objectID if found
  ObjectID RemoveConnectedBlock(const ActiveID& activeID);

  // Adds the given object to the BlockWorld according to its current objectID and pose. The objectID is expected
  // to be set, and not be currently in use in the BlockWorld. Otherwise it's a sign that something went
  // wrong matching the current BlockWorld objects
  void AddLocatedObject(const std::shared_ptr<ObservableObject>& object);

  // Set the charger's pose relative to the robot's pose as if the robot is on the charger contacts. If the charger
  // does not exist in the current origin, it will be created (i.e., it is safe to call this even if there is no
  // known charger in the world).
  void SetRobotOnChargerContacts();
  
  // Set the pose of the object with the given ID. If makeWrtOrigin is true, then the given pose will be converted
  // to be with respect to the robot's world origin before assigning it to the object. Otherwise, the passed-in pose
  // is assigned directly to the object.
  Result SetObjectPose(const ObjectID& objId,
                       const Pose3d& newPose,
                       const PoseState poseState,
                       const bool makeWrtOrigin = true);
  
  // Set the given object's pose state to 'dirty'
  void MarkObjectDirty(ObservableObject* object);
  
  // Called when robot gets delocalized in order to do internal bookkeeping and broadcast updated object states
  void OnRobotDelocalized(PoseOriginID_t newWorldOriginID);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Object Access
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // Delete located instances will delete object instances of both active and passive objects. However
  // from connected objects, only the located instances are affected. The unlocated instances, that are stored
  // regardless of pose, are not affected by this. Passive objects don't have connected instances.
  void DeleteLocatedObjects(const BlockWorldFilter& filter);   // objects that pass the filter will be deleted
  
  // Clear the object from shared uses, like localization, selection or carrying, etc. So that it can be removed
  // without those system lingering
  void ClearLocatedObjectByIDInCurOrigin(const ObjectID& withID);
  void ClearLocatedObject(ObservableObject* object);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Get objects by ID or activeID
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Return a pointer to an object with the specified ID in the current world coordinate frame.
  // If that object does not exist in the current origin, nullptr is returned.
  // If you want an object regardless of its pose, use GetConnectedBlockById instead.
  // For more complex queries, use one of the Find methods with a BlockWorldFilter.
  inline const ObservableObject* GetLocatedObjectByID(const ObjectID& objectID) const;
  inline       ObservableObject* GetLocatedObjectByID(const ObjectID& objectID);
  
  // Returns a pointer to a connected object with the specified objectID without any pose information. If you need to obtain
  // the instance of this object in the current origin (if it exists), you can do so with GetLocatedObjectByID
  inline const Block* GetConnectedBlockByID(const ObjectID& objectID) const;
  inline       Block* GetConnectedBlockByID(const ObjectID& objectID);
  
  // Returns a pointer to a connected object with the specified objectID without any pose information. If you need to obtain
  // the instance of this object in the current origin (if it exists), you can do so with GetLocatedObjectByID
  inline const Block* GetConnectedBlockByActiveID(const ActiveID& activeID) const;
  inline       Block* GetConnectedBlockByActiveID(const ActiveID& activeID);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Find connected objects by filter query
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Returns first object matching filter, among objects that are currently connected (regardless of pose)
  // Note OriginMode in the filter is ignored (TODO: provide ConnectedFilter API without origin)
  inline const Block* FindConnectedMatchingBlock(const BlockWorldFilter& filter) const;
  inline       Block* FindConnectedMatchingBlock(const BlockWorldFilter& filter);

  // Returns (in arguments) all objects matching a filter, among objects that are currently connected (regardless
  // of pose). Note OriginMode in the filter is ignored (TODO: provide ConnectedFilter API without origin)
  // NOTE: does not clear result (thus can be used multiple times with the same vector)
  void FindConnectedMatchingBlocks(const BlockWorldFilter& filter, std::vector<const Block*>& result) const;
  void FindConnectedMatchingBlocks(const BlockWorldFilter& filter, std::vector<Block*>& result);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Find objects by filter query
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Applies given modifier to all located objects that match 'filter'
  using ModifierFcn = std::function<void(ObservableObject*)>;
  inline void ModifyLocatedObjects(const ModifierFcn& modifierFcn, const BlockWorldFilter& filter);
  
  // Returns (in arguments) all objects matching a filter, among objects that are currently located (their pose
  // is valid in the origins matching the filter)
  // NOTE: does not clear result (thus can be used multiple times with the same vector)
  void FindLocatedMatchingObjects(const BlockWorldFilter& filter, std::vector<const ObservableObject*>& result) const;
  void FindLocatedMatchingObjects(const BlockWorldFilter& filter, std::vector<ObservableObject*>& result);
  
  // Returns first object matching filter, among objects that are currently located (their pose is valid in
  // the origins matching the filter)
  inline const ObservableObject* FindLocatedMatchingObject(const BlockWorldFilter& filter) const;
  inline       ObservableObject* FindLocatedMatchingObject(const BlockWorldFilter& filter);
  
  // Finds an object closer than the given distance (optional) (rotation -- not implemented yet) with respect
  // to the given pose. Returns nullptr if no objects match, closest one if multiple matches are found.
  inline const ObservableObject* FindLocatedObjectClosestTo(const Pose3d& pose,
                                                            const BlockWorldFilter& filter) const;
  inline       ObservableObject* FindLocatedObjectClosestTo(const Pose3d& pose,
                                                            const BlockWorldFilter& filter);
  inline const ObservableObject* FindLocatedObjectClosestTo(const Pose3d& pose,
                                                            const Vec3f& distThreshold,
                                                            const BlockWorldFilter& filter) const;
  inline       ObservableObject* FindLocatedObjectClosestTo(const Pose3d& pose,
                                                            const Vec3f& distThreshold,
                                                            const BlockWorldFilter& filter);
  
  // Finds a matching object (one with the same type) that is closest to the given object, within the
  // specified distance and angle thresholds.
  // Returns nullptr if none found.
  inline const ObservableObject* FindLocatedClosestMatchingObject(const ObservableObject& object,
                                                                  const Vec3f& distThreshold,
                                                                  const Radians& angleThreshold,
                                                                  const BlockWorldFilter& filter) const;
  inline       ObservableObject* FindLocatedClosestMatchingObject(const ObservableObject& object,
                                                                  const Vec3f& distThreshold,
                                                                  const Radians& angleThreshold,
                                                                  const BlockWorldFilter& filter);
  
  // Finds the object of the given type that is closest to the given pose, within the specified distance and
  // angle thresholds.
  // Returns nullptr if none found.
  inline const ObservableObject* FindLocatedClosestMatchingObject(ObjectType withType,
                                                                  const Pose3d& pose,
                                                                  const Vec3f& distThreshold,
                                                                  const Radians& angleThreshold,
                                                                  const BlockWorldFilter& filter) const;
  inline       ObservableObject* FindLocatedClosestMatchingObject(ObjectType withType,
                                                                  const Pose3d& pose,
                                                                  const Vec3f& distThreshold,
                                                                  const Radians& angleThreshold,
                                                                  const BlockWorldFilter& filter);
  
  const ObservableObject* FindMostRecentlyObservedObject(const BlockWorldFilter& filter) const;
  
  // Finds existing objects whose XY bounding boxes intersect with objectSeen's XY bounding box, and pass
  // the given filter.
  void FindLocatedIntersectingObjects(const ObservableObject* objectSeen,
                                      std::vector<const ObservableObject*>& intersectingExistingObjects,
                                      f32 padding_mm,
                                      const BlockWorldFilter& filter) const;
  void FindLocatedIntersectingObjects(const ObservableObject* objectSeen,
                                      std::vector<ObservableObject*>& intersectingExistingObjects,
                                      f32 padding_mm,
                                      const BlockWorldFilter& filter);
  // same as above, except it takes Quad2f instead of an object
  void FindLocatedIntersectingObjects(const Quad2f& quad,
                                      std::vector<const ObservableObject*> &intersectingExistingObjects,
                                      f32 padding_mm,
                                      const BlockWorldFilter& filter) const;
  void FindLocatedIntersectingObjects(const Quad2f& quad,
                                      std::vector<ObservableObject*> &intersectingExistingObjects,
                                      f32 padding_mm,
                                      const BlockWorldFilter& filter);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // BoundingBoxes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Finds all blocks in the world whose centers are within the specified
  // heights off the ground (z dimension, relative to world origin!) and
  // returns a vector of quads of their outlines on the ground plane (z=0).
  // Can also pad the bounding boxes by a specified amount.
  // Optionally, will filter according to given BlockWorldFilter.
  void GetLocatedObjectBoundingBoxesXY(const f32 minHeight, const f32 maxHeight, const f32 padding,
                                       std::vector<std::pair<Quad2f,ObjectID> >& boundingBoxes,
                                       const BlockWorldFilter& filter) const;

  // Wrapper for GetLocatedObjectBoundingBoxesXY that returns bounding boxes of objects that are
  // obstacles given the robot's current z height.
  void GetObstacles(std::vector<std::pair<Quad2f,ObjectID> >& boundingBoxes,
                    const f32 padding = 0.f) const;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Localization
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Returns true if the given origin is a zombie origin. A zombie origin means that no localizable objects are
  // currently in that origin/frame, which would make it impossible to relocalize to any other origin. Note that
  // current origin is not a zombie even if it doesn't have any objects yet.
  bool IsZombiePoseOrigin(PoseOriginID_t originID) const;
  
  // Returns true if there are any localizable objects remaining in the specified origin. Passing in
  // PoseOriginList::UnknownOriginID will cause this to check for any localizable objects among _all_ origins.
  bool AnyRemainingLocalizableObjects(PoseOriginID_t origin) const;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Others. TODO Categorize/organize
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Get/Set currently-selected object
  ObjectID GetSelectedObject() const { return _selectedObjectID; }
  void     CycleSelectedObject();
  
  // Try to select the object with the specified ID. Return true if that
  // object ID is found and the object is successfully selected.
  bool SelectObject(const ObjectID& objectID);
  void DeselectCurrentObject();

  // Find all objects with the given parent and update them to have flatten
  // their objects w.r.t. the origin. Call this when the robot rejiggers
  // origins.
  Result UpdateObjectOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID);
  
  // Find the given objectID in the given origin, and update it so that it is
  // stored according to its _current_ origin. (Move from old origin to current origin.)
  // If the origin is already correct, nothing changes. If the objectID is not
  // found in the given origin, RESULT_FAIL is returned.
  Result UpdateObjectOrigin(const ObjectID& objectID, PoseOriginID_t oldOriginID);
  
  // Looks for any origins that are 'zombies' (see comment for IsZombiePoseOrigin()) and removes them.
  void DeleteZombieOrigins();
  
  size_t GetNumAliveOrigins() const { return _locatedObjects.size(); }
  
  //
  // Visualization
  //

  // Call every existing object's Visualize() method and call the
  // VisualizePreActionPoses() on the currently-selected ActionableObject.
  void DrawAllObjects() const;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Messages
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // template for all events we subscribe to
  template<typename T>
  void HandleMessage(const T& msg);

private:

  // active objects
  using ConnectedObjectsContainer_t = std::vector<std::shared_ptr<Block>>;

  // observable objects
  using ObjectsContainer_t = std::vector<std::shared_ptr<ObservableObject>>;
  using ObjectsByOrigin_t  = std::map<PoseOriginID_t, ObjectsContainer_t>;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Helpers for accessors and queries
  // Note: these helpers return non-const pointers despite being marked const,
  // because they are private helpers wrapped by const/non-const public methods.
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // Located by filter (most basic, other helpers rely on it)
  // If modifierFcn is non-null, it is applied to the matching object. Furthermore, if returnFirstFound is false,
  // then modifierFcn is applied to all matching objects, and the final object that matched is returned.
  ObservableObject* FindLocatedObjectHelper(const BlockWorldFilter& filter,
                                            const ModifierFcn& modifierFcn = nullptr,
                                            bool returnFirstFound = false) const;
  
  // Connected by filter (most basic, other helpers rely on it)
  // If modifierFcn is non-null, it is applied to the matching object. Furthermore, if returnFirstFound is false,
  // then modifierFcn is applied to all matching objects, and the final object that matched is returned.
  Block* FindConnectedObjectHelper(const BlockWorldFilter& filter,
                                   const ModifierFcn& modifierFcn = nullptr,
                                   bool returnFirstFound = false) const;

  // By ID or activeID
  ObservableObject* GetLocatedObjectByIdHelper(const ObjectID& objectID) const;
  Block* GetConnectedBlockByIdHelper(const ObjectID& objectID) const;
  Block* GetConnectedBlockByActiveIdHelper(const ActiveID& activeID) const;
  
  // By location/pose
  ObservableObject* FindLocatedObjectClosestToHelper(const Pose3d& pose,
                                                     const Vec3f&  distThreshold,
                                                     const BlockWorldFilter& filterIn) const;

  // Matching object
  ObservableObject* FindLocatedClosestMatchingObjectHelper(const ObservableObject& object,
                                                           const Vec3f& distThreshold,
                                                           const Radians& angleThreshold,
                                                           const BlockWorldFilter& filter) const;
  
  // Matching type and location
  ObservableObject* FindLocatedClosestMatchingTypeHelper(ObjectType withType,
                                                         const Pose3d& pose,
                                                         const Vec3f& distThreshold,
                                                         const Radians& angleThreshold,
                                                         const BlockWorldFilter& filter) const;
  
  // Helper for finding the object with a specified ID in the given container.
  // Returns an iterator to that object's entry.
  ObjectsContainer_t::const_iterator FindInContainerWithID(const ObjectsContainer_t& container,
                                                           const ObjectID& objectID) const;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Looks for objects that should have been observed and deletes them.
  void CheckForUnobservedObjects(RobotTimeStamp_t atTimestamp);
  
  // Checks all objects in the current origin to see if any of them intersect with the robot's bounding box, and if
  // so marks them dirty. The idea is that if the robot is driving through where an object should be, then either
  // the object is not there or its pose is now inaccurate due to being bumped by the robot.
  void CheckForRobotObjectCollisions();
  
  // Returns true if the given object intersects the robot's current bounding box.
  bool IntersectsRobotBoundingBox(const ObservableObject* object) const;
  
  // This method gets called whenever we have new observations available from the vision system. It processes the
  // candidate object observations given in objectsSeenRaw, and appropriately adds to or updates our record of
  // located objects (via UpdateKnownObjects). This updates the poses of objects, localizes to the charger if
  // appropriate, and broadcasts information about the observations.
  Result ProcessVisualObservations(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeenRaw,
                                   const RobotTimeStamp_t atTimestamp);
  
  // Try to match the objects in objectsSeen to existing located objects. If no match is found, the new object is
  // added to our records. If a match is found, update the located instance's pose and metadata (e.g., pose state).
  // If ignoreCharger = true, then any charger objects are _not_ updated.
  void UpdateKnownObjects(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeen,
                          const RobotTimeStamp_t atTimestamp,
                          const bool ignoreCharger = false);
  
  // Given a list of raw object observations, return a 'filtered' list of objects using the following logic:
  //   - Ignore objects which were observed from too far away
  //   - If multiple instaces of a 'unique' object were observed, ignore all but the closest one.
  //   - The returned list of objects is guaranteed to be sorted on observation distance.
  std::vector<std::shared_ptr<ObservableObject>> FilterRawObservedObjects(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeenRaw);
  
  // Clear the object from shared uses, like localization, selection or carrying, etc. So that it can be removed
  // without those system lingering
  void ClearLocatedObjectHelper(ObservableObject* object);
  
  Result BroadcastObjectObservation(const ObservableObject* observedObject) const;

  // broadcast currently located objects (in current origin)
  void BroadcastLocatedObjectStates();
  // broadcast currently connected objects (regardless of pose states in any origin)
  void BroadcastConnectedObjects();
  
  void SetupEventHandlers(IExternalInterface& externalInterface);
  
  void SanityCheckBookkeeping() const;
  
  void SendObjectUpdateToWebViz( const ExternalInterface::RobotDeletedLocatedObject& msg ) const;
  void SendObjectUpdateToWebViz( const ExternalInterface::RobotObservedObject& msg ) const;
  
  //
  // Member Variables
  //
  
  Robot*             _robot = nullptr;
  
  // Store all known observable objects (these are everything we know about,
  // separated by class of object, not necessarily what we've actually seen
  // yet, but what everything we are aware of)
  ObservableObjectLibrary _objectLibrary;
  
  // Objects that we know about because they have connected, but for which we may or may not know their location.
  // The instances of objects in this container are expected to NEVER have a valid Pose/PoseState. If they are
  // present in any origin, a copy of the object with the proper pose will be placed in the located objects container.
  ConnectedObjectsContainer_t _connectedObjects;

  // Objects that we have located indexed by the origin they belong to.
  // The instances of objects in this container are expected to always have a valid Pose/PoseState. If they are
  // lost from an origin (for example by being unobserved), their master copy should be available through the
  // connected objects container.
  ObjectsByOrigin_t _locatedObjects;
  
  ObjectID _selectedObjectID;
  
  std::vector<Signal::SmartHandle> _eventHandles;
  
  // Note: This is only required for SDK v0.5.1 compatibility
  ObjectFamily LegacyGetObjectFamily(const ObservableObject* const object) const;
}; // class BlockWorld


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::GetLocatedObjectByID(const ObjectID& objectID) const {
  return GetLocatedObjectByIdHelper(objectID); // returns const*
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::GetLocatedObjectByID(const ObjectID& objectID) {
  return GetLocatedObjectByIdHelper(objectID); // returns non-const*
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const Block* BlockWorld::GetConnectedBlockByID(const ObjectID& objectID) const {
  return GetConnectedBlockByIdHelper(objectID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::GetConnectedBlockByID(const ObjectID& objectID) {
  return GetConnectedBlockByIdHelper(objectID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const Block* BlockWorld::GetConnectedBlockByActiveID(const ActiveID& activeID) const {
  return GetConnectedBlockByActiveIdHelper(activeID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::GetConnectedBlockByActiveID(const ActiveID& activeID)
{
  return GetConnectedBlockByActiveIdHelper(activeID);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const Block* BlockWorld::FindConnectedMatchingBlock(const BlockWorldFilter& filter) const
{
  return FindConnectedObjectHelper(filter); // returns non-const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::FindConnectedMatchingBlock(const BlockWorldFilter& filter)
{
  return FindConnectedObjectHelper(filter); // returns non-const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::ModifyLocatedObjects(const ModifierFcn& modifierFcn, const BlockWorldFilter& filter)
{
  if(modifierFcn == nullptr) {
    PRINT_NAMED_WARNING("BlockWorld.ModifyLocatedObjects.NullModifierFcn", "Consider just using FilterLocatedObjects?");
  }
  FindLocatedObjectHelper(filter, modifierFcn, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindLocatedMatchingObject(const BlockWorldFilter& filter) const
{
  return FindLocatedObjectHelper(filter, nullptr, true); // returns const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedMatchingObject(const BlockWorldFilter& filter)
{
  return FindLocatedObjectHelper(filter, nullptr, true); // returns non-const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindLocatedObjectClosestTo(const Pose3d& pose,
                                                               const BlockWorldFilter& filter) const
{
  return FindLocatedObjectClosestTo(pose, Vec3f{FLT_MAX}, filter); // returns const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedObjectClosestTo(const Pose3d& pose,
                                                         const BlockWorldFilter& filter)
{
  return FindLocatedObjectClosestTo(pose, Vec3f{FLT_MAX}, filter); // returns non-const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindLocatedObjectClosestTo(const Pose3d& pose,
                                                               const Vec3f&  distThreshold,
                                                               const BlockWorldFilter& filter) const
{
  return FindLocatedObjectClosestToHelper(pose, distThreshold, filter); // returns const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedObjectClosestTo(const Pose3d& pose,
                                                         const Vec3f&  distThreshold,
                                                         const BlockWorldFilter& filter)
{
  return FindLocatedObjectClosestToHelper(pose, distThreshold, filter); // returns non-const
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindLocatedClosestMatchingObject(const ObservableObject& object,
                                                                     const Vec3f& distThreshold,
                                                                     const Radians& angleThreshold,
                                                                    const BlockWorldFilter& filter) const
{
  return FindLocatedClosestMatchingObjectHelper(object, distThreshold, angleThreshold, filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedClosestMatchingObject(const ObservableObject& object,
                                                                const Vec3f& distThreshold,
                                                                const Radians& angleThreshold,
                                                                const BlockWorldFilter& filter)
{
  return FindLocatedClosestMatchingObjectHelper(object, distThreshold, angleThreshold, filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindLocatedClosestMatchingObject(ObjectType withType,
                                                                     const Pose3d& pose,
                                                                     const Vec3f& distThreshold,
                                                                     const Radians& angleThreshold,
                                                                     const BlockWorldFilter& filter) const
{
  return FindLocatedClosestMatchingTypeHelper(withType, pose, distThreshold, angleThreshold, filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedClosestMatchingObject(ObjectType withType,
                                                               const Pose3d& pose,
                                                               const Vec3f& distThreshold,
                                                               const Radians& angleThreshold,
                                                               const BlockWorldFilter& filter)
{
  return FindLocatedClosestMatchingTypeHelper(withType, pose, distThreshold, angleThreshold, filter);
}

} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BLOCKWORLD_H
