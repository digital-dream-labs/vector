/**
 * File: cozmoObservableObject.cpp
 *
 * Author: Andrew Stein
 * Date:   2/21/2017
 *
 * Description: 
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "engine/cozmoObservableObject.h"

#include "util/console/consoleInterface.h"

namespace Anki {
namespace Vector {
  
CONSOLE_VAR_RANGED(f32, kDefaultMaxObservationDistance_mm, "PoseConfirmation", 500.f, 50.f, 1000.f);

const FactoryID ObservableObject::InvalidFactoryID = "";
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
f32 ObservableObject::GetMaxObservationDistance_mm() const
{
  return kDefaultMaxObservationDistance_mm;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ObservableObject::SetID()
{
  if(IsUnique())
  {
    static std::map<ObjectType, ObjectID> typeToUniqueID;
    
    const ObjectType objType = GetType();
    
    auto iter = typeToUniqueID.find(objType);
    if(iter == typeToUniqueID.end())
    {
      // First instance with this type. Add new entry.
      Vision::ObservableObject::SetID();
      const ObjectID& newID = GetID();
      typeToUniqueID.emplace(objType, newID);
    }
    else
    {
      // Use existing ID for this type
      _ID = iter->second;
    }
  }
  else
  {
    Vision::ObservableObject::SetID();
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ObservableObject::InitPose(const Pose3d& pose, PoseState poseState, const float fromDistance_mm)
{
  // This indicates programmer error: InitPose should only be called once on
  // an object and never once SetPose has been called
  DEV_ASSERT_MSG(!_poseHasBeenSet,
                 "ObservableObject.InitPose.PoseAlreadySet",
                 "%s Object %d",
                 EnumToString(GetType()), GetID().GetValue());
  
  SetPose(pose, fromDistance_mm, poseState);
  _poseHasBeenSet = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ObservableObject::SetPose(const Pose3d& newPose, f32 fromDistance, PoseState newPoseState)
{
  Vision::ObservableObject::SetPose(newPose, fromDistance, newPoseState);
  _poseHasBeenSet = true; // Make sure InitPose can't be called after this
  
  // Every object's pose should always be able to find a path to a valid origin without crashing
  if(ANKI_DEV_CHEATS) {
    ANKI_VERIFY(GetPose().FindRoot().IsRoot(), "ObservableObject.SetPose.PoseRootIsNotRoot",
                "%s ID:%d at %s with parent '%s'",
                EnumToString(GetType()), GetID().GetValue(),
                GetPose().GetTranslation().ToString().c_str(),
                GetPose().GetParentString().c_str());
  }
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool ObservableObject::IsMoving(TimeStamp_t* t) const
{
  if( t != nullptr ) {
    RobotTimeStamp_t roboTime{*t};
    const bool ret = IsMoving( &roboTime );
    *t = (TimeStamp_t)roboTime;
    return ret;
  } else {
    return IsMoving((RobotTimeStamp_t*)nullptr);
  }
}
  
} // namespace Vector
} // namespace Anki
