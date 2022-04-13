/**
 * File: actionWatcher.h
 *
 * Author: Al Chaussee
 * Date:   11/10/2016
 *
 * Description: Monitors actions as they run to track what sub-actions get created and what
 *              their results are. This is a passive system and does not modify the robot or any
 *              of the actions
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Cozmo_Basestation_Actions_ActionWatcher_H__
#define __Cozmo_Basestation_Actions_ActionWatcher_H__

#include "engine/actions/actionDefinitions.h"
#include "clad/types/actionTypes.h"
#include "clad/types/robotCompletedAction.h"

#include <list>
#include <map>

namespace Anki {
namespace Vector {
  
class IActionRunner;
  
class ActionWatcher
{
  // TODO: Use clad defined actionTag COZMO-7701
  using ActionTag = uint32_t;

public:
  ActionWatcher();
  ~ActionWatcher();

  // Register a callback. This callback will be called when _any_ action ends (including sub-actions or actions
  // inside compound actions). It will be called after the action completed message is broadcast, and after
  // the action itself has been fully deleted. Returns a unique integer id for the callback (so it can later
  // be removed). Eventually this will move to the action container (see COZMO-11465 )
  ActionEndedCallbackID RegisterActionEndedCallbackForAllActions(ActionEndedCallback callback);

  // Remove a registered callback. Returns true if the callback was found, false otherwise
  bool UnregisterCallback(ActionEndedCallbackID callbackID);
  
  // Called every tick of basestation after the action list has updated its actions
  void Update();
  
  // Called when a parent action in the actionQueue is being updated
  void ParentActionUpdating(const IActionRunner* action);
  
  // Called at the start of IActionRunner::Update()
  void ActionStartUpdating(const IActionRunner* action);
  
  // Called at the end of IActionRunner::Update()
  void ActionEndUpdating(const IActionRunner* action);
  
  // Called when an action is destroyed, will delete the action from the _actionTree map but its node will still
  // exist as a child of its parent action. If a parent action is being destroyed then the full action tree will be destroyed
  // so it is invalid to access the tree for this action
  void ActionEnding(const IActionRunner* action);
  
  // Returns all unique ActionResults of all actions that are children of action with tag
  void GetSubActionResults(const ActionTag tag, std::vector<ActionResult>& results);
  
  // Prints the action tree for this tag
  void Print(const ActionTag tag);
  
  // Deletes all nodes in this action's tree and removes all tag->node mappings for each of the actions in the
  // this action's tree from the _actionTrees map
  void DeleteActionTree(const ActionTag tag);

private:

  struct Node
  {
    const ActionTag actionTag;
    ExternalInterface::RobotCompletedAction completion;
    std::string name;
    bool neverUpdated;
    
    Node* parent = nullptr;
    std::vector<Node*> children;
    
    Node(const ActionTag actionTag)
    : actionTag(actionTag)
    , name("")
    , neverUpdated(false)
    {
      children.clear();
      completion.idTag = ActionConstants::INVALID_TAG;
      completion.actionType = RobotActionType::UNKNOWN;
      completion.result = ActionResult::NOT_STARTED;
    }
  };
  
  void PrintHelper(const Node* node, int level, int child, int& numLeaves);
  
  // Maps an action's tag to its tree of subactions
  std::map<ActionTag, Node*> _actionTrees;
  
  // The tag of the parent action that is being updated by the ActionQueue
  ActionTag _parentActionTag  = ActionConstants::INVALID_TAG;
  
  // The tag of the action that is currently updating
  ActionTag _currentActionTag = ActionConstants::INVALID_TAG;
  
  // The tag of the last/previous action that is updating
  ActionTag _lastActionTag    = ActionConstants::INVALID_TAG;
  
  // Maps a parent action's tag to a stack of subActions that are in the process of updating
  std::map<ActionTag, std::list<ActionTag>> _parentToUpdatingActions;

  // Map of callbacks
  std::map<int, ActionEndedCallback> _actionEndingCallbacks;

  // the next available handle for an action ending callback
  int _nextActionEndingCallbackID = 1;

  // A queue of completed event info to call callbacks on during the next update
  std::deque< ExternalInterface::RobotCompletedAction > _callbackQueue;
};
  
}
}

#endif
