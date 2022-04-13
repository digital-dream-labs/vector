/**
 * File: blockWorld.cpp
 *
 * Author: Andrew Stein (andrew)
 * Created: 10/1/2013
 *
 * Description: Implements a container for tracking the state of all objects in Cozmo's world.
 *
 * Copyright: Anki, Inc. 2013
 *
 **/
#include "engine/blockWorld/blockWorld.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "coretech/common/engine/math/poseOriginList.h"
#include "coretech/common/engine/math/quad.h"
#include "coretech/common/shared/math/rect.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/common/shared/utilities_shared.h"
#include "engine/aiComponent/aiWhiteboard.h"
#include "engine/aiComponent/aiComponent.h"
#include "engine/ankiEventUtil.h"
#include "engine/block.h"
#include "engine/blockWorld/blockWorldFilter.h"
#include "engine/charger.h"
#include "engine/components/carryingComponent.h"
#include "engine/components/sensors/cliffSensorComponent.h"
#include "engine/components/dockingComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/components/sensors/proxSensorComponent.h"
#include "engine/components/visionComponent.h"
#include "engine/cozmoContext.h"
#include "engine/customObject.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/namedColors/namedColors.h"
#include "engine/navMap/mapComponent.h"
#include "engine/navMap/memoryMap/data/memoryMapData_Cliff.h"
#include "engine/robot.h"
#include "engine/robotInterface/messageHandler.h"
#include "engine/robotStateHistory.h"
#include "engine/viz/vizManager.h"
#include "coretech/vision/engine/observableObjectLibrary.h"
#include "coretech/vision/engine/visionMarker.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/global/globalDefinitions.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/DAS.h"
#include "util/math/math.h"
#include "webServerProcess/src/webVizSender.h"

// Giving this its own local define, in case we want to control it independently of DEV_CHEATS / SHIPPING, etc.
#define ENABLE_DRAWING ANKI_DEV_CHEATS

#define LOG_CHANNEL "BlockWorld"

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BlockWorld::BlockWorld()
: UnreliableComponent<BCComponentID>(this, BCComponentID::BlockWorld)
, IDependencyManagedComponent<RobotComponentID>(this, RobotComponentID::BlockWorld)
{
} // BlockWorld() Constructor

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::InitDependent(Robot* robot, const RobotCompMap& dependentComps)
{
  _robot = robot;
  DEV_ASSERT(_robot != nullptr, "BlockWorld.Constructor.InvalidRobot");
  
  //////////////////////////////////////////////////////////////////////////
  // 1x1 Light Cubes
  //
  DefineObject(std::make_unique<Block>(ObjectType::Block_LIGHTCUBE1));
#ifdef SIMULATOR
  // VIC-12886 These object types are only used in Webots tests (not in the real world), so only define them if this
  // is sim. The physical robot can sometimes hallucinate these objects, which causes issues.
  DefineObject(std::make_unique<Block>(ObjectType::Block_LIGHTCUBE2));
  DefineObject(std::make_unique<Block>(ObjectType::Block_LIGHTCUBE3));
#endif

  //////////////////////////////////////////////////////////////////////////
  // Charger
  //
  DefineObject(std::make_unique<Charger>());

  if(_robot->HasExternalInterface())
  {
    SetupEventHandlers(*_robot->GetExternalInterface());
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::UpdateDependent(const RobotCompMap& dependentComps)
{
  // Check for any objects that overlap with the robot's position, and mark them dirty
  CheckForRobotObjectCollisions();
  
  if (ANKI_DEVELOPER_CODE) {
    SanityCheckBookkeeping();
  }
}


void BlockWorld::SetupEventHandlers(IExternalInterface& externalInterface)
{
  using namespace ExternalInterface;
  auto helper = MakeAnkiEventUtil(externalInterface, *this, _eventHandles);
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DeleteAllCustomObjects>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::UndefineAllCustomMarkerObjects>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DeleteCustomMarkerObjects>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DeleteFixedCustomObjects>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::SelectNextObject>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::CreateFixedCustomObject>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DefineCustomBox>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DefineCustomCube>();
  helper.SubscribeGameToEngine<MessageGameToEngineTag::DefineCustomWall>();
}

BlockWorld::~BlockWorld()
{

} // ~BlockWorld() Destructor

Result BlockWorld::DefineObject(std::unique_ptr<const ObservableObject>&& object)
{
  // Store due to std::move
  const ObjectType objType = object->GetType();

  // Find objects that already exist with this type
  BlockWorldFilter filter;
  filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  filter.AddAllowedType(objType);
  ObservableObject* objWithType = FindLocatedMatchingObject(filter);
  const bool redefiningExistingType = (objWithType != nullptr);

  const Result addResult = _objectLibrary.AddObject(std::move(object));

  if(RESULT_OK == addResult)
  {
    PRINT_CH_DEBUG("BlockWorld", "BlockWorld.DefineObject.AddedObjectDefinition",
                   "Defined %s in Object Library", EnumToString(objType));

    if(redefiningExistingType)
    {
      PRINT_NAMED_WARNING("BlockWorld.DefineObject.RemovingObjectsWithPreviousDefinition",
                          "Type %s was already defined, removing object(s) with old definition",
                          EnumToString(objType));

      DeleteLocatedObjects(filter);
    }
  }
  else
  {
    PRINT_NAMED_WARNING("BlockWorld.DefineObject.FailedToDefineObject",
                        "Failed defining %s", EnumToString(objType));
  }

  return addResult;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DeleteFixedCustomObjects& msg)
{
  BlockWorldFilter filter;
  filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  filter.AddFilterFcn(&BlockWorldFilter::IsCustomObjectFilter);
  filter.AddAllowedType(ObjectType::CustomFixedObstacle);
  DeleteLocatedObjects(filter);
  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::RobotDeletedFixedCustomObjects>();
}

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DeleteCustomMarkerObjects& msg)
{
  BlockWorldFilter filter;
  filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  filter.AddFilterFcn(&BlockWorldFilter::IsCustomObjectFilter);
  filter.AddIgnoreType(ObjectType::CustomFixedObstacle); // everything custom _except_ fixed obstacles
  DeleteLocatedObjects(filter);
  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::RobotDeletedCustomMarkerObjects>();
}

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DeleteAllCustomObjects& msg)
{
  BlockWorldFilter filter;
  filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  filter.AddFilterFcn(&BlockWorldFilter::IsCustomObjectFilter);
  DeleteLocatedObjects(filter);
  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::RobotDeletedAllCustomObjects>();
};

template<>
void BlockWorld::HandleMessage(const ExternalInterface::UndefineAllCustomMarkerObjects& msg)
{
  // First we need to delete any custom marker objects we already have
  // Note that this does
  HandleMessage(ExternalInterface::DeleteCustomMarkerObjects());

  // Remove the definition of anything that uses any Custom marker from the ObsObjLibrary
  static_assert(Util::EnumToUnderlying(CustomObjectMarker::Circles2) == 0,
                "Assuming first CustomObjectMarker is Circles2");

  s32 numRemoved = 0;
  for(auto customMarker = CustomObjectMarker::Circles2; customMarker < CustomObjectMarker::Count; ++customMarker)
  {
    const Vision::MarkerType markerType = CustomObject::GetVisionMarkerType(customMarker);
    const bool removed = _objectLibrary.RemoveObjectWithMarker(markerType);
    if(removed) {
      ++numRemoved;
    }
  }

  PRINT_CH_INFO("BlockWorld", "BlockWorld.HandleMessage.UndefineAllCustomObjects",
                "%d objects removed from library", numRemoved);
}

template<>
void BlockWorld::HandleMessage(const ExternalInterface::SelectNextObject& msg)
{
  CycleSelectedObject();
};

template<>
void BlockWorld::HandleMessage(const ExternalInterface::CreateFixedCustomObject& msg)
{
  Pose3d newObjectPose(msg.pose, _robot->GetPoseOriginList());

  ObjectID id = BlockWorld::CreateFixedCustomObject(newObjectPose, msg.xSize_mm, msg.ySize_mm, msg.zSize_mm);

  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::CreatedFixedCustomObject>(id);
};

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DefineCustomBox& msg)
{
  bool success = false;

  CustomObject* customBox = CustomObject::CreateBox(msg.customType,
                                                    msg.markerFront,
                                                    msg.markerBack,
                                                    msg.markerTop,
                                                    msg.markerBottom,
                                                    msg.markerLeft,
                                                    msg.markerRight,
                                                    msg.xSize_mm, msg.ySize_mm, msg.zSize_mm,
                                                    msg.markerWidth_mm, msg.markerHeight_mm,
                                                    msg.isUnique);

  if(nullptr != customBox)
  {
    const Result defineResult = DefineObject(std::unique_ptr<CustomObject>(customBox));
    success = (defineResult == RESULT_OK);
  }

  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::DefinedCustomObject>(success);
};

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DefineCustomCube& msg)
{
  bool success = false;

  CustomObject* customCube = CustomObject::CreateCube(msg.customType,
                                                      msg.marker,
                                                      msg.size_mm,
                                                      msg.markerWidth_mm, msg.markerHeight_mm,
                                                      msg.isUnique);

  if(nullptr != customCube)
  {
    const Result defineResult = DefineObject(std::unique_ptr<CustomObject>(customCube));
    success = (defineResult == RESULT_OK);
  }

  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::DefinedCustomObject>(success);
};

template<>
void BlockWorld::HandleMessage(const ExternalInterface::DefineCustomWall& msg)
{
  bool success = false;

  CustomObject* customWall = CustomObject::CreateWall(msg.customType,
                                                      msg.marker,
                                                      msg.width_mm, msg.height_mm,
                                                      msg.markerWidth_mm, msg.markerHeight_mm,
                                                      msg.isUnique);
  if(nullptr != customWall)
  {
    const Result defineResult = DefineObject(std::unique_ptr<CustomObject>(customWall));
    success = (defineResult == RESULT_OK);
  }

  _robot->GetContext()->GetExternalInterface()->BroadcastToGame<ExternalInterface::DefinedCustomObject>(success);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedObjectHelper(const BlockWorldFilter& filter,
                                                      const ModifierFcn& modifierFcn,
                                                      bool returnFirstFound) const
{
  ObservableObject* matchingObject = nullptr;

  const auto& currRobotOriginId = _robot->GetPoseOriginList().GetCurrentOriginID();
  
  for(const auto & objectsByOrigin : _locatedObjects) {
    const auto& originID = objectsByOrigin.first;
    if (!filter.ConsiderOrigin(originID, currRobotOriginId)) {
      continue;
    }
    for (const auto& object : objectsByOrigin.second) {
      if (nullptr == object) {
        LOG_ERROR("BlockWorld.FindLocatedObjectHelper.NullObject", "origin %d", originID);
        continue;
      }
      const bool objectMatches = filter.ConsiderType(object->GetType()) &&
                                 filter.ConsiderObject(object.get());
      if (objectMatches) {
        matchingObject = object.get();
        if(nullptr != modifierFcn) {
          modifierFcn(matchingObject);
        }
        if(returnFirstFound) {
          return matchingObject;
        }
      }
    }
  }

  return matchingObject;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::FindConnectedObjectHelper(const BlockWorldFilter& filter,
                                             const ModifierFcn& modifierFcn,
                                             bool returnFirstFound) const
{
  Block* matchingObject = nullptr;

  for (auto& connectedObject : _connectedObjects) {
    if(nullptr == connectedObject) {
      LOG_ERROR("BlockWorld.FindConnectedObjectHelper.NullObject", "");
      continue;
    }
    const bool objectMatches = filter.ConsiderType(connectedObject->GetType()) &&
                               filter.ConsiderObject(connectedObject.get());
    if (objectMatches) {
      matchingObject = connectedObject.get();
      if(nullptr != modifierFcn) {
        modifierFcn(matchingObject);
      }
      if(returnFirstFound) {
        return matchingObject;
      }
    }
  }

  return matchingObject;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::GetLocatedObjectByIdHelper(const ObjectID& objectID) const
{
  // Find the object with the given ID with any pose state, in the current world origin
  BlockWorldFilter filter;
  filter.AddAllowedID(objectID);

  // Find and return match
  ObservableObject* match = FindLocatedObjectHelper(filter, nullptr, true);
  return match;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::GetConnectedBlockByIdHelper(const ObjectID& objectID) const
{
  // Find the object with the given ID
  BlockWorldFilter filter;
  filter.AddAllowedID(objectID);

  // Find and return among ConnectedObjects
  Block* object = FindConnectedObjectHelper(filter, nullptr, true);
  return object;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Block* BlockWorld::GetConnectedBlockByActiveIdHelper(const ActiveID& activeID) const
{
  // Find object that matches given activeID
  BlockWorldFilter filter;
  filter.SetFilterFcn([activeID](const ObservableObject* object) {
    return object->GetActiveID() == activeID;
  });

  // Find and return among ConnectedObjects
  Block* object = FindConnectedObjectHelper(filter, nullptr, true);
  return object;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedObjectClosestToHelper(const Pose3d& pose,
                                                               const Vec3f&  distThreshold,
                                                               const BlockWorldFilter& filterIn) const
{
  // TODO: Keep some kind of OctTree data structure to make these queries faster?

  // Note: This function only considers the magnitude of distThreshold, not the individual elements (see VIC-12526)
  float closestDist = distThreshold.Length();

  BlockWorldFilter filter(filterIn);
  filter.AddFilterFcn([&pose, &closestDist](const ObservableObject* current)
                      {
                        float dist = 0.f;
                        if (!ComputeDistanceBetween(pose, current->GetPose(), dist)) {
                          LOG_ERROR("BlockWorld.FindLocatedObjectClosestToHelper.FilterFcn",
                                    "Failed to compute distance between input pose and block pose");
                          return false;
                        }
                        if(dist < closestDist) {
                          closestDist = dist;
                          return true;
                        } else {
                          return false;
                        }
                      });

  return FindLocatedObjectHelper(filter);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedClosestMatchingObjectHelper(const ObservableObject& object,
                                                                     const Vec3f& distThreshold,
                                                                     const Radians& angleThreshold,
                                                                     const BlockWorldFilter& filterIn) const
{
  Vec3f closestDist(distThreshold);
  Radians closestAngle(angleThreshold);

  // Don't check the object we're using as the comparison
  BlockWorldFilter filter(filterIn);
  filter.AddIgnoreID(object.GetID());
  filter.AddFilterFcn([&object,&closestDist,&closestAngle](const ObservableObject* current)
  {
    Vec3f Tdiff;
    Radians angleDiff;
    if(current->IsSameAs(object, closestDist, closestAngle, Tdiff, angleDiff)) {
      closestDist = Tdiff.GetAbs();
      closestAngle = angleDiff.getAbsoluteVal();
      return true;
    } else {
      return false;
    }
  });

  ObservableObject* closestObject = FindLocatedObjectHelper(filter);
  return closestObject;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObservableObject* BlockWorld::FindLocatedClosestMatchingTypeHelper(ObjectType withType,
                                                                   const Pose3d& pose,
                                                                   const Vec3f& distThreshold,
                                                                   const Radians& angleThreshold,
                                                                   const BlockWorldFilter& filterIn) const
{
  Vec3f closestDist(distThreshold);
  Radians closestAngle(angleThreshold);

  BlockWorldFilter filter(filterIn);
  filter.AddFilterFcn([withType,&pose,&closestDist,&closestAngle](const ObservableObject* current)
  {
    Vec3f Tdiff;
    Radians angleDiff;
    if(current->GetType() == withType &&
       current->GetPose().IsSameAs(pose, closestDist, closestAngle, Tdiff, angleDiff))
    {
      closestDist = Tdiff.GetAbs();
      closestAngle = angleDiff.getAbsoluteVal();
      return true;
    } else {
      return false;
    }
  });

  ObservableObject* closestObject = FindLocatedObjectHelper(filter);
  return closestObject;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BlockWorld::ObjectsContainer_t::const_iterator BlockWorld::FindInContainerWithID(const ObjectsContainer_t& container,
                                                                                 const ObjectID& objectID) const
{
  return std::find_if(container.begin(),
                      container.end(),
                      [&objectID](const ObjectsContainer_t::value_type& existingObj){
                        return (existingObj != nullptr) && (existingObj->GetID() == objectID);
                      });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result BlockWorld::BroadcastObjectObservation(const ObservableObject* observedObject) const
{
  // Project the observed object into the robot's camera to get the bounding box
  // within the image
  std::vector<Point2f> projectedCorners;
  f32 observationDistance = 0;
  _robot->GetVisionComponent().GetCamera().ProjectObject(*observedObject, projectedCorners, observationDistance);
  const Rectangle<f32> boundingBox(projectedCorners);

  // Compute the orientation of the top marker
  Radians topMarkerOrientation(0);
  if(observedObject->IsActive()) {
    if (IsValidLightCube(observedObject->GetType(), false))
    {
      const auto* activeCube = dynamic_cast<const Block*>(observedObject);

      if(activeCube == nullptr) {
        PRINT_NAMED_ERROR("BlockWorld.BroadcastObjectObservation.NullActiveCube",
                          "ObservedObject %d with IsActive()==true could not be cast to ActiveCube.",
                          observedObject->GetID().GetValue());
        return RESULT_FAIL;
      }

      topMarkerOrientation = activeCube->GetTopMarkerOrientation();
    }
  }

  using namespace ExternalInterface;

  RobotObservedObject observation(observedObject->GetLastObservedTime(),
                                  LegacyGetObjectFamily(observedObject),
                                  observedObject->GetType(),
                                  observedObject->GetID(),
                                  CladRect(boundingBox.GetX(),
                                           boundingBox.GetY(),
                                           boundingBox.GetWidth(),
                                           boundingBox.GetHeight()),
                                  observedObject->GetPose().ToPoseStruct3d(_robot->GetPoseOriginList()),
                                  topMarkerOrientation.ToFloat(),
                                  observedObject->IsActive());

  if( ANKI_DEV_CHEATS ) {
    SendObjectUpdateToWebViz( observation );
  }

  _robot->Broadcast(MessageEngineToGame(std::move(observation)));

  return RESULT_OK;

} // BroadcastObjectObservation()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::BroadcastLocatedObjectStates()
{
  using namespace ExternalInterface;

  // Create default filter: current origin, any object, any poseState
  BlockWorldFilter filter;

  LocatedObjectStates objectStates;
  filter.SetFilterFcn([this,&objectStates](const ObservableObject* obj)
                      {
                        const bool isConnected = obj->GetActiveID() >= 0;
                        LocatedObjectState objectState(obj->GetID(),
                                                obj->GetLastObservedTime(),
                                                obj->GetType(),
                                                obj->GetPose().ToPoseStruct3d(_robot->GetPoseOriginList()),
                                                obj->GetPoseState(),
                                                isConnected);

                        objectStates.objects.push_back(std::move(objectState));
                        return true;
                      });

  // Iterate over all objects and add them to the available objects list if they pass the filter
  FindLocatedObjectHelper(filter, nullptr, false);

  _robot->Broadcast(MessageEngineToGame(std::move(objectStates)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::BroadcastConnectedObjects()
{
  using namespace ExternalInterface;

  // Create default filter: any object
  BlockWorldFilter filter;

  ConnectedObjectStates objectStates;
  filter.SetFilterFcn([&objectStates](const ObservableObject* obj)
                      {
                        ConnectedObjectState objectState(obj->GetID(),
                                                obj->GetType());

                        objectStates.objects.push_back(std::move(objectState));
                        return true;
                      });

  // Iterate over all objects and add them to the available objects list if they pass the filter
  FindConnectedObjectHelper(filter, nullptr, false);

  _robot->Broadcast(MessageEngineToGame(std::move(objectStates)));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result BlockWorld::UpdateObjectOrigin(const ObjectID& objectID, const PoseOriginID_t oldOriginID)
{
  auto originIter = _locatedObjects.find(oldOriginID);
  if(originIter == _locatedObjects.end())
  {
    PRINT_CH_INFO("BlockWorld", "BlockWorld.UpdateObjectOrigin.BadOrigin",
                  "Origin %d not found", oldOriginID);

    return RESULT_FAIL;
  }

  DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(oldOriginID),
                 "BlockWorld.UpdateObjectOrigin.OldOriginNotInOriginList",
                 "ID:%d", oldOriginID);

  const Pose3d& oldOrigin = _robot->GetPoseOriginList().GetOriginByID(oldOriginID);

  auto& objectsInOldOrigin = originIter->second;

  auto objectIt = FindInContainerWithID(objectsInOldOrigin, objectID);
  
  if (objectIt == objectsInOldOrigin.end()) {
    LOG_INFO("BlockWorld.UpdateObjectOrigin.ObjectNotFound",
             "Object %d not found in origin %s",
             objectID.GetValue(), oldOrigin.GetName().c_str());
    return RESULT_FAIL;
  }
  
  const auto& object = *objectIt;
  if (!object->GetPose().HasSameRootAs(oldOrigin)) {
    const Pose3d& newOrigin = object->GetPose().FindRoot();
    
    LOG_INFO("BlockWorld.UpdateObjectOrigin.ObjectFound",
             "Updating ObjectID %d from origin %s to %s",
             objectID.GetValue(),
             oldOrigin.GetName().c_str(),
             newOrigin.GetName().c_str());
    
    const PoseOriginID_t newOriginID = newOrigin.GetID();
    DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(newOriginID),
                   "BlockWorld.UpdateObjectOrigin.ObjectOriginNotInOriginList",
                   "Name:%s", object->GetPose().FindRoot().GetName().c_str());
    
    // Add to object's current origin (if it's there already, issue a warning and remove the duplicate first)
    auto& objectsInNewOrigin = _locatedObjects[newOriginID];
    auto existingIt = FindInContainerWithID(objectsInNewOrigin, object->GetID());
    if (existingIt != objectsInNewOrigin.end()) {
      LOG_WARNING("BlockWorld.UpdateObjectOrigin.ObjectAlreadyInNewOrigin",
                  "Removing existing object. ObjectID %d, old origin %s, new origin %s",
                  objectID.GetValue(),
                  oldOrigin.GetName().c_str(),
                  newOrigin.GetName().c_str());
      objectsInNewOrigin.erase(existingIt);
    }

    _locatedObjects[newOriginID].push_back(object);
    
    // Delete from old origin
    objectsInOldOrigin.erase(objectIt);
  }
  
  // Delete any now-zombie origins
  DeleteZombieOrigins();
  
  return RESULT_OK;
}

Result BlockWorld::UpdateObjectOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID)
{
  Result result = RESULT_OK;

  if(!ANKI_VERIFY(PoseOriginList::UnknownOriginID != oldOriginID &&
                  PoseOriginList::UnknownOriginID != newOriginID,
                  "BlockWorld.UpdateObjectOrigins.OriginFail",
                  "Old and new origin IDs must not be Unknown"))
  {
    return RESULT_FAIL;
  }

  DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(oldOriginID),
                 "BlockWorld.UpdateObjectOrigins.BadOldOriginID", "ID:%d", oldOriginID);

  DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(newOriginID),
                 "BlockWorld.UpdateObjectOrigins.BadNewOriginID", "ID:%d", newOriginID);

  const Pose3d& oldOrigin = _robot->GetPoseOriginList().GetOriginByID(oldOriginID);
  const Pose3d& newOrigin = _robot->GetPoseOriginList().GetOriginByID(newOriginID);

  // Look for objects in the old origin
  BlockWorldFilter filterOld;
  filterOld.SetOriginMode(BlockWorldFilter::OriginMode::Custom);
  filterOld.AddAllowedOrigin(oldOriginID);

  // Use the modifier function to update matched objects to the new origin
  ModifierFcn originUpdater = [&oldOrigin,&newOrigin,newOriginID,&result,this](ObservableObject* oldObject)
  {
    Pose3d newPose;

    if(_robot->GetCarryingComponent().IsCarryingObject(oldObject->GetID()))
    {
      // Special case: don't use the pose w.r.t. the origin b/c carried objects' parent
      // is the lift. The robot is already in the new frame by the time this called,
      // so we don't need to adjust anything
      DEV_ASSERT(_robot->GetPoseOriginList().GetCurrentOriginID() == newOriginID,
                 "BlockWorld.UpdateObjectOrigins.RobotNotInNewOrigin");
      DEV_ASSERT(oldObject->GetPose().GetRootID() == newOriginID,
                 "BlockWorld.UpdateObjectOrigins.OldCarriedObjectNotInNewOrigin");
      newPose = oldObject->GetPose();
    }
    else if(false == oldObject->GetPose().GetWithRespectTo(newOrigin, newPose))
    {
      PRINT_NAMED_ERROR("BlockWorld.UpdateObjectOrigins.OriginFail",
                        "Could not get object %d w.r.t new origin %s",
                        oldObject->GetID().GetValue(),
                        newOrigin.GetName().c_str());

      result = RESULT_FAIL;
      return;
    }

    const Vec3f& T_old = oldObject->GetPose().GetTranslation();
    const Vec3f& T_new = newPose.GetTranslation();

    // Look for a matching object in the new origin. Should have same type. If unique, should also have
    // same ID, or if not unique, the poses should match.
    BlockWorldFilter filterNew;
    filterNew.SetOriginMode(BlockWorldFilter::OriginMode::Custom);
    filterNew.AddAllowedOrigin(newOriginID);
    filterNew.AddAllowedType(oldObject->GetType());

    ObservableObject* newObject = nullptr;

    if(oldObject->IsUnique())
    {
      filterNew.AddFilterFcn(BlockWorldFilter::UniqueObjectsFilter);
      filterNew.AddAllowedID(oldObject->GetID());
      newObject = FindLocatedMatchingObject(filterNew);
    }
    else
    {
      newObject = FindLocatedObjectClosestTo(oldObject->GetPose(),
                                             oldObject->GetSameDistanceTolerance(),
                                             filterNew);
    }

    bool addNewObject = false;
    if(nullptr == newObject)
    {
      PRINT_CH_INFO("BlockWorld", "BlockWorld.UpdateObjectOrigins.NoMatchFound",
                    "No match found for %s %d, adding new at T=(%.1f,%.1f,%.1f)",
                    EnumToString(oldObject->GetType()),
                    oldObject->GetID().GetValue(),
                    T_new.x(), T_new.y(), T_new.z());

      newObject = oldObject->CloneType();
      newObject->CopyID(oldObject);

      addNewObject = true;
    }
    else
    {
      PRINT_CH_INFO("BlockWorld", "BlockWorld.UpdateObjectOrigins.ObjectOriginChanged",
                    "Updating %s %d's origin from %s to %s (matched by %s to ID:%d). "
                    "T_old=(%.1f,%.1f,%.1f), T_new=(%.1f,%.1f,%.1f)",
                    EnumToString(oldObject->GetType()),
                    oldObject->GetID().GetValue(),
                    oldOrigin.GetName().c_str(),
                    newOrigin.GetName().c_str(),
                    oldObject->IsUnique() ? "type" : "pose",
                    newObject->GetID().GetValue(),
                    T_old.x(), T_old.y(), T_old.z(),
                    T_new.x(), T_new.y(), T_new.z());

      // we also want to keep the MOST recent objectID, rather than the one we used to have for this object, because
      // if clients are bookkeeping IDs, the know about the new one (for example, if an action is already going
      // to pick up that objectID, it should not change by virtue of rejiggering)
      // Note: despite the name, oldObject is the most recent instance of this match. Thanks, Andrew.
      newObject->CopyID( oldObject );
    }

    // Use all of oldObject's time bookkeeping, then update the pose and pose state
    newObject->SetObservationTimes(oldObject);
    newObject->SetPose(newPose, oldObject->GetLastPoseUpdateDistance(), oldObject->GetPoseState());

    if(addNewObject)
    {
      // Note: need to call SetPose first because that sets the origin which
      // controls which map the object gets added to
      AddLocatedObject(std::shared_ptr<ObservableObject>(newObject));

      PRINT_CH_INFO("BlockWorld", "BlockWorld.UpdateObjectOrigins.NoMatchingObjectInNewFrame",
                    "Adding %s object with ID %d to new origin %s",
                    EnumToString(newObject->GetType()),
                    newObject->GetID().GetValue(),
                    newOrigin.GetName().c_str());
    }

  };

  // Apply the filter and modify each object that matches
  ModifyLocatedObjects(originUpdater, filterOld);

  if(RESULT_OK == result) {
    // Erase all the objects in the old frame now that their counterparts in the new
    // frame have had their poses updated
    // rsam: Note we don't have to call Delete since we don't clear or notify. There is no way that we could
    // be deleting any objects in this origin during rejigger, since we bring objects to the previously known map or
    // override their pose. For that reason, directly remove the origin rather than calling DeleteLocatedObjectsByOrigin
    // Note that we decide to not notify of objects that merge (passive matched by pose), because the old ID in the
    // old origin is not in the current one.
    _locatedObjects.erase(oldOriginID);
  }

  // Notify the world about the objects in the new coordinate frame, in case
  // we added any based on rejiggering (not observation). Include unconnected
  // ones as well.
  BroadcastLocatedObjectStates();

  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::DeleteZombieOrigins()
{
  for (auto originIt = _locatedObjects.begin() ; originIt != _locatedObjects.end() ; ) {
    if (IsZombiePoseOrigin(originIt->first)) {
      LOG_INFO("BlockWorld.DeleteZombieOrigins.DeletingOrigin",
               "Deleting origin %d (which contained %zu objects) because it was zombie",
               originIt->first, originIt->second.size());
      // With their tanks, and their bombs, and their bombs, and their guns
      originIt = _locatedObjects.erase(originIt);
    } else {
      ++originIt;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result BlockWorld::ProcessVisualObservations(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeenRaw,
                                             const RobotTimeStamp_t atTimestamp)
{
  // if there are no objects, then exit early. This might happen if we see an SDK marker
  // but have not created a custom object for it.
  if (objectsSeenRaw.empty()) {
    return RESULT_OK;
  }
  
  // We cannot trust observations of objects if we were off treads, so no need to continue
  if (_robot->GetOffTreadsState() != OffTreadsState::OnTreads) {
    return RESULT_OK;
  }
  
  // First, filter the raw observations
  auto objectsSeen = FilterRawObservedObjects(objectsSeenRaw);
  
  // Have we observed a charger?
  std::shared_ptr<ObservableObject> observedCharger = nullptr;
  const auto chargerIt = std::find_if(objectsSeen.begin(), objectsSeen.end(),
                                      [](const std::shared_ptr<ObservableObject>& obj) {
                                        return IsChargerType(obj->GetType(), false);
                                      });
  if (chargerIt != objectsSeen.end()) {
    observedCharger = *chargerIt;
  }

  // Do we have any existing chargers?
  BlockWorldFilter filt;
  filt.SetAllowedTypes({ObjectType::Charger_Basic});
  filt.SetOriginMode(BlockWorldFilter::OriginMode::InRobotFrame);
  ObservableObject* existingCharger = FindLocatedMatchingObject(filt);

  const bool wasCameraMoving = (_robot->GetMoveComponent().IsCameraMoving() ||
                                _robot->GetMoveComponent().WasCameraMoving(atTimestamp));
  const bool canRobotLocalize = (_robot->GetLocalizedTo().IsUnknown() || _robot->HasMovedSinceBeingLocalized()) &&
                                !wasCameraMoving && 
                                (observedCharger != nullptr);
  
  auto result = Result::RESULT_OK;


  // VIC-14462: we no longer relocalize to objects in other origins due to rejiggering bugs, and the map timing out anyway
  if (canRobotLocalize && (existingCharger != nullptr)) {
    // We already have a charger in the current origin - is it close enough to its last pose to localize to it?
    const bool localizeToCharger = existingCharger->GetPose().IsSameAs(observedCharger->GetPose(),
                                                                        existingCharger->GetSameDistanceTolerance(),
                                                                        existingCharger->GetSameAngleTolerance());
    if (localizeToCharger) {
      // Keep track of poses of the observed objects wrt to robot so 
      // that they can be corrected after the robot has relocalized
      std::vector<Pose3d> objectsSeenPosesWrtRobot;
      for (const auto& obj : objectsSeen) {
        Pose3d poseWrtRobot;
        obj->GetPose().GetWithRespectTo(_robot->GetPose(), poseWrtRobot);
        objectsSeenPosesWrtRobot.push_back(poseWrtRobot);
      }

      // Localize to the charger instance in this origin
      _robot->LocalizeToObject(observedCharger.get(), existingCharger);

      // Update pose of objects seen after robot relocalization
      auto newObjSeenIt = objectsSeenPosesWrtRobot.begin();
      for (auto& obj : objectsSeen) {
        obj->SetPose(newObjSeenIt->GetWithRespectToRoot(),
                      obj->GetLastPoseUpdateDistance(), 
                      obj->GetPoseState());
        ++newObjSeenIt;
      }
    }
  }

  UpdateKnownObjects(objectsSeen, atTimestamp);
  

  if (canRobotLocalize && (existingCharger == nullptr)) {
    // We found a charger and can localize to it, but there was no prior charger
    // NOTE: this just sets the "localizedTo" fields, and shouldn't update the robot
    // pose since the pose transformation with itself is the identity transformation
    result = _robot->LocalizeToObject(observedCharger.get(), observedCharger.get());	
  }

  // For any objects whose poses were just updated, broadcast information about them now. Note that this list could
  // be different from the objectsSeen list, since we may have decided to ignore an object observation for some
  // reason (e.g. robot was moving too fast)

  // TODO: this can go right into the last step of `UpdateKnownObjects`
  BlockWorldFilter updatedNowFilter;
  updatedNowFilter.SetFilterFcn([&atTimestamp](const ObservableObject* obj){
    return (obj->GetLastObservedTime() == atTimestamp);
  });
  std::vector<ObservableObject*> updatedNowObjects;
  FindLocatedMatchingObjects(updatedNowFilter, updatedNowObjects);

  for (auto* object : updatedNowObjects) {
    // Add all observed markers of this object as occluders
    std::vector<const Vision::KnownMarker *> observedMarkers;
    object->GetObservedMarkers(observedMarkers);
    for(auto marker : observedMarkers) {
      _robot->GetVisionComponent().GetCamera().AddOccluder(*marker);
    }
    
    // If we are observing an object that we are supposed to be carrying, then tell the robot we are no longer
    // carrying it
    if (_robot->GetCarryingComponent().IsCarryingObject(object->GetID())) {
      LOG_INFO("BlockWorld.ProcessVisualObservations.SeeingCarriedObject",
               "We have observed object %d, so we must not be carrying it anymore. Unsetting as carried object.",
               object->GetID().GetValue());
      _robot->GetCarryingComponent().UnSetCarryingObject();
    }
    
    // Update map component
    const Pose3d oldPoseCopy = object->GetPose();
    _robot->GetMapComponent().UpdateObjectPose(*object, &oldPoseCopy, PoseState::Known);
    
    BroadcastObjectObservation(object);
  }
  
  return result;
} // ProcessVisualObservations()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::UpdateKnownObjects(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeen,
                                    const RobotTimeStamp_t atTimestamp,
                                    const bool ignoreCharger)
{
  // Go through each observation and, if possible, associate it to an already-known object. If the observation does
  // not match any existing known objects, we generate a new objectID for it and add it to the list of known objects.
  for (const auto& objSeen : objectsSeen) {
    DEV_ASSERT(!objSeen->GetID().IsSet(), "BlockWorld.UpdateKnownObjects.SeenObjectAlreadyHasID");

    BlockWorldFilter filter;
    filter.SetAllowedTypes({objSeen->GetType()});
    const bool isUnique = objSeen->IsUnique();
    if (!isUnique) {
      // For non-unique objects, match by pose (by using IsSameAs)
      filter.AddFilterFcn([&objSeen](const ObservableObject* obj){
        return obj->IsSameAs(*objSeen);
      });
    }
    // Check for matches in the current origin
    ObservableObject* matchingObject = FindLocatedMatchingObject(filter);

    const bool isSelectedObject = (matchingObject != nullptr) &&
                                  ((_robot->GetDockingComponent().GetDockObject() == matchingObject->GetID()) ||
                                    (_robot->GetCarryingComponent().IsCarryingObject(matchingObject->GetID())));

    // If we haven't found a match in the current origin, then continue looking in other origins (for unique objects)
    if ((matchingObject == nullptr) && isUnique) {
      filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
      matchingObject = FindLocatedMatchingObject(filter);
    }

    // Was the camera moving? If so, we must skip this observation _unless_ this is the dock object or carry object.
    // Might be sufficient to check for movement at historical time, but to be conservative (and account for
    // timestamping inaccuracies?) we will also check _current_ moving status.
    const bool wasCameraMoving = (_robot->GetMoveComponent().IsCameraMoving() ||
                                  _robot->GetMoveComponent().WasCameraMoving(atTimestamp));
    const bool ignoreChargerAndIsCharger = (ignoreCharger && IsChargerType(objSeen->GetType(), false));
    if (ignoreChargerAndIsCharger || (wasCameraMoving && !isSelectedObject)) {
      continue;
    }
    
    if (matchingObject != nullptr) {
      // We found a matching object
      const PoseID_t matchingObjectOrigin = matchingObject->GetPose().GetRootID();

      objSeen->CopyID(matchingObject);
      
      // Update the matching object's pose
      matchingObject->SetObservationTimes(objSeen.get());
      const float distToObjSeen = objSeen->GetLastPoseUpdateDistance();
      matchingObject->SetPose(objSeen->GetPose(), distToObjSeen, PoseState::Known);
      
      // If we matched an object from a previous origin, we need to move it into the current origin
      if (matchingObjectOrigin != _robot->GetWorldOriginID()) {
        UpdateObjectOrigin(matchingObject->GetID(), matchingObjectOrigin);
      }
    } else {
      // Did not find _any_ match for this object among located objects. If this is an active object, maybe there is
      // a known connected instance (e.g., robot has connected to a cube but has not yet visually observed it) from
      // which we can grab the objectID.
      if (objSeen->IsActive()) {
        BlockWorldFilter connectedFilter;
        connectedFilter.SetAllowedTypes({objSeen->GetType()});
        const auto* connectedBlock = FindConnectedMatchingBlock(connectedFilter);
        if (connectedBlock != nullptr) {
          DEV_ASSERT(connectedBlock->GetID().IsSet(), "BlockWorld.UpdateKnownObjects.ConnectedObjectHasNoId");
          objSeen->CopyID(connectedBlock);
        }
      }
      
      // If we _still_ don't have an ID yet, then generate a new one now
      if (!objSeen->GetID().IsSet()) {
        objSeen->SetID();
      }
      
      // Add this object to the located objects container
      AddLocatedObject(objSeen);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<std::shared_ptr<ObservableObject>> BlockWorld::FilterRawObservedObjects(const std::vector<std::shared_ptr<ObservableObject>>& objectsSeenRaw)
{
  // First copy the raw objects container
  auto objectsSeenFilt = objectsSeenRaw;
  
  // Remove any objects that were observed from too far away
  objectsSeenFilt.erase(std::remove_if(objectsSeenFilt.begin(),
                                       objectsSeenFilt.end(),
                                       [](const std::shared_ptr<ObservableObject>& obj) {
                                         return obj->GetLastPoseUpdateDistance() > obj->GetMaxObservationDistance_mm();
                                       }),
                        objectsSeenFilt.end());
  
  // Ignore duplicate 'unique' objects. For example, chargers are supposed to be 'unique' objects, meaning we can only
  // ever know about one of them at a time. However, it is still possible to see two of them in the same image. We
  // only want to keep the closest one for consistency.
  //
  // First, sort the container by distance so that we can use std::unique to do the work for us.
  
  std::sort(objectsSeenFilt.begin(), objectsSeenFilt.end(),
            [](const std::shared_ptr<ObservableObject>& objA, const std::shared_ptr<ObservableObject>& objB) {
              return objA->GetLastPoseUpdateDistance() < objB->GetLastPoseUpdateDistance();
            });
  
  auto last = std::unique(objectsSeenFilt.begin(), objectsSeenFilt.end(),
                          [](const std::shared_ptr<ObservableObject>& objA, const std::shared_ptr<ObservableObject>& objB) {
                            return (objA->GetType() == objB->GetType()) && objA->IsUnique() && objB->IsUnique();
                          });
  objectsSeenFilt.erase(last, objectsSeenFilt.end());
  
  return objectsSeenFilt;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::CheckForUnobservedObjects(RobotTimeStamp_t atTimestamp)
{
  // Don't bother if the robot is picked up or if it was rotating too fast to
  // have been able to see the markers on the objects anyway.
  // NOTE: Just using default speed thresholds, which should be conservative.
  if(_robot->GetOffTreadsState() != OffTreadsState::OnTreads ||
     _robot->GetMoveComponent().WasMoving(atTimestamp) ||
     _robot->GetImuComponent().GetImuHistory().WasRotatingTooFast(atTimestamp))
  {
    return;
  }

  auto originIter = _locatedObjects.find(_robot->GetPoseOriginList().GetCurrentOriginID());
  if(originIter == _locatedObjects.end()) {
    // No objects relative to this origin: Nothing to do
    return;
  }

  // Create a list of unobserved object IDs (IDs since we can remove several of them while iterating)
  std::vector<ObjectID> unobservedObjectIDs;

  for (const auto& object : originIter->second) {
    if (nullptr == object) {
      LOG_ERROR("BlockWorld.CheckForUnobservedObjects.NullObject", "");
      continue;
    }
    
    // Look for "unobserved" objects not seen atTimestamp -- but skip objects:
    //    - that are currently being carried
    //    - that we are currently docking to
    const RobotTimeStamp_t lastObservedTime = object->GetLastObservedTime();
    const bool isUnobserved = ( (lastObservedTime < atTimestamp) &&
                               (_robot->GetCarryingComponent().GetCarryingObjectID() != object->GetID()) &&
                               (_robot->GetDockingComponent().GetDockObject() != object->GetID()) );
    if ( isUnobserved )
    {
      unobservedObjectIDs.push_back(object->GetID());
    }
  }

  // TODO: Don't bother with this if the robot is docking? (picking/placing)??
  // Now that the occlusion maps are complete, check each unobserved object's
  // visibility in each camera
  const Vision::Camera& camera = _robot->GetVisionComponent().GetCamera();
  DEV_ASSERT(camera.IsCalibrated(), "BlockWorld.CheckForUnobservedObjects.CameraNotCalibrated");
  for(const auto& objectID : unobservedObjectIDs)
  {
    // if the object doesn't exist anymore, it was deleted by another one, for example through a stack
    // or if doesn't have markers (like unexpected move objects), skip
    ObservableObject* unobservedObject = GetLocatedObjectByID(objectID);
    if ( nullptr == unobservedObject || unobservedObject->GetMarkers().empty() ) {
      continue;
    }

    // calculate padding based on distance to object pose
    u16 xBorderPad = 0;
    u16 yBorderPad = 0;
    Pose3d objectPoseWrtCamera;
    if ( unobservedObject->GetPose().GetWithRespectTo(camera.GetPose(), objectPoseWrtCamera) )
    {
      // should have markers
      const auto& markerList = unobservedObject->GetMarkers();
      if ( !markerList.empty() )
      {
        const float observationDistance = unobservedObject->GetMaxObservationDistance_mm();
        const Point2f& markerSize = markerList.front().GetSize();
        const f32 focalLenX = camera.GetCalibration()->GetFocalLength_x();
        const f32 focalLenY = camera.GetCalibration()->GetFocalLength_y();
        // distFactor = (1-distNorm) + 1; 1-distNorm to invert normalization, +1 because we want 100% at distNorm=1
        const f32 distToObjInvFactor = 2-(objectPoseWrtCamera.GetTranslation().Length()/observationDistance);
        float xPadding = focalLenX*markerSize.x()*distToObjInvFactor/observationDistance;
        float yPadding = focalLenY*markerSize.y()*distToObjInvFactor/observationDistance;
        xBorderPad = static_cast<u16>(xPadding);
        yBorderPad = static_cast<u16>(yPadding);
      }
      else {
        PRINT_NAMED_ERROR("BlockWorld.CheckForUnobservedObjects.NoMarkers",
                          "Object %d (Type:%s)",
                          objectID.GetValue(),
                          EnumToString(unobservedObject->GetType()) );
        continue;
      }
    }
    else
    {
      PRINT_NAMED_ERROR("BlockWorld.CheckForUnobservedObjects.ObjectNotInCameraPoseOrigin",
                        "Object %d (PosePath:%s)",
                        objectID.GetValue(),
                        unobservedObject->GetPose().GetNamedPathToRoot(false).c_str() );
      continue;
    }

    // We want to remove objects that should have been visible from the current pose, but were not observed for some
    // reason. There are two scenarios:
    //   - If the object's pose is marked 'dirty' and we didn't see it, we immediately remove it.
    //   - If the object's pose is 'known', we only remove the object if we saw _another_ object behind it (proving
    //     that it really must not be there).
    //
    // Note: The return value of IsVisibleFrom() can be a source of confusion. See VIC-13732 for details.
    bool hasNothingBehind = false;
    const bool shouldBeVisible = unobservedObject->IsVisibleFrom(camera,
                                                                 MAX_MARKER_NORMAL_ANGLE_FOR_SHOULD_BE_VISIBLE_CHECK_RAD,
                                                                 MIN_MARKER_SIZE_FOR_SHOULD_BE_VISIBLE_CHECK_PIX,
                                                                 xBorderPad, yBorderPad,
                                                                 hasNothingBehind);

    const bool isDirtyPoseState = (PoseState::Dirty == unobservedObject->GetPoseState());

    const bool removeObject = shouldBeVisible || (hasNothingBehind && isDirtyPoseState);
    if (removeObject)
    {
      LOG_INFO("BlockWorld.CheckForUnobservedObjects.MarkingUnobservedObject",
               "Removing object %d, which should have been seen, but wasn't. "
               "(shouldBeVisible:%d hasNothingBehind:%d isDirty:%d)",
               unobservedObject->GetID().GetValue(),
               shouldBeVisible, hasNothingBehind, isDirtyPoseState);

      _robot->GetMapComponent().MarkObjectUnobserved(*unobservedObject);
      
      BlockWorldFilter filter;
      filter.SetAllowedIDs({objectID});
      DeleteLocatedObjects(filter);
    }

  } // for each unobserved object

} // CheckForUnobservedObjects()

ObjectID BlockWorld::CreateFixedCustomObject(const Pose3d& p, const f32 xSize_mm, const f32 ySize_mm, const f32 zSize_mm)
{
  // Create an instance of the custom obstacle
  CustomObject* customObstacle = CustomObject::CreateFixedObstacle(xSize_mm, ySize_mm, zSize_mm);
  if(nullptr == customObstacle)
  {
    PRINT_NAMED_ERROR("BlockWorld.CreateFixedCustomObject.CreateFailed", "");
    return ObjectID{};
  }

  Pose3d obsPose(p);
  obsPose.SetParent(_robot->GetPose().GetParent());

  // Initialize with Known pose so it won't delete immediately because it isn't re-seen
  auto customObject = std::shared_ptr<CustomObject>(customObstacle);
  customObject->InitPose(obsPose, PoseState::Known);

  // set new ID before adding to the world, since this is a new object
  DEV_ASSERT( !customObject->GetID().IsSet(), "BlockWorld.CreateFixedCustomObject.NewObjectHasID" );
  customObject->SetID();

  AddLocatedObject(customObject);

  return customObject->GetID();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObjectID BlockWorld::AddConnectedBlock(const ActiveID& activeID,
                                              const FactoryID& factoryID,
                                              const ObjectType& objType)
{
  // only connected objects should be added through this method, so a required activeID is a must
  DEV_ASSERT(activeID != ObservableObject::InvalidActiveID, "BlockWorld.AddConnectedBlock.CantAddInvalidActiveID");

  // Validate that ActiveID is not already referring to a connected object
  const auto* conObjWithActiveID = GetConnectedBlockByActiveID( activeID );
  if ( nullptr != conObjWithActiveID)
  {
    // Verify here that factoryID and objectType match, and if they do, simply ignore the message, since we already
    // have a valid instance
    const bool isSameObject = (factoryID == conObjWithActiveID->GetFactoryID()) &&
                              (objType == conObjWithActiveID->GetType());
    if ( isSameObject ) {
      LOG_INFO("BlockWorld.AddConnectedBlock.FoundExistingObject",
               "objectID %d, activeID %d, factoryID %s, type %s",
               conObjWithActiveID->GetID().GetValue(),
               conObjWithActiveID->GetActiveID(),
               conObjWithActiveID->GetFactoryID().c_str(),
               EnumToString(conObjWithActiveID->GetType()));
      return conObjWithActiveID->GetID();
    }

    // if it's not the same, then what the hell, we are currently using that activeID for other object!
    LOG_ERROR("BlockWorld.AddConnectedBlock.ConflictingActiveID",
              "ActiveID:%d found when we tried to add that activeID as connected object. Removing previous.",
              activeID);

    // clear the pointer and destroy it
    conObjWithActiveID = nullptr;
    RemoveConnectedBlock(activeID);
  }

  // Validate that factoryId is not currently a connected object
  BlockWorldFilter filter;
  filter.SetFilterFcn([factoryID](const ObservableObject* object) {
    return object->GetFactoryID() == factoryID;
  });
  const auto* const conObjectWithFactoryID = FindConnectedObjectHelper(filter, nullptr, true);
  ANKI_VERIFY( nullptr == conObjectWithFactoryID, "BlockWorld.AddConnectedBlock.FactoryIDAlreadyUsed", "%s", factoryID.c_str() );

  // This is the new object we are going to create. We can't insert it in _connectedObjects until
  // we know the objectID, so we create it first, and then we look for unconnected matches (we have seen the
  // object but we had not connected to it.) If we find one, we will inherit the objectID from that match; if
  // we don't find a match, we will assign it a new objectID. Then we can add to the container of _connectedObjects.
  std::shared_ptr<Block> newActiveObjectPtr;
  newActiveObjectPtr.reset(new Block(objType, activeID, factoryID));
  if ( nullptr == newActiveObjectPtr ) {
    // failed to create the object (that function should print the error, exit here with unSet ID)
    return ObjectID();
  }

  // we can't add to the _connectedObjects until the objectID has been decided

  // Is there an active object with the same activeID and type that already exists?
  BlockWorldFilter filterByActiveID;
  filterByActiveID.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  filterByActiveID.AddFilterFcn([activeID](const ObservableObject* object) { return object->GetActiveID() == activeID;} );
  filterByActiveID.SetAllowedTypes({objType});
  std::vector<ObservableObject*> matchingObjects;
  FindLocatedMatchingObjects(filterByActiveID, matchingObjects);

  if (matchingObjects.empty())
  {
    // If no match found, find one of the same type with an invalid activeID and assume that's the one we are
    // connecting to
    BlockWorldFilter filterInAny;
    filterInAny.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
    filterInAny.SetAllowedTypes({objType});
    std::vector<ObservableObject*> objectsOfSameType;
    FindLocatedMatchingObjects(filterInAny, objectsOfSameType);

    if(!objectsOfSameType.empty())
    {
      ObjectID matchObjectID;

      // we found located instances of this object that we were not connected to
      for (auto& sameTypeObject : objectsOfSameType)
      {
        if ( matchObjectID.IsSet() ) {
          // check they all have the same objectID across frames
          DEV_ASSERT( matchObjectID == sameTypeObject->GetID(), "BlockWorld.AddConnectedBlock.NotSameObjectID");
        } else {
          // set once
          matchObjectID = sameTypeObject->GetID();
        }

        MarkObjectDirty(sameTypeObject);

        // check if the instance has activeID
        if (sameTypeObject->GetActiveID() == ObservableObject::InvalidActiveID)
        {
          // it doesn't have an activeID, we are connecting to it, set
          sameTypeObject->SetActiveID(activeID);
          sameTypeObject->SetFactoryID(factoryID);
          LOG_INFO("BlockWorld.AddConnectedBlock.FoundMatchingObjectWithNoActiveID",
                   "objectID %d, activeID %d, type %s",
                   sameTypeObject->GetID().GetValue(), sameTypeObject->GetActiveID(), EnumToString(objType));
        } else {
          // it has an activeID, we were connected. Is it the same object?
          if ( sameTypeObject->GetFactoryID() != factoryID )
          {
            // uhm, this is a different object (or factoryID was not set)
            LOG_INFO("AddActiveObject.FoundOtherActiveObjectOfSameType",
                     "ActiveID %d (factoryID %s) is same type as another existing object (objectID %d, activeID %d, factoryID %s, type %s) updating ids to match",
                     activeID,
                     factoryID.c_str(),
                     sameTypeObject->GetID().GetValue(),
                     sameTypeObject->GetActiveID(),
                     sameTypeObject->GetFactoryID().c_str(),
                     EnumToString(objType));

            // if we have a new factoryID, override the old instances with the new one we connected to
            if(!factoryID.empty())
            {
              sameTypeObject->SetActiveID(activeID);
              sameTypeObject->SetFactoryID(factoryID);
            }
          } else {
            LOG_INFO("BlockWorld.AddConnectedBlock.FoundIdenticalObjectOnDifferentSlot",
                     "Updating activeID of block with factoryID %s from %d to %d",
                     sameTypeObject->GetFactoryID().c_str(), sameTypeObject->GetActiveID(), activeID);
            // same object, somehow in different activeID now
            sameTypeObject->SetActiveID(activeID);
          }
        }
      }

      // inherit objectID from matches
      newActiveObjectPtr->CopyID(objectsOfSameType.front());
    }
    else
    {
      // there are no matches of the same type, set new objectID
      newActiveObjectPtr->SetID();
    }
  }
  else
  {
    // We can't find more than one object of the same type in a single origin. Otherwise something went really bad
    DEV_ASSERT(matchingObjects.size() <= 1,"BlockWorld.AddConnectedBlock.TooManyMatchingObjects" );

    // NOTE: [MAM] This error has not happened for some time, so I removed a lot of logic from this else block.
    // Find it again here if it is needed: https://github.com/anki/victor/blob/319a692ed2fc7cd85aa9009e415809cce827689a/engine/blockWorld/blockWorld.cpp#L1699
    // We should not find any objects in any origins that have this activeID. Otherwise that means they have
    // not disconnected properly. If there's a timing issue with connecting an object to an activeID before
    // disconnecting a previous object, we would like to know, so we can act accordingly. Add this error here
    // to detect that situation.
    LOG_ERROR("BlockWorld.AddConnectedBlock.ConflictingActiveID",
              "Objects with ActiveID:%d were found when we tried to add that activeID as connected object.",
              activeID);
  }

  // at this point the new active connected object has a valid objectID, we can finally add it to the world
  DEV_ASSERT( newActiveObjectPtr->GetID().IsSet(), "BlockWorld.AddConnectedBlock.ObjectIDWasNeverSet" );
  _connectedObjects.push_back(newActiveObjectPtr);

  // return the assigned objectID
  return newActiveObjectPtr->GetID();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObjectID BlockWorld::RemoveConnectedBlock(const ActiveID& activeID)
{
  ObjectID removedObjectID;
  
  for (auto it = _connectedObjects.begin() ; it != _connectedObjects.end() ; ) {
    const auto& objPtr = *it;
    if (objPtr == nullptr) {
      LOG_ERROR("BlockWorld.RemoveConnectedBlock.NullEntryInConnectedObjects", "");
      it = _connectedObjects.erase(it);
    } else if (objPtr->GetActiveID() == activeID) {
      if (removedObjectID.IsSet()) {
        LOG_ERROR("BlockWorld.RemoveConnectedBlock.DuplicateEntry",
                  "Duplicate entry found in _connectedObjects for object with activeID %d. "
                  "Existing object ID %d, this object ID %d. Removing this entry as well",
                  activeID, removedObjectID.GetValue(), objPtr->GetID().GetValue());
      }
      removedObjectID = objPtr->GetID();
      it = _connectedObjects.erase(it);
    } else {
      ++it;
    }
  }

  // Clear the activeID from any located instances of the removed object
  if ( removedObjectID.IsSet() )
  {
    BlockWorldFilter matchingIDInAnyOrigin;
    matchingIDInAnyOrigin.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
    matchingIDInAnyOrigin.SetAllowedIDs({removedObjectID});
    ModifierFcn clearActiveID = [](ObservableObject* object) {
      object->SetActiveID(ObservableObject::InvalidActiveID);
      object->SetFactoryID(ObservableObject::InvalidFactoryID); // should not be needed. Should we keep it?
    };
    ModifyLocatedObjects(clearActiveID, matchingIDInAnyOrigin);
  }

  // return the objectID
  return removedObjectID;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::AddLocatedObject(const std::shared_ptr<ObservableObject>& object)
{
  DEV_ASSERT(object->HasValidPose(), "BlockWorld.AddLocatedObject.NotAValidPoseState");
  DEV_ASSERT(object->GetID().IsSet(), "BlockWorld.AddLocatedObject.ObjectIDNotSet");

  const PoseOriginID_t objectOriginID = object->GetPose().GetRootID();

  // allow adding only in current origin
  DEV_ASSERT(objectOriginID == _robot->GetPoseOriginList().GetCurrentOriginID(),
             "BlockWorld.AddLocatedObject.NotCurrentOrigin");

  // hook activeID/factoryID if a connected object is available.
  // rsam: I would like to do this in a cleaner way, maybe just refactoring the code, but here seems fishy design-wise
  {
    // should not be connected if we are just adding to the world
    DEV_ASSERT(object->GetActiveID() == ObservableObject::InvalidActiveID,
               "BlockWorld.AddLocatedObject.AlreadyHadActiveID");
    DEV_ASSERT(object->GetFactoryID() == ObservableObject::InvalidFactoryID,
               "BlockWorld.AddLocatedObject.AlreadyHadFactoryID");

    // find by ObjectID. The objectID should match, since observations search for objectID even in connected
    auto* connectedObj = GetConnectedBlockByID(object->GetID());
    if ( nullptr != connectedObj ) {
      object->SetActiveID( connectedObj->GetActiveID() );
      object->SetFactoryID( connectedObj->GetFactoryID() );
    }
  }

  // not asserting in case SDK tries to do this, but do not add it to the BlockWorld
  if(ObjectType::Block_LIGHTCUBE_GHOST == object->GetType())
  {
    PRINT_NAMED_ERROR("BlockWorld.AddLocatedObject.AddingGhostObject",
                      "Adding ghost objects to BlockWorld is not permitted");
    return;
  }

  // grab the current pointer and check it's empty (do not expect overwriting)
  auto& objectsInThisOrigin = _locatedObjects[objectOriginID];
  auto existingIt = FindInContainerWithID(objectsInThisOrigin, object->GetID());
  if (existingIt != objectsInThisOrigin.end()) {
    DEV_ASSERT(false, "BlockWorld.AddLocatedObject.ObjectIDInUseInOrigin");
    objectsInThisOrigin.erase(existingIt);
  }
  
  objectsInThisOrigin.push_back(object); // store the new object, this increments refcount

  // set the viz manager on this new object
  object->SetVizManager(_robot->GetContext()->GetVizManager());

  PRINT_CH_INFO("BlockWorld", "BlockWorld.AddLocatedObject",
                "Adding new %s%s object and ID=%d ActID=%d FacID=%s at (%.1f, %.1f, %.1f), in frame %s.",
                object->IsActive() ? "active " : "",
                EnumToString(object->GetType()),
                object->GetID().GetValue(),
                object->GetActiveID(),
                object->GetFactoryID().c_str(),
                object->GetPose().GetTranslation().x(),
                object->GetPose().GetTranslation().y(),
                object->GetPose().GetTranslation().z(),
                object->GetPose().FindRoot().GetName().c_str());

  // fire DAS event
  DASMSG(robot.object_located, "robot.object_located", "First time object has been seen in this origin");
  DASMSG_SET(s1, EnumToString(object->GetType()), "ObjectType");
  DASMSG_SET(s2, object->GetPose().FindRoot().GetName(), "Name of frame");
  DASMSG_SET(i1, object->GetID().GetValue(), "ObjectID");
  DASMSG_SEND();

  // make sure that everyone gets notified that there's a new object in town, I mean in this origin
  {
    const Pose3d* oldPosePtr = nullptr;
    const PoseState oldPoseState = PoseState::Invalid;
    _robot->GetMapComponent().UpdateObjectPose(*object, oldPosePtr, oldPoseState);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::SetRobotOnChargerContacts()
{
  const Pose3d& poseWrtRobot = Charger::GetDockPoseRelativeToRobot(*_robot);
  const Pose3d poseWrtOrigin = poseWrtRobot.GetWithRespectToRoot();
  
  BlockWorldFilter chargerFilter;
  chargerFilter.SetAllowedTypes({ObjectType::Charger_Basic});
  auto* charger = FindLocatedMatchingObject(chargerFilter);
  if (charger != nullptr) {
    // Found a match in this origin - simply update its pose
    SetObjectPose(charger->GetID(), poseWrtOrigin, PoseState::Known);
    charger->SetLastObservedTime((TimeStamp_t) _robot->GetLastImageTimeStamp());
  } else {
    // Don't have a match in this origin, so create a new instance. If we have a match in _another_ origin, copy its
    // ID and delete it.
    // Note: We could localize to the existing charger here, but if we've gotten to this point it likely means
    // someone has picked up the robot and placed it on the charger. If that's the case, we make the assumption that
    // the world has changed enough that we should just start anew rather than use an old origin.
    std::shared_ptr<ObservableObject> newCharger;
    newCharger.reset(new Charger{});
    
    chargerFilter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
    charger = FindLocatedMatchingObject(chargerFilter);
    if (charger != nullptr) {
      newCharger->CopyID(charger);
      DeleteLocatedObjects(chargerFilter);
    } else {
      newCharger->SetID();
    }
    newCharger->SetPose(poseWrtOrigin, poseWrtRobot.GetTranslation().Length(), PoseState::Known);
    newCharger->SetLastObservedTime((TimeStamp_t) _robot->GetLastImageTimeStamp());
    AddLocatedObject(newCharger);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result BlockWorld::SetObjectPose(const ObjectID& objId,
                                 const Pose3d& newPose,
                                 const PoseState poseState,
                                 const bool makeWrtOrigin)
{
  auto* object = GetLocatedObjectByID(objId);
  if (object == nullptr) {
    LOG_ERROR("BlockWorld.SetObjectPose.ObjectDoesNotExist",
              "Object %d does not exist in the current origin",
              objId.GetValue());
    return RESULT_FAIL;
  }
  
  // Even if makeWrtOrigin is false, we still want to ensure that the given pose is in the same origin as the robot's
  // world origin.
  Pose3d poseWrtOrigin;
  if (!newPose.GetWithRespectTo(_robot->GetWorldOrigin(), poseWrtOrigin)) {
    LOG_ERROR("BlockWorld.SetObjectPose.BadPose",
              "Could not get pose w.r.t. origin");
    return RESULT_FAIL;
  }
  
  const auto& newObjectPose = makeWrtOrigin ? poseWrtOrigin : newPose;
  object->SetPose(newObjectPose, object->GetLastPoseUpdateDistance(), poseState);
  
  // Inform map component of the updated pose
  _robot->GetMapComponent().UpdateObjectPose(*object, &object->GetPose(), object->GetPoseState());
  
  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::MarkObjectDirty(ObservableObject* object)
{
  DEV_ASSERT(object->HasValidPose(), "BlockWorld.MarkObjectDirty.CantChangePoseStateOfInvalidObjects");

  if (_robot->GetCarryingComponent().IsCarryingObject(object->GetID())) {
    LOG_WARNING("BlockWorld.MarkObjectDirty.CarryingObject", "Not marking carried object as dirty");
    return;
  }
  
  const PoseState oldPoseState = object->GetPoseState();
  if (oldPoseState != PoseState::Dirty) {
    object->SetPoseState(PoseState::Dirty);
    
    if(_robot->GetLocalizedTo() == object->GetID()) {
      _robot->SetLocalizedTo(nullptr);
    }
    
    if (_robot->IsPoseInWorldOrigin(object->GetPose())) {
      _robot->GetMapComponent().UpdateObjectPose(*object, &object->GetPose(), oldPoseState);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::OnRobotDelocalized(PoseOriginID_t newWorldOriginID)
{
  // Since we are no longer relocalizing between deloc events, clear the current set of objects
  _locatedObjects.clear();

  // create a new memory map for this origin
  _robot->GetMapComponent().CreateLocalizedMemoryMap(newWorldOriginID);

  // deselect blockworld's selected object, if it has one
  DeselectCurrentObject();

  // notify about updated object states
  BroadcastLocatedObjectStates();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::SanityCheckBookkeeping() const
{
  // Sanity checks for robot's origin
  DEV_ASSERT(_robot->GetPose().IsChildOf(_robot->GetWorldOrigin()),
             "BlockWorld.Update.RobotParentShouldBeOrigin");
  DEV_ASSERT(_robot->IsPoseInWorldOrigin(_robot->GetPose()),
             "BlockWorld.Update.BadRobotOrigin");
  
  // Sanity check our containers to make sure each located object's properties
  // match the keys of the containers within which it is stored
  
  ANKI_VERIFY(_locatedObjects.size() <= 2,
              "BlockWorld.SanityCheckBookkeeping.TooManyOrigins",
              "Should only have at most 2 origins");
  
  std::set<ObjectType> knownTypes;
  const auto worldOrigin = _robot->GetWorldOriginID();
  for(auto const& objectsByOrigin : _locatedObjects)
  {
    const auto& originID = objectsByOrigin.first;
    const auto& objects  = objectsByOrigin.second;
    
    // if any origin besides from the current origin has no observable objects,
    // it should have been deleted
    ANKI_VERIFY(worldOrigin == originID || !objects.empty(),
                "BlockWorld.SanityCheckBookkeeping.NoObjectsInOrigin",
                "OriginId: %d", objectsByOrigin.first);
    
    for(auto const& object : objects) {
      if (object == nullptr) {
        ANKI_VERIFY(false, "BlockWorld.SanityCheckBookkeeping.NullObject", "");
        continue;
      }
      const Pose3d& origin = object->GetPose().FindRoot();
      const PoseOriginID_t objectsOriginID = origin.GetID();
      const auto& objType = object->GetType();
      ANKI_VERIFY(PoseOriginList::UnknownOriginID != objectsOriginID,
                  "BlockWorld.SanityCheckBookkeeping.ObjectWithUnknownOriginID",
                  "Origin: %s", origin.GetName().c_str());
      ANKI_VERIFY(objectsByOrigin.first == objectsOriginID,
                  "BlockWorld.SanityCheckBookkeeping.MismatchedOrigin",
                  "%s Object %d is in Origin:%d but is keyed by Origin:%d",
                  EnumToString(objType), object->GetID().GetValue(),
                  objectsOriginID, objectsByOrigin.first);
      

      if (object->IsUnique()) {
        ANKI_VERIFY(knownTypes.find(objType) == knownTypes.end(),
                    "BlockWorld.SanityCheckBookkeeping.MultipleUniqueInstances",
                    "%s Object %d in Origin:%d already exists in another origin!",
                    EnumToString(objType), object->GetID().GetValue(), objectsOriginID);
      }
      knownTypes.insert(objType);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result BlockWorld::UpdateObservedMarkers(const std::list<Vision::ObservedMarker>& currentObsMarkers)
{
  ANKI_CPU_PROFILE("BlockWorld::UpdateObservedMarkers");

  if(!currentObsMarkers.empty())
  {
    const RobotTimeStamp_t atTimestamp = currentObsMarkers.front().GetTimeStamp();

    // Sanity check
    if(ANKI_DEVELOPER_CODE)
    {
      for(auto const& marker : currentObsMarkers)
      {
        if(marker.GetTimeStamp() != atTimestamp)
        {
          PRINT_NAMED_ERROR("BlockWorld.UpdateObservedMarkers.MisMatchedTimestamps", "Expected t=%u, Got t=%u",
                            (TimeStamp_t)atTimestamp, marker.GetTimeStamp());
          return RESULT_FAIL;
        }
      }
    }

    // New timestep, new set of occluders. Get rid of anything registered as
    // an occluder with the robot's camera
    _robot->GetVisionComponent().GetCamera().ClearOccluders();
    _robot->GetVisionComponent().AddLiftOccluder(atTimestamp);

    // Optional: don't allow markers seen enclosed in other markers
    // Note: If we still need this method at some future point, take a look at
    // https://github.com/anki/victor/blob/dce0730b201f1cd8f1cacdf7c101152c545c08ad/engine/blockWorld/blockWorld.cpp#L2193
    // or
    // https://github.com/anki/cozmo-one/blob/18ae758c6fd0f4f7b47356410a5612c1d46526bd/engine/blockWorld/blockWorld.cpp#L2356
    //RemoveMarkersWithinMarkers(currentObsMarkers);

    // Add, update, and/or localize the robot to any objects indicated by the
    // observed markers
    {
      std::vector<std::shared_ptr<ObservableObject>> objectsSeen;

      _objectLibrary.CreateObjectsFromMarkers(currentObsMarkers, objectsSeen);

      const Result result = ProcessVisualObservations(objectsSeen, atTimestamp);
      if(result != RESULT_OK) {
        PRINT_NAMED_ERROR("BlockWorld.UpdateObservedMarkers.AddAndUpdateFailed", "");
        return result;
      }
    }

    // Delete any objects that should have been observed but weren't,
    // visualize objects that were observed:
    CheckForUnobservedObjects(atTimestamp);
  }
  else
  {
    const RobotTimeStamp_t lastImgTimestamp = _robot->GetLastImageTimeStamp();
    if(lastImgTimestamp > 0) // Avoid warning on first Update()
    {
      // Even if there were no markers observed, check to see if there are
      // any previously-observed objects that are partially visible (some part
      // of them projects into the image even if none of their markers fully do)
      _robot->GetVisionComponent().GetCamera().ClearOccluders();
      _robot->GetVisionComponent().AddLiftOccluder(lastImgTimestamp);
      CheckForUnobservedObjects(lastImgTimestamp);
    }
  }

#   define DISPLAY_ALL_OCCLUDERS 0
  if(DISPLAY_ALL_OCCLUDERS)
  {
    static Vision::Image dispOcc(240,320);
    dispOcc.FillWith(0);
    std::vector<Rectangle<f32>> occluders;
    _robot->GetVisionComponent().GetCamera().GetAllOccluders(occluders);
    for(auto const& rect : occluders)
    {
      std::vector<cv::Point2i> points{rect.GetTopLeft().get_CvPoint_(), rect.GetTopRight().get_CvPoint_(),
        rect.GetBottomRight().get_CvPoint_(), rect.GetBottomLeft().get_CvPoint_()};
      cv::fillConvexPoly(dispOcc.get_CvMat_(), points, 255);
    }
    dispOcc.Display("Occluders");
  }

  return RESULT_OK;

} // UpdateObservedMarkers()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::CheckForRobotObjectCollisions()
{
  BlockWorldFilter intersectingObjectFilter;
  intersectingObjectFilter.SetOriginMode(BlockWorldFilter::OriginMode::InRobotFrame);
  intersectingObjectFilter.SetFilterFcn([this](const ObservableObject* object) {
    return IntersectsRobotBoundingBox(object);
  });
  
  ModifierFcn markAsDirty = [this](ObservableObject* object) {
    MarkObjectDirty(object);
  };
  
  ModifyLocatedObjects(markAsDirty, intersectingObjectFilter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool BlockWorld::IntersectsRobotBoundingBox(const ObservableObject* object) const
{
  // If this object is _allowed_ to intersect with the robot, no reason to
  // check anything
  if(object->CanIntersectWithRobot()) {
    return false;
  }

  // Only check objects that are in accurate/known pose state
  if(!object->IsPoseStateKnown()) {
    return false;
  }

  const ObjectID& objectID = object->GetID();

  // Don't worry about collision with an object being carried or that we are
  // docking with, since we are expecting to be in close proximity to either
  const bool isCarryingObject = _robot->GetCarryingComponent().IsCarryingObject(objectID);
  const bool isDockingWithObject = _robot->GetDockingComponent().GetDockObject() == objectID;
  if(isCarryingObject || isDockingWithObject) {
    return false;
  }

  // Check block's bounding box in same coordinates as this robot to
  // see if it intersects with the robot's bounding box. Also check to see
  // block and the robot are at overlapping heights.  Skip this check
  // entirely if the block isn't in the same coordinate tree as the
  // robot.
  Pose3d objectPoseWrtRobotOrigin;
  if(false == object->GetPose().GetWithRespectTo(_robot->GetWorldOrigin(), objectPoseWrtRobotOrigin))
  {
    LOG_WARNING("BlockWorld.IntersectsRobotBoundingBox.BadOrigin",
                "Could not get %s %d pose (origin: %s) w.r.t. robot origin (%s)",
                EnumToString(object->GetType()), objectID.GetValue(),
                object->GetPose().FindRoot().GetName().c_str(),
                _robot->GetWorldOrigin().GetName().c_str());
    return false;
  }

  // Check if the object is in the same plane as the robot
  // Note: we pad the robot's height by the object's half-height and then
  //       just treat the object as a point (similar to configuration-space
  //       expansion we do for the planner)
  const f32 objectHalfZDim = 0.5f*object->GetDimInParentFrame<'Z'>();
  const f32 objectHeight   = objectPoseWrtRobotOrigin.GetTranslation().z();
  const f32 robotBottom    = _robot->GetPose().GetTranslation().z();
  const f32 robotTop       = robotBottom + ROBOT_BOUNDING_Z;

  const bool inSamePlane = ((objectHeight >= (robotBottom - objectHalfZDim)) &&
                            (objectHeight <= (robotTop + objectHalfZDim)));

  if(!inSamePlane) {
    return false;
  }

  // Check if the object's bounding box intersects the robot's
  const Quad2f objectBBox = object->GetBoundingQuadXY(objectPoseWrtRobotOrigin);
  const Quad2f robotBBox = _robot->GetBoundingQuadXY(_robot->GetPose().GetWithRespectToRoot(),
                                                     ROBOT_BBOX_PADDING_FOR_OBJECT_COLLISION);

  const bool bboxIntersects = robotBBox.Intersects(objectBBox);
  if(bboxIntersects)
  {
    LOG_INFO("BlockWorld.IntersectsRobotBoundingBox.ObjectRobotIntersection",
             "Object %s %d intersects robot's bounding quad.",
             EnumToString(object->GetType()), object->GetID().GetValue());

    return true;
  }

  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::ClearLocatedObjectHelper(ObservableObject* object)
{
  if(object == nullptr) {
    PRINT_NAMED_WARNING("BlockWorld.ClearObjectHelper.NullObjectPointer",
                        "BlockWorld asked to clear a null object pointer.");
    return;
  }

  // Check to see if this object is the one the robot is localized to.
  // If so, the robot needs to be marked as localized to nothing.
  if(_robot->GetLocalizedTo() == object->GetID()) {
    PRINT_CH_INFO("BlockWorld", "BlockWorld.ClearObjectHelper.LocalizeRobotToNothing",
                  "Setting robot as localized to no object, because it "
                  "is currently localized to %s object with ID=%d, which is "
                  "about to be cleared.",
                  ObjectTypeToString(object->GetType()), object->GetID().GetValue());
    _robot->SetLocalizedTo(nullptr);
  }

  // Check to see if this object is the one the robot is carrying.
  if(_robot->GetCarryingComponent().GetCarryingObjectID() == object->GetID()) {
    PRINT_CH_INFO("BlockWorld", "BlockWorld.ClearObjectHelper.ClearingCarriedObject",
                  "Clearing %s object %d which robot thinks it is carrying.",
                  ObjectTypeToString(object->GetType()),
                  object->GetID().GetValue());
    _robot->GetCarryingComponent().UnSetCarryingObject();
  }

  if(_selectedObjectID == object->GetID()) {
    PRINT_CH_INFO("BlockWorld", "BlockWorld.ClearObjectHelper.ClearingSelectedObject",
                  "Clearing %s object %d which is currently selected.",
                  ObjectTypeToString(object->GetType()),
                  object->GetID().GetValue());
    _selectedObjectID.UnSet();
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindConnectedMatchingBlocks(const BlockWorldFilter& filter, std::vector<const Block*>& result) const
{
  // slight abuse of the FindObjectHelper, I just use it for filtering, then I add everything that passes
  // the filter to the result vector
  ModifierFcn addToResult = [&result](ObservableObject* candidateObject) {
    // TODO this could be a checked_cast (dynamic in dev, static in shipping)
    const auto* candidateActiveObject = dynamic_cast<Block*>(candidateObject);
    result.push_back(candidateActiveObject);
  };

  // ignore return value, since the findLambda stored everything in result
  FindConnectedObjectHelper(filter, addToResult, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindConnectedMatchingBlocks(const BlockWorldFilter& filter, std::vector<Block*>& result)
{
  // slight abuse of the FindObjectHelper, I just use it for filtering, then I add everything that passes
  // the filter to the result vector
  ModifierFcn addToResult = [&result](ObservableObject* candidateObject) {
    auto* candidateActiveObject = dynamic_cast<Block*>(candidateObject);
    result.push_back(candidateActiveObject);
  };

  // ignore return value, since the findLambda stored everything in result
  FindConnectedObjectHelper(filter, addToResult, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedMatchingObjects(const BlockWorldFilter& filter, std::vector<ObservableObject*>& result)
{
  // slight abuse of the FindLocatedObjectHelper, I just use it for filtering, then I add everything that passes
  // the filter to the result vector
  ModifierFcn addToResult = [&result](ObservableObject* candidateObject) {
    result.push_back(candidateObject);
  };

  // ignore return value, since the findLambda stored everything in result
  FindLocatedObjectHelper(filter, addToResult, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedMatchingObjects(const BlockWorldFilter& filter, std::vector<const ObservableObject*>& result) const
{
  // slight abuse of the FindLocatedObjectHelper, I just use it for filtering, then I add everything that passes
  // the filter to the result vector
  ModifierFcn addToResult = [&result](ObservableObject* candidateObject) {
    result.push_back(candidateObject);
  };

  // ignore return value, since the findLambda stored everything in result
  FindLocatedObjectHelper(filter, addToResult, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ObservableObject* BlockWorld::FindMostRecentlyObservedObject(const BlockWorldFilter& filterIn) const
{
  RobotTimeStamp_t bestTime = 0;

  BlockWorldFilter filter(filterIn);
  filter.AddFilterFcn([&bestTime](const ObservableObject* current)
  {
    const RobotTimeStamp_t currentTime = current->GetLastObservedTime();
    if(currentTime > bestTime) {
      bestTime = currentTime;
      return true;
    } else {
      return false;
    }
  });

  return FindLocatedObjectHelper(filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace {

// Helper to create a filter common to several methods below
static inline BlockWorldFilter GetIntersectingObjectsFilter(const Quad2f& quad, f32 padding_mm,
                                                            const BlockWorldFilter& filterIn)
{
  BlockWorldFilter filter(filterIn);
  filter.AddFilterFcn([&quad,padding_mm](const ObservableObject* object) {
    // Get quad of object and check for intersection
    Quad2f quadExist = object->GetBoundingQuadXY(object->GetPose(), padding_mm);
    if( quadExist.Intersects(quad) ) {
      return true;
    } else {
      return false;
    }
  });

  return filter;
}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedIntersectingObjects(const ObservableObject* objectSeen,
                                         std::vector<const ObservableObject*>& intersectingExistingObjects,
                                         f32 padding_mm,
                                         const BlockWorldFilter& filter) const
{
  Quad2f quadSeen = objectSeen->GetBoundingQuadXY(objectSeen->GetPose(), padding_mm);
  FindLocatedMatchingObjects(GetIntersectingObjectsFilter(quadSeen, padding_mm, filter), intersectingExistingObjects);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedIntersectingObjects(const ObservableObject* objectSeen,
                                         std::vector<ObservableObject*>& intersectingExistingObjects,
                                         f32 padding_mm,
                                         const BlockWorldFilter& filter)
{
  Quad2f quadSeen = objectSeen->GetBoundingQuadXY(objectSeen->GetPose(), padding_mm);
  FindLocatedMatchingObjects(GetIntersectingObjectsFilter(quadSeen, padding_mm, filter), intersectingExistingObjects);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedIntersectingObjects(const Quad2f& quad,
                                         std::vector<const ObservableObject *> &intersectingExistingObjects,
                                         f32 padding_mm,
                                         const BlockWorldFilter& filterIn) const
{
  FindLocatedMatchingObjects(GetIntersectingObjectsFilter(quad, padding_mm, filterIn), intersectingExistingObjects);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::FindLocatedIntersectingObjects(const Quad2f& quad,
                                         std::vector<ObservableObject *> &intersectingExistingObjects,
                                         f32 padding_mm,
                                         const BlockWorldFilter& filterIn)
{
  FindLocatedMatchingObjects(GetIntersectingObjectsFilter(quad, padding_mm, filterIn), intersectingExistingObjects);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::GetLocatedObjectBoundingBoxesXY(const f32 minHeight, const f32 maxHeight, const f32 padding,
                                                 std::vector<std::pair<Quad2f,ObjectID> >& rectangles,
                                                 const BlockWorldFilter& filterIn) const
{
  BlockWorldFilter filter(filterIn);

  // Note that we add this filter function, meaning we still rely on the
  // default filter function which rules out objects with unknown pose state
  filter.AddFilterFcn([minHeight, maxHeight, padding, &rectangles](const ObservableObject* object)
  {
    const Point3f rotatedSize( object->GetPose().GetRotation() * object->GetSize() );
    const f32 objectCenter = object->GetPose().GetWithRespectToRoot().GetTranslation().z();

    const f32 objectTop = objectCenter + (0.5f * rotatedSize.z());
    const f32 objectBottom = objectCenter - (0.5f * rotatedSize.z());

    const bool bothAbove = (objectTop >= maxHeight) && (objectBottom >= maxHeight);
    const bool bothBelow = (objectTop <= minHeight) && (objectBottom <= minHeight);

    if( !bothAbove && !bothBelow )
    {
      rectangles.emplace_back(object->GetBoundingQuadXY(padding), object->GetID());
      return true;
    }

    return false;
  });

  FindLocatedObjectHelper(filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::GetObstacles(std::vector<std::pair<Quad2f,ObjectID> >& boundingBoxes, const f32 padding) const
{
  BlockWorldFilter filter;
  if (_robot->GetCarryingComponent().IsCarryingObject()) {
    filter.SetIgnoreIDs({{_robot->GetCarryingComponent().GetCarryingObjectID()}});
  }

  // Figure out height filters in world coordinates (because GetLocatedObjectBoundingBoxesXY()
  // uses heights of objects in world coordinates)
  const Pose3d robotPoseWrtOrigin = _robot->GetPose().GetWithRespectToRoot();
  const f32 minHeight = robotPoseWrtOrigin.GetTranslation().z();
  const f32 maxHeight = minHeight + _robot->GetHeight();

  GetLocatedObjectBoundingBoxesXY(minHeight, maxHeight, padding, boundingBoxes, filter);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool BlockWorld::IsZombiePoseOrigin(PoseOriginID_t originID) const
{
  // really, pass in a valid origin ID
  DEV_ASSERT(_robot->GetPoseOriginList().ContainsOriginID(originID), "BlockWorld.IsZombiePoseOrigin.InvalidOriginID");

  // current world is not a zombie
  const bool isCurrent = (originID == _robot->GetPoseOriginList().GetCurrentOriginID());
  if ( isCurrent ) {
    return false;
  }

  // check if there are any objects we can localize to
  const bool hasLocalizableObjects = AnyRemainingLocalizableObjects(originID);
  const bool isZombie = !hasLocalizableObjects;
  return isZombie;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool BlockWorld::AnyRemainingLocalizableObjects(PoseOriginID_t originID) const
{
  // Filter out anything that can't be used for localization (i.e. only allow charger)
  BlockWorldFilter filter;
  filter.SetAllowedTypes({ObjectType::Charger_Basic});

  // Allow all origins if UnknownOriginID was passed in, otherwise allow only the specified origin
  if (originID == PoseOriginList::UnknownOriginID) {
    filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
  } else {
    filter.SetOriginMode(BlockWorldFilter::OriginMode::Custom);
    filter.AddAllowedOrigin(originID);
  }

  const bool localizableObjectFound = (nullptr != FindLocatedObjectHelper(filter, nullptr, true));
  return localizableObjectFound;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::DeleteLocatedObjects(const BlockWorldFilter& filter)
{
  // cache object info since we are going to destroy the objects
  struct DeletedObjectInfo {
    DeletedObjectInfo(const Pose3d& oldPose, const PoseState oldPoseState, const ObservableObject* objCopy)
    : _oldPose(oldPose), _oldPoseState(oldPoseState), _objectCopy(objCopy) { }
    const Pose3d _oldPose;
    const PoseState _oldPoseState;
    const ObservableObject* _objectCopy;
  };
  std::vector<DeletedObjectInfo> objectsToBroadcast;

  for (auto& originPair : _locatedObjects) {
    const PoseOriginID_t crntOriginID = originPair.first;

    if(filter.ConsiderOrigin(crntOriginID, _robot->GetPoseOriginList().GetCurrentOriginID()))
    {
      auto& objectContainer = originPair.second;
      for (auto objectIter = objectContainer.begin() ; objectIter != objectContainer.end() ; ) {
        auto* object = objectIter->get();
        if (object == nullptr) {
          LOG_ERROR("BlockWorld.DeleteLocatedObjects.NullObject", "origin %d", crntOriginID);
          objectIter = objectContainer.erase(objectIter);
        } else if (filter.ConsiderType(object->GetType()) &&
                   filter.ConsiderObject(object)) {
          // clear objects in current origin (others should not be needed)
          const bool isCurrentOrigin = (crntOriginID == _robot->GetPoseOriginList().GetCurrentOriginID());
          if ( isCurrentOrigin )
          {
            ClearLocatedObjectHelper(object);

            // Create a copy of the object so we can notify listeners
            {
              DEV_ASSERT(object->HasValidPose(), "BlockWorld.DeleteLocatedObjects.InvalidPoseState");
              ObservableObject* objCopy = object->CloneType();
              objCopy->CopyID(object);
              if (objCopy->IsActive()) {
                objCopy->SetActiveID(object->GetActiveID()); // manually having to copy all IDs is fishy design
                objCopy->SetFactoryID(object->GetFactoryID());
              }
              objectsToBroadcast.emplace_back( object->GetPose(), object->GetPoseState(), objCopy );
            }
          }

          objectIter = objectContainer.erase(objectIter);
        } else {
          ++objectIter;
        }
      }
    }
  }

  // Remove any now-zombie origins
  DeleteZombieOrigins();
  
  // notify of the deleted objects
  for(auto& objectDeletedInfo : objectsToBroadcast)
  {
    using namespace ExternalInterface;

    // cache values
    const ObjectID& deletedID = objectDeletedInfo._objectCopy->GetID(); // note this won't be valid after delete
    const Pose3d* oldPosePtr = &objectDeletedInfo._oldPose;
    const PoseState oldPoseState = objectDeletedInfo._oldPoseState;

    // PoseConfirmer::PoseChanged (should not have valid pose)
    DEV_ASSERT(!objectDeletedInfo._objectCopy->HasValidPose(), "BlockWorld.DeleteLocatedObjects.CopyInheritedPose");
    _robot->GetMapComponent().UpdateObjectPose(*objectDeletedInfo._objectCopy, oldPosePtr, oldPoseState);

    RobotDeletedLocatedObject msg(deletedID);

    if( ANKI_DEV_CHEATS ) {
      SendObjectUpdateToWebViz( msg );
    }

    // RobotDeletedLocatedObject
    _robot->Broadcast(MessageEngineToGame(std::move(msg)));

    // delete the copy we made now, since it won't be useful anymore
    Util::SafeDelete(objectDeletedInfo._objectCopy);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::DeselectCurrentObject()
{
  if(_selectedObjectID.IsSet())
  {
    if(ENABLE_DRAWING)
    {
      // Erase the visualization of the selected object's preaction poses/lines. Note we do this
      // across all frames in case the selected object is in a different origin and we have delocalized
      BlockWorldFilter filter;
      filter.SetOriginMode(BlockWorldFilter::OriginMode::InAnyFrame);
      filter.AddAllowedID(_selectedObjectID);

      const ObservableObject* object = FindLocatedMatchingObject(filter);
      if(nullptr != object) {
        object->EraseVisualization();
      }
    }
    _selectedObjectID.UnSet();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::ClearLocatedObjectByIDInCurOrigin(const ObjectID& withID)
{
  ObservableObject* object = GetLocatedObjectByID(withID);
  ClearLocatedObjectHelper(object);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::ClearLocatedObject(ObservableObject* object)
{
  ClearLocatedObjectHelper(object);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool BlockWorld::SelectObject(const ObjectID& objectID)
{
  ObservableObject* newSelection = GetLocatedObjectByID(objectID);

  if(newSelection != nullptr) {
    // Unselect current object of interest, if it still exists (Note that it may just get
    // reselected here, but I don't think we care.)
    DeselectCurrentObject();

    // Record new object of interest as selected so it will draw differently
    _selectedObjectID = objectID;
    PRINT_CH_INFO("BlockWorld", "BlockWorld.SelectObject",
                  "Selected Object with ID=%d", objectID.GetValue());

    return true;
  } else {
    PRINT_CH_INFO("BlockWorld", "BlockWorld.SelectObject.InvalidID",
                  "Object with ID=%d not found. Not updating selected object.", objectID.GetValue());
    return false;
  }
} // SelectObject()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::CycleSelectedObject()
{
  bool currSelectedObjectFound = false;
  bool newSelectedObjectSet = false;

  // Iterate through all the objects
  BlockWorldFilter filter;
  std::vector<ObservableObject*> allObjects;
  FindLocatedMatchingObjects(filter, allObjects);
  for(auto const & obj : allObjects)
  {
    ActionableObject* object = dynamic_cast<ActionableObject*>(obj);
    if(object != nullptr &&
       !_robot->GetCarryingComponent().IsCarryingObject(object->GetID()))
    {
      //PRINT_INFO("currID: %d", block.first);
      if (currSelectedObjectFound) {
        // Current block of interest has been found.
        // Set the new block of interest to the next block in the list.
        _selectedObjectID = object->GetID();
        newSelectedObjectSet = true;
        //PRINT_INFO("new block found: id %d  type %d", block.first, blockType.first);
        break;
      } else if (object->GetID() == _selectedObjectID) {
        currSelectedObjectFound = true;
        if(ENABLE_DRAWING)
        {
          // Erase the visualization of the current selection so we can draw only the
          // the new one (even if we end up just re-drawing this one)
          object->EraseVisualization();
        }
        //PRINT_INFO("curr block found: id %d  type %d", block.first, blockType.first);
      }
    }

    if (newSelectedObjectSet) {
      break;
    }
  } // for all objects

  // If the current object of interest was found, but a new one was not set
  // it must have been the last block in the map. Set the new object of interest
  // to the first object in the map as long as it's not the same object.
  if (!currSelectedObjectFound || !newSelectedObjectSet) {

    // Find first object
    ObjectID firstObject; // initialized to un-set
    for(auto const & obj : allObjects) {
      const ActionableObject* object = dynamic_cast<ActionableObject*>(obj);
      if(object != nullptr &&
         !_robot->GetCarryingComponent().IsCarryingObject(object->GetID()))
      {
        firstObject = obj->GetID();
        break;
      }
    }


    if (firstObject == _selectedObjectID || !firstObject.IsSet()){
      //PRINT_INFO("Only one object in existence.");
    } else {
      //PRINT_INFO("Setting object of interest to first block");
      _selectedObjectID = firstObject;
    }
  }

  if(_selectedObjectID.IsSet())
  {
    DEV_ASSERT(nullptr != GetLocatedObjectByID(_selectedObjectID),
               "BlockWorld.CycleSelectedObject.ObjectNotFound");
    PRINT_CH_DEBUG("BlockWorld", "BlockWorld.CycleSelectedObject",
                   "Object of interest: ID = %d",_selectedObjectID.GetValue());
  }
  else
  {
    PRINT_CH_DEBUG("BlockWorld", "BlockWorld.CycleSelectedObject.NoObject", "No object of interest found");
  }

} // CycleSelectedObject()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::DrawAllObjects() const
{
  if(!ENABLE_DRAWING) {
    // Don't draw anything in shipping builds
    return;
  }

  auto webSender = WebService::WebVizSender::CreateWebVizSender("navmap", _robot->GetContext()->GetWebService());

  const ObjectID& locObject = _robot->GetLocalizedTo();

  // Note: only drawing objects in current coordinate frame!
  BlockWorldFilter filter;
  filter.SetOriginMode(BlockWorldFilter::OriginMode::InRobotFrame);
  ModifierFcn visualizeHelper = [this,&locObject,webSender](ObservableObject* object)
  {
    if(object->GetID() == _selectedObjectID) {
      // Draw selected object in a different color and draw its pre-action poses
      object->Visualize(NamedColors::SELECTED_OBJECT);

      const ActionableObject* selectedObject = dynamic_cast<const ActionableObject*>(object);
      if(selectedObject == nullptr) {
        PRINT_NAMED_WARNING("BlockWorld.DrawAllObjects.NullSelectedObject",
                            "Selected object ID = %d, but it came back null.",
                            GetSelectedObject().GetValue());
      } else {
        std::vector<std::pair<Quad2f,ObjectID> > obstacles;
        _robot->GetBlockWorld().GetObstacles(obstacles);
        selectedObject->VisualizePreActionPoses(obstacles, _robot->GetPose());
      }
    }
    else if(object->GetID() == locObject) {
      // Draw object we are localized to in a different color
      object->Visualize(NamedColors::LOCALIZATION_OBJECT);
    }
    else if(object->GetPoseState() == PoseState::Dirty) {
      // Draw dirty objects in a special color
      object->Visualize(NamedColors::DIRTY_OBJECT);
    }
    else if(object->GetPoseState() == PoseState::Invalid) {
      // Draw unknown objects in a special color
      object->Visualize(NamedColors::UNKNOWN_OBJECT);
    }
    else {
      // Draw "regular" objects in current frame in their internal color
      object->Visualize();
    }

    if( webSender ) {
      if( IsValidLightCube(object->GetType(), false) ) {
        Json::Value cubeInfo;
        const auto& pose = object->GetPose();
        cubeInfo["x"] = pose.GetTranslation().x();
        cubeInfo["y"] = pose.GetTranslation().y();
        cubeInfo["z"] = pose.GetTranslation().z();
        cubeInfo["angle"] = pose.GetRotationAngle<'Z'>().ToFloat();
        webSender->Data()["cubes"].append( cubeInfo );
      }
    }
  };

  FindLocatedObjectHelper(filter, visualizeHelper, false);

  // don't fill type unless there's some actual data (to avoid unnecessary sends)
  if( webSender && !webSender->Data().empty() ) {
    webSender->Data()["type"] = "MemoryMapCubes";
  }
} // DrawAllObjects()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::SendObjectUpdateToWebViz( const ExternalInterface::RobotDeletedLocatedObject& msg ) const
{
  if( auto webSender = WebService::WebVizSender::CreateWebVizSender("observedobjects",
                                                                    _robot->GetContext()->GetWebService()) ) {
    webSender->Data()["type"] = "RobotDeletedLocatedObject";
    webSender->Data()["objectID"] = msg.objectID;
  }

} // SendObjectUpdateToWebViz()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void BlockWorld::SendObjectUpdateToWebViz( const ExternalInterface::RobotObservedObject& msg ) const
{
  if( auto webSender = WebService::WebVizSender::CreateWebVizSender("observedobjects",
                                                                    _robot->GetContext()->GetWebService()) ) {
    webSender->Data()["type"] = "RobotObservedObject";
    webSender->Data()["objectID"] = msg.objectID;
    webSender->Data()["objectType"] = ObjectTypeToString(msg.objectType);
    webSender->Data()["isActive"] = msg.isActive;
    webSender->Data()["timestamp"] = msg.timestamp;
  }

} // SendObjectUpdateToWebViz()

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ObjectFamily BlockWorld::LegacyGetObjectFamily(const ObservableObject* const object) const
{
  auto fam = ObjectFamily::Unknown;
  const auto& type = object->GetType();
  if (IsValidLightCube(type, false)) {
    fam = ObjectFamily::LightCube;
  } else if (IsBlockType(type, false)) {
    fam = ObjectFamily::Block;
  } else if (IsChargerType(type, false)) {
    fam = ObjectFamily::Charger;
  } else if (IsCustomType(type, false)) {
    fam = ObjectFamily::CustomObject;
  }
  return fam;
}
  
} // namespace Vector
} // namespace Anki
