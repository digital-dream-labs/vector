/**
 * File: sayNameProbabilityTable.h
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

#ifndef __Anki_Vector_SayNameProbabilityTable_H__
#define __Anki_Vector_SayNameProbabilityTable_H__

#include "util/random/randomGenerator.h"
#include <map>

namespace Anki {
namespace Vector {
  
class SayNameProbabilityTable
{
public:
  
  SayNameProbabilityTable(Util::RandomGenerator& rng);
  
  // Check whether we should say the given name, based on current probabilities.
  // If so, the probabilities are updated for the next call.
  bool UpdateShouldSayName(const std::string& name);
  
  void Reset() { _LUT.clear(); }
  
private:
  
  struct Entry {
    float prob = 1.f;
    float lastTimeSaid_sec = 0.f;
  };
  std::map<std::string, Entry>  _LUT;
  Util::RandomGenerator&        _rng;

};

} // namespace Anki
} // namespace Vector

#endif /* __Anki_Vector_SayNameProbabilityTable_H__*/

