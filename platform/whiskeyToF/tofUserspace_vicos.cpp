 /**
 * File: tofUserspace_vicos.cpp
 *
 * Author: Al Chaussee
 * Created: 10/18/2018
 *
 * Description: Defines interface to a some number(2) of tof sensors
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include "whiskeyToF/tofUserspace_vicos.h"
#include "whiskeyToF/tofCalibration_vicos.h"
#include "whiskeyToF/tofError_vicos.h"

#include "whiskeyToF/vicos/vl53l1/core/inc/vl53l1_api.h"
#include "whiskeyToF/vicos/vl53l1/core/inc/vl53l1_api_core.h"
#include "whiskeyToF/vicos/vl53l1/core/inc/vl53l1_error_codes.h"
#include "whiskeyToF/vicos/vl53l1/platform/inc/vl53l1_platform_user_config.h"
#include "whiskeyToF/vicos/vl53l1/platform/inc/vl53l1_platform_init.h"
#include "whiskeyToF/vicos/vl53l1/platform/inc/vl53l1_platform_log.h"

#include "clad/types/tofTypes.h"

#include "util/logging/logging.h"

#include "platform/gpio/gpio.h"

#include <fcntl.h>
#include <unistd.h>

#define SPAD_COLS (16)  ///< What's in the sensor
#define SPAD_ROWS (16)  ///< What's in the sensor
#define SPAD_MIN_ROI (4) ///< Minimum ROI size in spads
#define MAX_ROWS (SPAD_ROWS / SPAD_MIN_ROI)
#define MAX_COLS (SPAD_COLS / SPAD_MIN_ROI)

#define POWER_GPIO 0 // XSHUT1

#define LOG_CHANNEL "ToF"

namespace {
  GPIO _powerGPIO = nullptr;
}

int open_dev(VL53L1_Dev_t* dev)
{
  int res = gpio_create(POWER_GPIO, gpio_DIR_OUTPUT, gpio_LOW, &_powerGPIO);
  if(res < 0)
  {
    LOG_ERROR("ToF.open_dev", "Failed to open gpio %d", POWER_GPIO);
    return VL53L1_ERROR_GPIO_NOT_EXISTING;
  }

  usleep(100000);

  gpio_set_value(_powerGPIO, gpio_HIGH);

  // Wait for FW boot coming out of HW standby
  usleep(100000);

  VL53L1_Error status = VL53L1_ERROR_NONE;

  // Initialize the platform interface
  dev->platform_data.i2c_file_handle = open("/dev/i2c-6", O_RDWR);
  if(dev->platform_data.i2c_file_handle < 0)
  {
    LOG_ERROR("ToF.open_dev", "Failed to open /dev/i2c-6 %d", errno);
    return VL53L1_ERROR_INVALID_PARAMS;
  }
  
  status = VL53L1_platform_init(dev,
                                0x29,
                                1, /* comms_type  I2C*/
                                400);       /* comms_speed_khz - 400kHz recommended */
  return_if_error(status, "Failed to init platform");

  // Wait 2 sec for supplies to stabilize
  status = VL53L1_WaitMs(dev, 2000);
  return_if_error(status, "WaitMS failed");
  
  // Wait for firmware to finish booting
  status = VL53L1_WaitDeviceBooted(dev);
  return_if_error(status, "WaitDeviceBooted failed");

  // Initialise Dev data structure
  status = VL53L1_DataInit(dev);
  return_if_error(status, "DataInit failed");

  VL53L1_DeviceInfo_t deviceInfo;
  status = VL53L1_GetDeviceInfo(dev, &deviceInfo);
  return_if_error(status, "GetDeviceInfo failed");

  LOG_INFO("ToF.open_dev",
           "Name: %s Type: %s ID: %s Ver: %d.%d",
           deviceInfo.Name,
           deviceInfo.Type,
           deviceInfo.ProductId,
           deviceInfo.ProductRevisionMajor,
           deviceInfo.ProductRevisionMinor);

  if ((deviceInfo.ProductRevisionMajor != 1) || (deviceInfo.ProductRevisionMinor != 1))
  {
    LOG_ERROR("ToF.open_dev.UnexpecedVersion",
              "Warning expected cut 1.1 but found cut %d.%d",
              deviceInfo.ProductRevisionMajor,
              deviceInfo.ProductRevisionMinor);
  }

  status = VL53L1_StaticInit(dev);
  return_if_error(status, "StaticInit failed");

  const int rc = load_calibration(dev);
  return_if_error(rc, "load_calibration failed");

  return status;
}

int close_dev(VL53L1_Dev_t* dev)
{
  int rc = VL53L1_StopMeasurement(dev);
  
  rc = VL53L1_platform_terminate(dev);
  if(rc == VL53L1_ERROR_NONE)
  {
    dev->platform_data.i2c_file_handle = -1;
  }

  if(_powerGPIO != nullptr)
  {
    gpio_set_value(_powerGPIO, gpio_LOW);
    
    gpio_close(_powerGPIO);
    _powerGPIO = nullptr;
  }
  
  return rc;
}

/// Setup grid of ROIs for scanning
int setup_roi_grid(VL53L1_Dev_t* dev, const int rows, const int cols)
{
  VL53L1_RoiConfig_t roiConfig;
  int i, r, c;
  const int n_roi = rows*cols;
  const int row_step = SPAD_ROWS / rows;
  const int col_step = SPAD_COLS / cols;

  if (rows > MAX_ROWS)
  {
    LOG_ERROR("ToF.setup_roi_grid", "Cannot set %d rows (max %d)", rows, MAX_ROWS);
    return -1;
  }
  
  if (rows < 1)
  {
    LOG_ERROR("ToF.setup_roi_grid", "Cannot set %d rows, min 1", rows);
    return -1;
  }
  
  if (cols > MAX_COLS)
  {
    LOG_ERROR("ToF.setup_roi_grid", "Cannot set %d cols (max %d)", cols, MAX_ROWS);
    return -1;
  }
  
  if (cols < 1)
  {
    LOG_ERROR("ToF.setup_roi_grid", "Cannot set %d cols, min 1", cols);
    return -1;
  }
  
  if (n_roi > VL53L1_MAX_USER_ZONES)
  {
    LOG_ERROR("ToF.setup_roi_grid", "%drows * %dcols = %d > %d max user zones",
              rows, cols, n_roi, VL53L1_MAX_USER_ZONES);
    return -1;
  }

  i = 0;
  for (r=0; r<rows; ++r)
  {
    for (c=0; c<cols; ++c)
    {
      roiConfig.UserRois[i].TopLeftX = c * col_step;
      roiConfig.UserRois[i].TopLeftY = ((r+1) * row_step) - 1;
      roiConfig.UserRois[i].BotRightX = ((c+1) * col_step) - 1;
      roiConfig.UserRois[i].BotRightY = r * row_step;
      ++i;
    }
  }

  roiConfig.NumberOfRoi = n_roi;
  
  return VL53L1_SetROI(dev, &roiConfig);
}

/// Setup 4x4 multi-zone imaging
int setup(VL53L1_Dev_t* dev)
{
  VL53L1_Error rc = 0;

  // // Stop all ranging so we can change settings
  // rc = VL53L1_StopMeasurement(dev);
  // return_if_error(rc, "ioctl error stopping ranging");

  // Switch to multi-zone scanning mode
  rc = VL53L1_SetPresetMode(dev, VL53L1_PRESETMODE_MULTIZONES_SCANNING);
  return_if_error(rc, "ioctl error setting preset_mode");

  // Setup ROIs
  rc = setup_roi_grid(dev, 4, 4);
  return_if_error(rc, "ioctl error setting up preset grid");

  // Set distance mode
  rc = VL53L1_SetDistanceMode(dev, VL53L1_DISTANCEMODE_SHORT);
  return_if_error(rc, "ioctl error setting distance mode");

  // Set output mode
  rc = VL53L1_SetOutputMode(dev, VL53L1_OUTPUTMODE_STRONGEST);
  return_if_error(rc, "ioctl error setting distance mode");

  rc = VL53L1_SetXTalkCompensationEnable(dev, 0);
  return_if_error(rc, "ioctl error setting live xtalk");

  rc = VL53L1_SetOffsetCorrectionMode(dev, VL53L1_OFFSETCORRECTIONMODE_PERZONE);
  return_if_error(rc, "ioctl error setting offset correction mode");

  // Setup timing budget
  rc = VL53L1_SetMeasurementTimingBudgetMicroSeconds(dev, 16000);
  return_if_error(rc, "ioctl error setting timing budged");

  return rc;
}

/// Get multi-zone ranging data measurements.
int get_mz_data(VL53L1_Dev_t* dev, const int blocking, VL53L1_MultiRangingData_t *data)
{
  VL53L1_Error rc = 0;
  if(blocking)
  {
    rc = VL53L1_WaitMeasurementDataReady(dev);
    if(rc < 0)
    {
      VL53L1_ClearInterruptAndStartMeasurement(dev);
    }
    return_if_error(rc, "get_mz_data WaitMeasurementDataReady Failed");
  }
  else
  {
    uint8_t dataReady = 0;
    rc = VL53L1_GetMeasurementDataReady(dev, &dataReady);
    return_if_error(rc, "get_mz_data GetMeasurementDataReady failed");

    if(!dataReady)
    {
      return -1;
    }
  }

  rc = VL53L1_GetMultiRangingData(dev, data);
  if(rc == VL53L1_ERROR_NONE)
  {
    VL53L1_ClearInterruptAndStartMeasurement(dev);
  }
 
  return rc;
}

int start_ranging(VL53L1_Dev_t* dev)
{
  const int rc = VL53L1_StartMeasurement(dev);
  return_if_error(rc, "start_ranging failed");
  return 0;
}

int stop_ranging(VL53L1_Dev_t* dev)
{
  const int rc = VL53L1_StopMeasurement(dev);
  return_if_error(rc, "stop_ranging failed");
  return 0;
}
