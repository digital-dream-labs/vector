/**
 * File: retryWrapperAction.cpp
 *
 * Author: Al Chaussee
 * Date:   7/21/16
 *
 * Description: A wrapper action for handling retrying an action and playing retry animations
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/actions/actionInterface.h"
#include "engine/actions/actionWatcher.h"
#include "engine/actions/animActions.h"
#include "engine/actions/compoundActions.h"
#include "engine/actions/retryWrapperAction.h"
#include "engine/robot.h"

#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {
  
  RetryWrapperAction::RetryWrapperAction(IAction* action,
                                         RetryCallback retryCallback,
                                         u8 numRetries)
  : IAction("RetryWrapper",
            RobotActionType::UNKNOWN,
            (u8)AnimTrackFlag::NO_TRACKS)
  , _subAction(action)
  , _retryCallback(retryCallback)
  , _numRetries(numRetries)
  {
    if(action == nullptr)
    {
      PRINT_NAMED_WARNING("RetryWrapperAction.Constructor.NullArg_0", "");
      return;
    }
    
    SetType(action->GetType());
    SetName("Retry["+action->GetName()+"]");
  }
  
  RetryWrapperAction::RetryWrapperAction(ICompoundAction* action,
                                         RetryCallback retryCallback,
                                         u8 numRetries)
  : IAction("RetryWrapper",
            RobotActionType::UNKNOWN,
            (u8)AnimTrackFlag::NO_TRACKS)
  , _subAction(action)
  , _retryCallback(retryCallback)
  , _numRetries(numRetries)
  {
    if(action == nullptr)
    {
      PRINT_NAMED_WARNING("RetryWrapperAction.Constructor.NullArg_1", "");
      return;
    }
    
    // Don't delete actions from the compound action on completion so that they can be retried
    action->SetDeleteActionOnCompletion(false);
    
    SetType(action->GetType());
    SetName("Retry["+action->GetName()+"]");
  }

  RetryWrapperAction::RetryWrapperAction(IAction* action, AnimationTrigger retryTrigger, u8 numRetries)
    : RetryWrapperAction(action, RetryCallback{}, numRetries)
  {
    _retryCallback = [retryTrigger](const ExternalInterface::RobotCompletedAction&,
                                    const u8 retryCount,
                                    AnimationTrigger& retryAnimTrigger) {
      retryAnimTrigger = retryTrigger;
      return true;
    };
  }

  RetryWrapperAction::RetryWrapperAction(ICompoundAction* action,
                                         AnimationTrigger retryTrigger,
                                         u8 numRetries)
    : RetryWrapperAction(action, RetryCallback{}, numRetries)
  {
    _retryCallback = [retryTrigger](const ExternalInterface::RobotCompletedAction&,
                                    const u8 retryCount,
                                    AnimationTrigger& retryAnimTrigger) {
      retryAnimTrigger = retryTrigger;
      return true;
    };
  }
  
  RetryWrapperAction::~RetryWrapperAction()
  {
    if(_subAction != nullptr)
    {
      _subAction->PrepForCompletion();
    }
    
    if(_animationAction != nullptr)
    {
      _animationAction->PrepForCompletion();
    }
  }

  void RetryWrapperAction::OnRobotSet()
  {
    if(_subAction != nullptr){
      _subAction->SetRobot(&GetRobot());
    }
  }
  
  f32 RetryWrapperAction::GetTimeoutInSeconds() const
  {
    // Add 1 to account for the initial run
    return (_numRetries+1) * 20.f;
  }
  
  ActionResult RetryWrapperAction::Init()
  {
    if(_subAction == nullptr)
    {
      return ActionResult::NULL_SUBACTION;
    }
    return ActionResult::SUCCESS;
  }
  
  ActionResult RetryWrapperAction::CheckIfDone()
  {
    // Animation action is null so run the subAction
    if(_animationAction == nullptr)
    {
      const ActionResult res = _subAction->Update();
      
      // Update the retryWrapperAction's type to match the subAction's type in case
      // it is changing at runtime
      SetType(_subAction->GetType());
      
      // Only attempt to retry on failure results
      // TODO Could be updated to use ActionResultCategory
      if(res != ActionResult::RUNNING &&
         res != ActionResult::SUCCESS &&
         res != ActionResult::CANCELLED_WHILE_RUNNING &&
         res != ActionResult::INTERRUPTED)
      {
        ActionCompletedUnion completionUnion;
        _subAction->GetCompletionUnion(completionUnion);
        
        std::vector<ActionResult> subActionResults;
        GetRobot().GetActionList().GetActionWatcher().GetSubActionResults(_subAction->GetTag(), subActionResults);
        
        using RCA = ExternalInterface::RobotCompletedAction;
        RCA robotCompletedAction = RCA(_subAction->GetTag(),
                                       _subAction->GetType(),
                                       _subAction->GetState(),
                                       subActionResults,
                                       completionUnion);
        
        // Retry callback should NOT modify things that would be reset by reset (ie action's state)
        LOG_DEBUG("RetryWrapperAction.CheckIfDone.CallingRetryCallback", "");
        AnimationTrigger animTrigger = AnimationTrigger::Count;
        bool shouldRetry = _retryCallback(robotCompletedAction, _retryCount, animTrigger);
        
        // If the action shouldn't retry return whatever its update returned
        if(!shouldRetry)
        {
          return res;
        }
        
        // If the animationTrigger to play is Count (indicates None/No animation) check retry count
        // and don't new the animation action
        if(animTrigger == AnimationTrigger::Count)
        {
          LOG_DEBUG("RetryWrapperAction.CheckIfDone.NoAnimation",
                    "RetryCallback returned AnimationTrigger::Count so not playing animation");
          if(_retryCount++ >= _numRetries)
          {
            LOG_INFO("RetryWrapperAction.CheckIfDone.MaxRetriesReached","");
            return res;
          }
          // Reset the subaction and unlock the tracks locked by the subaction.
          _subAction->Reset(true);
          return ActionResult::RUNNING;
        }
        else
        {
          LOG_DEBUG("RetryWrapperAction.CheckIfDone.Animation",
                    "Resetting subaction and unlocking tracks");
          // Reset the subaction again, and unlock the tracks locked by the subaction.
          _subAction->Reset(true);
          _animationAction.reset(new TriggerLiftSafeAnimationAction(animTrigger));
          _animationAction->SetRobot(&GetRobot());
        }
      }
      else
      {
        return res;
      }
    }
    
    // Animation action not null so run it
    // Note: Doing an if here instead of an else so we can "fallthrough" to here if we just
    // newed the _animationAction
    if(_animationAction != nullptr)
    {
      ActionResult res = _animationAction->Update();
      if(res != ActionResult::RUNNING)
      {
        LOG_DEBUG("RetryWrapperAction.CheckIfDone.RetryAnimFinished", "");
        _animationAction->PrepForCompletion();
        _animationAction.reset();
        
        // Check retry count here so that if we end up reaching our max number of retries
        // this action ends when the animation does
        if(_retryCount++ >= _numRetries)
        {
          LOG_INFO("RetryWrapperAction.CheckIfDone.MaxAnimRetriesReached","");
          return res;
        }
      }
      return ActionResult::RUNNING;
    }
    
    // Should not be possible to get here
    PRINT_NAMED_WARNING("RetryWrapperAction.CheckIfDone.ReachedUnreachableCode", "");
    return ActionResult::ABORT;
  }
  
  void RetryWrapperAction::GetCompletionUnion(ActionCompletedUnion& completionUnion) const
  {
    if(_subAction != nullptr)
    {
      _subAction->GetCompletionUnion(completionUnion);
    }
  }
  
}
}
