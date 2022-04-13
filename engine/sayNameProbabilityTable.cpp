/**
 * File: sayNameProbabilityTable.cpp
 *
 * Author: Andrew Stein
 * Date:   10/7/2018
 *
 * Description: Tracks how likely it is the robot should say a given name.
 *              Probability of saying reduces each time the table returns that the name should be said,
 *              down to a minimum. A minimum time between saying a each name must have passed as well.
 *              Statistics/decay are tracked on a per-name basis.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "coretech/common/engine/utils/timer.h"
#include "engine/sayNameProbabilityTable.h"
#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "Behaviors"

namespace Anki {
namespace Vector {

namespace {
  
  // Controls how fast the probability of saying each name goes down after each time ShouldSayName returns true.
  // Higher is _slower_ decay.
  CONSOLE_VAR_RANGED(float, kSayNameProbDecayFactor, "SayNameProbability", 0.75f, 0.f, 1.f);
  
  // Sets the minimum probability of saying a name, despite the decay factor above
  CONSOLE_VAR_RANGED(float, kSayNameMinProb,         "SayNameProbability", 0.1f,  0.f, 1.f);
  
  // Set the minimum time between saying the same name
  CONSOLE_VAR(float, kSayNameSpacing_sec,            "SayNameProbability", 10.f);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SayNameProbabilityTable::SayNameProbabilityTable(Util::RandomGenerator& rng)
: _rng(rng)
{
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SayNameProbabilityTable::UpdateShouldSayName(const std::string& name)
{
  if(name.empty())
  {
    return false;
  }
  
  const auto currentTime_sec = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
  
  auto result = _LUT.insert({name, Entry()});
  float& prob = result.first->second.prob;
  float& lastTime_sec = result.first->second.lastTimeSaid_sec;
  const float timePassed = currentTime_sec - lastTime_sec;
  const bool enoughTimePassed = timePassed > kSayNameSpacing_sec;
  const bool shouldSay = enoughTimePassed && (_rng.RandDbl() < prob);
  
  LOG_INFO("SayNameProbabilityTable.UpdateShouldSayName",
           "Name:%s%s Prob:%.2f TimeDelta:%.2fs ShouldSay:%d",
           Util::HidePersonallyIdentifiableInfo(name.c_str()),
           (result.second ? "[NEW]" : ""),
           prob, timePassed,
           shouldSay);
  
  if(shouldSay)
  {
    // Update for next query:
    prob *= kSayNameProbDecayFactor;
    prob = std::max(kSayNameMinProb, prob);
    lastTime_sec = currentTime_sec;
  }
  
  return shouldSay;
}

} // namespace Anki
} // namespace Vector

