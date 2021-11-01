/**
 * File: retryWrapperAction.h
 *
 * Author: Al Chaussee
 * Date:   7/21/16
 *
 * Description: A wrapper action for handling retrying an action and playing retry animations
 *
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_RETRY_WRAPPER_ACTION_H
#define ANKI_COZMO_RETRY_WRAPPER_ACTION_H


#include "clad/externalInterface/messageEngineToGameTag.h"

namespace Anki {
namespace Vector {
  
  enum class AnimationTrigger : int32_t;
  class IAction;
  class IActionRunner;
  class ICompoundAction;
  class PlayAnimationAction;
  namespace ExternalInterface {
    struct RobotCompletedAction;
  }
  
  class RetryWrapperAction : public IAction
  {
  public:
    // The callback should return whether or not the action should be retried
    // as well as setting retryAnimTrigger to the appropriate animation trigger based on
    // the result code of the RobotCompeltedAction message
    // The callback will be called for all possible failure results (ActionResults starting with FAILURE_*)
    using RetryCallback = std::function<bool(const ExternalInterface::RobotCompletedAction&,
                                             const u8 retryCount,
                                             AnimationTrigger& retryAnimTrigger)>;
    
    // Provide this wrapper action an action to retry, a callback to call when the action is going to be retried
    // and the number of times to retry the action
    RetryWrapperAction(IAction* action, RetryCallback retryCallback, u8 numRetries);
    RetryWrapperAction(ICompoundAction* action, RetryCallback retryCallback, u8 numRetries);

    // alternatively, simply pass in an animation trigger to play (and always retry until the limit)
    RetryWrapperAction(IAction* action, AnimationTrigger retryTrigger, u8 numRetries);
    RetryWrapperAction(ICompoundAction* action, AnimationTrigger retryTrigger, u8 numRetries);
    
    virtual ~RetryWrapperAction();
    
  protected:
    virtual ActionResult Init() override;
    virtual ActionResult CheckIfDone() override;
    
    virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;
    virtual f32 GetTimeoutInSeconds() const override;

    virtual void OnRobotSet() override final;
    
  private:
    std::unique_ptr<IActionRunner>       _subAction       = nullptr;
    std::unique_ptr<PlayAnimationAction> _animationAction = nullptr;
    RetryCallback        _retryCallback;
    const u8             _numRetries;
    u8                   _retryCount = 0;
    
  };
  
}
}

#endif // ANKI_COZMO_RETRY_WRAPPER_ACTION_H
