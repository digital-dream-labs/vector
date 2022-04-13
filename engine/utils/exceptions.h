/**
 * File: exceptions.h
 *
 * Author: Damjan Stulic
 * Created: 17/Oct/2012
 *
 * Description: exception definitions
 *
 * Copyright: Anki, Inc. 2012
 *
 **/


#ifndef BASESTATION_UTILS_EXCEPTIONS_H_
#define BASESTATION_UTILS_EXCEPTIONS_H_


namespace Anki {
namespace Vector {

/**
 * exception definitions
 * 
 * @author   damjan
 */


// List all the exceptions that your heart desires
typedef enum {
  ET_GENERIC_ERROR = 0,
  ET_STD_EXCEPTION,
  ET_INVALID_INDEX,

  ET_ROBOT_MISSING_INTERFACE_ID,
  ET_ROBOT_DUPLICATE_INTERFACE_ID,
  ET_CATEGORY_POINTER_INVALID,
  ET_FMOD_INIT_ERROR,
  ET_FMOD_ERROR,
  ET_INVALID_FILE,
  ET_GAME_TYPE_UNDEFINED,
  ET_CONFIG_ERROR,
  ET_EFFECT_NOT_FOUND,
  ET_INVALID_EFFECT_TARGET,
  ET_INFINITE_LOOP,
  ET_NOT_INITIALIZED,

  // Planner exceptions:
  ET_PLANNER_THREADING_ERROR,
  ET_PLANNER_INVALID_MIDX,
  ET_PLANNER_INFINITE_LOOP,
  ET_PLANNER_INCONSISTENT,
  ET_PLANNER_MATH_BUG,
  ET_PLANNER_BAD_START_STATE,

  ET_PLANNER_INVALID_ACTION_ID,

  ET_INVALID_JSON,
  ET_BUFFER_OVERRUN_DETECTED,

  // Behavior exceptions:
  ET_BEHAVIOR_UNDEFINED_BEHAVIOR,
  ET_BEHAVIOR_MISSING_TRANSITION_CONDITION,
  ET_BEHAVIOR_CURRENT_BEHAVIOR_NOT_FOUND,
  ET_BEHAVIOR_DIFFICULTY_NOT_FOUND,
  
  ET_LAST_VALUE
} ExceptionType;


static const char* ExceptionDefinitions[ET_LAST_VALUE] = {

  //ET_GENERIC_ERROR
  "Generic Error. Please do not use, lazy developer!",

  //ET_STD_EXCEPTION
  "std::exception thrown",

  //ET_INVALID_INDEX,
  "Index out of bounds",

  //ET_ROBOT_MISSING_INTERFACE_ID
  "robot definition missing unique id",

  //ET_ROBOT_DUPLICATE_INTERFACE_ID
  "duplicate interface id found",

  //ET_CATEGORY_POINTER_INVALID
  "category cannot be NULL",

  //ET_FMOD_INIT_ERROR,
  "Error in FMOD init",

  //ET_FMOD_ERROR
  "Error with FMOD system",

  //ET_INVALID_FILE
  "Invalid file use in Basestation",

  //ET_GAME_TYPE_UNDEFINED
  "game type must be defined",

  //ET_CONFIG_ERROR,
  "error with configuration parameters",

  //ET_EFFECT_NOT_FOUND
  "effect not found in the table",

  //ET_INVALID_EFFECT_TARGET
  "effect target cannot be NULL",

  //ET_INFINITE_LOOP
  "possible infinite loop detected",

  //ET_NOT_INITIALIZED
  "A data structure has not been properly initialized",

  //ET_PLANNER_THREADING_ERROR
  "Error with synchronization with Planner thread",

  //ET_PLANNER_INVALID_MIDX
  "Invalid planner mode index",

  //ET_PLANNER_INFINITE_LOOP,
  "Possible infinite loop detected in planner",

  //ET_PLANNER_INCONSISTENT,
  "Planner inconsistency detected",

  //ET_PLANNER_MATH_BUG,
  "Problem with planner's math (lane computation, spiral math, etc)",

  //ET_PLANNER_BAD_START_STATE,
  "Invalid initial state for planning",

  //ET_PLANNER_INVALID_ACTION_ID,
  "Invalid action ID",

  //  ET_INVALID_JSON,
  "Invalid JSON string or function call",

  // ET_BUFFER_OVERRUN_DETECTED
  "Buffer overrun detected.",
  
  // ET_BEHAVIOR_UNDEFINED_BEHAVIOR
  "Behavior FSM references undefined behavior",
  
  // ET_BEHAVIOR_MISSING_TRANSITION_CONDITION
  "No transition condition defined for transition-to behavior",
  
  // ET_BEHAVIOR_CURRENT_BEHAVIOR_NOT_FOUND,
  "Current behavior not found in behavior FSM",
  
  // ET_BEHAVIOR_DIFFICULTY_NOT_FOUND
  "BehaviorFSM not found for specified difficulty"
    
};

__attribute__((used))
static const char* GetExceptionDefinition(ExceptionType exception)
{
  return ExceptionDefinitions[exception];
}

__attribute__((used))
static const char* GetExceptionDefinition(int exception)
{
  if (exception < ET_GENERIC_ERROR ||
      exception > ET_LAST_VALUE)
    return "Exception value not defined";
  return ExceptionDefinitions[exception];
}


} // namespace Vector
} // namespace Anki

#endif // BASESTATION_UTILS_EXCEPTIONS_H_
