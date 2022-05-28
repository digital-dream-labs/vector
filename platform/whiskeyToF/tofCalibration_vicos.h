/**
 * File: tofCalibration_vicos.h
 *
 * Author: Al Chaussee
 * Created: 1/24/2019
 *
 * Description: ToF calibration related function
 *
 * Copyright: Anki, Inc. 2019
 *
 **/


#ifndef __tof_calibration_vicos_h__
#define __tof_calibration_vicos_h__

#include "whiskeyToF/vicos/vl53l1/platform/inc/vl53l1_platform_user_data.h"

#include <string>

// Run refspad, xtalk, and offset calibration at the given distance and target reflectance
int perform_calibration(VL53L1_Dev_t* dev, uint32_t dist_mm, float reflectance);

// Loads calibration data from disk and applies it to sensor
int load_calibration(VL53L1_Dev_t* dev);

// Sets where to save calibration data
void set_calibration_save_path(const std::string& path);

#endif
