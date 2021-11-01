/**
 * File: compoundActions.h
 *
 * Author: Andrew Stein
 * Date:   7/9/2014
 *
 * Description: Defines compound actions, which are groups of IActions to be
 *              run together in series or in parallel.
 *
 *              Note about inheriting from CompoundActions
 *              If you are storing pointers to actions in the compound action
 *              store them as weak_ptrs returned from AddAction. Once an action
 *              is added to a compound action, the compound action completely manages
 *              the action including deleting it
 *              (see IDriveToInteractWithObject for examples)
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_COMPOUND_ACTIONS_H
#define ANKI_COZMO_COMPOUND_ACTIONS_H

#include "engine/actions/actionInterface.h"

#include <map>

namespace Anki {
  namespace Vector {
    
    // Interface for compound actions, which are fixed sets of actions to be
    // run together or in order (determined by derived type)
    class ICompoundAction : public IActionRunner
    {
    public:
      ICompoundAction(const std::list<IActionRunner*>& actions);
      
      // Adds an action to this compound action. Completely hands ownership and memory management
      // of the action over to this compoundAction
      // Internally creates a shared_ptr and will return a weak_ptr to it should the caller
      // want to do something with the action at a later time
      using ShouldIgnoreFailureFcn = std::function<bool(ActionResult, const IActionRunner*)>;
      virtual std::weak_ptr<IActionRunner> AddAction(IActionRunner* action,
                                                     ShouldIgnoreFailureFcn fcn,
                                                     bool emitCompletionSignal = false);
      virtual std::weak_ptr<IActionRunner> AddAction(IActionRunner* action,
                                                     bool ignoreFailure = false,
                                                     bool emitCompletionSignal = false);
      
      // First calls cleanup on any constituent actions and then removes them
      // from this compound action completely.
      void ClearActions();
      
      const std::list<std::shared_ptr<IActionRunner>>& GetActionList() const { return _actions; }
      
      // Constituent actions will be deleted upon destruction of the group
      virtual ~ICompoundAction();
      
      virtual void GetCompletionUnion(ActionCompletedUnion& completionUnion) const override;

      // The proxy action, if set, is the one whose type and completion info are used.
      // Specify it by the constituent action's tag.
      void SetProxyTag(u32 tag);
      
      // Sets whether or not to delete actions from the compound action when they complete
      // By default actions will be destroyed on completion
      void SetDeleteActionOnCompletion(bool deleteOnCompletion);
      
      // Return the number of constituent actions in this compound action
      size_t GetNumActions() const { return _actions.size(); }

    protected:
      
      // Call the constituent actions' Reset() methods and mark them each not done.
      virtual void Reset(bool shouldUnlockTracks = true) override;
      
      // The list of actions in this compound action stored as shared_ptrs
      std::list<std::shared_ptr<IActionRunner>> _actions;
      
      bool ShouldIgnoreFailure(ActionResult result, const std::shared_ptr<IActionRunner>& action) const;
      
      // Stack of pairs of actionCompletionUnions and actionTypes of the already completed actions
      struct CompletionData {
        ActionCompletedUnion  completionUnion;
        RobotActionType       type;
      };
      
      // Map of action tag to completion data
      std::map<u32, CompletionData> _completedActionInfoStack;
      
      // NOTE: Moves currentAction iterator to next action after deleting
      void StoreUnionAndDelete(std::list<std::shared_ptr<IActionRunner>>::iterator& currentAction);

      virtual void OnRobotSet() override final;
      virtual void OnRobotSetInternalCompound() {};
      
    private:
      
      // If actions are in this list, we ignore their failures
      std::map<const IActionRunner*, ShouldIgnoreFailureFcn> _ignoreFailure;
      u32  _proxyTag;
      bool _proxySet = false;
      
      bool _deleteActionOnCompletion = true;
      
      void DeleteActions();
    };
    
    
    // Executes a fixed set of actions sequentially
    class CompoundActionSequential : public ICompoundAction
    {
    public:
      CompoundActionSequential();
      CompoundActionSequential(const std::list<IActionRunner*>& actions);
      
      // Add a delay, in seconds, between running each action in the group.
      // Default is 0 (no delay).
      void SetDelayBetweenActions(f32 seconds);
      
      // Called at the very beginning of UpdateInternal, so derived classes can
      // do additional work. If this does not return RESULT_OK, then UpdateInternal
      // will return ActionResult::UPDATE_DERIVED_FAILED.
      virtual Result UpdateDerived() { return RESULT_OK; }
      
    private:
      virtual void Reset(bool shouldUnlockTracks = true) override final;
      
      virtual ActionResult UpdateInternal() override final;
      
      ActionResult MoveToNextAction(float currentTime_secs);
      
      f32 _delayBetweenActionsInSeconds;
      f32 _waitUntilTime;
      std::list<std::shared_ptr<IActionRunner>>::iterator _currentAction;
      bool _wasJustReset;
      
    }; // class CompoundActionSequential
    
    inline void CompoundActionSequential::SetDelayBetweenActions(f32 seconds) {
      _delayBetweenActionsInSeconds = seconds;
    }
    
    // Executes a fixed set of actions in parallel
    class CompoundActionParallel : public ICompoundAction
    {
    public:
      CompoundActionParallel();
      CompoundActionParallel(const std::list<IActionRunner*>& actions);
      
      // By default, CompoundActionParallel continues as long as its longest sub-action. Setting this
      // to false will end the CompoundActionParallel the moment any of its sub-actions end
      void SetShouldEndWhenFirstActionCompletes(bool shouldEnd) { _endWhenFirstActionCompletes = shouldEnd; }
      
      // Called at the very beginning of UpdateInternal, so derived classes can
      // do additional work. If this does not return RESULT_OK, then UpdateInternal
      // will return ActionResult::UPDATE_DERIVED_FAILED.
      virtual Result UpdateDerived() { return RESULT_OK; }
      
    protected:
      
      virtual ActionResult UpdateInternal() override final;
    private:
      bool _endWhenFirstActionCompletes = false;
      
    }; // class CompoundActionParallel
    
  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_COMPOUND_ACTIONS_H


