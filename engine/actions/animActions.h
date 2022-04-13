/**
 * File: animActions.h
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements animation and audio cozmo-specific actions, derived from the IAction interface.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_ANIM_ACTIONS_H
#define ANKI_COZMO_ANIM_ACTIONS_H

#include "engine/actions/actionInterface.h"
#include "engine/actions/compoundActions.h"
#include "engine/components/animationComponent.h"
#include "coretech/common/engine/math/pose.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "clad/externalInterface/messageActions.h"
#include "clad/types/actionTypes.h"
#include "clad/types/animationTypes.h"


namespace Anki {
  
  namespace Vector {
    
    enum class AnimationTrigger : int32_t;

    class PlayAnimationAction : public IAction
    {
    public:
    
      // Numloops 0 causes the action to loop forever
      // tracksToLock indicates tracks of the animation which should not play
      PlayAnimationAction(const std::string& animName,
                          u32 numLoops = 1,
                          bool interruptRunning = true,
                          u8 tracksToLock = (u8)AnimTrackFlag::NO_TRACKS,
                          float timeout_sec = _kDefaultTimeout_sec,
                          TimeStamp_t startAtTime_ms = 0,
                          // this callback will contain the time the animation ended
                          AnimationComponent::AnimationCompleteCallback callback = {});
      
      virtual ~PlayAnimationAction();
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
      
      virtual f32 GetTimeoutInSeconds() const override { return _timeout_sec; }
      
      virtual void SetRenderInEyeHue(bool renderInEyeHue) { _renderInEyeHue = renderInEyeHue; }

      static constexpr f32 GetDefaultTimeoutInSeconds() { return _kDefaultTimeout_sec; }
      static constexpr f32 GetInfiniteTimeoutInSeconds() { return _kDefaultTimeoutForInfiniteLoops_sec; }
      
    protected:
      
      virtual ActionResult Init() override;
      virtual ActionResult CheckIfDone() override;

      virtual void OnRobotSet() override final;
      virtual void OnRobotSetInternalAnim() {};

      // called at the end of Init, can (and should) be overridden to log additional stats to DAS / webviz
      virtual void InitSendStats();

      // helper that can be called from InitSendStats to send stats with the specified information. May use
      // GetRobot() to add robot info. Note that this will _only_ send to DAS if a trigger name is specified
      void SendStatsToDasAndWeb(const std::string& animClipName,
                                const std::string& animGroupName,
                                const AnimationTrigger& animTrigger);


      std::string               _animName;
      u32                       _numLoopsRemaining;
      bool                      _stoppedPlaying;
      bool                      _wasAborted;
      bool                      _interruptRunning;
      float                     _timeout_sec = _kDefaultTimeout_sec;
      bool                      _bodyTrackManuallyLocked = false;
      TimeStamp_t               _startAtTime_ms = 0;
      bool                      _renderInEyeHue = true;
      AnimationComponent::AnimationCompleteCallback _passedInCallback = nullptr;
      
      static constexpr float _kDefaultTimeout_sec = 60.f;
      static constexpr float _kDefaultTimeoutForInfiniteLoops_sec = std::numeric_limits<f32>::max();

    private:

      void InitTrackLockingForCharger();

    }; // class PlayAnimationAction


    class TriggerAnimationAction : public PlayAnimationAction
    {
    public:
      // Preferred constructor, used by the factory CreatePlayAnimationAction
      // Numloops 0 causes the action to loop forever
      explicit TriggerAnimationAction(AnimationTrigger animEvent,
                                      u32 numLoops = 1,
                                      bool interruptRunning = true,
                                      u8 tracksToLock = (u8)AnimTrackFlag::NO_TRACKS,
                                      float timeout_sec = _kDefaultTimeout_sec,
                                      bool strictCooldown = false);
      
    protected:
      virtual ActionResult Init() override;

      void SetAnimGroupFromTrigger(AnimationTrigger animTrigger);

      bool HasAnimTrigger() const;
      virtual void OnRobotSetInternalAnim() override final;
      virtual void OnRobotSetInternalTrigger() {};

      virtual void InitSendStats() override;


    private:
      AnimationTrigger _animTrigger;
      std::string _animGroupName;
      bool _strictCooldown;
      
    }; // class TriggerAnimationAction

    // Directly call an animation group. Note that this should almost never be used from code, instead you
    // should use the animation trigger so that it can be mapped properly to a group. However, this may be
    // useful for dev tools or data-driven use cases where the animation group is provided in data
    class PlayAnimationGroupAction : public PlayAnimationAction
    {
    public:
      explicit PlayAnimationGroupAction(const std::string& animGroupName);

    protected:
      virtual ActionResult Init() override;

    private:
      std::string _animGroupName;
    };
    
    // A special subclass of TriggerAnimationAction which checks to see
    // if the robot is holding a cube and locks the tracks
    class TriggerLiftSafeAnimationAction : public TriggerAnimationAction
    {
    public:
      // Preferred constructor, used by the factory CreatePlayAnimationAction
      // Numloops 0 causes the action to loop forever
      explicit TriggerLiftSafeAnimationAction(AnimationTrigger animEvent,
                                              u32 numLoops = 1,
                                              bool interruptRunning = true,
                                              u8 tracksToLock = (u8)AnimTrackFlag::NO_TRACKS,
                                              float timeout_sec = _kDefaultTimeout_sec,
                                              bool strictCooldown = false);
      static u8 TracksToLock(Robot& robot, u8 tracksCurrentlyLocked);
    protected:
        virtual void OnRobotSetInternalTrigger() override final;
      
    };
    
    #pragma mark ---- ReselectingLoopAnimationAction ----
    // Repeatedly creates and plays TriggerLiftSafeAnimationAction numLoops times. This is different
    // than using a TriggerLiftSafeAnimationAction with the param numLoops, since that will select
    // one animation from the anim group at Init and loop it, whereas this reselects the
    // animation each loop.
    class ReselectingLoopAnimationAction : public IAction
    {
    public:
      ReselectingLoopAnimationAction(AnimationTrigger animEvent,
                                     u32 numLoops = 0, // default is loop forever
                                     bool interruptRunning = true,
                                     u8 tracksToLock = (u8)AnimTrackFlag::NO_TRACKS,
                                     float timeout_sec = PlayAnimationAction::GetDefaultTimeoutInSeconds(),
                                     bool strictCooldown = false);
      
      virtual ~ReselectingLoopAnimationAction();
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
      
      // once called, the action will end as soon as the current loop finishes, and Init() must be called to reset
      void StopAfterNextLoop();
      
      void SetRenderInEyeHue(bool renderInEyeHue) { _renderInEyeHue = renderInEyeHue; }
      
    protected:
      
      virtual ActionResult Init() override;
      
      virtual ActionResult CheckIfDone() override;
      
      virtual f32 GetTimeoutInSeconds() const override { return _animParams.timeout_sec; }
      
    private:
      
      void ResetSubAction();
      
      static std::string GetDebugString(const AnimationTrigger& trigger);
      
      struct AnimParams {
        AnimationTrigger animEvent;
        bool interruptRunning;
        float timeout_sec;
        bool strictCooldown;
      };
      
      AnimParams _animParams;
      u32        _numLoops;
      bool       _loopForever;
      u32        _numLoopsRemaining;
      bool       _completeImmediately;
      std::unique_ptr<TriggerLiftSafeAnimationAction> _subAction;
      bool       _renderInEyeHue = true;
      
    };
    
    #pragma mark ---- LoopAnimWhileAction ----
    // Loops the given animation while doing the 'primary' action in parallel. Once the primary action completes, we
    // wait for either the current animation loop to finish or maxWaitTime_sec (whichever comes first).
    class LoopAnimWhileAction : public CompoundActionParallel
    {
    public:
      LoopAnimWhileAction(IActionRunner* primaryAction,
                          const AnimationTrigger loopAnim,
                          const float maxWaitTime_sec = -1.f);
      
      virtual ~LoopAnimWhileAction() {};
      
      virtual Result UpdateDerived() override;
      
      // Once the primary action completes, this is the maximum amount of time we will wait for the current animation
      // loop to finish before bailing. A negative value disables this check.
      void SetMaxWaitTime_sec(const float maxWaitTime_sec);
      
    private:
      std::weak_ptr<IActionRunner> _primaryAction;
      std::weak_ptr<IActionRunner> _animAction;
      
      float _timePrimaryActionCompleted = -1.f;
      float _maxWaitTime_sec = -1.f;
    };

  }
}

#endif /* ANKI_COZMO_ANIM_ACTIONS_H */
