/**
 * File: tof_vicos.cpp
 *
 * Author: Al Chaussee
 * Created: 10/18/2018
 *
 * Description: Defines interface to a tof sensor
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "whiskeyToF/tof.h"

#include "whiskeyToF/tofUserspace_vicos.h"
#include "whiskeyToF/tofCalibration_vicos.h"

#include "util/console/consoleInterface.h"
#include "util/console/consoleSystem.h"
#include "util/logging/logging.h"

#include "anki/cozmo/shared/cozmoEngineConfig.h"
#include "anki/cozmo/shared/factory/emrHelper.h"

#include <thread>
#include <mutex>
#include <queue>
#include <iomanip>
#include <chrono>

#ifdef SIMULATOR
#error SIMULATOR should be defined by any target using tof_vicos.cpp
#endif

namespace Anki {
namespace Vector {

namespace {

  // Handle to the actual device
  VL53L1_Dev_t _dev;

  // Thread and mutex for setting up and reading from the sensor
  std::thread _processor;
  std::mutex _mutex;
  std::atomic<bool> _stopProcessing;

  // Asynchonous commands in order to interact with the device on a thread
  std::mutex _commandLock;
  enum Command
    {
     StartRanging,
     StopRanging,
     SetupSensors,
     PerformCalibration,
    };
  std::queue<std::pair<Command, ToFSensor::CommandCallback>> _commandQueue;

  // Whether or not ranging is currently enabled
  std::atomic<bool> _rangingEnabled;

  // Whether or not calibration is currently running
  std::atomic<bool> _isCalibrating;

  // The latest available tof data
  RangeDataRaw _latestData;

  // Whether or not _latestData has updated since the last call to get it
  bool _dataUpdatedSinceLastGetCall = false;

  // Settings for calibration
  uint32_t _distanceToCalibTarget_mm = 0;
  float _calibTargetReflectance = 0;
}

ToFSensor* ToFSensor::_instance = nullptr;

ToFSensor* ToFSensor::getInstance()
{
  if(!IsWhiskey())
  {
    return nullptr;
  }
  
  if(nullptr == _instance)
  {
    _instance = new ToFSensor();
  }
  return _instance;
}

void ToFSensor::removeInstance()
{
  if(_instance != nullptr)
  {
    delete _instance;
    _instance = nullptr;
  }
}

void ToFSensor::SetLogPath(const std::string& path)
{
  #if FACTORY_TEST
  set_calibration_save_path(path);
  #endif
}

ToFSensor::CommandResult run_calibration(uint32_t distanceToTarget_mm,
                                         float targetReflectance)
{
  int rc = perform_calibration(&_dev, distanceToTarget_mm, targetReflectance);
  if(rc < 0)
  {
    PRINT_NAMED_ERROR("ToFSensor.PerformCalibration.RightFailed",
                      "Failed to calibrate right sensor %d",
                      rc);
  }

  _isCalibrating = false;

  ToFSensor::CommandResult res = (rc < 0 ?
                                  ToFSensor::CommandResult::CalibrateFailed :
                                  ToFSensor::CommandResult::Success);
  return res;
}

int ToFSensor::PerformCalibration(uint32_t distanceToTarget_mm,
                                  float targetReflectance,
                                  const CommandCallback& callback)
{
  #if FACTORY_TEST
  std::lock_guard<std::mutex> lock(_commandLock);
  _distanceToCalibTarget_mm = distanceToTarget_mm;
  _calibTargetReflectance = targetReflectance;
  _commandQueue.push({Command::PerformCalibration, callback});
  _isCalibrating = true;
  #endif
  
  return 0;
}


#define CONVERT_1616_TO_FLOAT(fixed) ((float)(fixed) * (float)(1/(2<<16))) 

// Parses and converts VL53L1_MultiRangingData_t into RangeDataRaw
void ParseData(VL53L1_MultiRangingData_t& mz_data,
               RangeDataRaw& rangeData)
{
  const int index = mz_data.RoiNumber;

  RangingData& roiData = rangeData.data[index];
  // Populate singular data from this ROI
  roiData.readings.clear();
  roiData.roi = index;
  roiData.numObjects = mz_data.NumberOfObjectsFound;
  roiData.roiStatus = mz_data.RoiStatus;
  roiData.spadCount = mz_data.EffectiveSpadRtnCount / 256.f;
  roiData.processedRange_mm = 0;

  // Populate the list of "readings" for this ROIs RangingData for each object detected
  if(mz_data.NumberOfObjectsFound > 0)
  {
    int16_t minDist = 1000; // Assuming max distance of 1000mm
    for(int r = 0; r < mz_data.NumberOfObjectsFound; r++)
    {
      RangeReading reading;
      reading.status = mz_data.RangeData[r].RangeStatus;

      // The following three readings come up in 16.16 fixed point so convert
      reading.signalRate_mcps = CONVERT_1616_TO_FLOAT(mz_data.RangeData[r].SignalRateRtnMegaCps);
      reading.ambientRate_mcps = CONVERT_1616_TO_FLOAT(mz_data.RangeData[r].AmbientRateRtnMegaCps);
      reading.sigma_mm = CONVERT_1616_TO_FLOAT(mz_data.RangeData[r].SigmaMilliMeter);
      reading.rawRange_mm = mz_data.RangeData[r].RangeMilliMeter;

      // For all valid detected objects in this ROI keep track of which one was the closest and
      // use that for the overall processedRange_mm
      if(mz_data.RangeData[r].RangeStatus == VL53L1_RANGESTATUS_RANGE_VALID)
      {
        const int16_t dist = mz_data.RangeData[r].RangeMilliMeter;
        if(dist < minDist)
        {
          minDist = dist;
        }
      }
      roiData.readings.push_back(reading);
    }
    roiData.processedRange_mm = minDist;
  }

}

// Get the most recent ranging data and parse it to a useable format
int ReadDataFromSensor(RangeDataRaw& rangeData)
{
  int rc = 0;  
  VL53L1_MultiRangingData_t mz_data;
  rc = get_mz_data(&_dev, 1, &mz_data);
  if(rc == 0)
  {
    ParseData(mz_data, rangeData);
  }
  else
  {
    PRINT_NAMED_ERROR("ReadDataFromSensor", "Failed to get mz data %d", rc);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  
  return rc;
}

// There are currently two issues with Start/StopRanging
// 1) Sometimes when starting ranging, we only ever get back invalid range readings.
//    You can stop and start ranging again and sometimes the sensor will recover.
//    It is also possible to go from valid readings to invalid readings after calling
//    stop and start.
// 2) It appears that calibration or some initial setting is changing when the sensor
//    is stopped/started. If you calibrate and then look at the readings, they look
//    very good. They are accurate within a couple of millimeters. However, if you stop
//    then start ranging again, all of the readings will have a ~30mm offset.
//    Most of calibration is still good as the readings are indifferent towards the target/material.
int ToFSensor::StartRanging(const CommandCallback& callback)
{
  std::lock_guard<std::mutex> lock(_commandLock);
  _commandQueue.push({Command::StartRanging, callback});
  return 0;
}

int ToFSensor::StopRanging(const CommandCallback& callback)
{
  std::lock_guard<std::mutex> lock(_commandLock);
  _commandQueue.push({Command::StopRanging, callback});
  return 0;
}

int ToFSensor::SetupSensors(const CommandCallback& callback)
{
  std::lock_guard<std::mutex> lock(_commandLock);
  _commandQueue.push({Command::SetupSensors, callback});
  return 0;
}

void ProcessLoop()
{
  while(!_stopProcessing)
  {
    // Process the next command if there is one
    _commandLock.lock();
    if(_commandQueue.size() > 0)
    {
      auto cmd = _commandQueue.front();
      _commandQueue.pop();
      _commandLock.unlock();

      ToFSensor::CommandResult res = ToFSensor::CommandResult::Success;
      switch(cmd.first)
      {
        case Command::StartRanging:
          {
            PRINT_NAMED_INFO("ToF.ProcessLoop.StartRanging", "");
            int rc = start_ranging(&_dev);
            if(rc < 0)
            {
              res = ToFSensor::CommandResult::StartRangingFailed;
              break;
            }

            _rangingEnabled = true;
          }
          break;
        case Command::StopRanging:
          {
            PRINT_NAMED_INFO("ToF.ProcessLoop.StopRanging","");
            int rc = stop_ranging(&_dev);
            if(rc < 0)
            {
              res = ToFSensor::CommandResult::StopRangingFailed;
              break;
            }
            
            _rangingEnabled = false;
          }
          break;
        case Command::SetupSensors:
          {
            PRINT_NAMED_INFO("ToF.ProcessLoop.SetupSensors","");
            _rangingEnabled = false;

            int rc = 0;
            // Only attempt to open the device if the file handle is invalid
            if(_dev.platform_data.i2c_file_handle <= 0)
            {
              rc = open_dev(&_dev);
              if(rc < 0)
              {
                res = ToFSensor::CommandResult::OpenDevFailed;
                PRINT_NAMED_ERROR("ToF.ProcessLoop.SetupSensors","Failed to open sensor");
                break;
              }
            }

            // Device is open so set it up for multizone ranging
            rc = setup(&_dev);
            if(rc < 0)
            {
              res = ToFSensor::CommandResult::SetupFailed;
              PRINT_NAMED_ERROR("ToF.ProcessLoop.SetupFailed","Failed to setup sensor");
            }

          }
          break;
        case Command::PerformCalibration:
          {
            PRINT_NAMED_INFO("ToF.ProcessLoop.PerformCalibration","");
            _rangingEnabled = false;
            res = run_calibration(_distanceToCalibTarget_mm, _calibTargetReflectance);
          }
          break;
      }

      // Call command callback
      if(cmd.second != nullptr)
      {
        cmd.second(res);
      }
    }
    else
    {
      _commandLock.unlock();
    }

    // As long as ranging is enabled, try to read data from the sensor
    if(_rangingEnabled)
    {
      // Note: static is important here in order to preserve previous readings
      // as, typically, only one ROI is read at a time
      static RangeDataRaw data;
      const int rc = ReadDataFromSensor(data);

      // static RangeDataRaw lastValid = data;
      // std::stringstream ss;
      // for(int i = 0; i < 4; i++)
      // {
      //   for(int j = 0; j < 4; j++)
      //   {
      //     if(data.data[i*4 + j].numObjects > 0 && data.data[i*4 + j].readings[0].status == 0)
      //     {
      //       ss << std::setw(7) << (uint32_t)(data.data[i*4 + j].processedRange_mm);
      //       lastValid.data[i*4 + j] = data.data[i*4 + j];
      //     }
      //     else
      //     {
      //       ss << std::setw(7) << (uint32_t)(lastValid.data[i*4 + j].processedRange_mm);
      //     }
      //   }
      //   ss << "\n";
      // }
      // //PRINT_NAMED_ERROR("","%s", ss.str().c_str());
      // printf("Data\n%s\n", ss.str().c_str());

      // Got a valid reading so update our latest data
      if(rc >= 0)
      {
        std::lock_guard<std::mutex> lock(_mutex);
        _dataUpdatedSinceLastGetCall = true;
        _latestData = data;
      }
    }
    // Ranging is not enabled so sleep
    else
    {
      // Sleep for half an engine tick so we can process commands by the time
      // the next engine tick happens
      std::this_thread::sleep_for(std::chrono::milliseconds(BS_TIME_STEP_MS/2));
    }
  }

  stop_ranging(&_dev);
  close_dev(&_dev);
}

bool ToFSensor::IsRanging() const
{
  return _rangingEnabled;
}

bool ToFSensor::IsCalibrating() const
{
  return _isCalibrating;
}

ToFSensor::ToFSensor()
{
  _rangingEnabled = false;
  _isCalibrating = false;
  _stopProcessing = false;
  memset(&_dev, 0, sizeof(_dev));
  _processor = std::thread(ProcessLoop);
}

ToFSensor::~ToFSensor()
{
  _stopProcessing = true;
  if(_processor.joinable())
  {
    _processor.join();
  }
}

Result ToFSensor::Update()
{
  return RESULT_OK;
}

RangeDataRaw ToFSensor::GetData(bool& hasDataUpdatedSinceLastCall)
{
  std::lock_guard<std::mutex> lock(_mutex);
  hasDataUpdatedSinceLastCall = _dataUpdatedSinceLastGetCall;
  _dataUpdatedSinceLastGetCall = false;
  return _latestData;
}

bool ToFSensor::IsValidRoiStatus(uint8_t status) const
{
  return (status != VL53L1_ROISTATUS_NOT_VALID);
}

}
}
