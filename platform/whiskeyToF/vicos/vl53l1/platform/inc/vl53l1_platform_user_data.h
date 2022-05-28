
/*******************************************************************************
 This file is part of VL53L1 Platform

 Copyright (c) 2017, STMicroelectronics - All Rights Reserved

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _VL53L1_PLATFORM_USER_DATA_H_
#define _VL53L1_PLATFORM_USER_DATA_H_

#ifndef __KERNEL__
#include <stdlib.h>
#endif

#include "vl53l1_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// I2C driver handle etc.
typedef struct {
  int i2c_file_handle;
  uint16_t slave_address;
} PlatformDevData;

typedef struct {
  VL53L1_DevData_t   Data;
  uint8_t   i2c_slave_address;
  uint8_t   comms_type;
  uint16_t  comms_speed_khz;
  uint32_t  new_data_ready_poll_duration_ms;
  PlatformDevData platform_data;
} VL53L1_Dev_t;

typedef VL53L1_Dev_t *VL53L1_DEV;

#define VL53L1DevDataGet(Dev, field) (Dev->Data.field)
#define VL53L1DevDataSet(Dev, field, VL53L1_p_002) ((Dev->Data.field) = (VL53L1_p_002))
#define VL53L1DevStructGetLLDriverHandle(Dev) (&Dev->Data.LLData)
#define VL53L1DevStructGetLLResultsHandle(Dev) (&Dev->Data.llresults)

#ifdef __cplusplus
}
#endif

#endif


