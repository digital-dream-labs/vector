/**
 * File: petWorld.h
 *
 * Author: Andrew Stein (andrew)
 * Created: 10-24-2016
 *
 * Description: Defines a container for mirroring on the main thread any pet faces
 *              detected on the vision system thread.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/


#ifndef __Anki_Cozmo_Basestation_PetWorld_H__
#define __Anki_Cozmo_Basestation_PetWorld_H__

#include "coretech/vision/engine/trackedPet.h"

#include "engine/aiComponent/behaviorComponent/behaviors/iCozmoBehavior_fwd.h"
#include "util/entityComponent/entity.h"

#include <list>
#include <map>
#include <set>

namespace  Anki {
namespace Vector {
  
// Forward declarations:
class Robot;

class PetWorld : public IDependencyManagedComponent<RobotComponentID>
{
public: 
  using PetContainer = std::map<Vision::FaceID_t, Vision::TrackedPet>;
  
  PetWorld();

  //////
  // IDependencyManagedComponent functions
  //////
  virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override;
  virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {
    dependencies.insert(RobotComponentID::CozmoContextWrapper);
  };
  virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {};
  //////
  // end IDependencyManagedComponent functions
  //////
  
  // Pass in observed faces (e.g. from Vision thread to keep this class in sync)
  // Also takes care of Broadcasting RobotObservedPet messages and updating Viz.
  Result Update(const std::list<Vision::TrackedPet>& observedPetFaces);
  
  // Return a container with all currently known pets in it
  const PetContainer& GetAllKnownPets() const { return _knownPets; }
  
  // Return the IDs of the pets with the given type. If PetType::UnknownType is
  // passed in, all IDs will be returned
  std::set<Vision::FaceID_t> GetKnownPetsWithType(Vision::PetType type) const;
 
  // Get the TrackedPet corresponding to the given ID.
  // Will return nullptr if ID is not found.
  const Vision::TrackedPet* GetPetByID(Vision::FaceID_t faceID) const;
  
private:
  
  Robot* _robot;
  
  PetContainer _knownPets;
  
}; // class PetWorld
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_PetWorld_H__ */

