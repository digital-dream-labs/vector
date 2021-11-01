/**
 * File: actionWatcher.cpp
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

#include "engine/actions/actionWatcher.h"
#include "engine/actions/actionInterface.h"
#include "util/helpers/templateHelpers.h"

#define DEBUG_ACTION_WATCHER 0
#define LOG_CHANNEL "Actions"

namespace Anki {
namespace Vector {

ActionWatcher::ActionWatcher()
{

}

ActionWatcher::~ActionWatcher()
{
  for(auto& pair : _parentToUpdatingActions)
  {
    DeleteActionTree(pair.first);
  }
}

ActionEndedCallbackID ActionWatcher::RegisterActionEndedCallbackForAllActions(ActionEndedCallback callback)
{
  _actionEndingCallbacks[ _nextActionEndingCallbackID ] = callback;
  return _nextActionEndingCallbackID++;
}  
  
bool ActionWatcher::UnregisterCallback(ActionEndedCallbackID callbackID)
{
  auto it = _actionEndingCallbacks.find(callbackID);
  if( it != _actionEndingCallbacks.end() ) {
    _actionEndingCallbacks.erase(it);
    return true;
  }
  else {
    return false;
  }
}

void ActionWatcher::Update()
{
  while( !_callbackQueue.empty() ) {
    const auto& args = _callbackQueue.front();

    for( auto& callback : _actionEndingCallbacks ) {
      callback.second( args );
    }

    _callbackQueue.pop_front();
  }
}

void ActionWatcher::ParentActionUpdating(const IActionRunner* action)
{
  DEV_ASSERT(action != nullptr, "ActionWatcher.ParentActionUpdating.NullAction");

  _parentActionTag = action->GetTag();
  _currentActionTag = ActionConstants::INVALID_TAG;
  _lastActionTag = ActionConstants::INVALID_TAG;
  
  const auto root = _actionTrees.find(_parentActionTag);
  
  // If this is a new parent action
  if (root == _actionTrees.end())
  {
    Node* node = new Node(_parentActionTag);
    _actionTrees[_parentActionTag] = node;
  }

  // All of the updating action stacks should be empty since no actions besides the parent action are
  // currently updating
  #if ANKI_DEV_ASSERT_ENABLED
  for (const auto& i : _parentToUpdatingActions)
  {
    DEV_ASSERT(i.second.empty(), "ActionWatcher.ParentActionUpdating.ParentToUpdatingActionsNotEmpty");
  }
  #endif
}

void ActionWatcher::ActionStartUpdating(const IActionRunner* action)
{
  _lastActionTag = _currentActionTag;
  _currentActionTag = action->GetTag();
  
  // Add a node for this action if it does not already exist
  const auto& root = _actionTrees.find(action->GetTag());
  if(root == _actionTrees.end())
  {
    Node* node = new Node(action->GetTag());
    
    // If lastActionTag is valid then update parents and children
    if(_lastActionTag != ActionConstants::INVALID_TAG)
    {
      const auto& iter = _actionTrees.find(_lastActionTag);
      DEV_ASSERT(iter != _actionTrees.end(), "ActionWatcher.ActionStartUpdating.LastActionNotInTree");
      node->parent = iter->second;
      iter->second->children.push_back(node);
    }
    
    _actionTrees[action->GetTag()] = node;
  }

  _parentToUpdatingActions[_parentActionTag].push_back(_currentActionTag);
}

void ActionWatcher::ActionEndUpdating(const IActionRunner* action)
{
  auto& updatingActionsStack = _parentToUpdatingActions[_parentActionTag];
  
  // Remove this action from the updating action stack
  updatingActionsStack.pop_back();
  
  // Update _currentActionTag and _lastActionTag to be the top of the stack and the second to top
  // of the stack, respectively
  auto iter = updatingActionsStack.rbegin();
  
  if(iter == updatingActionsStack.rend())
  {
    _currentActionTag = ActionConstants::INVALID_TAG;
    _lastActionTag = ActionConstants::INVALID_TAG;
    return;
  }
  
  _currentActionTag = *iter;
  
  if(++iter == updatingActionsStack.rend())
  {
    _lastActionTag = ActionConstants::INVALID_TAG;
    return;
  }
  
  _lastActionTag = *iter;
}

void ActionWatcher::ActionEnding(const IActionRunner* action)
{  
  // Populate a RobotCompletedAction for this action
  ExternalInterface::RobotCompletedAction r;
  r.idTag = action->GetTag();
  r.actionType = action->GetType();
  r.result = action->GetState();
  action->GetCompletionUnion(r.completionInfo);
  GetSubActionResults(action->GetTag(), r.subActionResults);

  // queue a copy now for callbacks that will be called in Update
  _callbackQueue.push_back( r );
  
  // If this action is not in the tree then it was never updated so add it
  if(_actionTrees.find(action->GetTag()) == _actionTrees.end())
  {
    Node* node = new Node(action->GetTag());
    
    auto parent = _actionTrees.find(_parentActionTag);
    if(_parentActionTag != ActionConstants::INVALID_TAG &&
       parent != _actionTrees.end())
    {
      node->parent = parent->second;
      parent->second->children.push_back(node);
    }
    
    node->neverUpdated = true;
    _actionTrees[action->GetTag()] = node;
  }

  _actionTrees[action->GetTag()]->completion = r;
  _actionTrees[action->GetTag()]->name = action->GetName();
  
  const auto iter = _parentToUpdatingActions.find(action->GetTag());
  // If the action that is ending is a parent action then print information about it and delete its action tree
  if(iter != _parentToUpdatingActions.end())
  {
    if(DEBUG_ACTION_WATCHER)
    {
      Print(_parentActionTag);
    }
    DeleteActionTree(action->GetTag());
    _parentToUpdatingActions.erase(iter);
  }
  else
  {
    // If the action is not a parent action and the action itself does not have a parent then delete it from the tree
    if(_actionTrees[action->GetTag()]->parent == nullptr)
    {
      Util::SafeDelete(_actionTrees[action->GetTag()]);
    }
    _actionTrees.erase(_actionTrees.find(action->GetTag()));
  }
}

void ActionWatcher::GetSubActionResults(const ActionTag tag, std::vector<ActionResult>& results)
{
  const auto& root = _actionTrees.find(tag);
  if(root == _actionTrees.end())
  {
    return;
  }
  
  std::set<ActionResult> r;
  
  // Helper function to recursively add each child's completion result to the set of results
  std::function<void(const Node* node)> helper = [&](const Node* node){
    for(const auto& n : node->children)
    {
      r.insert(n->completion.result);
      helper(n);
    }
  };
  
  helper(root->second);
  
  results.clear();
  results.insert(results.begin(), r.begin(), r.end());
}

void ActionWatcher::DeleteActionTree(const ActionTag tag)
{
  const auto& root = _actionTrees.find(tag);
  if(root == _actionTrees.end())
  {
    return;
  }
  
  // Helper function to recursively delete nodes and actionTags from _actionTrees
  std::function<void(Node* node)> helper = [&](Node* node){
    for(auto& n : node->children)
    {
      helper(n);
      const auto& iter = _actionTrees.find(n->actionTag);

      Util::SafeDelete(n);
      if(iter != _actionTrees.end())
      {
        _actionTrees.erase(iter);
      }
    }
  };
  
  helper(root->second);
  
  Util::SafeDelete(root->second);
  _actionTrees.erase(root);
}

void ActionWatcher::Print(const ActionTag tag)
{
  std::stringstream ss;
  
  ss << "Parent: " << _actionTrees[tag]->name << "[" << _actionTrees[tag]->actionTag << "]" <<" created ";
  
  int c = 0;
  int numLeaves = 0;
  for(const auto& node : _actionTrees[tag]->children)
  {
    ss << (node->neverUpdated ? "**" : "");
    ss << node->name << "[" << node->actionTag << "]";
    if(node->children.empty())
    {
      ++numLeaves;
    }
    ss << ", ";
    PrintHelper(node, 0, c, numLeaves);
    c++;
  }
  
  ss << " with a total of " << numLeaves << " leaf actions";
  
  LOG_DEBUG("ActionWatcher.Print", "%s", ss.str().c_str());
}

void ActionWatcher::PrintHelper(const Node* node, int level, int child, int& numLeaves)
{
  std::stringstream ss;
  ss << (node->neverUpdated ? "**" : "") << node->name << "[" << node->actionTag << "]" << " created ";
  
  int c = 0;
  for(const auto& node : node->children)
  {
    ss << (node->neverUpdated ? "**" : "");
    ss << node->name << "[" << node->actionTag << "]";
    if(node->children.empty())
    {
      ++numLeaves;
    }
    ss << ", ";
    PrintHelper(node, level + 1, c, numLeaves);
    c++;
  }
  LOG_DEBUG("ActionWatcher.Print", "%s", ss.str().c_str());
}
  
  
}
}
