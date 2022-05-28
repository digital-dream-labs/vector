/**
 * File: tof.h
 *
 * Author: Al Chaussee
 * Created: 10/18/2018
 *
 * Description: Defines interface to a some number(2) of tof sensors
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __platform_tof_h__
#define __platform_tof_h__

#include "coretech/common/shared/types.h"
#include "clad/types/tofTypes.h"

namespace webots {
  class Supervisor;
}

namespace Anki {
namespace Vector {

class ToFSensor
{
public:

  static ToFSensor* getInstance();
  static bool hasInstance() { return nullptr != _instance; }
  static void removeInstance();

  ~ToFSensor();

  Result Update();

#ifdef SIMULATOR

  static void SetSupervisor(webots::Supervisor* sup);

#endif

  // Get the latest ToF reading
  // hasDataUpdatedSinceLastCall indicates if the reading has changed since the last time this
  // function was called
  // Data is only updated while ranging is enabled
  RangeDataRaw GetData(bool& hasDataUpdatedSinceLastCall);

  enum class CommandResult
  {
    Success = 0,
    Failure = -1,
    OpenDevFailed,
    SetupFailed,
    StartRangingFailed,
    StopRangingFailed,
    CalibrateFailed,
  };
  // All CommandCallbacks are called from a thread
  using CommandCallback = std::function<void(CommandResult)>;

  // Request the ToF device to be setup and configured for ranging
  int SetupSensors(const CommandCallback& callback);

  // Start ranging
  int StartRanging(const CommandCallback& callback);

  // Stop ranging
  int StopRanging(const CommandCallback& callback);

  // Whether or not the device is actively ranging
  bool IsRanging() const;

  // Whether or not the RoiStatus is considered valid
  bool IsValidRoiStatus(uint8_t status) const;
  
  // Run the calibration procedure at the given distance and target reflectance percentage
  // There are 3 calibration steps, each has certain requirements
  // - Reference SPAD calibration
  //     No target should be directly on top of device
  // - Crosstalk calibration
  //     No target below 800mm, dark environment/no IR contribution
  // - Offset calibration
  //     Suggests a 5% reflectance target at 140mm, no IR contribution
  // These are just suggested setups, other setups will work for crosstalk and offset calibration.
  // - Crosstalk
  //     Idea is you just want to capture photons coming back due to the coverglass which is
  //     why you don't want to be looking at a target as the photons reflecting off the target
  //     will overwhelm the photons from the coverglass. It is possible to use a target with low
  //     reflectance at a closer distance instead of no target. To figure out proper setup, do offset
  //     calibration first and then try different targets and distances until you are 10% under ranging
  //     (distance returned is 10% of what it should be). The under ranging is due to the photons coming
  //     from the coverglass as opposed to the target.
  // - Offset
  //     Lots of different setups available by varying distance and target reflectance. Need a distance
  //     and target such that you get enough photons to not be affected by the coverglass but not so many
  //     that you saturate the sensor. Medium reflectance at somewhere between 100mm and 400mm should be good.
  //     To go closer < 100mm, you would need lower reflectance like 5%.
  int PerformCalibration(uint32_t distanceToTarget_mm,
                         float targetReflectance,
                         const CommandCallback& callback);

  // Whether or not the device is currently calibrating
  bool IsCalibrating() const;

  // Set where to save calibration
  void SetLogPath(const std::string& path);
  
private:
  
  ToFSensor();
  
  static ToFSensor* _instance;
};
  
}
}

#endif
