/**
 * File: faceWorld.cpp
 *
 * Author: Andrew Stein (andrew)
 * Created: 2014
 *
 * Description: Implements a container for mirroring on the main thread the faces
 *              from the vision system (which generally runs on another thread).
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "engine/faceWorld.h"


#include "anki/cozmo/shared/cozmoConfig.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/externalInterface/messageGameToEngine.h"
#include "clad/types/enrolledFaceStorage.h"
#include "coretech/common/engine/math/poseOriginList.h"
#include "coretech/common/shared/math/radians.h"

#include "engine/actions/animActions.h"
#include "engine/actions/basicActions.h"
#include "engine/components/robotStatsTracker.h"
#include "engine/components/visionComponent.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"
#include "engine/smartFaceId.h"

#include "util/console/consoleInterface.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/logging/DAS.h"
#include "webServerProcess/src/webService.h"

#include "clad/types/featureGateTypes.h"
#include "engine/utils/cozmoFeatureGate.h"



namespace Anki {
namespace Vector {

  // How long before deleting an unnamed, unobserved face.
  // NOTE: we never delete _named_ faces.
  // This used to be smaller, but we're starting it on the order of minutes. This might need to be
  // settable by whatever behaviors are running
  CONSOLE_VAR(u32, kDeletionTimeout_ms, "Vision.FaceWorld", 10*60*1000);

  // The distance threshold inside of which to head positions are considered to be the same face
  CONSOLE_VAR(float, kHeadCenterPointThreshold_mm, "Vision.FaceWorld", 220.f);

  // We don't log session-only (unnamed) faces to DAS until we consider them "stable"
  CONSOLE_VAR(u32, kNumTimesToSeeFrontalToBeStable, "Vision.FaceWorld", 30);

  // Log recognition to DAS if we haven't seen a face for this long and then re-see it
  CONSOLE_VAR(u32, kTimeUnobservedBeforeReLoggingToDAS_ms, "Vision.FaceWorld", 10000);

  // Ignore faces detected below the robot (except when picked up), to help reduce false positives
  CONSOLE_VAR(bool, kIgnoreFacesBelowRobot, "Vision.FaceWorld", true);

  // Ignore new faces detected while rotating too fast
  CONSOLE_VAR(f32, kHeadTurnSpeedThreshFace_degs,  "WasRotatingTooFast.Face.Head_deg/s",    10.f);
  CONSOLE_VAR(f32, kBodyTurnSpeedThreshFace_degs,  "WasRotatingTooFast.Face.Body_deg/s",    30.f);
  CONSOLE_VAR(u8,  kNumImuDataToLookBackFace,      "WasRotatingTooFast.Face.NumToLookBack", 5);

  CONSOLE_VAR(bool,  kRenderGazeDirectionPoints,      "Vision.GazeDirection", false);

  static const char * const kLoggingChannelName = "FaceRecognizer";

  static const Point3f kHumanHeadSize{148.f, 225.f, 195.f};
  static const Point3f kGazeGroundPointSize{100.f, 100.f, 100.f};

  static const std::string kWebVizObservedObjectsName = "observedobjects";
  static const std::string kWebVizNavMapName = "navmap";


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  FaceWorld::FaceEntry::FaceEntry(const Vision::TrackedFace& faceIn)
  : face(faceIn)
  , vizHandle(VizManager::INVALID_HANDLE)
  {

  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  inline bool FaceWorld::FaceEntry::HasStableID() const
  {
    // Only true for non-tracking faces which are named or have been seen enough
    // times from the front
    DEV_ASSERT(!IsNamed() || face.GetID() > 0, "FaceWorld.FaceEntry.HasStableID.NamedFaceWithNonPositiveID");
    return face.GetID() > 0 && (IsNamed() || numTimesObservedFacingCamera >= kNumTimesToSeeFrontalToBeStable);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  FaceWorld::FaceWorld()
  : UnreliableComponent<BCComponentID>(this, BCComponentID::FaceWorld)
  , IDependencyManagedComponent<RobotComponentID>(this, RobotComponentID::FaceWorld)
  {

  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps)
  {
    _robot = robot;
    if(robot->HasExternalInterface()) {
      SetupEventHandlers(*robot->GetExternalInterface());
    }
  }


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SetupEventHandlers(IExternalInterface& externalInterface)
  {
    //    using namespace ExternalInterface;
    //    auto helper = MakeAnkiEventUtil(externalInterface, *this, _eventHandles);
    //    helper.SubscribeGameToEngine<MessageGameToEngineTag::ClearAllObjects>();
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::EraseFaceViz(FaceEntry& faceEntry)
  {
    if(faceEntry.vizHandle != VizManager::INVALID_HANDLE)
    {
      _robot->GetContext()->GetVizManager()->EraseVizObject(faceEntry.vizHandle);
      faceEntry.vizHandle = VizManager::INVALID_HANDLE;
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::RemoveFace(FaceEntryIter& faceEntryIter, bool broadcast)
  {
    if(broadcast)
    {
      using namespace ExternalInterface;

      RobotDeletedFace msg(faceEntryIter->first);

      if( ANKI_DEV_CHEATS ) {
        SendObjectUpdateToWebViz( msg );
      }

      _robot->Broadcast(MessageEngineToGame(std::move(msg)));
    }

    EraseFaceViz(faceEntryIter->second);

    faceEntryIter = _faceEntries.erase(faceEntryIter);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::ClearAllFaces()
  {
    for(auto faceEntryIter = _faceEntries.begin(); faceEntryIter != _faceEntries.end(); /* inc in loop */)
    {
      // NOTE: RemoveFace increments the iterator for us
      RemoveFace(faceEntryIter);
    }

    _lastObservedFaceTimeStamp = 0;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::RemoveFaceByID(Vision::FaceID_t faceID)
  {
    auto faceEntryIter = _faceEntries.find(faceID);

    if(faceEntryIter != _faceEntries.end())
    {
      PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.RemoveFaceByID",
                    "Removing face %d", faceID);

      RemoveFace(faceEntryIter);
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Result FaceWorld::ChangeFaceID(const Vision::UpdatedFaceID& update)
  {
    const Vision::FaceID_t oldID   = update.oldID;
    const Vision::FaceID_t newID   = update.newID;
    const std::string&     newName = update.newName;

    auto faceEntryIter = _faceEntries.find(oldID);
    if(faceEntryIter != _faceEntries.end())
    {
      Vision::TrackedFace& face = faceEntryIter->second.face;

      PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.ChangeFaceID.Success",
                    "Updating old face %d (%s) to new ID %d (%s)",
                    oldID, face.HasName() ? Util::HidePersonallyIdentifiableInfo(face.GetName().c_str()) : "<NoName>",
                    newID, newName.empty() ? "<NoName>" : Util::HidePersonallyIdentifiableInfo(newName.c_str()));

      const bool existingFaceHasDifferentName = face.HasName() && (newName != face.GetName());
      if(existingFaceHasDifferentName)
      {
        PRINT_NAMED_WARNING("FaceWorld.ChangeFaceID.ChangingName",
                            "OldID:%d OldName:%s, NewID:%d NewName:%s",
                            oldID, Util::HidePersonallyIdentifiableInfo(face.GetName().c_str()),
                            newID, Util::HidePersonallyIdentifiableInfo(newName.c_str()));
      }

      face.SetID(newID);
      face.SetName(newName);

      // TODO: Is there a more efficient move operation I could do here?
      auto result = _faceEntries.insert({newID, face});
      RemoveFace(faceEntryIter, false); // NOTE: don't broadcast the deletion

      // Re-draw the face and update the viz handle
      DrawFace(result.first->second);

      // Log ID changes to DAS when they are not tracking IDs and the new face is
      // either named or a "stable" session-only face.
      // Store old ID in DDATA and new ID in s_val.
      const FaceEntry& newFaceEntry = result.first->second;
      if(oldID > 0 && newID > 0 && newFaceEntry.HasStableID())
      {
        DASMSG(robot.vision.update_face_id,
               "robot.vision.update_face_id",
               "Face ID updated");
        DASMSG_SET(i1, oldID, "Old ID");
        DASMSG_SET(i2, newID, "New ID");
        DASMSG_SEND();
      }

    } else if(oldID > 0){
      PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.ChangeFaceID.UnknownOldID",
                    "ID %d does not exist, cannot update to %d",
                    oldID, newID);
    } else {
      // Probably no match for old ID because it was a tracked ID and wasn't
      // even added to face world before being recognized and being assigned this
      // new recognized ID
    }

    // Always notify game: let it decide whether or not it cares or knows about oldID
    using namespace ExternalInterface;
    _robot->Broadcast(MessageEngineToGame(RobotChangedObservedFaceID(oldID, newID)));

    return RESULT_OK;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Result FaceWorld::AddOrUpdateFace(const Vision::TrackedFace& face)
  {

    // Head pose is stored w.r.t. historical world origin, but needs its parent
    // set up to be the robot's world origin here, using the origin ID from the
    // time the face was seen
    DEV_ASSERT(!face.GetHeadPose().HasParent(), "FaceWorld.AddOrUpdateFace.HeadPoseHasParent");
    RobotTimeStamp_t t=0;
    HistRobotState* histStatePtr = nullptr;
    HistStateKey histStateKey;
    const Result histStateResult = _robot->GetStateHistory()->ComputeAndInsertStateAt(face.GetTimeStamp(), t,
                                                                                     &histStatePtr, &histStateKey,
                                                                                     true);

    if(RESULT_OK != histStateResult || histStatePtr == nullptr)
    {
      PRINT_NAMED_WARNING("FaceWorld.AddOrUpdateFace.GetComputedStateAtFailed", "face timestamp=%u", face.GetTimeStamp());
      return histStateResult;
    }

    const auto& origin = _robot->GetPoseOriginList().GetOriginByID(histStatePtr->GetPose().GetRootID());
    Pose3d headPoseWrtWorldOrigin(face.GetHeadPose());
    headPoseWrtWorldOrigin.SetParent(origin);

    Pose3d eyePoseWrtWorldOrigin(face.GetEyePose());
    eyePoseWrtWorldOrigin.SetParent(origin);

    const bool robotOnTreads = _robot->GetOffTreadsState() == OffTreadsState::OnTreads;
    const bool headBelowRobot = headPoseWrtWorldOrigin.GetTranslation().z() < 0.f;
    if(kIgnoreFacesBelowRobot && robotOnTreads && headBelowRobot)
    {
      // Don't report faces that are below the origin (which we are assuming is on the ground plane)
      PRINT_CH_DEBUG(kLoggingChannelName, "FaceWorld.AddOrUpdateFace.IgnoringFaceBelowRobot",
                     "z=%f", headPoseWrtWorldOrigin.GetTranslation().z());
      return RESULT_OK;
    }

    /*
    PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.AddOrUpdateFace",
                     "Updating with face at (x,y,w,h)=(%.1f,%.1f,%.1f,%.1f), "
                     "at t=%d Pose: roll=%.1f, pitch=%.1f yaw=%.1f, T=(%.1f,%.1f,%.1f).",
                     face.GetRect().GetX(), face.GetRect().GetY(),
                     face.GetRect().GetWidth(), face.GetRect().GetHeight(),
                     face.GetTimeStamp(),
                     face.GetHeadRoll().getDegrees(),
                     face.GetHeadPitch().getDegrees(),
                     face.GetHeadYaw().getDegrees(),
                     face.GetHeadPose().GetTranslation().x(),
                     face.GetHeadPose().GetTranslation().y(),
                     face.GetHeadPose().GetTranslation().z());
    */

    FaceEntry* faceEntry = nullptr;
    TimeStamp_t timeSinceLastSeen_ms = 0;

    if(false == Vision::FaceTracker::IsRecognitionSupported())
    {
      // Can't get an ID from face recognition, so use pose instead
      bool foundMatch = false;

      // Look through all faces and compare pose and image rectangles:
      f32 IOU_threshold = 0.5f;
      for(auto faceEntryIter = _faceEntries.begin(); faceEntryIter != _faceEntries.end(); ++faceEntryIter)
      {

        // Note we're using really loose thresholds for checking pose sameness
        // since our ability to accurately localize face's 3D pose is limited.
        Radians angleDiff;

        const auto & entryRect = faceEntryIter->second.face.GetRect();
        const f32 currentIOU = face.GetRect().ComputeOverlapScore(entryRect);
        if(currentIOU > IOU_threshold)
        {
          if(foundMatch) {
            // If we had already found a match, delete the last one, because this
            // new face matches multiple existing faces
            assert(nullptr != faceEntry);
            RemoveFaceByID(faceEntry->face.GetID());
          }

          IOU_threshold = currentIOU;
          foundMatch = true;
          faceEntry = &faceEntryIter->second;
        }
        else
        {
          auto posDiffVec = (faceEntryIter->second.face.GetHeadPose().GetTranslation() -
                             headPoseWrtWorldOrigin.GetTranslation());
          float posDiffSq = posDiffVec.LengthSq();
          if(posDiffSq <= kHeadCenterPointThreshold_mm * kHeadCenterPointThreshold_mm) {
            if(foundMatch) {
              // If we had already found a match, delete the last one, because this
              // new face matches multiple existing faces
              assert(nullptr != faceEntry);
              RemoveFaceByID(faceEntry->face.GetID());
            }

            faceEntry = &faceEntryIter->second;
            foundMatch = true;
          }
        }
      } // for each face entry

      if(foundMatch) {
        const Vision::FaceID_t matchedID = faceEntry->face.GetID();

        // Verbose! Useful for debugging
        //PRINT_CH_DEBUG(kLoggingChannelName, "FaceWorld.UpdateFace.UpdatingFaceEntryByPose",
        //               "Updating face with ID=%lld from t=%d to %d, observed %d times",
        //               matchedID, faceEntry->face.GetTimeStamp(), face.GetTimeStamp(),
        //               face.GetNumTimesObserved());

        faceEntry->face = face;
        faceEntry->face.SetID(matchedID);
      }

      // Didn't find a match based on pose, so add a new face with a new ID:
      else {
        PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.UpdateFace.NewFace",
                      "Added new face with ID=%d at t=%d.", _idCtr, face.GetTimeStamp());

        auto insertResult = _faceEntries.insert({_idCtr, face});
        if(insertResult.second == false) {
          PRINT_NAMED_ERROR("FaceWorld.UpdateFace.ExistingID",
                            "Did not find a match by pose, but ID %d already in use.",
                            _idCtr);
          return RESULT_FAIL;
        }
        faceEntry = &insertResult.first->second;
        faceEntry->face.SetID(_idCtr); // Use our own ID here for the new face

        ++_idCtr;
      }

    }
    else
    {
      // Use face recognition to get ID
      auto existingIter = _faceEntries.find(face.GetID());

      const bool isNewFace = (existingIter == _faceEntries.end());
      if(isNewFace)
      {
        // Make sure we aren't rotating too fast to add a new face (this helps safeguard against false positives)
        const bool fastRotationAllowed = (_robot->GetVisionComponent().IsVisionWhileRotatingFastEnabled() &&
                                          (Util::IsFltGT(kBodyTurnSpeedThreshFace_degs, 0.f) ||
                                           Util::IsFltGT(kHeadTurnSpeedThreshFace_degs, 0.f)));

        auto const& imuHistory = _robot->GetImuComponent().GetImuHistory();
        const bool wasRotatingTooFast = (!fastRotationAllowed &&
                                         imuHistory.WasRotatingTooFast(face.GetTimeStamp(),
                                                                       DEG_TO_RAD(kBodyTurnSpeedThreshFace_degs),
                                                                       DEG_TO_RAD(kHeadTurnSpeedThreshFace_degs),
                                                                       (face.IsBeingTracked() ? kNumImuDataToLookBackFace : 0)));

        if(wasRotatingTooFast)
        {
          return RESULT_OK;
        }
        else
        {
          PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.UpdateFace.NewFace",
                        "Added new face with ID=%d at t=%d.",
                        face.GetID(), face.GetTimeStamp());

          auto result = _faceEntries.emplace(face.GetID(), face);
          faceEntry = &result.first->second;
        }
      }
      else
      {
        // Update the existing face:
        faceEntry = &existingIter->second;

        if(face.GetTimeStamp() > faceEntry->face.GetTimeStamp())
        {
          timeSinceLastSeen_ms = face.GetTimeStamp() - faceEntry->face.GetTimeStamp();
        }
        else
        {
          PRINT_NAMED_WARNING("FaceWorld.UpdateFace.BadTimeStamp",
                              "Face observed before previous observation (%u <= %u)",
                              face.GetTimeStamp(), faceEntry->face.GetTimeStamp());
        }

        faceEntry->face = face;
      }

      // update the observation time if this is a named face. Note that this is using current wall time, which
      // is slightly different from the actual image timestamp when the face was observed, but should be close
      // enough. Only store if time is accurate.
      if( face.HasName() ) {
        WallTime::TimePoint_t wallTime;
        if( WallTime::getInstance()->GetTime( wallTime ) ) {
          auto it = _wallTimesObserved.find(face.GetID());
          if( it != _wallTimesObserved.end() ) {
            // update existing entry
            it->second.push_back(wallTime);
            while(it->second.size() > 2) {
              it->second.pop_front();
            }

            // if the new sighting is in a different day than the last one, we need to update robot stats
            const auto lastSeen = it->second.front();
            if( !WallTime::AreTimePointsInSameDay(lastSeen, wallTime) ) {
              PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.UpdateFace.FaceSeenOnNewDay",
                            "face %d seen on new day",
                            face.GetID());
              _robot->GetComponent<RobotStatsTracker>().IncrementNamedFacesPerDay();
            }
          }
          else {
            // new entry
            _wallTimesObserved.emplace( face.GetID(), ObservationTimeHistory{{wallTime}} );

            PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.UpdateFace.NamedFaceFirstDaySeen",
                          "face %d has been seen for the first time",
                          face.GetID());

            _robot->GetComponent<RobotStatsTracker>().IncrementNamedFacesPerDay();
          }
        }
      }

    } // if(false == Vision::FaceTracker::IsRecognitionSupported()

    // By now, we should have either created a new face or be pointing at an
    // existing one!
    assert(faceEntry != nullptr);

    faceEntry->face.SetHeadPose(headPoseWrtWorldOrigin);
    faceEntry->face.SetEyePose(eyePoseWrtWorldOrigin);
    faceEntry->numTimesObserved++;

    const auto* featureGate = _robot->GetContext()->GetFeatureGate();
    if (featureGate->IsFeatureEnabled(FeatureType::GazeDirection)) {
      AddOrUpdateGazeDirection(faceEntry->face);
    }

    // Keep up with how many times non-tracking-only faces have been seen facing
    // facing the camera (and thus potentially recognizable)
    if(faceEntry->face.IsFacingCamera())
    {
      faceEntry->numTimesObservedFacingCamera++;
    }

    // Log any DAS events based on this face observation
    const bool isNamed = faceEntry->IsNamed();
    if(faceEntry->numTimesObserved == 1 && isNamed)
    {
      DASMSG(robot.vision.face_recognition.immediate_recognition,
             "robot.vision.face_recognition.immediate_recognition",
             "We immediately recognized a new face with a name");
      DASMSG_SET(i1, faceEntry->face.GetID(), "Face ID");
      DASMSG_SEND();
    }
    else if(!isNamed && faceEntry->face.GetID() > 0 && faceEntry->numTimesObservedFacingCamera == kNumTimesToSeeFrontalToBeStable)
    {
      DASMSG(robot.vision.face_recognition.persistent_session_only,
             "robot.vision.face_recognition.persistent_session_only",
             "We have seen a session-only face for awhile and not recognized it as someone else (so this is a stable "
             "session-only face) NOTE: we do this just once, when we cross the num times observed threshold");
      DASMSG_SET(i1, faceEntry->face.GetID(), "Face ID");
      DASMSG_SEND();

      // HACK: increment the counter again so we don't send this multiple times if not seeing frontal anymore
      faceEntry->numTimesObservedFacingCamera++;
    }
    else if(timeSinceLastSeen_ms > kTimeUnobservedBeforeReLoggingToDAS_ms && faceEntry->HasStableID())
    {
      DASMSG(robot.vision.face_recognition.persistent_session_only,
             "robot.vision.face_recognition.persistent_session_only",
             "We are re-seeing a face after not having seen it for a bit (and recognizing it as an existing named "
             "person or stable session-only ID)");
      DASMSG_SET(i1, faceEntry->face.GetID(), "Face ID");
      DASMSG_SET(i2, isNamed, "1 if this is a named face, 0 or null otherwise");
      DASMSG_SEND();
    }

    // Wait to report this face until we've seen it enough times to be convinced it's
    // not a false positive (random detection), or if it has been recognized already.
    if(faceEntry->numTimesObserved >= MinTimesToSeeFace || !faceEntry->face.GetName().empty())
    {
      // Update the last observed face pose.
      // If more than one was observed in the same timestamp then take the closest one.
      const bool newerThanLastObservation = (faceEntry->face.GetTimeStamp() > _lastObservedFaceTimeStamp);
      bool closerThanLastObservation = false; // note: only computed if there were multiple observations in one tick
      if (!newerThanLastObservation) {
        // More than one face was observed in the same timestamp, so see if this one is closest
        float lastObservedFaceDist_mm = 0.f;
        float thisFaceDist_mm = 0.f;
        if (!ComputeDistanceBetween(_robot->GetPose(), _lastObservedFacePose, lastObservedFaceDist_mm) ||
            !ComputeDistanceBetween(_robot->GetPose(), faceEntry->face.GetHeadPose(), thisFaceDist_mm)) {
          LOG_ERROR("FaceWorld.AddOrUpdateFace.ComputeDistanceFailure",
                    "Failed computing distance between robot and faces");
          return RESULT_FAIL;
        }
        closerThanLastObservation = (thisFaceDist_mm < lastObservedFaceDist_mm);
      }

      if (newerThanLastObservation || closerThanLastObservation) {
        _lastObservedFacePose = faceEntry->face.GetHeadPose();
        _lastObservedFaceTimeStamp = faceEntry->face.GetTimeStamp();

        // Draw 3D face for the last observed pose
        faceEntry->vizHandle = _robot->GetContext()->GetVizManager()->DrawHumanHead(0,
                                                                                   kHumanHeadSize,
                                                                                   faceEntry->face.GetHeadPose(),
                                                                                   ::Anki::NamedColors::DARKGRAY);
      }

      // Draw face in 3D and in camera
      DrawFace(*faceEntry);


      /*
      PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.AddOrUpdateFace",
                       "Known face at (x,y,w,h)=(%.1f,%.1f,%.1f,%.1f), "
                       "at t=%d Pose: roll=%.1f, pitch=%.1f yaw=%.1f, T=(%.1f,%.1f,%.1f).",
                       faceEntry->face.GetRect().GetX(), faceEntry->face.GetRect().GetY(),
                       faceEntry->face.GetRect().GetWidth(), faceEntry->face.GetRect().GetHeight(),
                       faceEntry->face.GetTimeStamp(),
                       faceEntry->face.GetHeadRoll().getDegrees(),
                       faceEntry->face.GetHeadPitch().getDegrees(),
                       faceEntry->face.GetHeadYaw().getDegrees(),
                       faceEntry->face.GetHeadPose().GetTranslation().x(),
                       faceEntry->face.GetHeadPose().GetTranslation().y(),
                       faceEntry->face.GetHeadPose().GetTranslation().z());
      */

      // Send out an event about this face being observed
      using namespace ExternalInterface;

      std::vector<CladPoint2d> leftEye;
      std::vector<CladPoint2d> rightEye;
      std::vector<CladPoint2d> nose;
      std::vector<CladPoint2d> mouth;

      const std::vector<std::pair<std::vector<CladPoint2d>*,Vision::TrackedFace::FeatureName>> features{
        {&leftEye,   Vision::TrackedFace::FeatureName::LeftEye},
        {&rightEye,  Vision::TrackedFace::FeatureName::RightEye},
        {&nose,      Vision::TrackedFace::FeatureName::Nose},
        {&mouth,     Vision::TrackedFace::FeatureName::UpperLip},
      };

      for(auto const& feature : features)
      {
        for(auto const& pt : face.GetFeature(feature.second))
        {
          feature.first->emplace_back(pt.x(), pt.y());
        }
      }

      RobotObservedFace msg(faceEntry->face.GetID(),
                            faceEntry->face.GetTimeStamp(),
                            faceEntry->face.GetHeadPose().ToPoseStruct3d(_robot->GetPoseOriginList()),
                            CladRect(faceEntry->face.GetRect().GetX(),
                                     faceEntry->face.GetRect().GetY(),
                                     faceEntry->face.GetRect().GetWidth(),
                                     faceEntry->face.GetRect().GetHeight()),
                            faceEntry->face.GetName(),
                            faceEntry->face.GetMaxExpression(),
                            faceEntry->face.GetSmileAmount(),
                            faceEntry->face.GetGaze(),
                            faceEntry->face.GetBlinkAmount(),
                            faceEntry->face.GetExpressionValues(),
                            leftEye, rightEye, nose, mouth);

      if( ANKI_DEV_CHEATS ) {
        SendObjectUpdateToWebViz( msg );
      }

      _robot->Broadcast(MessageEngineToGame(std::move(msg)));



      /*
      const Vision::Image& faceThumbnail = faceEntry->face.GetThumbnail();
      if(!faceThumbnail.IsEmpty()) {
        // TODO: Expose quality parameter
        const std::vector<int> compressionParams = {
          CV_IMWRITE_JPEG_QUALITY, 75
        };

        ExternalInterface::FaceEnrollmentImage msg;
        msg.faceID = faceEntry->face.GetID();
        cv::imencode(".jpg",  faceThumbnail.get_CvMat_(), msg.jpgImage, compressionParams);

        _robot->Broadcast(ExternalInterface::MessageEngineToGame(std::move(msg)));
      }
      */
    }

    return RESULT_OK;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Result FaceWorld::AddOrUpdateGazeDirection(Vision::TrackedFace& face)
  {
    // Only update the gaze direction for the given face if
    // we have succesfully found parts for this face which are
    // needed to determine the rotation of the head pose. The
    // HasEyes method is proxy for this.
    if (face.HasEyes())
    {
      auto& entry = _gazeDirection[face.GetID()];
      entry.Update(face);

      if (entry.GetExpired(face.GetTimeStamp()))
      {
        _gazeDirection.erase(face.GetID());
      }
      else
      {
        const bool isGazeStable = entry.IsStable();
        face.SetGazeDirectionStable(isGazeStable);
        if (isGazeStable)
        {
          auto faceDirectionAverage = entry.GetGazeDirectionAverage();
          Pose3d gazeDirectionPose(0.f, Z_AXIS_3D(), faceDirectionAverage, _robot->GetWorldOrigin());
          face.SetGazeDirectionPose(gazeDirectionPose);
        }
      }
    }
    return RESULT_OK;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Result FaceWorld::Update(const std::list<Vision::TrackedFace>& observedFaces)
  {
    ANKI_CPU_PROFILE("FaceWorld::Update");

    for(auto & obsFace : observedFaces)
    {
      Result result = AddOrUpdateFace(obsFace);
      if(RESULT_OK != result)
      {
        PRINT_NAMED_WARNING("FaceWorld.Update.AddOrUpdateFaceFailed",
                            "ObservedFace ID=%d", obsFace.GetID());
      }
    }

    const RobotTimeStamp_t lastProcImageTime = _robot->GetVisionComponent().GetLastProcessedImageTimeStamp();

    // Delete any unnamed faces we haven't seen in awhile
    for(auto faceIter = _faceEntries.begin(); faceIter != _faceEntries.end(); )
    {
      Vision::TrackedFace& face = faceIter->second.face;

      if(face.GetName().empty() && (lastProcImageTime > kDeletionTimeout_ms + face.GetTimeStamp()))
      {
        PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.Update.DeletingOldFace",
                      "Removing unnamed face %d at t=%d, because it hasn't been seen since t=%d.",
                      faceIter->first, (TimeStamp_t)lastProcImageTime, face.GetTimeStamp());

        if(faceIter->second.HasStableID())
        {
          DASMSG(robot.vision.remove_unobserved_session_only_face,
                 "robot.vision.remove_unobserved_session_only_face",
                 "Removing a 'stable' face because we have not seen it in awhile");
          DASMSG_SET(i1, faceIter->first, "Face ID");
          DASMSG_SET(i2, face.GetTimeStamp(), "Face time stamp");
          DASMSG_SEND();
        }

        RemoveFace(faceIter); // Increments faceIter!
      }
      else
      {
        ++faceIter;
      }
    }

    // Update anim focus (for keep face alive) with eye contact
    static const std::string kKeepFaceAliveEyeContactName = "EyeContact";
    const bool currentEyeContact = IsMakingEyeContact(0);
    if (_previousEyeContact != currentEyeContact)
    {
      if (currentEyeContact)
      {
        _robot->GetAnimationComponent().AddKeepFaceAliveFocus(kKeepFaceAliveEyeContactName);
      }
      else
      {
        _robot->GetAnimationComponent().RemoveKeepFaceAliveFocus(kKeepFaceAliveEyeContactName);
      }
      _previousEyeContact = currentEyeContact;
    }

    return RESULT_OK;
  } // Update()

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::ShouldReturnFace(const FaceEntry& faceEntry, RobotTimeStamp_t seenSinceTime_ms, bool includeRecognizableOnly,
                                   float relativeRobotAngleTolerance_rad, const Radians& angleRelativeRobot_rad) const
  {
    if (faceEntry.face.GetTimeStamp() >= seenSinceTime_ms)
    {
      if( !includeRecognizableOnly || faceEntry.face.GetID() > 0 )
      {
        bool isFaceValid = _robot->IsPoseInWorldOrigin(faceEntry.face.GetHeadPose());
        if(isFaceValid &&
           (relativeRobotAngleTolerance_rad != kDontCheckRelativeAngle)){
          const Pose3d& robotPose = _robot->GetPose();
          Pose3d relPose;
          if( faceEntry.face.GetHeadPose().GetWithRespectTo( robotPose, relPose ) ) {
            Radians angle{ atan2f(relPose.GetTranslation().y(), relPose.GetTranslation().x()) };
            if(!angle.IsNear(angleRelativeRobot_rad, relativeRobotAngleTolerance_rad)){
              isFaceValid = false;
            }
          }
        }
        return isFaceValid;
      }
    }

    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::OnRobotDelocalized(PoseOriginID_t worldOriginID)
  {
    // Erase all face visualizations
    for(auto & pair : _faceEntries)
    {
      EraseFaceViz(pair.second);
    }

    // Note that we deliberately do not clear the last observed face pose! Sometimes
    // we use it (despite it's incorrect origin) as a best guess for where to look
    // to find a face.
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  int FaceWorld::UpdateFaceOrigins(PoseOriginID_t oldOriginID, PoseOriginID_t newOriginID)
  {
    DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(oldOriginID),
                   "FaceWorld.UpdateFaceOrigins.InvalidOldOrigin", "ID:%d", oldOriginID);
    DEV_ASSERT_MSG(_robot->GetPoseOriginList().ContainsOriginID(newOriginID),
                   "FaceWorld.UpdateFaceOrigins.InvalidNewOrigin", "ID:%d", newOriginID);

    const Pose3d& oldOrigin = _robot->GetPoseOriginList().GetOriginByID(oldOriginID);
    const Pose3d& newOrigin = _robot->GetPoseOriginList().GetOriginByID(newOriginID);

    s32 updateCount = 0;

    // Update all regular face entries
    for(auto & pair : _faceEntries)
    {
      Vision::TrackedFace& face = pair.second.face;

      // If this entry's face is directly w.r.t. the old origin, flatten it to the new origin
      if(oldOrigin.IsParentOf(face.GetHeadPose()))
      {
        Pose3d poseWrtNewOrigin;
        const bool success = face.GetHeadPose().GetWithRespectTo(newOrigin, poseWrtNewOrigin);
        if(success)
        {
          PRINT_CH_DEBUG(kLoggingChannelName, "FaceWorld.UpdateFaceOrigins.FlatteningFace",
                         "Flattened FaceID:%d w.r.t. %s", pair.first, newOrigin.GetName().c_str());

          face.SetHeadPose(poseWrtNewOrigin);
          ++updateCount;
        }
        else
        {
          PRINT_NAMED_WARNING("FaceWorld.UpdateFaceOrigins.HeadPoseUpdateFailed",
                              "Head pose of FaceID:%d is w.r.t. to old origin %s but "
                              "failed to flatten to be w.r.t new origin %s",
                              pair.first, oldOrigin.GetName().c_str(), newOrigin.GetName().c_str());
        }
      }

      if(newOrigin.IsParentOf(face.GetHeadPose()))
      {
        // Draw everything in the new origin (but don't draw in the image since we're not actually observing it)
        const bool kDrawInImage = false;
        DrawFace(pair.second, kDrawInImage);
      }
    }

    // Also update the lastObservedFace pose
    if(_lastObservedFaceTimeStamp > 0)
    {
      if(oldOrigin.IsParentOf(_lastObservedFacePose))
      {
        const bool success = _lastObservedFacePose.GetWithRespectTo(newOrigin, _lastObservedFacePose);
        if(!success)
        {
          PRINT_NAMED_WARNING("FaceWorld.UpdateFaceOrigins.UpdateLastObservedPoseFailed",
                              "Failed to flatten last observed pose from %s to %s",
                              oldOrigin.GetName().c_str(), newOrigin.GetName().c_str());
        }
      }
    }

    return updateCount;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  SmartFaceID FaceWorld::GetSmartFaceID(Vision::FaceID_t faceID) const
  {
    return SmartFaceID{*_robot, faceID};
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::UpdateSmartFaceToID(const Vision::FaceID_t faceID, SmartFaceID& smartFaceID)
  {
    smartFaceID.Reset(*_robot, faceID);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const Vision::TrackedFace* FaceWorld::GetFace(Vision::FaceID_t faceID) const
  {
    const bool kIncludeRecognizableOnly = false; // FaceID directly specified, search everything
    auto faceIter = _faceEntries.find(faceID);
    if(faceIter != _faceEntries.end() && ShouldReturnFace(faceIter->second, 0, kIncludeRecognizableOnly)) {
      return &faceIter->second.face;
    } else {
      return nullptr;
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const Vision::TrackedFace* FaceWorld::GetFace(const SmartFaceID& faceID) const
  {
    return GetFace(faceID.GetID());
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  std::set<Vision::FaceID_t> FaceWorld::GetFaceIDs(RobotTimeStamp_t seenSinceTime_ms,
                                                   bool includeRecognizableOnly,
                                                   float relativeRobotAngleTolerance_rad,
                                                   const Radians& angleRelativeRobot_rad) const
  {
    std::set<Vision::FaceID_t> faceIDs;
    auto faceEntryIter = _faceEntries.begin();
    while(faceEntryIter != _faceEntries.end())
    {
      DEV_ASSERT_MSG(faceEntryIter->first == faceEntryIter->second.face.GetID(),
                     "FaceWorld.GetFaceIDs.MismatchedIDs",
                     "Entry keyed with ID:%d but face has ID:%d",
                     faceEntryIter->first, faceEntryIter->second.face.GetID());

      if(ShouldReturnFace(faceEntryIter->second,
                          seenSinceTime_ms,
                          includeRecognizableOnly,
                          relativeRobotAngleTolerance_rad,
                          angleRelativeRobot_rad))
      {
        faceIDs.insert(faceEntryIter->first);
      }

      ++faceEntryIter;
    }

    return faceIDs;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  std::vector<SmartFaceID> FaceWorld::GetSmartFaceIDs(RobotTimeStamp_t seenSinceTime_ms,
                                                      bool includeRecognizableOnly,
                                                      float relativeRobotAngleTolerance_rad,
                                                      const Radians& angleRelativeRobot_rad) const
  {
    std::set< Vision::FaceID_t > faces = GetFaceIDs(seenSinceTime_ms,
                                                    includeRecognizableOnly,
                                                    relativeRobotAngleTolerance_rad,
                                                    angleRelativeRobot_rad);

    std::vector<SmartFaceID> smartFaces;
    for(auto& entry : faces){
      smartFaces.emplace_back(GetSmartFaceID(entry));
    }
    return smartFaces;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::HasAnyFaces(RobotTimeStamp_t seenSinceTime_ms, bool includeRecognizableOnly) const
  {
    auto faceEntryIter = _faceEntries.begin();
    while(faceEntryIter != _faceEntries.end())
    {
      if (ShouldReturnFace(faceEntryIter->second, seenSinceTime_ms, includeRecognizableOnly) )
      {
        // As soon as we find any face that matches the criteria, we're done
        return true;
      }

      ++faceEntryIter;
    }

    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  RobotTimeStamp_t FaceWorld::GetLastObservedFace(Pose3d& poseWrtRobotOrigin, bool inRobotOriginOnly) const
  {
    RobotTimeStamp_t returnTime = 0;

    if(_lastObservedFaceTimeStamp > 0)
    {
      const bool lastPoseIsWrtRobotOrigin = _robot->IsPoseInWorldOrigin(_lastObservedFacePose);

      // We have a last observed pose at all...
      if(lastPoseIsWrtRobotOrigin)
      {
        // The pose is in the current origin, so just use it.
        poseWrtRobotOrigin = _lastObservedFacePose;
        returnTime = _lastObservedFaceTimeStamp;
      }
      else if(!inRobotOriginOnly)
      {
        // Pose is not w.r.t. robot origin, but we're allowed to use it anyway.
        // Create a fake pose as if it were w.r.t. current robot origin.
        poseWrtRobotOrigin = _lastObservedFacePose.GetWithRespectToRoot();
        poseWrtRobotOrigin.SetParent(_robot->GetWorldOrigin()); // Totally not true, but we're faking it!
        returnTime = _lastObservedFaceTimeStamp;
      }
    }

    return returnTime;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::HasTurnedTowardsFace(Vision::FaceID_t faceID) const
  {
    const auto& it = _faceEntries.find(faceID);
    if( it == _faceEntries.end() ) {
      // either this is a bad ID, or the face was deleted, so assume we haven't animated at it. Note that (as
      // of this comment writing...) named faces are not deleted
      return false;
    }

    return it->second.hasTurnedTowards;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::HasTurnedTowardsFace(const SmartFaceID& faceID) const
  {
    return HasTurnedTowardsFace(faceID.GetID());
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SetTurnedTowardsFace(Vision::FaceID_t faceID, bool val)
  {
    auto it = _faceEntries.find(faceID);
    if( it == _faceEntries.end() ) {
      PRINT_NAMED_WARNING("FaceWorld.SetTurnedTowardsFaceAndAnimation.InvalidFace",
                          "Claiming that we animated at face %d, but that face doesn't exist in FaceWorld",
                          faceID);
      return;
    }

    it->second.hasTurnedTowards = val;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SetTurnedTowardsFace(const SmartFaceID& faceID, bool val)
  {
    SetTurnedTowardsFace(faceID.GetID(), val);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::DrawFace(FaceEntry& faceEntry, bool drawInImage) const
  {
    if(!ANKI_DEV_CHEATS) {
      // Don't draw anything in shipping builds
      return;
    }

    const Vision::TrackedFace& trackedFace = faceEntry.face;

    ColorRGBA drawFaceColor = ColorRGBA::CreateFromColorIndex((u32)trackedFace.GetID());

    const s32 vizID = (s32)trackedFace.GetID() + (trackedFace.GetID() >= 0 ? 1 : 0);
    faceEntry.vizHandle = _robot->GetContext()->GetVizManager()->DrawHumanHead(vizID,
                                                                              kHumanHeadSize,
                                                                              trackedFace.GetHeadPose(),
                                                                              drawFaceColor);

    const auto* featureGate = _robot->GetContext()->GetFeatureGate();
    if (kRenderGazeDirectionPoints && featureGate->IsFeatureEnabled(FeatureType::GazeDirection)) {
      const auto& entry = _gazeDirection.find(trackedFace.GetID());
      if (entry != _gazeDirection.end()) {
        const auto& gazeDirection = entry->second;
        const s32 startingObjectId = 2345;

        const auto currentGazeDirection = gazeDirection.GetCurrentGazeDirection();
        Pose3d currentGazePose(Transform3d(Rotation3d(0.f, Z_AXIS_3D()), currentGazeDirection));
        faceEntry.vizHandle = _robot->GetContext()->GetVizManager()->DrawCuboid(startingObjectId,
                                                                                kGazeGroundPointSize,
                                                                                currentGazePose,
                                                                                ::Anki::NamedColors::ORANGE);

        if (gazeDirection.IsStable()) {
          const auto averageGazeDirection = gazeDirection.GetGazeDirectionAverage();
          Pose3d averageGazePose(Transform3d(Rotation3d(0.f, Z_AXIS_3D()), averageGazeDirection));
          faceEntry.vizHandle = _robot->GetContext()->GetVizManager()->DrawCuboid(startingObjectId + 1,
                                                                                  kGazeGroundPointSize,
                                                                                  averageGazePose,
                                                                                  ::Anki::NamedColors::GREEN);
        }
      }
    }

    if(drawInImage)
    {
      // Draw box around recognized face (with ID) now that we have the real ID set
      _robot->GetContext()->GetVizManager()->DrawCameraFace(trackedFace, drawFaceColor);
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::Enroll(Vision::FaceID_t faceID, bool forceNewID)
  {
    SetFaceEnrollmentComplete(false);

    // If starting session enrollment, then set the num enrollments to -1 to get "ongoing"
    // enrollment. Otherwise, use the max we can store.
    const bool sessionOnly = (Vision::UnknownFaceID == faceID);
    const s32 numEnrollmentsRequired = (sessionOnly ? -1 :
                                        (s32)Vision::FaceRecognitionConstants::MaxNumEnrollDataPerAlbumEntry);

    _robot->GetVisionComponent().SetFaceEnrollmentMode(faceID, numEnrollmentsRequired, forceNewID);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::Enroll(const SmartFaceID& faceID, bool forceNewID)
  {
    Enroll(faceID.GetID(), forceNewID);
  }

#if ANKI_DEV_CHEATS
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SaveAllRecognitionImages(const std::string& imagePathPrefix)
  {
    _robot->GetVisionComponent().SaveAllRecognitionImages(imagePathPrefix);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::DeleteAllRecognitionImages()
  {
    _robot->GetVisionComponent().DeleteAllRecognitionImages();
  }
#endif

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SendObjectUpdateToWebViz( const ExternalInterface::RobotDeletedFace& msg ) const
  {
    if( msg.faceID <= 0 ) {
      return; // ignore half-recognized or invalid faces
    }

    const auto* webService = _robot->GetContext()->GetWebService();
    if( webService != nullptr ) {
      Json::Value data;
      data["type"] = "RobotDeletedFace";
      data["faceID"] = msg.faceID;

      webService->SendToWebViz( kWebVizObservedObjectsName, data );
      webService->SendToWebViz( kWebVizNavMapName, data );
    }

  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::SendObjectUpdateToWebViz( const ExternalInterface::RobotObservedFace& msg ) const
  {
    if( msg.faceID <= 0 ) {
      return; // ignore half-recognized or invalid faces
    }

    const auto* webService = _robot->GetContext()->GetWebService();
    if( webService != nullptr ) {
      const bool isSubscribedObservedObjects = webService->IsWebVizClientSubscribed(kWebVizObservedObjectsName);
      const bool isSubscribedNavMap = webService->IsWebVizClientSubscribed(kWebVizNavMapName);

      // this is used by two modules
      if (isSubscribedObservedObjects || isSubscribedNavMap) {
        Json::Value data;
        data["faceID"] = msg.faceID;
        if( !msg.name.empty() ) {
          data["name"] = msg.name;
        }
        data["timestamp"] = msg.timestamp;
        data["originID"] = msg.pose.originID;

        if (isSubscribedObservedObjects) {
          data["type"] = "RobotObservedFace";
          webService->SendToWebViz( kWebVizObservedObjectsName, data );
        }

        if (isSubscribedNavMap) {
          data["type"] = "MemoryMapFace";
          auto& pose = data["pose"];
          Pose3d objPose( msg.pose, _robot->GetPoseOriginList() );
          pose["x"] = objPose.GetTranslation().x();
          pose["y"] = objPose.GetTranslation().y();
          pose["z"] = objPose.GetTranslation().z();
          pose["qW"] = objPose.GetRotation().GetQuaternion().w();
          pose["qX"] = objPose.GetRotation().GetQuaternion().x();
          pose["qY"] = objPose.GetRotation().GetQuaternion().y();
          pose["qZ"] = objPose.GetRotation().GetQuaternion().z();
          webService->SendToWebViz( kWebVizNavMapName, data );
        }
      }
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::IsMakingEyeContact(const u32 withinLast_ms) const
  {
    // Loop over all the faces and see if any of them are making eye contact
    const RobotTimeStamp_t lastImgTime = _robot->GetLastImageTimeStamp();
    const RobotTimeStamp_t recentTime = lastImgTime > withinLast_ms ?
                                        ( lastImgTime - withinLast_ms ) :
                                        0;
    // Loop over all the faces and see if any of them are making eye contact
    for (const auto& entry: _faceEntries)
    {
      if (ShouldReturnFace(entry.second, recentTime, false))
      {
        if (entry.second.face.IsMakingEyeContact())
        {
          return true;
        }
      }
    }
    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::GetGazeDirectionPose(const u32 withinLast_ms, Pose3d& gazeDirectionPose,
                                       SmartFaceID& faceID) const
  {
    const RobotTimeStamp_t lastImgTime = _robot->GetLastImageTimeStamp();
    const RobotTimeStamp_t recentTime = lastImgTime > withinLast_ms ?
                                        ( lastImgTime - withinLast_ms ) :
                                        0;

    for (const auto& entry: _faceEntries)
    {
      if (ShouldReturnFace(entry.second, recentTime, false))
      {
        if (entry.second.face.IsGazeDirectionStable())
        {
          gazeDirectionPose = entry.second.face.GetGazeDirectionPose();
          faceID.Reset(*_robot, entry.second.face.GetID());
          return true;
        }
      }
    }
    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::AnyStableGazeDirection(const u32 withinLast_ms) const
  {
    Pose3d gazeDirectionPose;
    SmartFaceID faceID;
    return GetGazeDirectionPose(withinLast_ms, gazeDirectionPose, faceID);
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::ClearGazeDirectionHistory(const SmartFaceID& faceID)
  {
    // Loop over all the faces and see if any of them are making eye contact
    for (const auto& entry: _faceEntries)
    {
      if (faceID.MatchesFaceID(entry.second.face.GetID()))
      {
        const auto& gazeDirectionEntry = _gazeDirection.find(entry.second.face.GetID());
        if (gazeDirectionEntry != _gazeDirection.end()) {
          gazeDirectionEntry->second.ClearHistory();
          return true;
        }
      }
    }
    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool FaceWorld::FaceInTurnAngle(const Radians& turnAngle, const SmartFaceID& smartFaceIDToIgnore,
                                  const Pose3d& robotPose, SmartFaceID& faceIDToTurnTowards) const
  {
    for (const auto& entry: _faceEntries)
    {
      const auto& headPose = entry.second.face.GetHeadPose();
      Pose3d headPoseWRTRobot;
      if (headPose.GetWithRespectTo(robotPose, headPoseWRTRobot))
      {
        const Radians& horizontalFOV = _robot->GetVisionComponent().GetCamera().GetCalibration()->ComputeHorizontalFOV();
        const Radians faceTurnAngle = TurnTowardsPoseAction::GetRelativeBodyAngleToLookAtPose(headPoseWRTRobot.GetTranslation());
        if (Util::InRange( turnAngle - faceTurnAngle, -horizontalFOV/2.f, horizontalFOV/2.f) &&
            !smartFaceIDToIgnore.MatchesFaceID(entry.second.face.GetID()))
        {
          faceIDToTurnTowards.Reset(*_robot, entry.second.face.GetID());
          return true;
        }
      }
    }
    return false;
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void FaceWorld::InitLoadedKnownFaces(const std::list<Vision::LoadedKnownFace>& loadedFaces)
  {
    for( const auto& loadedFace : loadedFaces ) {
      const auto epoch = WallTime::getInstance()->GetEpochTime();
      const auto sinceEpoch = std::chrono::seconds(loadedFace.lastSeenSecondsSinceEpoch);
      const auto wallTime = epoch + sinceEpoch;

      if( ANKI_VERIFY(!loadedFace.name.empty(),
                      "FaceWorld.InitLoadedKnownFaces.NoName",
                      "Face id %d loaded from disk but doesn't have name",
                      loadedFace.faceID) ) {
        _wallTimesObserved.emplace( loadedFace.faceID, ObservationTimeHistory{{wallTime}} );

        PRINT_CH_INFO(kLoggingChannelName, "FaceWorld.InitLoadedKnownFaces.InitFace",
                      "Loaded face %d, last observed at time (since epoch): %llu",
                      loadedFace.faceID,
                      wallTime.time_since_epoch().count());
      }
    }
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const FaceWorld::ObservationTimeHistory& FaceWorld::GetWallTimesObserved(const SmartFaceID& faceID)
  {
    return GetWallTimesObserved(faceID.GetID());
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const FaceWorld::ObservationTimeHistory& FaceWorld::GetWallTimesObserved(Vision::FaceID_t faceID)
  {
    auto it = _wallTimesObserved.find(faceID);
    if( it != _wallTimesObserved.end() ) {
      return it->second;
    }

    static const ObservationTimeHistory kEmptyQueue;
    return kEmptyQueue;
  }


} // namespace Vector
} // namespace Anki
