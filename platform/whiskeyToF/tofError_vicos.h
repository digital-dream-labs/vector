 /**
 * File: tofError_vicos.h
 *
 * Author: Al Chaussee
 * Created: 1/24/2019
 *
 * Description: Helper functions for VL53L1 error checking
 *
 * Copyright: Anki, Inc. 2019
 *
 **/

#ifndef __tof_error_h__
#define __tof_error_h__

#include "whiskeyToF/vicos/vl53l1/core/inc/vl53l1_error_codes.h"

#define LOG_CHANNEL "ToF"

const char * const VL53L1ErrorToString(VL53L1_Error status);

#define return_if_error(status, msg, ...) {         \
    if(status != VL53L1_ERROR_NONE) {               \
      LOG_ERROR("ToF.return_if_error", "%s(%d) " msg, VL53L1ErrorToString(status), status, ##__VA_ARGS__); \
      return status;                                \
    }                                               \
  }

#endif
