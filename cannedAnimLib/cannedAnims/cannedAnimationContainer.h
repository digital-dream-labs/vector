/**
 * File: cannedAnimationContainer.h
 *
 * Authors: Andrew Stein
 * Created: 2014-10-22
 *
 * Description: Container for hard-coded or json-defined "canned" animations
 *              stored on the basestation and send-able to the physical robot.
 *
 * Copyright: Anki, Inc. 2014
 *
 **/


#ifndef ANKI_COZMO_CANNED_ANIMATION_CONTAINER_H
#define ANKI_COZMO_CANNED_ANIMATION_CONTAINER_H

#include "cannedAnimLib/cannedAnims/animation.h"
#include <unordered_map>
#include <vector>

namespace Anki {

namespace Vector {

class CannedAnimationContainer
{
public:
  CannedAnimationContainer();
                          
  ~CannedAnimationContainer();
  
  bool  HasAnimation(const std::string& name) const;
  Animation* GetAnimation(const std::string& name);
  const Animation* GetAnimation(const std::string& name) const;
  // If adding the new animation overwrites an existing animation, outOverwriting will be set to true
  void AddAnimation(Animation&& animation, bool& outOverwriting);

  std::vector<std::string> GetAnimationNames();
  
private:
  using AnimMap = std::unordered_map<std::string, Animation>;
  std::unordered_map<std::string, Animation> _animations;
  
}; // class CannedAnimationContainer
  
} // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_CANNED_ANIMATION_CONTAINER_H
