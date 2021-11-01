/**
 * File: actionInterface.cpp
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements interfaces for action states for a robot.
 *
 *              Note about subActions (manually ticking actions inside another action)
 *              Store subActions as unique_ptrs since the subAction is unique to the
 *              parent and the parent is responsible for managing everything about the
 *              subAction. (see PickupObjectAction for examples)
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "coretech/common/engine/utils/timer.h"
#include "engine/actions/actionInterface.h"
#include "engine/actions/actionWatcher.h"
#include "engine/components/animTrackHelpers.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/components/movementComponent.h"
#include "engine/components/pathComponent.h"
#include "engine/components/visionScheduleMediator/visionScheduleMediator.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/moodSystem/moodManager.h"
#include "engine/robot.h"
#include "clad/externalInterface/messageEngineToGame.h"
#include "clad/robotInterface/messageEngineToRobot.h"
#include "util/helpers/templateHelpers.h"

#define DEBUG_ANIM_TRACK_LOCKING 0

#define DEBUG_ACTION_RUNNING 0

#define LOG_CHANNEL "Actions"

namespace Anki {

  namespace Vector {

    // Ensure that nobody sets bad tag ranges (we want them all to be mutually exclusive
    static_assert(ActionConstants::FIRST_GAME_TAG   > ActionConstants::INVALID_TAG,      "Game Tag Overlap");
    static_assert(ActionConstants::FIRST_SDK_TAG    > ActionConstants::LAST_GAME_TAG,    "Sdk Tag Overlap");
    static_assert(ActionConstants::FIRST_ENGINE_TAG > ActionConstants::LAST_SDK_TAG,     "Engine Tag Overlap");
    static_assert(ActionConstants::LAST_GAME_TAG    > ActionConstants::FIRST_GAME_TAG,   "Bad Game Tag Range");
    static_assert(ActionConstants::LAST_SDK_TAG     > ActionConstants::FIRST_SDK_TAG,    "Bad Sdk Tag Range");
    static_assert(ActionConstants::LAST_ENGINE_TAG  > ActionConstants::FIRST_ENGINE_TAG, "Bad Engine Tag Range");

    u32 IActionRunner::sTagCounter = ActionConstants::FIRST_ENGINE_TAG;
    std::set<u32> IActionRunner::sInUseTagSet;

    u32 IActionRunner::NextIdTag()
    {
      // Post increment IActionRunner::sTagCounter (and loop within the ENGINE_TAG range)
      u32 nextIdTag = IActionRunner::sTagCounter;
      if (IActionRunner::sTagCounter == ActionConstants::LAST_ENGINE_TAG)
      {
        IActionRunner::sTagCounter = ActionConstants::FIRST_ENGINE_TAG;
      }
      else
      {
        ++IActionRunner::sTagCounter;
      }

      assert((nextIdTag >= ActionConstants::FIRST_ENGINE_TAG) && (nextIdTag <= ActionConstants::LAST_ENGINE_TAG));
      assert(nextIdTag != ActionConstants::INVALID_TAG);

      return nextIdTag;
    }


    IActionRunner::IActionRunner(const std::string & name,
                                 const RobotActionType type,
                                 const u8 tracksToLock)
    : _completionUnion(ActionCompletedUnion())
    , _type(type)
    , _name(name)
    , _tracks(tracksToLock)
    {
      // Assign every action a unique tag that is not currently in use
      _idTag = NextIdTag();

      while (!IActionRunner::sInUseTagSet.insert(_idTag).second) {
        PRINT_NAMED_ERROR("IActionRunner.TagCounterClash", "TagCounters shouldn't overlap");
        _idTag = NextIdTag();
      }

      _customTag = _idTag;

      // This giant switch is necessary to set the appropriate completion union tags in order
      // to avoid emitting a completion union with an invalid tag
      // There is no default case in order to prevent people from forgetting to add it here
      switch(type)
      {
        case RobotActionType::ALIGN_WITH_OBJECT:
        case RobotActionType::DRIVE_TO_OBJECT:
        case RobotActionType::FACE_PLANT:
        case RobotActionType::PICK_AND_PLACE_INCOMPLETE:
        case RobotActionType::PICKUP_OBJECT_HIGH:
        case RobotActionType::PICKUP_OBJECT_LOW:
        case RobotActionType::PLACE_OBJECT_HIGH:
        case RobotActionType::PLACE_OBJECT_LOW:
        case RobotActionType::POP_A_WHEELIE:
        case RobotActionType::ROLL_OBJECT_LOW:
        case RobotActionType::TURN_TOWARDS_OBJECT:
        {
          _completionUnion.Set_objectInteractionCompleted(ObjectInteractionCompleted());
          break;
        }

        case RobotActionType::PLAY_ANIMATION:
        case RobotActionType::RESELECTING_LOOP_ANIMATION:
        {
          _completionUnion.Set_animationCompleted(AnimationCompleted());
          break;
        }

        case RobotActionType::DEVICE_AUDIO:
        {
          _completionUnion.Set_deviceAudioCompleted(DeviceAudioCompleted());
          break;
        }

        case RobotActionType::TRACK_FACE:
        case RobotActionType::TRACK_PET_FACE:
        {
          _completionUnion.Set_trackFaceCompleted(TrackFaceCompleted());
          break;
        }

        // These actions don't set completion unions
        case RobotActionType::BACKUP_ONTO_CHARGER:
        case RobotActionType::CALIBRATE_MOTORS:
        case RobotActionType::CLIFF_ALIGN_TO_WHITE:
        case RobotActionType::COMPOUND:
        case RobotActionType::DISPLAY_FACE_IMAGE:
        case RobotActionType::DISPLAY_PROCEDURAL_FACE:
        case RobotActionType::DRIVE_PATH:
        case RobotActionType::DRIVE_STRAIGHT:
        case RobotActionType::DRIVE_TO_FLIP_BLOCK_POSE:
        case RobotActionType::DRIVE_TO_PLACE_CARRIED_OBJECT:
        case RobotActionType::DRIVE_TO_POSE:
        case RobotActionType::FLIP_BLOCK:
        case RobotActionType::HANG:
        case RobotActionType::MOUNT_CHARGER:
        case RobotActionType::MOVE_HEAD_TO_ANGLE:
        case RobotActionType::MOVE_LIFT_TO_ANGLE:
        case RobotActionType::MOVE_LIFT_TO_HEIGHT:
        case RobotActionType::PAN_AND_TILT:
        case RobotActionType::PLAY_CUBE_ANIMATION:
        case RobotActionType::SAY_TEXT:
        case RobotActionType::SEARCH_FOR_NEARBY_OBJECT:
        case RobotActionType::TRACK_GROUND_POINT:
        case RobotActionType::TRACK_MOTION:
        case RobotActionType::TRACK_OBJECT:
        case RobotActionType::TRAVERSE_OBJECT:
        case RobotActionType::TURN_IN_PLACE:
        case RobotActionType::TURN_TO_ALIGN_WITH_CHARGER:
        case RobotActionType::TURN_TOWARDS_FACE:
        case RobotActionType::TURN_TOWARDS_IMAGE_POINT:
        case RobotActionType::TURN_TOWARDS_LAST_FACE_POSE:
        case RobotActionType::TURN_TOWARDS_POSE:
        case RobotActionType::UNKNOWN:
        case RobotActionType::VISUALLY_VERIFY_FACE:
        case RobotActionType::VISUALLY_VERIFY_NO_OBJECT_AT_POSE:
        case RobotActionType::VISUALLY_VERIFY_OBJECT:
        case RobotActionType::WAIT:
        case RobotActionType::WAIT_FOR_IMAGES:
        case RobotActionType::WAIT_FOR_LAMBDA:
        {
          _completionUnion.Set_defaultCompleted(DefaultCompleted());
          break;
        }
      }
    }

    IActionRunner::~IActionRunner()
    {
      if(!_preppedForCompletion) {
        if( HasStarted() ) {
          PRINT_NAMED_ERROR("IActionRunner.Destructor.NotPreppedForCompletion", "[%d]", GetTag());
        }
        else {
          LOG_INFO("IActionRunner.Destructor.NotPreppedForCompletionAndNotStarted",
                   "[%d] type [%s]", GetTag(), RobotActionTypeToString(_type));
        }
      }

      // Erase the tags as they are no longer in use
      IActionRunner::sInUseTagSet.erase(_customTag);
      IActionRunner::sInUseTagSet.erase(_idTag);

      if(!HasRobot()){
        if( HasStarted() ) {
          PRINT_NAMED_ERROR("IActionRunner.Destructor.RobotNotSet", "[%d]", GetTag());
        }
        else {
          LOG_INFO("IActionRunner.Destructor.RobotNotSetAndNotStarted",
                   "[%d] robot not set, but action [%s] also not started so this is OK", GetTag(), RobotActionTypeToString(_type));
        }
        return;
      }

      // clear the motion profile, if desired
      if(_shouldClearMotionProfile ) {
        GetRobot().GetPathComponent().ClearCustomMotionProfile();
      }

      // Stop motion on any movement tracks that are locked by this action
      auto& mc = GetRobot().GetMoveComponent();
      const auto& lockStr = std::to_string(GetTag());
      std::string debugStr;
      if (mc.AreAllTracksLockedBy((u8) AnimTrackFlag::HEAD_TRACK, lockStr)) {
        GetRobot().GetMoveComponent().StopHead();
        debugStr += "HEAD_TRACK, ";
      }
      if (mc.AreAllTracksLockedBy((u8) AnimTrackFlag::LIFT_TRACK, lockStr)) {
        GetRobot().GetMoveComponent().StopLift();
        debugStr += "LIFT_TRACK, ";
      }
      if (mc.AreAllTracksLockedBy((u8) AnimTrackFlag::BODY_TRACK, lockStr)) {
        GetRobot().GetMoveComponent().StopBody();
        debugStr += "BODY_TRACK, ";
      }
      // Log if we've stopped movement on any tracks
      if (!debugStr.empty()) {
        LOG_INFO("IActionRunner.Destroy.StopMovement",
                 "Stopping movement on the following tracks since they were locked: %s[%s][%d]",
                 debugStr.c_str(), _name.c_str(), _idTag);
      }

      if(!_suppressTrackLocking && _state != ActionResult::NOT_STARTED)
      {
        if(DEBUG_ANIM_TRACK_LOCKING)
        {
          LOG_INFO("IActionRunner.Destroy.UnlockTracks",
                   "unlocked: (0x%x) %s by %s [%d]",
                   _tracks,
                   AnimTrackHelpers::AnimTrackFlagsToString(_tracks).c_str(),
                   _name.c_str(),
                   _idTag);
        }
        mc.UnlockTracks(_tracks, lockStr);
      }

      // We should not be locking _any_ tracks at this point. If we are, then just unlock them and report this.
      const auto lockedTracks = mc.GetTracksLockedBy(lockStr);
      if (lockedTracks != 0) {
        LOG_ERROR("IActionRunner.Destroy.TracksStillLocked",
                  "%s [%d]: Somehow we are still locking tracks 0x%02X. Unlocking them. Current state %s, _suppressTrackLocking %d",
                  _name.c_str(), GetTag(), lockedTracks, ActionResultToString(_state), _suppressTrackLocking);
        mc.UnlockTracks(lockedTracks, lockStr);
      }
      
      GetRobot().GetActionList().GetActionWatcher().ActionEnding(this);
    }

    Robot& IActionRunner::GetRobot()
    {
      DEV_ASSERT_MSG(_robot != nullptr,
                     "IActionRunner.GetRobot.RobotIsNull",
                     "Robot not set for action %s with tag %d", GetName().c_str(), GetTag());
      return *_robot;
    }

    Robot& IActionRunner::GetRobot() const
    {
      DEV_ASSERT_MSG(_robot != nullptr,
                     "IActionRunner.GetRobot.RobotIsNull",
                     "Robot not set for action %s with tag %d", GetName().c_str(), GetTag());
      return *_robot;
    }

    bool IActionRunner::SetTag(u32 tag)
    {
      // Probably a bad idea to be able to change the tag while the action is running
      if(_state == ActionResult::RUNNING)
      {
        PRINT_NAMED_WARNING("IActionRunner.SetTag", "Action %s [%d] is running unable to set tag to %d",
                            GetName().c_str(),
                            GetTag(),
                            tag);
        _state = ActionResult::BAD_TAG;
        return false;
      }

      // If the tag has already been set and the action is not running then erase the current tag in order to
      // set the new one
      if(_customTag != _idTag)
      {
        IActionRunner::sInUseTagSet.erase(_customTag);
      }
      // If this is an invalid tag or is currently in use
      if(tag == static_cast<u32>(ActionConstants::INVALID_TAG) ||
         !IActionRunner::sInUseTagSet.insert(tag).second)
      {
        PRINT_NAMED_ERROR("IActionRunner.SetTag.InvalidTag", "Tag [%d] is invalid", tag);
        _state = ActionResult::BAD_TAG;
        return false;
      }
      _customTag = tag;
      return true;
    }

    bool IActionRunner::Interrupt()
    {
      if(InterruptInternal())
      {
        // Only need to unlock tracks if this is running because Update() locked tracks
        if(!_suppressTrackLocking &&
           (_state == ActionResult::RUNNING))
        {
          u8 tracks = GetTracksToLock();
          if(DEBUG_ANIM_TRACK_LOCKING)
          {
            LOG_INFO("IActionRunner.Interrupt.UnlockTracks",
                     "unlocked: (0x%x) %s by %s [%d]",
                     tracks,
                     AnimTrackHelpers::AnimTrackFlagsToString(tracks).c_str(),
                     _name.c_str(),
                     _idTag);
          }

          GetRobot().GetMoveComponent().UnlockTracks(tracks, GetTag());
        }
        Reset(false);
        _state = ActionResult::INTERRUPTED;
        return true;
      }
      return false;
    }

    void IActionRunner::ShouldSuppressTrackLocking(bool tf)
    {
      if (_state != ActionResult::NOT_STARTED) {
        PRINT_NAMED_WARNING("IActionRunner.ShouldSuppressTrackLocking.AlreadyStarted",
                            "Action %s [%d] not suppressing track locking since we have already started (current state %s)",
                            GetName().c_str(),
                            GetTag(),
                            ActionResultToString(_state));
        return;
      }
      _suppressTrackLocking = tf;
    }
    
    void IActionRunner::ForceComplete()
    {
      LOG_INFO("IActionRunner.ForceComplete",
               "Forcing %s[%d] in state %s to complete",
               GetName().c_str(),
               GetTag(),
               EnumToString(_state));

      _state = ActionResult::SUCCESS;
    }

    ActionResult IActionRunner::Update()
    {
      auto & actionList = GetRobot().GetActionList();
      auto & actionWatcher = actionList.GetActionWatcher();
      auto & moveComponent = GetRobot().GetMoveComponent();

      actionWatcher.ActionStartUpdating(this);

      switch (_state)
      {
        case ActionResult::RETRY:
        case ActionResult::NOT_STARTED:
        case ActionResult::INTERRUPTED:
        {
          // Before we set the action to running, apply the motion profile to this action if there is
          // one. This will apply the profile automatically to every action that runs, including nested or
          // compound actions. It's up to each individual action to implement the SetMotionProfile function to
          // update their motion profile appropriately
          if( GetRobot().GetPathComponent().HasCustomMotionProfile() ) {
            const bool profileApplied = SetMotionProfile( GetRobot().GetPathComponent().GetCustomMotionProfile() );
            if( !profileApplied ) {
              LOG_INFO("IActionRunner.SetMotionProfile.Unused",
                       "Action %s [%d] unable to set motion profile. Perhaps speeds already set manually?",
                       GetName().c_str(),
                       GetTag());
            }
          }

          _state = ActionResult::RUNNING;
          if (!_suppressTrackLocking)
          {
            // When the ActionRunner first starts, lock any specified subsystems
            u8 tracksToLock = GetTracksToLock();

            if (moveComponent.AreAnyTracksLocked(tracksToLock))
            {
              // Split this into two messages so we don't send giant strings to DAS
              PRINT_NAMED_WARNING("IActionRunner.Update.TracksLocked",
                                  "Action %s [%d] not running because required tracks are locked",
                                  GetName().c_str(),
                                  GetTag());

              PRINT_NAMED_WARNING("IActionRunner.Update.TracksLockedBecause",
                                  "Required tracks %s locked because %s",
                                  AnimTrackHelpers::AnimTrackFlagsToString(tracksToLock).c_str(),
                                  moveComponent.WhoIsLocking(tracksToLock).c_str());

              _state = ActionResult::TRACKS_LOCKED;
              actionWatcher.ActionEndUpdating(this);
              return ActionResult::TRACKS_LOCKED;
            }

            if (DEBUG_ANIM_TRACK_LOCKING)
            {
              LOG_INFO("IActionRunner.Update.LockTracks",
                       "locked: (0x%x) %s by %s [%d]",
                       tracksToLock,
                       AnimTrackHelpers::AnimTrackFlagsToString(tracksToLock).c_str(),
                       GetName().c_str(),
                       GetTag());
            }

            moveComponent.LockTracks(tracksToLock, GetTag(), GetName());
          }

          if (DEBUG_ACTION_RUNNING && _displayMessages)
          {
            LOG_DEBUG("IActionRunner.Update.IsRunning",
                      "Action [%d] %s running",
                      GetTag(),
                      GetName().c_str());
          }
        }
        case ActionResult::RUNNING:
        {
          _state = UpdateInternal();

          if (_state == ActionResult::RUNNING)
          {
            // Still running dont fall through
            break;
          }
          // UpdateInternal() returned something other than running so fall through to handle action
          // completion
        }
        // Every other case is a completion case (ie the action is no longer running due to success, failure, or
        // cancel)
        default:
        {
          if (_displayMessages) {
            LOG_INFO("IActionRunner.Update.ActionCompleted",
                     "%s [%d] %s with state %s.", GetName().c_str(),
                     GetTag(),
                     (_state==ActionResult::SUCCESS ? "succeeded" :
                      _state==ActionResult::CANCELLED_WHILE_RUNNING ? "was cancelled" : "failed"),
                     EnumToString(_state));
          }

          PrepForCompletion();

          if (DEBUG_ACTION_RUNNING && _displayMessages) {
            LOG_DEBUG("IActionRunner.Update.IsRunning",
                      "Action [%d] %s NOT running",
                      GetTag(),
                      GetName().c_str());
          }
        }
      }
      actionWatcher.ActionEndUpdating(this);
      return _state;
    }

    void IActionRunner::SetEnableMoodEventOnCompletion(bool enable)
    {
      GetRobot().GetMoodManager().SetEnableMoodEventOnCompletion(GetTag(), enable);
    }

    void IActionRunner::PrepForCompletion()
    {
      if(!_preppedForCompletion)
      {
        GetCompletionUnion(_completionUnion);
        _preppedForCompletion = true;
      } else {
        LOG_DEBUG("IActionRunner.PrepForCompletion.AlreadyPrepped",
                  "%s [%d]", _name.c_str(), GetTag());
      }
    }

    bool IActionRunner::RetriesRemain()
    {
      if(_numRetriesRemaining > 0) {
        --_numRetriesRemaining;
        return true;
      } else {
        return false;
      }
    }

#   if USE_ACTION_CALLBACKS
    void IActionRunner::AddCompletionCallback(ActionCompletionCallback callback)
    {
      _completionCallbacks.emplace_back(callback);
    }

    void IActionRunner::RunCallbacks(ActionResult result) const
    {
      for(const auto& callback : _completionCallbacks) {
        callback(result);
      }
    }
#   endif // USE_ACTION_CALLBACKS


    void IActionRunner::UnlockTracks()
    {
      // Tracks aren't locked until the action starts so don't unlock them until then
      if(!_suppressTrackLocking &&
         (_state != ActionResult::NOT_STARTED))
      {
        u8 tracks = GetTracksToLock();
        if(DEBUG_ANIM_TRACK_LOCKING)
        {
          LOG_INFO("IActionRunner.UnlockTracks",
                   "unlocked: (0x%x) %s by %s [%d]",
                   tracks,
                   AnimTrackHelpers::AnimTrackFlagsToString(tracks).c_str(),
                   _name.c_str(),
                   _idTag);
        }
        GetRobot().GetMoveComponent().UnlockTracks(tracks, GetTag());
      }
    }

    void IActionRunner::SetTracksToLock(const u8 tracks)
    {
      if(_state == ActionResult::NOT_STARTED)
      {
        _tracks = tracks;
      }
      else
      {
        PRINT_NAMED_WARNING("IActionRunner.SetTracksToLock", "Trying to set tracks to lock while running");
      }
    }

    void IActionRunner::Cancel()
    {
      if(_state != ActionResult::NOT_STARTED)
      {
        // LOG_INFO("IActionRunner.Cancel",
        //          "Cancelling action %s[%d]",
        //          _name.c_str(), GetTag());
        _state = ActionResult::CANCELLED_WHILE_RUNNING;
      }
#     if USE_ACTION_CALLBACKS
      {
        RunCallbacks(_state);
      }
#     endif
    }

    void IActionRunner::GetRobotCompletedActionMessage(ExternalInterface::RobotCompletedAction& msg)
    {
      std::vector<ActionResult> subActionResults;
      GetRobot().GetActionList().GetActionWatcher().GetSubActionResults(GetTag(), subActionResults);

      ActionCompletedUnion acu;
      GetCompletionUnion(acu);

      msg = ExternalInterface::RobotCompletedAction(GetTag(),
                                                    GetType(),
                                                    GetState(),
                                                    subActionResults,
                                                    acu);
    }

#pragma mark ---- IAction ----

    IAction::IAction(const std::string & name,
                     const RobotActionType type,
                     const u8 tracksToLock)
    : IActionRunner(name,
                    type,
                    tracksToLock)
    {
      Reset();
    }

    IAction::~IAction()
    {
      // release any subscriptions held by the VSM for this Action
      if(HasRobot() && !_requiredVisionModes.empty()) {
        LOG_DEBUG("IAction.Destructor.UnSettingVisionModes",
                  "Action %s [%d] Releasing VisionModes",
                  GetName().c_str(),
                  GetTag());
        GetRobot().GetVisionScheduleMediator().ReleaseAllVisionModeSubscriptions(this);
      }
    }

    void IAction::UnsubscribeFromVisionModes()
    {
      if(HasRobot() && !_requiredVisionModes.empty())
      {
        LOG_DEBUG("IAction.UnsubscribeFromVisionModes",
                  "Action %s [%d] releasing VisionModes",
                  GetName().c_str(), GetTag());
        GetRobot().GetVisionScheduleMediator().ReleaseAllVisionModeSubscriptions(this);
        _requiredVisionModes.clear(); 
      }
    }
    
    void IAction::Reset(bool shouldUnlockTracks)
    {
      LOG_DEBUG("IAction.Reset",
                "Resetting action,%s unlocking tracks", (shouldUnlockTracks ? "" : " NOT"));
      _actionSpecificPreconditionsMet = false;
      _startTime_sec = -1.f;
      if(shouldUnlockTracks)
      {
        UnlockTracks();
      }
      ResetState();
    }

    Util::RandomGenerator& IAction::GetRNG() const
    {
      return GetRobot().GetRNG();
    }
    
    bool IAction::DidTreadStateChangeFromOnTreads() const
    {
      const auto& currTreadState = GetRobot().GetOffTreadsState();
      return _prevTreadsState == OffTreadsState::OnTreads && currTreadState != OffTreadsState::OnTreads;
    }
    
    f32 IAction::GetCurrentRunTimeSeconds() const
    {
      const f32 currentTimeInSeconds = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
      
      if(_startTime_sec < 0.f) {
        // Action has not started running/updating yet!
        return 0.f;
      } else {
        return currentTimeInSeconds - _startTime_sec;
      }
    }

    ActionResult IAction::UpdateInternal()
    {
      ActionResult result = ActionResult::RUNNING;
      SetStatus(GetName());

      // On first call to Update(), figure out the waitUntilTime
      const f32 currentTimeInSeconds = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();

      if(_startTime_sec < 0.f) {
        // Record first update time
        _startTime_sec = currentTimeInSeconds;
      }

      // Update timeout/wait times in case they have been adjusted since the action
      // started. Time to wait until is always relative to original start, however.
      // (Include CheckIfDoneDelay in wait time if we have already met pre-conditions
      const f32 waitUntilTime = (_startTime_sec + GetStartDelayInSeconds() +
                                 (_actionSpecificPreconditionsMet ?
                                  GetCheckIfDoneDelayInSeconds() : 0.f));
      const f32 timeoutTime   = _startTime_sec + GetTimeoutInSeconds();

      // Fail if we have exceeded timeout time
      if(currentTimeInSeconds >= timeoutTime) {
        if(IsMessageDisplayEnabled()) {
          PRINT_NAMED_WARNING("IAction.Update.TimedOut",
                              "%s timed out after %.1f seconds.",
                              GetName().c_str(), GetTimeoutInSeconds());
        }
        result = ActionResult::TIMEOUT;
      }

      // Don't do anything until we have reached the waitUntilTime
      else if(currentTimeInSeconds >= waitUntilTime)
      {
        // Check the action-specific preconditions.
        if(!_actionSpecificPreconditionsMet) {
          //LOG_INFO("IAction.Update", "Updating %s: checking preconditions.", GetName().c_str());
          SetStatus(GetName() + ": check action-specific preconditions");

          // Note that derived classes will define what to do when action-specific
          // pre-conditions are not met: if they return RUNNING, then the action will
          // effectively wait for the preconditions to be met. Otherwise, a failure
          // will get propagated out as the return value of the Update method.
          result = Init();

          if(result == ActionResult::SUCCESS) {
            if(IsMessageDisplayEnabled()) {
              LOG_DEBUG("IAction.Update.ActionSpecificPreconditionsMet",
                        "Preconditions for %s [%d] successfully met.",
                        GetName().c_str(),
                        GetTag());
            }

            // This action is ready to run, subscribe to appropriate vision modes
            GetRequiredVisionModes(_requiredVisionModes);
            if(!_requiredVisionModes.empty()) {
              LOG_DEBUG("IAction.Update.SettingVisionModes",
                        "Action %s [%d] Requesting VisionModes",
                        GetName().c_str(),
                        GetTag());
              GetRobot().GetVisionScheduleMediator().SetVisionModeSubscriptions(this, _requiredVisionModes);
            }

            // If ALL preconditions were successfully met, switch result to RUNNING
            // so that we don't think the entire action is completed. (We still
            // need to do CheckIfDone() calls!)
            // TODO: there's probably a tidier way to do this.
            _actionSpecificPreconditionsMet = true;
            result = ActionResult::RUNNING;
          }
          // When attempting to initialize, cache the current treads state for comparison at runtime.
          _prevTreadsState = GetRobot().GetOffTreadsState();
        }

        // Re-check if ALL preconditions are met, since they could have _just_ been met
        if(_actionSpecificPreconditionsMet && currentTimeInSeconds >= waitUntilTime) {
          //LOG_INFO("IAction.Update", "Updating %s: checking if done.", GetName().c_str());
          SetStatus(GetName() + ": check if done");

          // Check if the previous OffTreadsState has changed from OnTreads.
          // If so, stop and fail the action.
          if (ShouldFailOnTransitionOffTreads() && DidTreadStateChangeFromOnTreads()) {
            result = ActionResult::INVALID_OFF_TREADS_STATE;
          } else {
            // Pre-conditions already met, just run until done
            result = CheckIfDone();
          }
          // Cache the current treads state for comparison at the next time that the action is updated.
          _prevTreadsState = GetRobot().GetOffTreadsState();
        }
      } // if(currentTimeInSeconds > _waitUntilTime)

      const bool shouldRetry = (IActionRunner::GetActionResultCategory(result) == ActionResultCategory::RETRY);
      if(shouldRetry && RetriesRemain()) {
        if(IsMessageDisplayEnabled()) {
          LOG_INFO("IAction.Update.CurrentActionFailedRetrying",
                   "Failed running action %s. Retrying.",
                   GetName().c_str());
        }

        // Don't unlock the tracks if retrying
        Reset(false);
        result = ActionResult::RUNNING;
      }

#     if USE_ACTION_CALLBACKS
      if(result != ActionResult::RUNNING) {
        RunCallbacks(result);
      }
#     endif
      return result;
    } // UpdateInternal()

  } // namespace Vector
} // namespace Anki
