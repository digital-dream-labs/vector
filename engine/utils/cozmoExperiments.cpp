/**
 * File: cozmoExperiments
 *
 * Author: baustin
 * Created: 8/3/17
 *
 * Description: Interface into A/B test system
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#include "engine/utils/cozmoExperiments.h"

#include "engine/cozmoContext.h"
#include "engine/robot.h"
#include "engine/robotManager.h"
#include "util/ankiLab/extLabInterface.h"
#if USE_DAS
#include <DAS/DAS.h>
#include <DAS/DASPlatform.h>
#endif

namespace Anki {
namespace Vector {

CozmoExperiments::CozmoExperiments(const CozmoContext* context)
: _context(context)
, _tags(context)
, _loadedLabAssignments()
, _assignments()
{
}

static const char* GetDeviceId()
{
#if USE_DAS
  DEV_ASSERT(DASGetPlatform() != nullptr, "CozmoExperiments.GetDeviceId.MissingDASPlatform");
  return DASGetPlatform()->GetDeviceId();
#else
  return "user"; // non-empty string keeps it from failing on mac release
#endif
}

void CozmoExperiments::InitExperiments()
{
  _lab.Enable(!Util::AnkiLab::ShouldABTestingBeDisabled());

  AutoActivateExperiments(GetDeviceId());

  (void) _tags.VerifyTags(_lab.GetKnownAudienceTags());

  // Provide external A/B interface what it needs to work with Cozmo
  auto labOpRunner = [this] (const Util::AnkiLab::LabOperation& op) {
    op(&_lab);
  };
  auto userIdAccessor = [this] {
    Robot* robot = _context->GetRobotManager()->GetRobot();
    return robot != nullptr ? std::to_string(robot->GetHeadSerialNumber()) : GetDeviceId();
  };
  Util::AnkiLab::InitializeABInterface(labOpRunner, userIdAccessor);
}

Util::AnkiLab::AssignmentStatus CozmoExperiments::ActivateExperiment(
  const Util::AnkiLab::ActivateExperimentRequest& request, std::string& outVariationKey)
{
  std::string deviceId;
  const std::string* userIdString = &request.user_id;
  if (request.user_id.empty()) {
    userIdString = &deviceId;
    deviceId = GetDeviceId();
  }
  const auto& tags = _tags.GetQualifiedTags();
  return _lab.ActivateExperiment(request.experiment_key, *userIdString, tags, outVariationKey);
}

void CozmoExperiments::AutoActivateExperiments(const std::string& userId)
{
  const auto& tags = _tags.GetQualifiedTags();
  (void) _lab.AutoActivateExperimentsForUser(userId, tags);
}

void CozmoExperiments::WriteLabAssignmentsToRobot(const std::vector<Util::AnkiLab::AssignmentDef>& assignments)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (robot != nullptr)
  {
    LabAssignments labAssignments;
    for (const auto& assignment : assignments)
    {
      LabAssignment labAssignment(assignment.GetExperiment_key(), assignment.GetVariation_key());
      labAssignments.labAssignments.push_back(std::move(labAssignment));
    }

    std::vector<u8> assignmentsVec(labAssignments.Size());
    labAssignments.Pack(assignmentsVec.data(), labAssignments.Size());
    if (!robot->GetNVStorageComponent().Write(NVStorage::NVEntryTag::NVEntry_LabAssignments,
                                              assignmentsVec.data(), assignmentsVec.size()))
    {
      PRINT_NAMED_ERROR("CozmoExperiments.WriteLabAssignmentsToRobot.Failed", "Write failed");
    }
  }
}

void CozmoExperiments::ReadLabAssignmentsFromRobot(const u32 serialNumber)
{
  Robot* robot = _context->GetRobotManager()->GetRobot();
  if (robot != nullptr)
  {
    _loadedLabAssignments.labAssignments.clear();
    if (!robot->GetNVStorageComponent().Read(NVStorage::NVEntryTag::NVEntry_LabAssignments,
                                             [this, serialNumber](u8* data, size_t size, NVStorage::NVResult res)
                                             {
                                               (void) RestoreLoadedActiveExperiments(data, size, res, serialNumber);
                                             }))
    {
      PRINT_NAMED_ERROR("CozmoExperiments.ReadLabAssignmentsFromRobot.Failed", "Read failed");
    }
  }
}

bool CozmoExperiments::RestoreLoadedActiveExperiments(const u8* data, const size_t size,
                                                      const NVStorage::NVResult res, u32 serialNumber)
{
  if (res < NVStorage::NVResult::NV_OKAY)
  {
    // The tag doesn't exist on the robot indicating the robot is new or has been wiped
    if (res == NVStorage::NVResult::NV_NOT_FOUND)
    {
      PRINT_NAMED_INFO("CozmoExperiments.RestoreLoadedActiveExperiments",
                       "No lab assignments data on robot");
    }
    else
    {
      PRINT_NAMED_ERROR("CozmoExperiments.RestoreLoadedActiveExperiments.ReadFailedFinish",
                        "Read failed with %s", EnumToString(res));
    }
    return false;
  }

  _loadedLabAssignments.Unpack(data, size);

  // We've just loaded any active assignments from the robot; so now apply them
  using namespace Util::AnkiLab;
  AnkiLab& lab = GetAnkiLab();

  const std::string userId = std::to_string(serialNumber);

  for (const auto& assignment : _loadedLabAssignments.labAssignments)
  {
    (void) lab.RestoreActiveExperiment(assignment.experiment_key, userId, assignment.variation_key);
  }

  return true;
}

void CozmoExperiments::PossiblyWriteLabAssignmentsToRobot()
{
  const size_t oldNumAssignments = _loadedLabAssignments.labAssignments.size();
  const size_t newNumAssignments = _assignments.size();
  bool needToWriteToRobot = oldNumAssignments != newNumAssignments;

  if (!needToWriteToRobot)
  {
    for (int i = 0; i < oldNumAssignments; i++)
    {
      if ((_loadedLabAssignments.labAssignments[i].experiment_key != _assignments[i].experiment_key) ||
          (_loadedLabAssignments.labAssignments[i].variation_key  != _assignments[i].variation_key))
      {
        needToWriteToRobot = true;
        break;
      }
    }
  }

  if (needToWriteToRobot)
  {
    PRINT_NAMED_INFO("CozmoExperiments.PossiblyWriteLabAssignmentsToRobot",
                     "Writing updated lab assignments to robot");
    _context->GetExperiments()->WriteLabAssignmentsToRobot(_assignments);

    // Now copy the new set of assignments over the 'loaded' set, so that when we
    // call this function again, we can tell if we need to write to robot again
    _loadedLabAssignments.labAssignments.clear();
    for (const auto& assignment : _assignments)
    {
      LabAssignment labAssignment(assignment.GetExperiment_key(), assignment.GetVariation_key());
      _loadedLabAssignments.labAssignments.push_back(std::move(labAssignment));
    }
  }
}

void CozmoExperiments::UpdateLabAssignments(const std::vector<Util::AnkiLab::AssignmentDef>& assignments)
{
  _assignments = assignments;
}

}
}
