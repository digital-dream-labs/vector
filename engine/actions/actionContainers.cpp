/**
 * File: actionContainers.cpp
 *
 * Author: Andrew Stein
 * Date:   8/29/2014
 *
 * Description: Implements containers for running actions, both as a queue and a
 *              concurrent list.
 *
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/actions/actionContainers.h"

#include "clad/externalInterface/messageEngineToGame.h"
#include "engine/actions/actionInterface.h"
#include "engine/actions/actionWatcher.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalInterface.h"
#include "engine/robot.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "Actions"

namespace Anki {
  namespace Vector {
    
#pragma mark ---- ActionList ----
    
    ActionList::ActionList()
    : IDependencyManagedComponent(this, RobotComponentID::ActionList)
    , _actionWatcher(new ActionWatcher())
    {
    
    }
    
    ActionList::~ActionList()
    {
      Clear();
    }
    
    void ActionList::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) 
    {
      _robot = robot;
    }


    Result ActionList::QueueAction(QueueActionPosition inPosition,
                                   IActionRunner* action, u8 numRetries)
    {
      if(action == nullptr)
      {
        PRINT_NAMED_ERROR("ActionList.QueueAction.NullAction", "Can't queue null action");
        return RESULT_FAIL;
      }
      
      action->SetRobot(_robot);
      
      // If we are ignoring external actions and this is an external action or
      // if this action has a bad tag then delete it
      if((action->GetRobot().GetIgnoreExternalActions() &&
          IsExternalAction(action)) ||
         action->GetState() == ActionResult::BAD_TAG)
      {
        if (action->GetRobot().GetIgnoreExternalActions())
        {
          LOG_INFO("ActionQueue.QueueAction.ExternalActionsDisabled",
                   "Ignoring %s action while external actions are disabled",
                   EnumToString(action->GetType()));
        }
        else
        {
          PRINT_NAMED_ERROR("ActionQueue.QueueAction.ActionHasBadTag",
                            "Failed to set tag, deleting action %s",
                            EnumToString(action->GetType()));
        }
        
      
        GetActionQueueForSlot(0).DeleteAction(action);
        return RESULT_OK;
      }
    
      Result result = RESULT_OK;
      switch(inPosition)
      {
        case QueueActionPosition::NOW:
        {
          result = QueueActionNow(action, numRetries);
          break;
        }
        case QueueActionPosition::NOW_AND_CLEAR_REMAINING:
        {
          // Check before cancelling everything
          if(IsDuplicateOrCurrentlyClearing(action))
          {
            return RESULT_FAIL;
          }
          // Cancel all queued actions and make this action the next thing in it
          Cancel();
          result = QueueActionNext(action, numRetries);
          break;
        }
        case QueueActionPosition::NEXT:
        {
          result = QueueActionNext(action, numRetries);
          break;
        }
        case QueueActionPosition::AT_END:
        {
          result = QueueActionAtEnd(action, numRetries);
          break;
        }
        case QueueActionPosition::NOW_AND_RESUME:
        {
          result = QueueActionAtFront(action, numRetries);
          break;
        }
        case QueueActionPosition::IN_PARALLEL:
        {
          if(AddConcurrentAction(action, numRetries) == -1)
          {
            result = RESULT_FAIL;
          }
          break;
        }
        default:
        {
          PRINT_NAMED_ERROR("CozmoGameImpl.QueueActionHelper.InvalidPosition",
                            "Unrecognized 'position' %s for queuing action.",
                            EnumToString(inPosition));
          return RESULT_FAIL;
        }
      }
      
      return result;
    } // QueueAction()
    
    Result ActionList::QueueActionNext(IActionRunner* action, u8 numRetries)
    {
      if(IsDuplicateOrCurrentlyClearing(action))
      {
        return RESULT_FAIL;
      }
      return GetActionQueueForSlot(0).QueueNext(action, numRetries);
    }
    
    Result ActionList::QueueActionAtEnd(IActionRunner* action, u8 numRetries)
    {
      if(IsDuplicateOrCurrentlyClearing(action))
      {
        return RESULT_FAIL;
      }
      return GetActionQueueForSlot(0).QueueAtEnd(action, numRetries);
    }
    
    Result ActionList::QueueActionNow(IActionRunner* action, u8 numRetries)
    {
      if(IsDuplicateOrCurrentlyClearing(action))
      {
        return RESULT_FAIL;
      }
      return GetActionQueueForSlot(0).QueueNow(action, numRetries);
    }
    
    Result ActionList::QueueActionAtFront(IActionRunner* action, u8 numRetries)
    {
      if(IsDuplicateOrCurrentlyClearing(action))
      {
        return RESULT_FAIL;
      }
      return GetActionQueueForSlot(0).QueueAtFront(action, numRetries);
    }
    
    bool ActionList::Cancel(RobotActionType withType)
    {
      // Don't bother cancelling actions if we are in the process of clearing the queues
      if(_currentlyClearing)
      {
        return true;
      }
    
      bool found = false;
      
      // Clear specified slot / type
      for(auto & q : _queues) {
        found |= q.second.Cancel(withType);
      }
      return found;
    }
    
    bool ActionList::Cancel(u32 idTag)
    {
      // Don't bother cancelling actions if we are in the process of clearing the queues
      if(_currentlyClearing)
      {
        return true;
      }
    
      bool found = false;
      
      for(auto & q : _queues) {
        if(q.second.Cancel(idTag) == true) {
          if(found) {
            PRINT_NAMED_WARNING("ActionList.Cancel.DuplicateTags",
                                "Multiple actions from multiple slots cancelled with idTag=%d", idTag);
          }
          found = true;
        }
      }
      return found;
    }
    
    void ActionList::Clear()
    {
      if(_currentlyClearing)
      {
        return;
      }
      
      _currentlyClearing = true;
      _queues.clear();
      _currentlyClearing = false;
    }
    
    bool ActionList::IsEmpty() const
    {
      return _queues.empty();
    }
    
    void ActionList::Print() const
    {
      if(IsEmpty()) {
        PRINT_STREAM_INFO("ActionList.Print", "ActionList is empty.\n");
      } else {
        PRINT_STREAM_INFO("ActionList.Print", "ActionList contains " << _queues.size() << " queues:\n");
        for(auto const& queuePair : _queues) {
          queuePair.second.Print();
        }
      }
      
    } // Print()
    

    void ActionList::UpdateDependent(const RobotCompMap& dependentComps)
    {
      ANKI_CPU_PROFILE("ActionList::Update");
      
      Result lastResult = RESULT_OK;
      
      for(auto queueIter = _queues.begin(); queueIter != _queues.end(); )
      {
        Result thisResult = queueIter->second.Update();
        if( lastResult == RESULT_OK ) {
          // lastResult will be either the first failure, or OK
          lastResult = thisResult;
        }
        
        // If the queue is complete, remove it
        if(queueIter->second.IsEmpty()) {
          queueIter = _queues.erase(queueIter);
        } else {
          ++queueIter;
        }
      } // for each actionMemberPair

      if(lastResult != RESULT_OK){
        PRINT_NAMED_WARNING("ActionList.UpdateDependent.ActionResultNotOk",
                            "Action update returned result %d",
                            Util::EnumToUnderlying(lastResult));
      }

      GetActionWatcher().Update();      
    } // Update()
    
    
    ActionList::SlotHandle ActionList::AddConcurrentAction(IActionRunner* action, u8 numRetries)
    {
      if(action == nullptr) {
        PRINT_NAMED_WARNING("ActionList.AddAction.NullActionPointer", "Refusing to add null action");
        return -1;
      }
      
      if(IsDuplicateOrCurrentlyClearing(action))
      {
        return -1;
      }

      // Find an empty slot starting at 1 since all other queue positions will queue into slot 0
      SlotHandle currentSlot = 1;
      while(_queues.find(currentSlot) != _queues.end()) {
        ++currentSlot;
      }
      

      // create a new queue in the slot selected
      auto& queue = GetActionQueueForSlot(currentSlot);

      if(queue.QueueAtEnd(action, numRetries) != RESULT_OK) {
        PRINT_NAMED_ERROR("ActionList.AddAction.FailedToAdd", "Failed to add action to new queue");
        return -1;
      }
      
      return currentSlot;
    }
    
    bool ActionList::IsCurrAction(const std::string& actionName) const
    {
      for(auto queueIter = _queues.begin(); queueIter != _queues.end();  ++queueIter)
      {
        if (nullptr == queueIter->second.GetCurrentAction()) {
          return false;
        }
        if (queueIter->second.GetCurrentAction()->GetName() == actionName) {
          return true;
        }
      }
      return false;
    }

    bool ActionList::IsCurrAction(u32 idTag, SlotHandle fromSlot) const
    {
      const auto qIter = _queues.find(fromSlot);
      if( qIter == _queues.end() ) {
        // can't be playing if the slot doesn't exist
        return false;
      }

      if( nullptr == qIter->second.GetCurrentAction() ) {
        return false;
      }

      return qIter->second.GetCurrentAction()->GetTag() == idTag;
    }
    
    bool ActionList::IsDuplicateOrCurrentlyClearing(IActionRunner* action)
    {
      // If we are currently clearing just destroy the action
      if(_currentlyClearing)
      {
        if(action != nullptr)
        {
          action->PrepForCompletion();
        }
        Util::SafeDelete(action);
        return true;
      }
      else
      {
        for(auto &queue : _queues)
        {
          if(queue.second.IsDuplicate(action))
          {
            PRINT_NAMED_WARNING("ActionList.QueueAction.IsDuplicate",
                                "Attempting to queue duplicate action %s [%d]",
                                action->GetName().c_str(), action->GetTag());
            return true;
          }
        }
        return false;
      }
    }
    
    bool ActionList::IsExternalAction(const IActionRunner* action)
    {
      if(action != nullptr)
      {
        const u32 tag = action->GetTag();
        return (tag >= ActionConstants::FIRST_GAME_TAG &&
                tag <= ActionConstants::LAST_GAME_TAG) ||
               (tag >= ActionConstants::FIRST_SDK_TAG &&
                tag <= ActionConstants::LAST_SDK_TAG);
      }
      return false;
    }

    ActionEndedCallbackID ActionList::RegisterActionEndedCallbackForAllActions(ActionEndedCallback callback)
    {
      return GetActionWatcher().RegisterActionEndedCallbackForAllActions(callback);
    }

    bool ActionList::UnregisterCallback(ActionEndedCallbackID callbackID)
    {
      return GetActionWatcher().UnregisterCallback(callbackID);
    }

    ActionQueue& ActionList::GetActionQueueForSlot(SlotHandle handle)
    {
      auto iter = _queues.find(handle);
      if(iter == _queues.end()){
        ActionQueue newQueue(*_robot);
        const auto resultPair = _queues.insert(std::make_pair(handle, std::move(newQueue)));
        ANKI_VERIFY(resultPair.second, 
                    "ActionList.GetActionQueueForSlot.FailedInsert","");
        iter = resultPair.first;
      }

      return iter->second;
    }


#pragma mark ---- ActionQueue ----
    
    ActionQueue::ActionQueue(Robot& robot)
    : _robot(robot)
    {
      
    }
    
    ActionQueue::~ActionQueue()
    {
      Clear();
    }
    
    void ActionQueue::Clear()
    {
      if(_currentlyClearing)
      {
        return;
      }
      
      _currentlyClearing = true;
      
      if(_currentAction != nullptr)
      {
        _currentAction->Cancel();
      }
      DeleteAction(_currentAction);
      
      for (auto listIter = _queue.begin(); listIter != _queue.end(); ) {
        DEV_ASSERT(*listIter != nullptr, "ActionQueue.Clear.NullAction");
        DeleteActionIter(listIter);
      }
      
      _currentlyClearing = false;
    }

    bool ActionQueue::Cancel(RobotActionType withType)
    {
      bool found = false;
      
      if(_currentAction != nullptr)
      {
        if(withType == RobotActionType::UNKNOWN || _currentAction->GetType() == withType)
        {
          _currentAction->Cancel();
          DeleteAction(_currentAction);
          found = true;
        }
      }
      
      for(auto iter = _queue.begin(); iter != _queue.end();)
      {
        DEV_ASSERT(*iter != nullptr, "ActionQueue.CancelType.NullAction");
        
        if(withType == RobotActionType::UNKNOWN || (*iter)->GetType() == withType) {
          // If this doesn't actually delete the action then it must have already been deleted so
          // our iter will be invalid since the call that actually deleted the action will erase the iter
          if(!DeleteActionIter(iter))
          {
            break;
          }
          found = true;
        } else {
          ++iter;
        }
      }
      return found;
    }
    
    bool ActionQueue::Cancel(u32 idTag)
    {
      bool found = false;
      
      if(_currentAction != nullptr)
      {
        if(_currentAction->GetTag() == idTag)
        {
          _currentAction->Cancel();
          DeleteAction(_currentAction);
          found = true;
        }
      }
      
      for(auto iter = _queue.begin(); iter != _queue.end();)
      {
        DEV_ASSERT(*iter != nullptr, "ActionQueue.CancelTag.NullAction");
        
        if((*iter)->GetTag() == idTag) {
          if(found == true) {
            PRINT_NAMED_WARNING("ActionQueue.Cancel.DuplicateIdTags",
                                "Multiple actions with tag=%d found in queue",
                                idTag);
          }
          // If this doesn't actually delete the action then it must have already been deleted so
          // our iter will be invalid since the call that actually deleted the action will erase the iter
          if(!DeleteActionIter(iter))
          {
            break;
          }
          found = true;
        } else {
          ++iter;
        }
      }
      
      return found;
    }

    Result ActionQueue::QueueNow(IActionRunner *action, u8 numRetries)
    {
      if(action == nullptr) {
        PRINT_NAMED_ERROR("ActionQueue.QueueNow.NullActionPointer",
                          "Refusing to queue a null action pointer");
        return RESULT_FAIL;
      }
      
      if(_queue.empty()) {
        if(_currentAction != nullptr)
        {
          _currentAction->Cancel();
        }
        DeleteAction(_currentAction);
        return QueueAtEnd(action, numRetries);
      } else {
        const IActionRunner* currentAction = GetCurrentRunningAction();
        // Cancel whatever is running now and then queue this to happen next
        // (right after any cleanup due to the cancellation completes)
        if(currentAction != nullptr)
        {
          LOG_DEBUG("ActionQueue.QueueNow.CancelingPrevious", "Canceling %s [%d] in favor of action %s [%d]",
                    currentAction->GetName().c_str(),
                    currentAction->GetTag(),
                    action->GetName().c_str(),
                    action->GetTag());
        }
        DeleteAction(_currentAction);
        action->SetNumRetries(numRetries);
        _queue.push_front(action);
        return RESULT_OK;
      }
    }
    
    Result ActionQueue::QueueAtFront(IActionRunner* action, u8 numRetries)
    {
      if(action == nullptr) {
        PRINT_NAMED_ERROR("ActionQueue.QueueAFront.NullActionPointer",
                          "Refusing to queue a null action pointer.");
        return RESULT_FAIL;
      }
      
      Result result = RESULT_OK;
      
      if(IsEmpty()) {
        // Nothing in the queue, so this is the same as QueueAtEnd
        result = QueueAtEnd(action, numRetries);
      } else {
        // Try to interrupt whatever is running
        if(_currentAction != nullptr && _currentAction->Interrupt()) {
          // Current action is interruptable so push it back onto the queue and then
          // push new action in front of it
          LOG_INFO("ActionQueue.QueueAtFront.Interrupt",
                   "Interrupting %s to put %s in front of it.",
                   _currentAction->GetName().c_str(),
                   action->GetName().c_str());
          action->SetNumRetries(numRetries);
          _queue.push_front(_currentAction);
          _queue.push_front(action);
          // Set currentAction to null to force running of next action in queue
          _currentAction = nullptr;
        } else {
          // Current front action is not interruptible, so just use QueueNow and
          // cancel it
          if(_currentAction != nullptr)
          {
            LOG_INFO("ActionQueue.QueueAtFront.Interrupt",
                     "Could not interrupt %s. Will cancel and queue %s now.",
                     _currentAction->GetName().c_str(),
                     action->GetName().c_str());
          }
          result = QueueNow(action, numRetries);
        }
      }
      
      return result;
    }
    
    Result ActionQueue::QueueAtEnd(IActionRunner *action, u8 numRetries)
    {
      if(action == nullptr) {
        PRINT_NAMED_ERROR("ActionQueue.QueueAtEnd.NullActionPointer",
                          "Refusing to queue a null action pointer");
        return RESULT_FAIL;
      }
      
      action->SetNumRetries(numRetries);
      _queue.push_back(action);
      return RESULT_OK;
    }
    
    Result ActionQueue::QueueNext(IActionRunner *action, u8 numRetries)
    {
      if(action == nullptr) {
        PRINT_NAMED_ERROR("ActionQueue.QueueNext.NullActionPointer",
                          "Refusing to queue a null action pointer");
        return RESULT_FAIL;
      }
      
      action->SetNumRetries(numRetries);
      
      if(_queue.empty()) {
        return QueueAtEnd(action, numRetries);
      }
      
      std::list<IActionRunner*>::iterator queueIter = _queue.begin();
      ++queueIter;
      _queue.insert(queueIter, action);
      
      return RESULT_OK;
    }
    
    Result ActionQueue::Update()
    {
      Result lastResult = RESULT_OK;
      
      if(!_queue.empty() || _currentAction != nullptr)
      {
        if(_currentAction == nullptr)
        {
          _currentAction = GetNextActionToRun();
        }
        assert(_currentAction != nullptr);
        if(!_currentAction->HasRobot()){
          _currentAction->SetRobot(&_robot);
        }
        _currentAction->GetRobot().GetActionList().GetActionWatcher().ParentActionUpdating(_currentAction);
        
        const CozmoContext* cozmoContext = _currentAction->GetRobot().GetContext();
        VizManager* vizManager = cozmoContext->GetVizManager();
        DEV_ASSERT(nullptr != vizManager, "Expecting a non-null VizManager");
        
        const ActionResult actionResult = _currentAction->Update();
        const bool isRunning = (actionResult == ActionResult::RUNNING);
        
        if (isRunning)
        {
          vizManager->SetText(TextLabelType::ACTION, NamedColors::GREEN, "Action: %s", _currentAction->GetName().c_str());
          cozmoContext->SetSdkStatus(SdkStatusType::Action, std::string(_currentAction->GetName()));
        }
        else
        {
          vizManager->SetText(TextLabelType::ACTION, NamedColors::GREEN, "");
          cozmoContext->SetSdkStatus(SdkStatusType::Action, "");
        }
        
        if (!isRunning) {
          // Current action is no longer running delete it
          DeleteAction(_currentAction);
          
          if(actionResult != ActionResult::SUCCESS && actionResult != ActionResult::CANCELLED_WHILE_RUNNING) {
            lastResult = RESULT_FAIL;
          }
        }
      } // if queue not empty
      
      return lastResult;
    }
    
    IActionRunner* ActionQueue::GetNextActionToRun()
    {
      if(_queue.empty()) {
        return nullptr;
      }
      IActionRunner* action = _queue.front();
      _queue.pop_front();
      return action;
    }

    const IActionRunner* ActionQueue::GetCurrentAction() const
    {
      // If don't have a current action (aren't running anything) but have things that will be run
      // then the current action is the first one in the queue
      if(nullptr == _currentAction && _queue.size() > 0)
      {
        return _queue.front();
      }
      else
      {
        return _currentAction;
      }
    }
    
    bool ActionQueue::DeleteActionAndIter(IActionRunner* &action, std::list<IActionRunner*>::iterator& iter)
    {
      if(action != nullptr)
      {
        if(iter != _queue.end())
        {
          DEV_ASSERT((*iter) == action, "ActionQueue.DeleteAction.IterAndActionNotTheSame");
        }
      
        // If the action isn't currently being deleted meaning it was successfully inserted into the set
        // so the second element of the pair will be true
        const auto pair = _tagsBeingDeleted.insert(action->GetTag());
        if(pair.second)
        {
          action->PrepForCompletion();
          
          IExternalInterface* externalInterface = nullptr;
          ExternalInterface::RobotCompletedAction rca;
          if(action->GetRobot().HasExternalInterface() &&
             action->GetState() != ActionResult::INTERRUPTED)
          {
            action->GetRobotCompletedActionMessage(rca);
            
            externalInterface = action->GetRobot().GetExternalInterface();
          }
          
          Util::SafeDelete(action);
          
          // Remove the action from the queue before emitting a RobotActionCompleted signal
          // This is to prevent there being a null action in the queue while systems are handling
          // the action completion
          if(iter != _queue.end())
          {
            iter = _queue.erase(iter);
          }
          
          if(externalInterface != nullptr)
          {
            externalInterface->Broadcast(ExternalInterface::MessageEngineToGame(std::move(rca)));
          }
          
          _tagsBeingDeleted.erase(pair.first);
        }
        return pair.second;
      }
      return false;
    }
    
    bool ActionQueue::DeleteAction(IActionRunner* &action)
    {
      auto iter = _queue.end();
      return DeleteActionAndIter(action, iter);
    }
    
    bool ActionQueue::DeleteActionIter(std::list<IActionRunner*>::iterator& iter)
    {
      return DeleteActionAndIter(*iter, iter);
    }
    
    bool ActionQueue::IsDuplicate(IActionRunner* action)
    {
      for(auto &action1 : _queue)
      {
        if(action1 == action)
        {
          return true;
        }
      }
      return false;
    }
    
    void ActionQueue::Print() const
    {
      if(IsEmpty()) {
        PRINT_STREAM_INFO("ActionQueue.Print", "ActionQueue is empty.\n");
      } else {
        std::stringstream ss;
        ss << "ActionQueue with " << _queue.size() << " actions: ";
        for(auto action : _queue) {
          ss << action->GetName() << ", ";
        }
        PRINT_STREAM_INFO("ActionQueue.Print", ss.str());
      }
    } // Print()
    
    
    
  } // namespace Vector
} // namespace Anki

