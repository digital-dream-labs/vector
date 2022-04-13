/**
 * File: faceWorld.h
 *
 * Author: Andrew Stein (andrew)
 * Created: 2014
 *
 * Description: Implements a container for mirroring on the main thread, the known faces 
 *              from the vision system (which generally runs on another thread).
 *
 * Copyright: Anki, Inc. 2014
 *
 **/


#ifndef __Anki_Cozmo_FaceWorld_H__
#define __Anki_Cozmo_FaceWorld_H__

#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/gazeDirection.h"
#include "coretech/vision/engine/trackedFace.h"

#include "engine/aiComponent/behaviorComponent/behaviorComponents_fwd.h"
#include "engine/robotComponents_fwd.h"
#include "engine/smartFaceId.h"
#include "engine/viz/vizManager.h"
#include "osState/wallTime.h"
#include "util/entityComponent/iDependencyManagedComponent.h"

#include "clad/types/actionTypes.h"

#include <map>
#include <set>
#include <vector>
#include <deque>

namespace Anki {
  
namespace Vision {
struct LoadedKnownFace;
class TrackedFace;
}
  
namespace Vector {
  
  // Forward declarations:
  class Robot;
  
  namespace ExternalInterface {
    struct RobotDeletedFace;
    struct RobotObservedFace;
  }
  
  // FaceWorld is updated at the robot component level, same as BehaviorComponent
  // Therefore BCComponents (which are managed by BehaviorComponent) can't declare dependencies on FaceWorld 
  // since when it's Init/Update relative to BehaviorComponent must be declared by BehaviorComponent explicitly, 
  // not by individual components within BehaviorComponent
  class FaceWorld : public UnreliableComponent<BCComponentID>, 
                    public IDependencyManagedComponent<RobotComponentID>
  {
  public:
    static const s32 MinTimesToSeeFace = 4;

    // NOTE: many functions in this API have two versions, one which takes a Vision::FaceID_t and one which
    // takes a SmartFaceID. The use of SmartFaceID is preferred because it automatically handles face id
    // changes and deleted faces. The raw face id API is maintained only for backwards
    // compatibility. COZMO-10839 is the task that will eventually remove this old interface
    
    FaceWorld();

    //////
    // IDependencyManagedComponent functions
    //////
    virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override;
    virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {
        dependencies.insert(RobotComponentID::CozmoContextWrapper);
    };
    virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {};

    // Prevent hiding function warnings by exposing the (valid) unreliable component methods
    using UnreliableComponent<BCComponentID>::InitDependent;
    using UnreliableComponent<BCComponentID>::GetInitDependencies;
    using UnreliableComponent<BCComponentID>::GetUpdateDependencies;
    //////
    // end IDependencyManagedComponent functions
    //////
    
    Result Update(const std::list<Vision::TrackedFace>& observedFaces);
    Result AddOrUpdateFace(const Vision::TrackedFace& face);
    Result AddOrUpdateGazeDirection(Vision::TrackedFace& face);
  
    Result ChangeFaceID(const Vision::UpdatedFaceID& update);
    
    // Called when robot delocalizes
    void OnRobotDelocalized(PoseOriginID_t worldOriginID);
    
    // Called when Robot rejiggers its pose. Returns number of faces updated
    int UpdateFaceOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID);

    // create a smart face ID or update an existing ID from a raw ID (useful, for example for IDs from CLAD
    // messages)
    SmartFaceID GetSmartFaceID(Vision::FaceID_t faceID) const;
    void UpdateSmartFaceToID(const Vision::FaceID_t faceID, SmartFaceID& smartFaceID);

    // Returns nullptr if not found
    const Vision::TrackedFace* GetFace(Vision::FaceID_t faceID) const;
    const Vision::TrackedFace* GetFace(const SmartFaceID& faceID) const;
    
    // Returns face IDs observed since seenSinceTime_ms (inclusive)
    // Set includeRecognizableOnly=true to only return faces that have been (or can be) recognized.
    // NOTE: This does not necessarily mean they have been recognized as a _named_ person introduced via
    //       MeetCozmo. They could simply be recognized as a session-only person already seen in this session.
    // If relativeRobotAngleTolerence_rad is set to something other than 0, only faces within +/- the relative robot
    // angle will be returned
    std::set<Vision::FaceID_t> GetFaceIDs(RobotTimeStamp_t seenSinceTime_ms = 0,
                                          bool includeRecognizableOnly = false,
                                          float relativeRobotAngleTolerence_rad = kDontCheckRelativeAngle,
                                          const Radians& angleRelativeRobot_rad = 0) const;

    // Returns smart face IDs observed since seenSinceTime_ms (inclusive)
    std::vector<SmartFaceID> GetSmartFaceIDs(RobotTimeStamp_t seenSinceTime_ms = 0,
                                             bool includeRecognizableOnly = false,
                                             float relativeRobotAngleTolerence_rad = kDontCheckRelativeAngle,
                                             const Radians& angleRelativeRobot_rad = 0) const;

    // Returns true if any faces are in the world
    bool HasAnyFaces(RobotTimeStamp_t seenSinceTime_ms = 0, bool includeRecognizableOnly = false) const;

    // If the robot has observed a face, sets poseWrtRobotOrigin to the pose of the last observed face
    // and returns the timestamp when that face was last seen. Otherwise, returns 0. Normally,
    // inRobotOrigin=true, so that the last observed pose is required to be w.r.t. the current origin.
    //
    // If inRobotOriginOnly=false, the returned pose is allowed to be that of a face observed w.r.t. a
    // different coordinate frame, modified such that its parent is the robot's current origin. This
    // could be a completely inaccurate guess for the last observed face pose, but may be "good enough"
    // for some uses.
    RobotTimeStamp_t GetLastObservedFace(Pose3d& poseWrtRobotOrigin, bool inRobotOriginOnly = true) const;

    // Returns true if any action has turned towards this face
    bool HasTurnedTowardsFace(Vision::FaceID_t faceID) const;
    bool HasTurnedTowardsFace(const SmartFaceID& faceID) const;

    // Tell FaceWorld that the robot has turned towards this face (or not, if val=false)
    void SetTurnedTowardsFace(Vision::FaceID_t faceID, bool val = true);
    void SetTurnedTowardsFace(const SmartFaceID& faceID, bool val = true);
    
    // Removes all faces and resets the last observed face timer to 0, so
    // GetLastObservedFace() will return 0.
    void ClearAllFaces();
    
    // Specify a faceID to start an enrollment of a specific ID, i.e. with the intention
    // of naming that person.
    // Use UnknownFaceID to enable (or return to) ongoing "enrollment" of session-only / unnamed faces.
    void Enroll(Vision::FaceID_t faceID, bool forceNewID = false);
    void Enroll(const SmartFaceID& faceID, bool forceNewID = false);

#if ANKI_DEV_CHEATS
    void SaveAllRecognitionImages(const std::string& imagePathPrefix);
    void DeleteAllRecognitionImages();
#endif
    
    bool IsFaceEnrollmentComplete() const { return _lastEnrollmentCompleted; }
    void SetFaceEnrollmentComplete(bool complete) { _lastEnrollmentCompleted = complete; }

    // IsMakingEyeContact with only return true if it finds a face that is making
    // eye contact and has a time stamp greater than seenSinceTime_ms
    bool IsMakingEyeContact(const u32 withinLast_ms) const;

    // This will return true and populate the pose and face with the first
    // stable gaze direction it finds. If this method returns false, no
    // stable gaze was found for all the faces that meet the ShouldReturnFace
    // condition.
    bool GetGazeDirectionPose(const u32 withinLast_ms, Pose3d& gazeDirectionPose,
                              SmartFaceID& faceID) const;
    // This will return true if it finds any stable gaze direction.
    bool AnyStableGazeDirection(const u32 withinLast_ms) const;
    // This will return true if we are able to clear a the gaze history for the
    // face given.
    bool ClearGazeDirectionHistory(const SmartFaceID& faceID);
    // This method checks whether there will be a different face than the one provided
    // in the FOV if the robot were to turn a specific angle, and populates that face
    // and returns true if it finds one. Otherwise if it does not find a face it
    // returns false.
    bool FaceInTurnAngle(const Radians& turnAngle, const SmartFaceID& smartFaceIDToIgnore,
                         const Pose3d& robotPose, SmartFaceID& faceIDToTurnTowards) const;


    // Get the wall times that the given face ID has been observed for named faces. This implementation
    // returns at most 2 entries with front() being the wall time that was recorded first. On loading time,
    // this will populate with wall times from enrolled face entries (even if those faces haven't been seen
    // since boot). It will be updated whenever the face is observed. If it returns 2 entries, then the
    // difference between them can be used as the delta between when we most recently saw the face and the
    // time before that, e.g. to determine when we see someone how long it's been since the last time we saw
    // them.  If the face is unknown, an empty queue will be returned. The queue may contain a single element
    // in the case that it's an enrolled face loaded from storage, or in the case that the face has only been
    // seen once. Tracking only (negative) face IDs are not returned here.
    // 
    // Note: times are only updated here if wall time is accurate (synced with NTP). Inaccurate times (e.g. if
    // we're off wifi) won't get added here at all (although times loaded from disk will)
    using ObservationTimeHistory = std::deque<WallTime::TimePoint_t>;
    const ObservationTimeHistory& GetWallTimesObserved(const SmartFaceID& faceID);
    const ObservationTimeHistory& GetWallTimesObserved(Vision::FaceID_t faceID);

    // this should only be called by robot when the face data is loaded
    void InitLoadedKnownFaces(const std::list<Vision::LoadedKnownFace>& loadedFaces);
    
    // template for all events we subscribe to
    template<typename T>
    void HandleMessage(const T& msg);
    
  private:
    static const int kDontCheckRelativeAngle = 0;

    Robot* _robot;
    
    // FaceEntry is the internal storage for faces in FaceWorld, which include
    // the public-facing TrackedFace plus additional bookkeeping
    struct FaceEntry {
      Vision::TrackedFace      face;
      VizManager::Handle_t     vizHandle;
      s32                      numTimesObserved = 0;
      s32                      numTimesObservedFacingCamera = 0;
      bool                     hasTurnedTowards = false;

      FaceEntry(const Vision::TrackedFace& faceIn);
      bool IsNamed() const { return !face.GetName().empty(); }
      bool HasStableID() const;
    };
    
    using FaceContainer = std::map<Vision::FaceID_t, FaceEntry>;
    using FaceEntryIter = FaceContainer::iterator;
    
    FaceContainer _faceEntries;
    
    Vision::FaceID_t _idCtr = 0;
    
    Pose3d      _lastObservedFacePose;
    RobotTimeStamp_t _lastObservedFaceTimeStamp = 0;

    bool _previousEyeContact = false;
    
    bool _lastEnrollmentCompleted = false;
    
    // Helper used by public Get() methods to determine if an entry should be returned
    bool ShouldReturnFace(const FaceEntry& faceEntry, RobotTimeStamp_t seenSinceTime_ms, bool includeRecognizableOnly,
                          float relativeRobotAngleTolerence_rad = kDontCheckRelativeAngle, const Radians& angleRelativeRobot_rad = 0) const;
    
    // Removes the face and advances the iterator. Notifies any listeners that
    // the face was removed if broadcast==true.
    void RemoveFace(FaceEntryIter& faceIter, bool broadcast = true);
    
    void RemoveFaceByID(Vision::FaceID_t faceID);

    void SetupEventHandlers(IExternalInterface& externalInterface);
    
    void DrawFace(FaceEntry& knownFace, bool drawInImage = true) const;
    void EraseFaceViz(FaceEntry& faceEntry);
    
    void SendObjectUpdateToWebViz( const ExternalInterface::RobotDeletedFace& msg ) const;
    void SendObjectUpdateToWebViz( const ExternalInterface::RobotObservedFace& msg ) const;
    
    std::vector<Signal::SmartHandle> _eventHandles;

    // For each enrolled face, keep track of the last wall time where we observed it as well as the time
    // before that in a deque of max size 2. On engine startup, this timestamp will be read from the known
    // faces saved album data for the initial entry so it can work across boots
    using ObservationHistoryMap = std::map<Vision::FaceID_t, ObservationTimeHistory>;
    ObservationHistoryMap _wallTimesObserved;

    std::map<Vision::FaceID_t, Vision::GazeDirection> _gazeDirection;
    
  }; // class FaceWorld
  
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_FaceWorld_H__
