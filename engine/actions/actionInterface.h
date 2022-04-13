/**
 * File: actionInterface.h
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Defines interfaces for action states for a robot.
 *
 *              Note about subActions (manually ticking actions inside another action)
 *              Store subActions as unique_ptrs since the subAction is unique to the
 *              parent and the parent is responsible for managing everything about the
 *              subAction. (see PickupObjectAction for examples)
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_ACTION_INTERFACE_H
#define ANKI_COZMO_ACTION_INTERFACE_H

#include "coretech/common/shared/types.h"
#include "coretech/common/engine/objectIDs.h"
#include "coretech/common/engine/math/pose.h"

#include "engine/actions/actionContainers.h"
#include "engine/components/visionScheduleMediator/iVisionModeSubscriber.h"
#include "engine/components/visionScheduleMediator/visionScheduleMediator_fwd.h"

#include "clad/types/actionTypes.h"
#include "clad/types/actionResults.h"
#include "clad/types/animationTypes.h"
#include "clad/types/offTreadsStates.h"

#include "util/random/randomGenerator.h"

#include <list>

// Not sure if we want to support callbacks yet, but this switch enables some
// preliminary callback code for functions to be run when an action completes.
#define USE_ACTION_CALLBACKS 0

// Enable/disable procedural eye leading
#define PROCEDURAL_EYE_LEADING 0

// TODO: Is this Cozmo-specific or can it be moved to coretech?
// (Note it does require a Robot, which is currently only present in Cozmo)

namespace Anki {

  namespace Vector {

    // Forward Declarations:
    class Robot;
    struct PathMotionProfile;

    namespace ExternalInterface {
      struct RobotCompletedAction;
    }

    // Parent container for running actions, which can hold simple actions as
    // well as "compound" ones, defined elsewhere.
    class IActionRunner
    {
    public:
      IActionRunner(const std::string & name,
                    const RobotActionType type,
                    const u8 tracksToLock);

      virtual ~IActionRunner();

      ActionResult Update();

      bool HasRobot() const { return _robot != nullptr;}

      Robot& GetRobot();
      Robot& GetRobot() const;

      void SetRobot(Robot* robot){ _robot = robot; OnRobotSet();}

      // Tags can be used to identify specific actions. A unique tag is assigned
      // at construction, or it can be overridden with SetTag(). The Tag is
      // returned in the ActionCompletion signal as well.
      // Returns true if the tag has been set, false if it is invalid or already exists
      bool SetTag(u32 tag);
      // If a custom tag has been set return will be that otherwise it is the same as the
      // auto-generated tag
      u32  GetTag() const { return _customTag; }

      // returns true if the action tag is currently "in use". Tags are in use from the moment the action is
      // created (in the constructor), until the action is deleted
      static bool IsTagInUse(u32 tag) { return sInUseTagSet.find(tag) != sInUseTagSet.end(); }

      // If a FAILURE_RETRY is encountered, how many times will the action
      // be retried before return FAILURE_ABORT.
      void SetNumRetries(const u8 numRetries) {_numRetriesRemaining = numRetries;}

      // If a subclass wants to update their name or type after construction they can call these
      // setters
      void SetName(const std::string& name) { _name = name; }
      const std::string& GetName() const { return _name; }

      void SetType(const RobotActionType type) { _type = type; }
      const RobotActionType GetType() const { return _type; }

      // Allow the robot to move certain subsystems while the action executes,
      // also disables any tracks used by animations that may have already been
      // streamed and are in the robot's buffer, so they don't interfere
      // with the action. I.e., by default actions will lockout all control of the robot,
      // and extra movement commands are ignored.
      // Note: uses the bits defined by AnimTrackFlag.
      void SetTracksToLock(const u8 tracks);
      const u8 GetTracksToLock() const { return _tracks; }

      // If this method returns true, then it means the derived class is interruptible,
      // can safely be re-queued using "NOW_AND_RESUME", and will pick back up safely
      // after the newly-queued action completes. Otherwise, this action will just
      // be cancelled when NOW_AND_RESUME is used. Note that this relies on
      // subclasses implementing InterruptInternal() and Reset().
      bool Interrupt();

      // Override this to take care of anything that needs to be done on Retry/Interrupt.
      virtual void Reset(bool shouldUnlockTracks) = 0;

      // Get last status message
      const std::string& GetStatus() const { return _statusMsg; }

      // Used (e.g. in initialization of CompoundActions) to specify that a
      // consituent action should not try to lock or unlock tracks it uses
      void ShouldSuppressTrackLocking(bool tf);
      bool IsSuppressingTrackLocking() const { return _suppressTrackLocking; }

      // By default, the completion of any action could cause a mood event (the robot's mood manager defines
      // this). If this is set to false, this action won't trigger any mood events
      void SetEnableMoodEventOnCompletion(bool enable);

      // Override this to fill in the ActionCompletedStruct emitted as part of the
      // completion signal with an action finishes. Note that this is public because
      // subclasses that are composed of other actions may want to make use of
      // the completion info of their constituent actions.
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const { completionUnion = _completionUnion; }

      void GetRobotCompletedActionMessage(ExternalInterface::RobotCompletedAction& msg);

      // Enable/disable message display (Default is true)
      void EnableMessageDisplay(bool tf) { _displayMessages = tf; }
      bool IsMessageDisplayEnabled() const { return _displayMessages; }

      // Called when the action stops running and sets variables needed for completion.
      // This calls the overload-able GetCompletionUnion() method above.
      void PrepForCompletion();

      void UnlockTracks();

      const ActionResult GetState() const { return _state; }

      // Marks the state as cancelled only if the action has been started
      void Cancel();

      // Forces the actions state to SUCCESS so in the next update call the action will immediately complete
      // Use caution when calling this because it could result in an incomplete completionUnion
      void ForceComplete();

      static ActionResultCategory GetActionResultCategory(const ActionResult& res) { return static_cast<ActionResultCategory>(static_cast<u32>(res) >> ARCBitShift::NUM_BITS); }

      // This should only be used from the PathComponent. If set, this action will clear the custom profile
      // when it finishes. This allows actions to be created with a custom motion profile (e.g. from Unity or
      // SDK)
      void ClearMotionProfileOnCompletion() { _shouldClearMotionProfile = true; }

    protected:
      // notify subclasses when the robot is set
      virtual void OnRobotSet(){};

      virtual ActionResult UpdateInternal() = 0;

      // By default, actions are not interruptable
      virtual bool InterruptInternal() { return false; }

      // Override to handle setting of a motion profile. Returns true if the profile was used correctly (or if
      // it was irrelevant, e.g. for an animation action). Returns false if the action is unable to use the
      // profile, e.g. because it is already using manually set speeds. Note that this action only needs to
      // worry about itself, any other actions created by this action (either as direct sub-actions or added
      // to a compound action), will have this function automatically called when appropriate
      virtual bool SetMotionProfile(const PathMotionProfile& motionProfile) { return true; }

      bool RetriesRemain();

      // Derived actions can use this to set custom status messages here.
      void SetStatus(const std::string& msg);

      void ResetState() { _state = ActionResult::NOT_STARTED; }

      bool IsRunning() const { return _state == ActionResult::RUNNING; }
      bool HasStarted() const { return _state != ActionResult::NOT_STARTED; }

      static u32 NextIdTag();

    private:
      Robot* _robot = nullptr;

      u8            _numRetriesRemaining = 0;

      std::string   _statusMsg;

      ActionResult         _state           = ActionResult::NOT_STARTED;
      ActionCompletedUnion _completionUnion;
      RobotActionType      _type;
      std::string          _name;
      u8                   _tracks          = (u8)AnimTrackFlag::NO_TRACKS;

      bool          _preppedForCompletion   = false;
      bool          _suppressTrackLocking   = false;
      bool          _displayMessages        = true;

      bool          _shouldClearMotionProfile = false;

      // Auto-generated tag
      u32           _idTag;
      u32           _customTag;

      static u32                sTagCounter;
      // Set of tags that are in use
      static std::set<u32> sInUseTagSet;

#   if USE_ACTION_CALLBACKS
    public:
      using ActionCompletionCallback = std::function<void(ActionResult)>;
      void  AddCompletionCallback(ActionCompletionCallback callback);

    protected:
      void RunCallbacks(ActionResult result) const;

    private:
      std::list<ActionCompletionCallback> _completionCallbacks;
#   endif

    }; // class IActionRunner

    inline void IActionRunner::SetStatus(const std::string& msg) {
      _statusMsg = msg;
    }


    // Action Interface
    class IAction : public IActionRunner, public IVisionModeSubscriber
    {
    public:

      IAction(const std::string & name,
              const RobotActionType type,
              const u8 tracksToLock);

      virtual ~IAction();


      // Provide a retry function that will be called by Update() if
      // FAILURE_RETRY is returned by the derived CheckIfDone() method.
      //void SetRetryFunction(std::function<Result(Robot&)> retryFcn);

      // Runs the retry function if one was specified.
      //Result Retry();

    protected:

      // This UpdateInternal() is what gets called by IActionRunner's Update().  It in turn
      // handles timing delays and runs (protected) Init() and CheckIfDone()
      // methods. Those are the virtual methods that specific classes should
      // implement to get desired action behaviors. Note that this method
      // is final and cannot be overridden by specific individual actions.
      virtual ActionResult UpdateInternal() override final;

      // If the derived action has specific vision mode requirements, they should be noted by implementing this
      // function, subscriptions to the VisionScheduleMediator will then be handled by IAction::UpdateInternal().
      // By default, we assume that no vision modes are required for the action
      virtual void GetRequiredVisionModes(std::set<VisionModeRequest>& requests) const { }
      
      // Normally, actions unsubscribe from vision modes automatically when they destruct.
      // Call this from a derived class to unsubscribe early.
      void UnsubscribeFromVisionModes();
      
      // If the derived action needs to fail if the robot's tread state transitions from OnTreads to any other
      // state at runtime, then the action returns an action result of INVALID_OFF_TREADS_STATE. The check to verify
      // this is handled by IAction::UpdateInternal, which calls IAction::IsCurrentTreadStateValid. By default,
      // we assume that the actions can run even if the tread state transitions from OnTreads to any other state.
      virtual bool ShouldFailOnTransitionOffTreads() const { return false; }

      // Derived Actions should implement these.
      virtual ActionResult  Init() { return ActionResult::SUCCESS; } // Optional: default is no preconditions to meet
      virtual ActionResult  CheckIfDone() = 0;

      //
      // Timing delays:
      //  (e.g. for allowing for communications to physical robot to have an effect)
      //

      // Before checking preconditions. Optional: default is no delay
      virtual f32 GetStartDelayInSeconds()       const { return 0.0f; }

      // Before first CheckIfDone() call, after preconditions are met. Optional: default is no delay
      virtual f32 GetCheckIfDoneDelayInSeconds() const { return 0.0f; }

      // Before giving up on entire action. Optional: default is 30 seconds
      virtual f32 GetTimeoutInSeconds()          const { return 30.f; }

      virtual void Reset(bool shouldUnlockTracks = true) override final;
      
      // Retrieves the number of seconds elapsed since the action started updating.
      f32 GetCurrentRunTimeSeconds() const;

      // A random number generator all subclasses can share
      Util::RandomGenerator& GetRNG() const;
      
      // Verifies that the robot's tread state has NOT recently transitioned from OnTreads to any other state.
      bool DidTreadStateChangeFromOnTreads() const;

    private:

      bool          _actionSpecificPreconditionsMet;
      f32           _startTime_sec;

      std::set<VisionModeRequest> _requiredVisionModes;
      OffTreadsState _prevTreadsState;

    }; // class IAction

  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_ACTION_INTERFACE_H
