/**
 * File: tofCalibration_vicos.cpp
 *
 * Author: Al Chaussee
 * Created: 1/24/2019
 *
 * Description: ToF calibration related functions
 *
 * Copyright: Anki, Inc. 2019
 *
 **/

#include "whiskeyToF/tofCalibration_vicos.h"
#include "whiskeyToF/tofUserspace_vicos.h"
#include "whiskeyToF/tofError_vicos.h"

#include "whiskeyToF/vicos/vl53l1/core/inc/vl53l1_api.h"

#include "util/logging/logging.h"

#include <sys/stat.h>
#include <errno.h>

namespace
{
  // Where to save calibration data
  std::string _savePath = "";
}

// --------------------Save/Load Calibration Data--------------------
void set_calibration_save_path(const std::string& path)
{
  _savePath = path;
}

// Path is expected to end in "/"
int save_calibration_to_disk(void* calib,
                             ssize_t size,
                             const std::string& path,
                             const std::string& filename)
{
  PRINT_NAMED_INFO("save_calibration_to_disk","saving to %s%s", path.c_str(), filename.c_str());
  
  char buf[128] = {0};
  sprintf(buf, "%s%s", path.c_str(), filename.c_str());

  int rc = -1;
  FILE* f = fopen(buf, "w+");
  if(f != nullptr)
  {
    const size_t count = 1;
    rc = fwrite(calib, size, count, f);
    if(rc != count)
    {
      PRINT_NAMED_ERROR("save_calibration_to_disk","fwrite to %s failed with %d", buf, ferror(f));
      rc = -1;
    }
    else
    {
      rc = 0;
    }
    (void)fclose(f);
  }
  else
  {
    PRINT_NAMED_ERROR("save_calibration_to_disk","failed to open file %s with %d", buf, errno);
  }
  return rc;

}

int save_calibration_to_disk(VL53L1_CalibrationData_t& data,
                             const std::string& meta = "")
{
  const std::string name = "tof" + meta + ".bin";
  (void)save_calibration_to_disk(&data, sizeof(data), _savePath, name);
  return save_calibration_to_disk(&data, sizeof(data), "/factory/", name);
}

int save_calibration_to_disk(VL53L1_ZoneCalibrationData_t& data,
                             const std::string& meta = "")
{
  const std::string name = "tofZone" + meta + ".bin";
  (void)save_calibration_to_disk(&data, sizeof(data), _savePath, name);
  return save_calibration_to_disk(&data, sizeof(data), "/factory/", name);
}

int load_calibration_from_disk(void* calib,
                               ssize_t size,
                               const std::string& path)
{
  int rc = -1;
  FILE* f = fopen(path.c_str(), "r");
  if(f != nullptr)
  {
    const size_t count = 1;
    rc = fread(calib, size, count, f);
    if(rc != count)
    {
      PRINT_NAMED_ERROR("load_calibration_from_disk","fread failed %d", ferror(f));
      rc = -1;
    }
    else
    {
      rc = 0;
    }
    (void)fclose(f);
  }
  else
  {
    PRINT_NAMED_ERROR("load_calibration_from_disk", "failed to open %s with %d", path.c_str(), errno);
  }
  
  return rc;
}

int load_calibration(VL53L1_Dev_t* dev)
{
  PRINT_NAMED_INFO("load_calibration", "Loading calibration");

  VL53L1_CalibrationData_t calib;
  memset(&calib, 0, sizeof(calib));

  int rc = load_calibration_from_disk(&calib, sizeof(calib), "/factory/tof.bin");
  if(rc < 0)
  {
    PRINT_NAMED_INFO("load_calibration", "Attempting to load old calib format");
    
    rc = load_calibration_from_disk(&calib, sizeof(calib), "/factory/tof_right.bin");
    if(rc < 0)
    {
      PRINT_NAMED_ERROR("load_calibration", "Failed to load old tof calibration");
    }
  }

  rc = VL53L1_SetCalibrationData(dev, &calib);
  if(rc < 0)
  {
    PRINT_NAMED_ERROR("load_calibration", "Failed to set tof calibration");
  }

  VL53L1_ZoneCalibrationData_t calibZone;
  memset(&calibZone, 0, sizeof(calibZone));

  // Check if the old format zone calibration exists
  struct stat st;
  rc = stat("/factory/tofZone_right.bin", &st);
  if(rc == 0)
  {
    // DVT1 zone calibration was saved as the "stmvl531_zone_calibration_data_t" structure
    // from the kernel driver. This structure contains a u32 id field before the VL53L1_ZoneCalibrationData_t.
    // So we need to recreate the layout of that struct in order to properly load the saved calibration data.
    struct {
      uint32_t garbage;
      VL53L1_ZoneCalibrationData_t data;
    } buf;

    PRINT_NAMED_INFO("load_calibration","Loading zone data as old format");
    rc = load_calibration_from_disk(&buf, sizeof(buf), "/factory/tofZone_right.bin");
    if(rc == 0)
    {
      calibZone = buf.data;
    }
  }
  else
  {
    PRINT_NAMED_INFO("load_calibration","Loading zone data");
    rc = load_calibration_from_disk(&calibZone, sizeof(calibZone), "/factory/tofZone.bin");
  }

  if(rc < 0)
  {
    PRINT_NAMED_ERROR("load_calibration","Failed to load tof zone calibration");
  }
  else
  {
    rc = VL53L1_SetZoneCalibrationData(dev, &calibZone);
    return_if_error(rc, "Failed to set tof zone calibration");
  }

  return rc;
}


// --------------------Reference SPAD Calibration--------------------
int run_refspad_calibration(VL53L1_Dev_t* dev)
{
  VL53L1_CalibrationData_t calib;
  memset(&calib, 0, sizeof(calib));
  
  VL53L1_Error rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "Get calibration data failed");

  rc = VL53L1_PerformRefSpadManagement(dev);
  return_if_error(rc, "RefSPAD calibration failed");

  memset(&calib, 0, sizeof(calib));
  rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "Get calibration data failed");

  rc = save_calibration_to_disk(calib);
  return_if_error(rc, "Save calibration to disk failed");

  rc = VL53L1_SetCalibrationData(dev, &calib);
  return_if_error(rc, "Set calibration data failed");

  return rc;
}


// --------------------Crosstalk Calibration--------------------
void zero_xtalk_calibration(VL53L1_CalibrationData_t& calib)
{
  calib.customer.algo__crosstalk_compensation_plane_offset_kcps = 0;
  calib.customer.algo__crosstalk_compensation_x_plane_gradient_kcps = 0;
  calib.customer.algo__crosstalk_compensation_y_plane_gradient_kcps = 0;                  
  memset(&calib.xtalkhisto, 0, sizeof(calib.xtalkhisto));
}

int perform_xtalk_calibration(VL53L1_Dev_t* dev)
{  
  // Make sure we are in the correct preset mode
  (void)VL53L1_SetPresetMode(dev, VL53L1_PRESETMODE_MULTIZONES_SCANNING);
  return VL53L1_PerformXTalkCalibration(dev, VL53L1_XTALKCALIBRATIONMODE_SINGLE_TARGET);
}

int run_xtalk_calibration(VL53L1_Dev_t* dev)
{
  VL53L1_CalibrationData_t calib;
  memset(&calib, 0, sizeof(calib));
  
  VL53L1_Error rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "run_xtalk_calibration: get calibration data failed");

  rc = perform_xtalk_calibration(dev);

  bool noXtalk = false;
  if(rc == VL53L1_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL)
  {
    PRINT_NAMED_INFO("run_xtalk_calibration","No crosstalk found");
    noXtalk = true;
  }
  
  memset(&calib, 0, sizeof(calib));
  rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "run_xtalk_calibration: get calibration data failed");

  // If there was no crosstalk calibration needed then zero-out xtalk calibration data
  // before setting it
  if(noXtalk)
  {
    zero_xtalk_calibration(calib);
  }
    
  rc = save_calibration_to_disk(calib);
  return_if_error(rc, "run_xtalk_calibration: Save calibration to disk failed");

  rc = VL53L1_SetCalibrationData(dev, &calib);
  return_if_error(rc, "run_xtalk_calibration: Set calibration data failed");
  
  return rc;
}

// --------------------Offset Calibration--------------------
int perform_offset_calibration(VL53L1_Dev_t* dev, uint32_t dist_mm, float reflectance)
{
  VL53L1_Error rc = VL53L1_SetOffsetCalibrationMode(dev, VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE);
  return_if_error(rc, "perform_offset_calibration: SetOffsetCalibrationMode failed");
  
  return VL53L1_PerformOffsetCalibration(dev, dist_mm, (FixPoint1616_t)(reflectance * (2 << 16)));
}

int run_offset_calibration(VL53L1_Dev_t* dev, uint32_t distanceToTarget_mm, float targetReflectance)
{
  VL53L1_CalibrationData_t calib;
  memset(&calib, 0, sizeof(calib));

  int rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "run_offset_calibration: Get calibration data failed");

  rc = setup_roi_grid(dev, 4, 4);
  return_if_error(rc, "run_offset_calibration: error setting up roi grid");

  rc = perform_offset_calibration(dev, distanceToTarget_mm, targetReflectance);
  return_if_error(rc, "run_offset_calibration: offset calibration failed");

  memset(&calib, 0, sizeof(calib));
  rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "run_offset_calibration: Get calibration data failed");

  rc = save_calibration_to_disk(calib);
  return_if_error(rc, "run_offset_calibration: Save calibration to disk failed");

  rc = VL53L1_SetCalibrationData(dev, &calib);
  return_if_error(rc, "run_offset_calibration: Set calibration data failed");

  // Offset calibration populates the ZoneCalibrationData struct when configured for multizones
  // so we also need to save that calibration data
  VL53L1_ZoneCalibrationData_t calibZone;
  memset(&calibZone, 0, sizeof(calibZone));
  
  rc = VL53L1_GetZoneCalibrationData(dev, &calibZone);
  return_if_error(rc, "run_offset_calibration: Get zone calib failed");
  
  rc = save_calibration_to_disk(calibZone);
  return_if_error(rc, "run_offset_calibration: Save zone calibration to disk failed");

  rc = VL53L1_SetZoneCalibrationData(dev, &calibZone);
  return_if_error(rc, "run_offset_calibration: Set zone calibration data failed");

  return rc;
}

int perform_calibration(VL53L1_Dev_t* dev, uint32_t dist_mm, float reflectance)
{
  // Stop all ranging so we can change settings
  VL53L1_Error rc = VL53L1_StopMeasurement(dev);
  return_if_error(rc, "perform_calibration: error stopping ranging");

  // Switch to multi-zone scanning mode
  rc = VL53L1_SetPresetMode(dev, VL53L1_PRESETMODE_MULTIZONES_SCANNING);
  return_if_error(rc, "perform_calibration: error setting preset_mode");

  // Setup ROIs
  rc = setup_roi_grid(dev, 4, 4);
  return_if_error(rc, "perform_calibration: error setting up roi grid");

  // Setup timing budget
  rc = VL53L1_SetMeasurementTimingBudgetMicroSeconds(dev, 8*2000);
  return_if_error(rc, "perform_calibration: error setting timing budged");

  // Set distance mode
  rc = VL53L1_SetDistanceMode(dev, VL53L1_DISTANCEMODE_SHORT);
  return_if_error(rc, "perform_calibration: error setting distance mode");

  // Set output mode
  rc = VL53L1_SetOutputMode(dev, VL53L1_OUTPUTMODE_STRONGEST);
  return_if_error(rc, "perform_calibration: error setting distance mode");

  // Disable xtalk compensation
  rc = VL53L1_SetXTalkCompensationEnable(dev, 0);
  return_if_error(rc, "perform_calibration: error setting live xtalk");

  VL53L1_CalibrationData_t calib;
  memset(&calib, 0, sizeof(calib));
  VL53L1_SetCalibrationData(dev, &calib);
  
  rc = VL53L1_GetCalibrationData(dev, &calib);
  return_if_error(rc, "perform_calibration: Get calibration data failed");

  rc = save_calibration_to_disk(calib, "Orig");
  return_if_error(rc, "perform_calibration: Save calibration to disk failed");

  rc = run_refspad_calibration(dev);
  return_if_error(rc, "perform_calibration: run_refspad_calibration");
  
  rc = run_xtalk_calibration(dev);
  return_if_error(rc, "perform_calibration: run_xtalk_calibration");

  rc = run_offset_calibration(dev, dist_mm, reflectance);
  return_if_error(rc, "perform_calibration: run_offset_calibration");

  return rc;
}





