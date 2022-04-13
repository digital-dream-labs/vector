/**
 * File: visionModeSchedule.h
 *
 * Author: Andrew Stein
 * Date:   10-28-2016
 *
 * Description: Container for keeping up with whether it is time to do a particular
 *              type of vision processing.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Anki_Cozmo_Basestation_VisionModeSchedule_H__
#define __Anki_Cozmo_Basestation_VisionModeSchedule_H__

#include "clad/types/visionModes.h"

#include "coretech/common/shared/types.h"

#include <array>
#include <list>
#include <vector>

namespace Anki {
namespace Vector {

class VisionModeSchedule
{
public:
  VisionModeSchedule(); // Default: always scheduled to run
  explicit VisionModeSchedule(std::vector<bool>&& initSchedule);
  explicit VisionModeSchedule(bool alwaysOnOrOff);
  explicit VisionModeSchedule(int onFrequency, int frameOffset = 0);
  
  Result SetFromJSON(const Json::Value& jsonSchedule);

  // Whether or not the schedule is true at index indicating it is time to process
  bool IsTimeToProcess(u32 index) const;

  // Returns whether the schedule will ever run (i.e. is not just "false" for all time)
  bool WillEverRun() const;

private:
  std::vector<bool> _schedule;

}; // class VisionModeSchedule


class AllVisionModesSchedule
{
public:
  using ModeScheduleList = std::list<std::pair<VisionMode, VisionModeSchedule>>;

  // If initWithDefaults=true, all modes' schedules are set to current defaults.
  // Otherwise, everything starts disabled.
  AllVisionModesSchedule(bool initWithDefaults = true);

  // Initialize specified modes with given schedules, and initialize any unspecified
  // modes' schedules to the current defaults. (If useDefaultsForUnspecified=false,
  // any unspecified modes will be disabled.)
  //
  // Example for setting one mode:
  //   AllVisionModesSchedule({{VisionMode::Markers, VisionModeSchedule(1)}})
  //
  // Example for setting two modes:
  //   AllVisionModesSchedule({{VisionMode::Pets,          VisionModeSchedule({true, false})},
  //                           {VisionMode::OverheadEdges, VisionModeSchedule({false, true})}});
  //
  AllVisionModesSchedule(const ModeScheduleList& schedules,
                         bool useDefaultsForUnspecified = true);

  // Get the schedule for a specific mode
  VisionModeSchedule& GetScheduleForMode(VisionMode mode);
  const VisionModeSchedule& GetScheduleForMode(VisionMode mode) const;

  // Returns whether it is time to process a mode
  bool IsTimeToProcess(VisionMode mode, u32 index) const;

  // Change the defaults to use for unspecified modes
  static Result SetDefaultSchedulesFromJSON(const Json::Value& config);
  static void SetDefaultSchedule(VisionMode mode, VisionModeSchedule&& schedule);

private:

  using ScheduleArray = std::array<VisionModeSchedule, (size_t)VisionMode::Count>;

  ScheduleArray _schedules;

  static ScheduleArray sDefaultSchedules;
  static ScheduleArray InitDefaultSchedules();

}; // class AllVisionModesSchedule


} // namespace Vector
} // namespace Anki

#endif /* __Anki_Cozmo_Basestation_VisionModeSchedule_H__ */
