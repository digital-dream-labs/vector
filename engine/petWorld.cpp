/**
 * File: petWorld.cpp
 *
 * Author: Andrew Stein (andrew)
 * Created: 10-24-2016
 *
 * Description: Implements a container for mirroring on the main thread any pet faces
 *              detected on the vision system thread.
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/components/visionComponent.h"
#include "engine/cozmoContext.h"
#include "engine/petWorld.h"
#include "engine/robot.h"
#include "engine/viz/vizManager.h"

#include "clad/externalInterface/messageEngineToGame.h"

#include "util/console/consoleInterface.h"
#include "util/logging/DAS.h"

namespace  Anki {
namespace Vector {

CONSOLE_VAR(f32, kHeadTurnSpeedThreshPet_degs, "WasRotatingTooFast.Pet.Head_deg/s", 10.f);
CONSOLE_VAR(f32, kBodyTurnSpeedThreshPet_degs, "WasRotatingTooFast.Pet.Body_deg/s", 30.f);
CONSOLE_VAR(u8,  kNumImuDataToLookBackPet,     "WasRotatingTooFast.Pet.NumToLookBack", 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PetWorld::PetWorld()
: IDependencyManagedComponent(this, RobotComponentID::PetWorld)
{

}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PetWorld::InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps)
{
  _robot = robot;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Result PetWorld::Update(const std::list<Vision::TrackedPet>& pets)
{
  // For now, we just keep up with what was seen in the most recent image,
  // while maintaining numTimesObserved
  {
    PetContainer newKnownPets;

    for(auto & petDetection : pets)
    {
      // Insert into the new map if:
      // (a) it's a new pet and we weren't rotating fast (try to avoid adding hallucinated pets), OR
      // (b) it's an existing pet and we want to keep it (even if we were moving too fast)

      // See if it was already in the old map
      auto existingIter = _knownPets.find(petDetection.GetID());
      if(existingIter != _knownPets.end())
      {
        // If we already knew about this ID, increment its num times seen
        const Vision::TrackedPet& oldKnownPet = existingIter->second;
        Vision::TrackedPet newKnownPet(petDetection);
        newKnownPet.SetNumTimesObserved(oldKnownPet.GetNumTimesObserved() + 1);
        newKnownPets.emplace(petDetection.GetID(), std::move(newKnownPet));
      }
      else
      {
        // New pet ID, make sure we weren't rotating too fast while seeing it
        const bool rotatingTooFastCheckEnabled = (Util::IsFltGT(kBodyTurnSpeedThreshPet_degs, 0.f) ||
                                                  Util::IsFltGT(kHeadTurnSpeedThreshPet_degs, 0.f));
        auto const& imuHistory = _robot->GetImuComponent().GetImuHistory();
        const bool wasRotatingTooFast = (rotatingTooFastCheckEnabled &&
                                         imuHistory.WasRotatingTooFast(petDetection.GetTimeStamp(),
                                                                                        DEG_TO_RAD(kBodyTurnSpeedThreshPet_degs),
                                                                                        DEG_TO_RAD(kHeadTurnSpeedThreshPet_degs),
                                                                                        (petDetection.IsBeingTracked() ? kNumImuDataToLookBackPet : 0)));
        if(!wasRotatingTooFast)
        {
          Vision::TrackedPet newKnownPet(petDetection);
          newKnownPet.SetNumTimesObserved(1);
          newKnownPets.emplace(petDetection.GetID(), std::move(newKnownPet));
        }
      }
    }

    std::swap(newKnownPets, _knownPets);
  }

  // Now that knownPets is updated, Broadcast and Visualize
  for(auto const& knownPetEntry : _knownPets)
  {
    const Vision::TrackedPet& knownPet = knownPetEntry.second;

    // Log to DAS if this is the first detection for this pet:
    if(!knownPet.IsBeingTracked()) // The very first time we see a pet, it is not being "tracked" yet
    {
      ANKI_VERIFY(knownPet.GetNumTimesObserved() == 1, "PetWorld.Update.NewPetDetectionShouldBeObservedOnce",
                  "ID:%d NumTimesObserved:%d", knownPet.GetID(), knownPet.GetNumTimesObserved());

      if(ANKI_DEVELOPER_CODE)
      {
        // DEV code to make sure we don't re-log an event for the same ID twice (unless our IDs roll over)
        static std::set<s32> DEBUG_broadcastIDs;
        static const Vision::FaceID_t kMaxPetID = 4095; // max output by OMCV detector
        auto insert = DEBUG_broadcastIDs.insert(knownPet.GetID());

        if(insert.second == false)
        {
          PRINT_NAMED_WARNING("PetWorld.Update.DuplicateEvent",
                              "Already logged event for Pet ID:%d", knownPet.GetID());
        }

        if(knownPet.GetID() == kMaxPetID) {
          // Not likely to see kMaxID pets, but if we reset the ID a bunch in a single session, we could
          // conceivably overrun this and the IDs would start over with ID=1. We shouldn't fail in this case.
          DEBUG_broadcastIDs.clear();
        }
      }

      DASMSG(robot.vision.detected_pet, "robot.vision.detected_pet", "Detected a pet");
      DASMSG_SET(s1, EnumToString(knownPet.GetType()), "PetType");
      DASMSG_SET(i1, knownPet.GetID(), "PetID");
      DASMSG_SEND();
    }

    // Broadcast the detection for Game/SDK
    {
      using namespace ExternalInterface;

      RobotObservedPet observedPet(knownPet.GetID(),
                                   knownPet.GetTimeStamp(),
                                   knownPet.GetNumTimesObserved(),
                                   knownPet.GetScore(),
                                   CladRect(knownPet.GetRect().GetX(),
                                            knownPet.GetRect().GetY(),
                                            knownPet.GetRect().GetWidth(),
                                            knownPet.GetRect().GetHeight()),
                                   knownPet.GetType());

      _robot->Broadcast(MessageEngineToGame(std::move(observedPet)));
    }

    // Visualize the detection
    if(ANKI_DEV_CHEATS)
    {
      const ColorRGBA& vizColor = ColorRGBA::CreateFromColorIndex(knownPet.GetID());
      _robot->GetContext()->GetVizManager()->DrawCameraOval(Point2f(knownPet.GetRect().GetXmid(),
                                                                   knownPet.GetRect().GetYmid()),
                                                           knownPet.GetRect().GetWidth() * 0.5f,
                                                           knownPet.GetRect().GetHeight() * 0.5f,
                                                           vizColor);



      const s32 kStrLen = 16;
      char strbuffer[kStrLen];
      snprintf(strbuffer, kStrLen, "%s%d[%d]",
               knownPet.GetType() == Vision::PetType::Cat ? "CAT" : "DOG",
               knownPet.GetID(),
               knownPet.GetNumTimesObserved());

      _robot->GetContext()->GetVizManager()->DrawCameraText(Point2f(knownPet.GetRect().GetX(),
                                                                   knownPet.GetRect().GetY()),
                                                           strbuffer, vizColor);
    }
  }

  return RESULT_OK;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<Vision::FaceID_t> PetWorld::GetKnownPetsWithType(Vision::PetType type) const
{
  std::set<Vision::FaceID_t> matchingPets;
  for(auto & pet : _knownPets)
  {
    if(Vision::PetType::Unknown == type || pet.second.GetType() == type)
    {
      matchingPets.insert(pet.first);
    }
  }

  return matchingPets;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const Vision::TrackedPet* PetWorld::GetPetByID(Vision::FaceID_t faceID) const
{
  auto iter = _knownPets.find(faceID);
  if(iter == _knownPets.end())
  {
    return nullptr;
  }
  else
  {
    return &iter->second;
  }
}

} // namespace Vector
} // namespace Anki
