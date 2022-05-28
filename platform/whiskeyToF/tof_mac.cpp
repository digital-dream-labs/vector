/**
 * File: tof_mac.cpp
 *
 * Author: Al Chaussee
 * Created: 10/18/2018
 *
 * Description: Defines interface to a some number(2) of tof sensors
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "whiskeyToF/tof.h"

#include "util/helpers/templateHelpers.h"
#include "util/logging/logging.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/factory/emrHelper.h"

#include "simulator/controllers/shared/webotsHelpers.h"

#include <webots/Supervisor.hpp>
#include <webots/RangeFinder.hpp>

#ifndef SIMULATOR
#error SIMULATOR should be defined by any target using tof_mac.cpp
#endif

namespace Anki {
namespace Vector {

namespace {
  bool _engineSupervisorSet = false;
  webots::Supervisor* _engineSupervisor = nullptr;
  webots::RangeFinder* leftSensor_;
}

ToFSensor* ToFSensor::_instance = nullptr;

ToFSensor* ToFSensor::getInstance()
{
  if(!IsWhiskey())
  {
    return nullptr;
  }
  
  DEV_ASSERT(_engineSupervisorSet, "tof_mac.NoSupervisorSet");
  if(nullptr == _instance)
  {
    _instance = new ToFSensor();
  }
  return _instance;
}

void ToFSensor::removeInstance()
{
  Util::SafeDelete(_instance);
}

void ToFSensor::SetSupervisor(webots::Supervisor* sup)
{
  _engineSupervisor = sup;
  _engineSupervisorSet = true;
}

ToFSensor::ToFSensor()
{
  if(nullptr != _engineSupervisor)
  {
    DEV_ASSERT(ROBOT_TIME_STEP_MS >= _engineSupervisor->getBasicTimeStep(), "tof_mac.UnexpectedTimeStep");

    leftSensor_ = _engineSupervisor->getRangeFinder("leftRangeSensor");
    leftSensor_->enable(ROBOT_TIME_STEP_MS);

    // Make the CozmoVizDisplay (which includes the nav map, etc.) invisible to the Rangefinder. Note that the call to
    // setVisibility() requires a pointer to the Rangefinder NODE, _not_ the Rangefinder device (which is of type
    // webots::Rangefinder*). There seems to be no good way to get the underlying node pointer of the Rangefinder, so we
    // have to do this somewhat hacky iteration over all of the nodes in the world to find the Rangefinder node.
    const auto& vizNodes = WebotsHelpers::GetMatchingSceneTreeNodes(*_engineSupervisor, "CozmoVizDisplay");
          
    webots::Node* tofNode = nullptr;
    const int maxNodesToSearch = 10000;
    for (int i=0 ; i < maxNodesToSearch ; i++) {
      auto* node = _engineSupervisor->getFromId(i);
      if ((node != nullptr) && (node->getTypeName() == "RangeFinder")) {
        tofNode = node;
        break;
      }
    }
    DEV_ASSERT(tofNode != nullptr, "ToF.NoWebotsRangeFinderFound");
          
    for (const auto& vizNode : vizNodes) {
      vizNode.nodePtr->setVisibility(tofNode, false);
    }

  }
}

ToFSensor::~ToFSensor()
{

}

Result ToFSensor::Update()
{
  return RESULT_OK;
}

RangeDataRaw ToFSensor::GetData(bool& dataUpdated)
{
  dataUpdated = true;
  
  RangeDataRaw rangeData;

  const float* leftImage = leftSensor_->getRangeImage();

  for(int i = 0; i < leftSensor_->getWidth(); i++)
  {
    for(int j = 0; j < 4; j++)
    {
      int index = i*4 + j;
      RangingData& rData = rangeData.data[index];

      rData.numObjects = 1;
      rData.roiStatus = 0;
      rData.spadCount = 90.f;

      rData.roi = index;
      
      rData.processedRange_mm = leftImage[i*4 + j] * 1000;

      RangeReading reading;
      reading.signalRate_mcps = 25.f;
      reading.ambientRate_mcps = 0.25f;
      reading.sigma_mm = 0.f;
      reading.status = 0;

      reading.rawRange_mm = leftImage[i*4 + j] * 1000;
      rData.readings.push_back(reading);
    }
  }
  
  return rangeData;
}

int ToFSensor::SetupSensors(const CommandCallback& callback)
{
  if(callback != nullptr)
  {
    callback(CommandResult::Success);
  }
  return 0;
}

int ToFSensor::StartRanging(const CommandCallback& callback)
{
  if(callback != nullptr)
  {
    callback(CommandResult::Success);
  }
  return 0;
}

int ToFSensor::StopRanging(const CommandCallback& callback)
{
  if(callback != nullptr)
  {
    callback(CommandResult::Success);
  }
  return 0;
}

bool ToFSensor::IsRanging() const
{
  return true;
}

bool ToFSensor::IsValidRoiStatus(uint8_t status) const
{
  return true;
}
  
int ToFSensor::PerformCalibration(uint32_t distanceToTarget_mm,
                                  float targetReflectance,
                                  const CommandCallback& callback)
{
  if(callback != nullptr)
  {
    callback(CommandResult::Success);
  }
  return 0;
}

bool ToFSensor::IsCalibrating() const
{
  return false;
}

void ToFSensor::SetLogPath(const std::string& path)
{
  return;
}
  
}
}
