/**
 * File: tofUserspace_vicos.h
 *
 * Author: Al Chaussee
 * Created: 1/24/2019
 *
 * Description: Interface to ToF Userspace driver functions
 *
 * Copyright: Anki, Inc. 2019
 *
 **/


#ifndef __tof_userspace_vicos_h__
#define __tof_userspace_vicos_h__

#include "whiskeyToF/vicos/vl53l1/platform/inc/vl53l1_platform_user_data.h"

// Open the ToF I2C device
int open_dev(VL53L1_Dev_t* dev);

// Close the ToF device
int close_dev(VL53L1_Dev_t* dev);

// Setup/configure the device for multizone ranging
int setup(VL53L1_Dev_t* dev);

// Apply a rows x cols roi grid to the device (for multizone ranging)
int setup_roi_grid(VL53L1_Dev_t* dev, const int rows, const int cols);

// Get current multizone ranging data
int get_mz_data(VL53L1_Dev_t* dev, const int blocking, VL53L1_MultiRangingData_t *data);

// Start ranging
int start_ranging(VL53L1_Dev_t* dev);

// Stop ranging
int stop_ranging(VL53L1_Dev_t* dev);

#endif
