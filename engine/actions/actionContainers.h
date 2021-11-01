/**
 * File: actionContainers.h
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Defines containers for running actions, both as a queue and a
 *              concurrent list.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_ACTION_CONTAINERS_H
#define ANKI_COZMO_ACTION_CONTAINERS_H

#include "coretech/common/shared/types.h"
#include "engine/actions/actionDefinitions.h"
#include "util/entityComponent/iDependencyManagedComponent.h"
#include "engine/robotComponents_fwd.h"

#include "clad/types/actionTypes.h"

#include <list>
#include <map>
#include <set>

// TODO: Is this Cozmo-specific or can it be moved to coretech?
// (Note it does require a Robot, which is currently only present in Cozmo)

namespace Anki {

  namespace Vector {

    // Forward declarations:
    class Robot;
    class IActionRunner;
    class ActionWatcher;

    // This is an ordered list of actions to be run. It is similar to an
    // CompoundActionSequential, but actions can be added to it dynamically,
    // either "next" or at the end of the queue. As actions are completed,
    // they are popped off the queue. Thus, when it is empty, it is "done".
    class ActionQueue
    {
    public:
      ActionQueue(Robot& robot);

      ~ActionQueue();

      Result   Update();

      // Queue action to run right after the current action, before anything else in the queue
      Result   QueueNext(IActionRunner    *action, u8 numRetries = 0);

      // Queue action to run after everything else currently in the queue
      Result   QueueAtEnd(IActionRunner   *action, u8 numRetires = 0);

      // Cancel the current action and immediately run the new action, preserving rest of queue
      Result   QueueNow(IActionRunner     *action, u8 numRetries = 0);

      // Stop current action and reset it, insert new action at the front, leaving
      // current action in the queue to run fresh next (after this newly-inserted action)
      Result   QueueAtFront(IActionRunner *action, u8 numRetries = 0);

      // Blindly clear the queue
      void     Clear();

      bool     Cancel(RobotActionType withType = RobotActionType::UNKNOWN);
      bool     Cancel(u32 idTag);

      bool     IsEmpty() const { return _queue.empty() && nullptr == _currentAction; }

      bool     IsDuplicate(IActionRunner* action);

      size_t   Length() const { return _queue.size(); }

      IActionRunner* GetNextActionToRun();
      const IActionRunner* GetCurrentAction() const;
      const IActionRunner* GetCurrentRunningAction() const { return _currentAction; }

      // Deletes the action only if it isn't in the process of being deleted
      // Safeguards against the action being deleted multiple times due to handling action
      // completion signals
      // Returns true if this call actually deleted the action, false if the action is already in the process
      // of being deleted
      bool DeleteAction(IActionRunner* &action);

      void Print() const;

      typedef std::list<IActionRunner*>::const_iterator const_iterator;
      const_iterator begin() const { return _queue.begin(); }
      const_iterator end()   const { return _queue.end();   }

    private:
      // Deletes the action only if it isn't in the process of being deleted
      // If iter is not the end of the queue, the iter will be removed from the queue (iter should always point to the action)
      bool DeleteActionIter(std::list<IActionRunner*>::iterator& iter);

      bool DeleteActionAndIter(IActionRunner* &action, std::list<IActionRunner*>::iterator& iter);

      // Reference to robot so that actions queues receive a robot to inject into actions
      Robot& _robot;

      IActionRunner*            _currentAction = nullptr;
      std::list<IActionRunner*> _queue;

      // A set of tags that are in the process of being deleted. This is used to protect
      // actions from being deleted multiple times/recursively
      std::set<u32>             _tagsBeingDeleted;

      // Whether or not the queue is currently being cleared
      bool _currentlyClearing = false;

    }; // class ActionQueue


    // This is a list of concurrent actions to be run, addressable by ID handle.
    // Each slot in the list is really a queue, to which new actions can be added
    // using that slot's ID handle. When a slot finishes, it is popped.
    class ActionList : public IDependencyManagedComponent<RobotComponentID>
    {
    public:
      using SlotHandle = s32;

      static const SlotHandle UnknownSlot = -1;

      ActionList();
      ~ActionList();

      //////
      // IDependencyManagedComponent functions
      /////

      virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override;
      virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {};
      virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {
        dependencies.insert(RobotComponentID::AIComponent);
      };
      virtual void UpdateDependent(const RobotCompMap& dependentComps) override;
      //////
      // end IDependencyManagedComponent functions
      /////



      // Add a new action to be run concurrently, generating a new slot, whose
      // handle is returned. If there is no desire to queue anything to run after
      // this action, the returned SlotHandle can be ignored.
      SlotHandle AddConcurrentAction(IActionRunner* action, u8 numRetries = 0);

      // Queue an action.  This function will take ownership over the memory pointed
      // to by action, regardless of whether it succeeds or fails.
      Result     QueueAction(QueueActionPosition inPosition,
                             IActionRunner* action, u8 numRetries = 0);

      bool       IsEmpty() const;

      const ActionQueue& GetQueue(SlotHandle atSlot) const { return _queues.at(atSlot); }

      size_t     GetQueueLength(SlotHandle atSlot) const;

      size_t     GetNumQueues() const;

      // Only cancels with the specified type. All slots are searched.
      // Returns true if any actions were cancelled.
      bool       Cancel(RobotActionType withType = RobotActionType::UNKNOWN);

      // Find and cancel the action with the specified ID Tag. All slots are searched.
      // Returns true if the action was found and cancelled.
      bool       Cancel(u32 idTag);

      void       Print() const;

      // Returns true if actionName is the name of one of the actions that are currently
      // being executed.
      bool       IsCurrAction(const std::string& actionName) const;

      // Returns true if the passed in action tag matches the action currently playing in the given slot
      bool       IsCurrAction(u32 idTag, SlotHandle fromSlot = 0) const;

      // If we are currently clearing the action will be destroyed and will return true
      // Otherwise will return if the action is a duplicate
      bool       IsDuplicateOrCurrentlyClearing(IActionRunner* action);

      // Blindly clears out the contents of the action list
      void       Clear();

      // Returns true if the action has a game or sdk tag
      bool       IsExternalAction(const IActionRunner* action);

      // NOTE: Currently these callback functions (below) are wrappers for the ActionWatcher functions, but
      // eventually they may be implemented here (see COZMO-11465), so prefer using these

      // Register a callback. This callback will be called when _any_ action ends (including sub-actions or actions
      // inside compound actions). It will be called after the action completed message is broadcast, and after
      // the action itself has been fully deleted. Returns a unique integer id for the callback (so it can later
      // be removed).
      ActionEndedCallbackID RegisterActionEndedCallbackForAllActions(ActionEndedCallback callback);

      // Remove a registered callback. Returns true if the callback was found, false otherwise
      bool UnregisterCallback(ActionEndedCallbackID callbackID);

      ActionWatcher& GetActionWatcher() { return *_actionWatcher.get(); }

      typedef std::map<SlotHandle, ActionQueue>::const_iterator const_iterator;
      const_iterator begin() const { return _queues.begin(); }
      const_iterator end()   const { return _queues.end();   }

    protected:
      // Queue an action
      // These wrap corresponding QueueFoo() methods in ActionQueue.
      Result     QueueActionNext(IActionRunner* action, u8 numRetries = 0);
      Result     QueueActionAtEnd(IActionRunner* action, u8 numRetries = 0);
      Result     QueueActionNow(IActionRunner* action, u8 numRetries = 0);
      Result     QueueActionAtFront(IActionRunner* action, u8 numRetries = 0);

      std::map<SlotHandle, ActionQueue> _queues;

    private:
      // Reference to robot so that actions queues receive a robot to inject into actions
      Robot* _robot = nullptr;

      // Whether or not the queues are in the process of being cleared
      bool _currentlyClearing = false;

      std::unique_ptr<ActionWatcher> _actionWatcher;

      ActionQueue& GetActionQueueForSlot(SlotHandle handle);

    }; // class ActionList

    // The current running action should be considered part of the queue
    inline size_t ActionList::GetQueueLength(SlotHandle atSlot) const
    {
      size_t length = 0;
      auto iter = _queues.find(atSlot);
      if(iter != _queues.end()) {
        length = iter->second.Length() + (nullptr == iter->second.GetCurrentRunningAction() ? 0 : 1);
      }
      return length;
    }

    inline size_t ActionList::GetNumQueues() const
    {
      return _queues.size();
    }

  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_ACTION_CONTAINERS_H
