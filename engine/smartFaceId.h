/**
 * File: smartFaceId.h
 *
 * Author: Brad Neuman
 * Created: 2017-04-13
 *
 * Description: Simple wrapper for faceID that automatically handles face deletion and id changes
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Cozmo_Basestation_SmartFaceId_H__
#define __Cozmo_Basestation_SmartFaceId_H__

#include "coretech/vision/engine/faceIdTypes.h"
#include <memory>

namespace Anki {
namespace Vector {

class SmartFaceIDImpl;
class Robot;
class FaceWorld;

class SmartFaceID
{
  friend class FaceWorld;
  friend class TrackFaceAction;
public:

  // construct empty face id (invalid face). Anyone can construct and invalid ID, but only friends can make it
  // track an actual face
  SmartFaceID();

  ~SmartFaceID();

  SmartFaceID(const SmartFaceID& other);
  SmartFaceID(SmartFaceID&& other);

  SmartFaceID& operator=(const SmartFaceID& other);

  bool operator==(const SmartFaceID& other) const;
  bool operator!=(const SmartFaceID& other) const { return !(other == *this);}

  void Reset();

  // true if this tracks a valid face, false otherwise
  bool IsValid() const;

  // Check if this ID matches a given raw face ID
  bool MatchesFaceID(Vision::FaceID_t faceID) const;

  // return a string with some short debug info about this ID
  std::string GetDebugStr() const;

private:

  // only friends can construct or change the actual face id stored inside here
  
  // construct face id which starts tracking the given ID (but will automatically update based on
  // FaceWorld). Robot required to handle id changes
  SmartFaceID(const Robot& robot, Vision::FaceID_t faceID);

  // clear the face id, or set a new one to track
  void Reset(const Robot& robot, Vision::FaceID_t faceID);

  // return the current value of the face id tracked here. Returns Vision::UnknownFaceID if this object is not
  // tracking a valid face (i.e. it never was, or the face got deleted). This should not be stored for more
  // than a single tick
  Vision::FaceID_t GetID() const;
  
  std::unique_ptr<SmartFaceIDImpl> _impl;
  
};

}
}


#endif
