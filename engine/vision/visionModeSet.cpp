/**
 * File: visionModeSet.cpp
 *
 * Author: Andrew Stein
 * Date:   10/12/2018
 *
 * Description: Simple container for VisionModes.
 *
 * Copyright: Anki, Inc. 2018
 **/

#include "engine/vision/visionModeSet.h"
#include "engine/vision/visionModesHelpers.h"

namespace Anki {
namespace Vector {
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
VisionModeSet::VisionModeSet(const std::initializer_list<VisionMode>& args)
: _modes(args)
{
  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
VisionModeSet VisionModeSet::Intersect(const VisionModeSet& other) const
{
  VisionModeSet intersection;
  std::set_intersection(_modes.begin(), _modes.end(),
                        other._modes.begin(), other._modes.end(),
                        std::inserter(intersection._modes, intersection._modes.begin()));
  return intersection;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string VisionModeSet::ToString() const
{
  if(IsEmpty())
  {
    return "Idle";
  }
  
  std::string retStr("");
  std::for_each(_modes.begin(), _modes.end(), [&retStr](const VisionMode mode) {
    if(!retStr.empty())
    {
      retStr += "+";
    }
    retStr += EnumToString(mode);
  });
  
  return retStr;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionModeSet::InsertAllModes()
{
  constexpr VisionMode FirstMode = VisionMode(0);
  for(VisionMode mode = FirstMode; mode < VisionMode::Count; mode++)
  {
    Insert(mode);
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void VisionModeSet::Enable(VisionMode mode, bool enable)
{
  if(enable) {
    Insert(mode);
  }
  else {
    Remove(mode);
  }
}
  
} // namespace Vector
} // namespace Anki
