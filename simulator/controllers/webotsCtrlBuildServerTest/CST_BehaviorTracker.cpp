/**
 * File: CST_BehaviorTracker.cpp
 *
 * Author: Kevin M. Karol
 * Created: 6/23/16
 *
 * Description: Provide analytics on the states cozmo spends time in
 *
 * Copyright: Anki, inc. 2016
 *
 */

#include "engine/actions/basicActions.h"
#include "engine/aiComponent/behaviorComponent/behaviorTypesWrapper.h"
#include "engine/robot.h"
#include "simulator/game/cozmoSimTestController.h"
#include "util/logging/logging.h"
#include <fstream>
#include <iostream>
#include <time.h>

namespace Anki {
namespace Vector {
    
enum class TestState {
  StartUpFreeplayMode,
  FreePlay,
  TestDone
};

// ============ Test class declaration ============
class CST_BehaviorTracker : public CozmoSimTestController {
public:
  CST_BehaviorTracker();

private:
  struct BehaviorStateChange {
    BehaviorID oldBehaviorID = BEHAVIOR_ID(Wait);
    BehaviorID newBehaviorID = BEHAVIOR_ID(Wait);
    float elapsedTime = 0.f;
  };
  
  virtual s32 UpdateSimInternal() override;
 
  //Constants
  const float kFreeplayLengthSeconds = 600;
  
  //Tracking Data
  time_t _startTime;
  
  std::vector<BehaviorID> _freeplayBehaviorList;
  TestState _testState = TestState::StartUpFreeplayMode;
  std::vector<BehaviorStateChange> _stateChangeList;
  
  // Message handlers
  virtual void HandleBehaviorTransition(const ExternalInterface::BehaviorTransition& msg) override;
};

// Register class with factory
REGISTER_COZMO_SIM_TEST_CLASS(CST_BehaviorTracker);

// =========== Test class implementation ===========
CST_BehaviorTracker::CST_BehaviorTracker()
{
  //Get the start time of the test
  time(&_startTime);
}
  

s32 CST_BehaviorTracker::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::StartUpFreeplayMode:
    {
      StartMovieConditional("BehaviorTracker", 8);
      TakeScreenshotsAtInterval("BehaviorTracker", 1.f);

      SET_TEST_STATE(FreePlay);
      break;
    }
    case TestState::FreePlay:
    {
      
      time_t timer;
      time(&timer);
      double totalElapsed = difftime(timer, _startTime);
      
      if(totalElapsed > kFreeplayLengthSeconds){
        //Final state
        BehaviorStateChange change;
        change.newBehaviorID = BEHAVIOR_ID(Wait);
        change.oldBehaviorID = _stateChangeList.back().newBehaviorID;
        change.elapsedTime = totalElapsed;
        _stateChangeList.push_back(change);
        
        SET_TEST_STATE(TestDone);
      }
      break;
    }
    case TestState::TestDone:
    {
      
      //Print info for graphs
      std::map<BehaviorID, float> timeMap;
      std::map<BehaviorID, int> countMap;
      std::map<BehaviorID, int>::iterator countMapIter;
      std::map<BehaviorID, float>::iterator timeMapIter;
      std::vector<BehaviorStateChange>::iterator stateChangeIter;
      float fullTime = 0.f;
      float timeDiff = 0.f;
      
      //Ensure all behaviors print
      for(const auto& behavior: _freeplayBehaviorList){
        timeMap.insert(std::make_pair(behavior, 0.f));
        countMap.insert(std::make_pair(behavior, 0));
      }
      
      for(stateChangeIter = _stateChangeList.begin(); stateChangeIter != _stateChangeList.end(); ++stateChangeIter){
        float elapsedTime = stateChangeIter->elapsedTime;
        
        //Track the total time per behavior
        if(stateChangeIter != _stateChangeList.begin()){
          std::vector<BehaviorStateChange>::iterator prev = std::prev(stateChangeIter);
          timeDiff = elapsedTime - fullTime;
          fullTime = elapsedTime;
          
          //time per behavior increment
          timeMapIter = timeMap.find(prev->newBehaviorID);
          
          if(timeMapIter != timeMap.end()){
            timeMapIter->second += timeDiff;
          }else{
            timeMap.insert(std::make_pair(prev->newBehaviorID, timeDiff));
          }
          
          //count encountered increment
          countMapIter = countMap.find(prev->newBehaviorID);
          if(countMapIter != countMap.end()){
            countMapIter->second += 1;
          }else{
            countMap.insert(std::make_pair(prev->newBehaviorID, 1));
          }
          
        }
      } //End for stateChangeIter loop
      
      //Change timeMapIter from elapsed time to percentage total time
      if(fullTime > 0) {
        for(auto& entry: timeMap){
          entry.second = entry.second / fullTime;
        }
      }
      
      //Check to ensure all behaviors ran, or fail test
      //Currently deactivated until we know we will hit all behaviors in the specified time
      /**for(const auto& behavior: _freeplayBehaviorList){
        CST_EXPECT(, "Behavior not played during freeplay");
      }**/
      
      for(timeMapIter = timeMap.begin(); timeMapIter != timeMap.end(); ++timeMapIter){
        PRINT_NAMED_INFO("Webots.BehaviorTracker.TestData",
                         "##teamcity[buildStatisticValue key='%s%s' value='%f']",
                         "wbtsBehavior_",
                         BehaviorTypesWrapper::BehaviorIDToString(timeMapIter->first),
                         timeMapIter->second);
      }
      
      for(countMapIter = countMap.begin(); countMapIter != countMap.end(); ++countMapIter){
        PRINT_NAMED_INFO("Webots.BehaviorTracker.TestData",
                         "##teamcity[buildStatisticValue key='%s%s' value='%d']",
                         "wbtsBehavior_count_",
                         BehaviorTypesWrapper::BehaviorIDToString(countMapIter->first),
                         countMapIter->second);
      }
      
      
      StopMovie();
      CST_EXIT();
      break;
    }
  }
  return _result;
}

// ================ Message handler callbacks ==================
void CST_BehaviorTracker::HandleBehaviorTransition(const ExternalInterface::BehaviorTransition& msg)
{
  time_t currentTime;
  time(&currentTime);
  BehaviorStateChange change;
  //change.newBehaviorID = msg.newBehaviorID;
  //change.oldBehaviorID = msg.oldBehaviorID;
  change.elapsedTime = difftime(currentTime, _startTime);
  
  _stateChangeList.push_back(change);
}


// ================ End of message handler callbacks ==================
  
} // end namespace Vector
} // end namespace Anki
