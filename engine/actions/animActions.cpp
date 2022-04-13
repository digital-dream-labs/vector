/**
 * File: animActions.cpp
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements animation and audio cozmo-specific actions, derived from the IAction interface.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/actions/animActions.h"
#include "engine/actions/compoundActions.h"
#include "engine/actions/dockActions.h"
#include "engine/actions/driveToActions.h"
#include "engine/aiComponent/aiComponent.h"
#include "engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior.h"
#include "engine/audio/engineRobotAudioClient.h"
#include "engine/components/battery/batteryComponent.h"
#include "engine/components/carryingComponent.h"
#include "engine/components/cubes/cubeLights/cubeLightComponent.h"
#include "engine/components/robotStatsTracker.h"
#include "engine/cozmoContext.h"
#include "engine/moodSystem/moodManager.h"
#include "engine/robot.h"
#include "engine/robotDataLoader.h"
#include "engine/robotInterface/messageHandler.h"

#include "clad/types/behaviorComponent/behaviorStats.h"
#include "coretech/common/engine/utils/timer.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/logging/DAS.h"

#include "webServerProcess/src/webVizSender.h"

#define LOG_CHANNEL "Actions"

namespace Anki {

  namespace Vector {

    namespace {
      static constexpr const char* kManualBodyTrackLockName = "PlayAnimationOnChargerSpecialLock";
      
      // Toggle to true so animators can play any animation on charger for testing
      CONSOLE_VAR(bool, kIgnoreAnimWhitelist, "Animation", false);
    }

    #pragma mark ---- PlayAnimationAction ----

    PlayAnimationAction::PlayAnimationAction(const std::string& animName,
                                             u32 numLoops,
                                             bool interruptRunning,
                                             u8 tracksToLock,
                                             float timeout_sec,
                                             TimeStamp_t startAtTime_ms,
                                             AnimationComponent::AnimationCompleteCallback callback)
    : IAction("PlayAnimation" + animName,
              RobotActionType::PLAY_ANIMATION,
              tracksToLock)
    , _animName(animName)
    , _numLoopsRemaining(numLoops)
    , _interruptRunning(interruptRunning)
    , _timeout_sec(timeout_sec)
    , _startAtTime_ms(startAtTime_ms)
    , _passedInCallback(callback)
    {
      // If an animation is supposed to loop infinitely, it should have a
      // much longer default timeout
      if((numLoops == 0) &&
         (timeout_sec == _kDefaultTimeout_sec)){
        _timeout_sec = _kDefaultTimeoutForInfiniteLoops_sec;
      }

    }

    PlayAnimationAction::~PlayAnimationAction()
    {
      if (HasStarted() && !_stoppedPlaying) {
        LOG_INFO("PlayAnimationAction.Destructor.StillStreaming",
                 "Action destructing, but AnimationComponent is still playing: %s. Telling it to stop.",
                 _animName.c_str());
        if (HasRobot()) {
          GetRobot().GetAnimationComponent().StopAnimByName(_animName);          
        } else {
          // This shouldn't happen if HasStarted()...
          LOG_WARNING("PlayAnimationAction.Dtor.NoRobot", "");
        }
      }

      if (HasStarted() && _bodyTrackManuallyLocked ) {
        GetRobot().GetMoveComponent().UnlockTracks( (u8) AnimTrackFlag::BODY_TRACK, kManualBodyTrackLockName );
        _bodyTrackManuallyLocked = false;
      }
    }

    void PlayAnimationAction::OnRobotSet()
    {
      OnRobotSetInternalAnim();
    }

    void PlayAnimationAction::InitTrackLockingForCharger()
    {
      if( _bodyTrackManuallyLocked ) {
        // already locked, nothing to do
        return;
      }

      if( GetRobot().GetBatteryComponent().IsOnChargerPlatform() && !kIgnoreAnimWhitelist ) {
        // default here is now to LOCK the body track, but first check the whitelist

        auto* dataLoader = GetRobot().GetContext()->GetDataLoader();
        const bool onWhitelist = dataLoader->IsAnimationAllowedToMoveBodyOnCharger(_animName);
        if( !onWhitelist ) {

          // time to lock the body track. Unfortunately, the action has already been Init'd, so it's tracks
          // are already locked. Therefore we have to manually lock the body to make this work
          GetRobot().GetMoveComponent().LockTracks( (u8) AnimTrackFlag::BODY_TRACK,
                                                        kManualBodyTrackLockName,
                                                        "PlayAnimationAction.LockBodyOnCharger" );
          _bodyTrackManuallyLocked = true;

          LOG_DEBUG("PlayAnimationAction.LockingBodyOnCharger",
                    "anim '%s' is not in the whitelist, locking the body track",
                    _animName.c_str());
        }
      }
    }
  
    ActionResult PlayAnimationAction::Init()
    {

      InitTrackLockingForCharger();

      _stoppedPlaying = false;
      _wasAborted = false;

      auto callback = [this](const AnimationComponent::AnimResult res, u32 streamTimeAnimEnded) {
        _stoppedPlaying = true;
        if (res != AnimationComponent::AnimResult::Completed) {
          _wasAborted = true;
        }
      };

      Result res = GetRobot().GetAnimationComponent().PlayAnimByName(_animName, _numLoopsRemaining,
                                                                     _interruptRunning, callback, GetTag(), 
                                                                     _timeout_sec, _startAtTime_ms, _renderInEyeHue);

      if(res != RESULT_OK) {
        _stoppedPlaying = true;
        _wasAborted = true;
        return ActionResult::ANIM_ABORTED;
      }else if(_passedInCallback != nullptr){
        const bool callEvenIfAnimCanceled = true;
        GetRobot().GetAnimationComponent().AddAdditionalAnimationCallback(_animName, _passedInCallback, callEvenIfAnimCanceled);
      }

      GetRobot().GetComponent<RobotStatsTracker>().IncrementBehaviorStat(BehaviorStat::AnimationPlayed);

      InitSendStats();

      return ActionResult::SUCCESS;
    }

    void PlayAnimationAction::InitSendStats()
    {
      // NOTE: this is overridden most of the time by TriggerAnimationAction
      SendStatsToDasAndWeb(_animName, "", AnimationTrigger::Count);
    }


    void PlayAnimationAction::SendStatsToDasAndWeb(const std::string& animClipName,
                                                   const std::string& animGroupName,
                                                   const AnimationTrigger& animTrigger)
    {
      const auto simpleMood = GetRobot().GetMoodManager().GetSimpleMood();
      const float headAngle_deg = Util::RadToDeg(GetRobot().GetComponent<FullRobotPose>().GetHeadAngle());

      bool isBlacklisted = false;
      std::string animTriggerStr;
      
      auto* dataLoader = GetRobot().GetContext()->GetDataLoader();
      if( animTrigger != AnimationTrigger::Count ) {
        const std::set<AnimationTrigger>& dasBlacklistedTriggers = dataLoader->GetDasBlacklistedAnimationTriggers();
        isBlacklisted = dasBlacklistedTriggers.find(animTrigger) != dasBlacklistedTriggers.end();
        animTriggerStr = AnimationTriggerToString(animTrigger);
      } else {
        const std::set<std::string>& dasBlacklistedNames = dataLoader->GetDasBlacklistedAnimationNames();
        isBlacklisted = dasBlacklistedNames.find(animClipName) != dasBlacklistedNames.end();
      }
      
      if( !isBlacklisted ) {
        // NOTE: you can add events to the blacklist in das_event_config.json to block them from sending
        // here.

        DASMSG(action_play_animation, "action.play_animation",
               "An animation action has been started on the robot (that wasn't blacklisted for DAS)");
        DASMSG_SET(s1, animClipName, "The animation clip name");
        DASMSG_SET(s2, animGroupName, "The animation group name");
        DASMSG_SET(s3, animTriggerStr, "The animation trigger name (may be null)");
        DASMSG_SET(s4, EnumToString(simpleMood), "The current SimpleMood value");
        DASMSG_SET(i1, std::round(headAngle_deg), "The current head angle (in degrees)");
        DASMSG_SEND();
      }


      if( auto webSender = WebService::WebVizSender::CreateWebVizSender("animationengine",
                             GetRobot().GetContext()->GetWebService()) ) {
        webSender->Data()["clip"] = animClipName;
        webSender->Data()["group"] = animGroupName;
        if( animTrigger != AnimationTrigger::Count ) {
          webSender->Data()["trigger"] = AnimationTriggerToString(animTrigger);
        }
        webSender->Data()["mood"] = EnumToString(simpleMood);
        webSender->Data()["headAngle_deg"] = headAngle_deg;
      }
    }

    ActionResult PlayAnimationAction::CheckIfDone()
    {
      if(_stoppedPlaying) {
        return ActionResult::SUCCESS;
      } else if(_wasAborted) {
        return ActionResult::ANIM_ABORTED;
      } else {
        return ActionResult::RUNNING;
      }
    }

    void PlayAnimationAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      AnimationCompleted info;
      info.animationName = _animName;
      completionUnion.Set_animationCompleted(std::move( info ));
    }

    #pragma mark ---- TriggerAnimationAction ----

    TriggerAnimationAction::TriggerAnimationAction(AnimationTrigger animEvent,
                                                   u32 numLoops,
                                                   bool interruptRunning,
                                                   u8 tracksToLock,
                                                   float timeout_sec,
                                                   bool strictCooldown)
    : PlayAnimationAction("", numLoops, interruptRunning, tracksToLock, timeout_sec)
    , _animTrigger(animEvent)
    , _animGroupName("")
    , _strictCooldown(strictCooldown)
    {
      SetName("PlayAnimation" + _animGroupName);
      // will FAILURE_ABORT on Init if not an event
    }

    void TriggerAnimationAction::OnRobotSetInternalAnim()
    {
      SetAnimGroupFromTrigger(_animTrigger);
      OnRobotSetInternalTrigger();
    }
    
    bool TriggerAnimationAction::HasAnimTrigger() const
    {
      return _animTrigger != AnimationTrigger::Count;
    }

    void TriggerAnimationAction::SetAnimGroupFromTrigger(AnimationTrigger animTrigger)
    {
      _animTrigger = animTrigger;

      auto* data_ldr = GetRobot().GetContext()->GetDataLoader();
      if( data_ldr->HasAnimationForTrigger(_animTrigger) )
      {
        _animGroupName = data_ldr->GetAnimationForTrigger(_animTrigger);
        if(_animGroupName.empty()) {
          LOG_WARNING("TriggerAnimationAction.EmptyAnimGroupNameForTrigger",
                      "Event: %s", EnumToString(_animTrigger));
        }
      }

    }

    ActionResult TriggerAnimationAction::Init()
    {
      if(_animGroupName.empty())
      {
        LOG_WARNING("TriggerAnimationAction.NoAnimationForTrigger",
                    "Event: %s", EnumToString(_animTrigger));

        return ActionResult::NO_ANIM_NAME;
      }

      _animName = GetRobot().GetAnimationComponent().GetAnimationNameFromGroup(_animGroupName, _strictCooldown);

      if( _animName.empty() ) {
        return ActionResult::NO_ANIM_NAME;
      }
      else {
        const ActionResult res = PlayAnimationAction::Init();
        return res;
      }
    }

    void TriggerAnimationAction::InitSendStats()
    {
      SendStatsToDasAndWeb(_animName, _animGroupName, _animTrigger);
    }

    #pragma mark ---- PlayAnimationGroupAction ----

    PlayAnimationGroupAction::PlayAnimationGroupAction(const std::string& animGroupName)
      : PlayAnimationAction("")
      , _animGroupName(animGroupName)
    {
      SetName("PlayAnimationGroup");
    }

    ActionResult PlayAnimationGroupAction::Init()
    {
      if(_animGroupName.empty())
      {
        LOG_ERROR("TriggerAnimationAction.NoAnimationGroupSet", "PlayAnimationGroup created with empty group name");
        return ActionResult::NO_ANIM_NAME;
      }

      const bool strictCooldown = false;
      _animName = GetRobot().GetAnimationComponent().GetAnimationNameFromGroup(_animGroupName, strictCooldown);

      if( _animName.empty() ) {
        return ActionResult::NO_ANIM_NAME;
      }
      else {
        const ActionResult res = PlayAnimationAction::Init();
        return res;
      }
    }

    #pragma mark ---- TriggerLiftSafeAnimationAction ----

    TriggerLiftSafeAnimationAction::TriggerLiftSafeAnimationAction(AnimationTrigger animEvent,
                                                                   u32 numLoops,
                                                                   bool interruptRunning,
                                                                   u8 tracksToLock,
                                                                   float timeout_sec,
                                                                   bool strictCooldown)
    : TriggerAnimationAction(animEvent, numLoops, interruptRunning, tracksToLock, timeout_sec, strictCooldown)
    {
    }


    u8 TriggerLiftSafeAnimationAction::TracksToLock(Robot& robot, u8 tracksCurrentlyLocked)
    {

      // Ensure animation doesn't throw cube down, but still can play get down animations
      if(robot.GetCarryingComponent().IsCarryingObject()
         && robot.GetOffTreadsState() == OffTreadsState::OnTreads){
        tracksCurrentlyLocked = tracksCurrentlyLocked | (u8) AnimTrackFlag::LIFT_TRACK;
      }

      return tracksCurrentlyLocked;
    }

    void TriggerLiftSafeAnimationAction::OnRobotSetInternalTrigger()
    {
      SetTracksToLock(TracksToLock(GetRobot(), GetTracksToLock()));
    }
    
    #pragma mark ---- ReselectingLoopAnimationAction ----

    ReselectingLoopAnimationAction::ReselectingLoopAnimationAction(AnimationTrigger animEvent,
                                                                   u32 numLoops,
                                                                   bool interruptRunning,
                                                                   u8 tracksToLock,
                                                                   float timeout_sec,
                                                                   bool strictCooldown)
      : IAction(GetDebugString(animEvent),
                RobotActionType::RESELECTING_LOOP_ANIMATION,
                tracksToLock)
      , _numLoops( numLoops )
      , _loopForever( 0 == numLoops )
      , _numLoopsRemaining( numLoops )
      , _completeImmediately( false )
    {
      _animParams.animEvent = animEvent;
      _animParams.interruptRunning = interruptRunning;
      _animParams.timeout_sec = timeout_sec;
      _animParams.strictCooldown = strictCooldown;
      
      constexpr f32 defaultTimeout_s = PlayAnimationAction::GetDefaultTimeoutInSeconds();
      if( (numLoops == 0) && (_animParams.timeout_sec == defaultTimeout_s) ) {
        _animParams.timeout_sec = PlayAnimationAction::GetInfiniteTimeoutInSeconds();
      }
    }
    
    std::string ReselectingLoopAnimationAction::GetDebugString(const AnimationTrigger& trigger)
    {
      return std::string{"ReselectingLoopAnimationAction"} + AnimationTriggerToString(trigger);
    }
    
    ReselectingLoopAnimationAction::~ReselectingLoopAnimationAction() {
      if( _subAction != nullptr ) {
        _subAction->PrepForCompletion();
      }
    }
    
    void ReselectingLoopAnimationAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
    {
      if( _subAction != nullptr ) {
        _subAction->GetCompletionUnion( completionUnion );
      }
    }

    ActionResult ReselectingLoopAnimationAction::Init() {
      ResetSubAction();
      _numLoopsRemaining = _numLoops;
      _loopForever = (0 == _numLoops);
      return ActionResult::SUCCESS;
    }
    
    void ReselectingLoopAnimationAction::ResetSubAction() {
      if( _subAction != nullptr ) {
        _subAction->PrepForCompletion();
      }
      _subAction.reset( new TriggerLiftSafeAnimationAction{_animParams.animEvent,
                                                           1, // only one loop here!
                                                           _animParams.interruptRunning,
                                                           // track locking is done by this action, don't double-lock
                                                           (u8)AnimTrackFlag::NO_TRACKS,
                                                           _animParams.timeout_sec,
                                                           _animParams.strictCooldown} );
      _subAction->SetRenderInEyeHue(_renderInEyeHue);
      _subAction->SetRobot( &GetRobot() );
    }
    
    ActionResult ReselectingLoopAnimationAction::CheckIfDone()
    {
      if( _subAction == nullptr ) {
        return ActionResult::NULL_SUBACTION;
      }
      if( _completeImmediately ) {
        return ActionResult::SUCCESS;
      }
      
      ActionResult subActionResult = _subAction->Update();
      const ActionResultCategory category = IActionRunner::GetActionResultCategory(subActionResult);
      
      if( (category == ActionResultCategory::SUCCESS)
          && (_loopForever || (--_numLoopsRemaining > 0)) )
      {
        ResetSubAction();
        return ActionResult::RUNNING;
      } else {
        return subActionResult;
      }
    }
    
    void ReselectingLoopAnimationAction::StopAfterNextLoop()
    {
      if( !HasStarted() )
      {
        // StopAfterNextLoop() was called before Init(). Set a flag to stop on the first call to
        // CheckIfDone(), since the other flags (_numLoopsRemaining, etc) get set during Init().
        _completeImmediately = true;
        LOG_INFO("ReselectingLoopAnimationAction.StopAfterNextLoop.NotStarted",
                 "Action was told to StopAfterNextLoop, but hasn't started, so will end before the first loop");
      }
      
      _numLoopsRemaining = 1;
      _loopForever = false;
    }

    #pragma mark ---- LoopAnimWhileAction ----
    
    LoopAnimWhileAction::LoopAnimWhileAction(IActionRunner* primaryAction,
                                             const AnimationTrigger loopAnim,
                                             const float maxWaitTime_sec)
      : CompoundActionParallel()
      , _maxWaitTime_sec(maxWaitTime_sec)
    {
      _primaryAction = AddAction(primaryAction);
      _animAction = AddAction(new ReselectingLoopAnimationAction(loopAnim));
    }
    
    Result LoopAnimWhileAction::UpdateDerived()
    {
      const auto now_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
      
      const bool primaryActionComplete = _primaryAction.expired();
      if (primaryActionComplete && _timePrimaryActionCompleted < 0.f) {
        // Primary action just completed
        _timePrimaryActionCompleted = now_sec;
        
        if (auto ptr = _animAction.lock()) {
          auto animActionPtr = std::static_pointer_cast<ReselectingLoopAnimationAction>(ptr);
          if (animActionPtr != nullptr) {
            animActionPtr->StopAfterNextLoop();
          }
        }
      }
      
      // Check for max wait timeout
      const bool hasMaxWaitTime = (_maxWaitTime_sec >= 0.f);
      if (primaryActionComplete &&
          hasMaxWaitTime &&
          (now_sec - _timePrimaryActionCompleted) > _maxWaitTime_sec) {
        LOG_WARNING("LoopAnimWhileAction.UpdateDerived.MaxWaitTimeExceeded",
                    "The primary action has completed, and we have been waiting for the animation to complete for too "
                    "long, so cancelling the action (maxWaitTime %.2f sec)",
                    _maxWaitTime_sec);
        return RESULT_FAIL;
      }
      
      return RESULT_OK;
    }

  }
}
