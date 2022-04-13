/**
* File: visionModesHelpers.h
*
* Author: Lee Crippen
* Created: 06/12/16
*
* Description: Helper functions for dealing with CLAD generated visionModes
*
* Copyright: Anki, Inc. 2016
*
**/


#ifndef __Cozmo_Basestation_VisionModesHelpers_H__
#define __Cozmo_Basestation_VisionModesHelpers_H__


#include "clad/types/visionModes.h"
#include "util/enums/enumOperators.h"

#include <set>

namespace Anki {
namespace Vector {

DECLARE_ENUM_INCREMENT_OPERATORS(VisionMode);

// NeuralNet <-> VisionMode
//   To "register" a VisionMode with an associated neural network name,
//   add it to the lookup table initialization in the .cpp source file.
  
// Find the neural network names registered to a given vision mode (or vice versa). Returns true if there are any and
// populates networkNames. Returns false (and does not modify networkNames) otherwise.
bool GetNeuralNetsForVisionMode(const VisionMode mode, std::set<std::string>& networkNames);
bool GetVisionModesForNeuralNet(const std::string& networkName, std::set<VisionMode>& modes);
  
// Return the set of VisionModes that have a neural net registered.
const std::set<VisionMode>& GetVisionModesUsingNeuralNets();
  
} // namespace Vector
} // namespace Anki


#endif // __Cozmo_Basestation_VisionModesHelpers_H__

