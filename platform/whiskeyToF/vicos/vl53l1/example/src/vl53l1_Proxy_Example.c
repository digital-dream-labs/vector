/*
* Copyright (c) 2017, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either
* 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones
* mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
********************************************************************************
*
*/
#include "vl53l1_api.h"
#include "vl53l1_platform_init.h"

#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_CORE, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)

void print_pal_error(VL53L1_Error Status){
	char buf[VL53L1_MAX_STRING_LENGTH];
	VL53L1_GetPalErrorString(Status, buf);
	printf("API Status: %i : %s\n", Status, buf);
}

uint8_t SD_Proxy(VL53L1_MultiRangingData_t *pMultiRangingData, uint8_t prevObjectInProximity, int LowThres, int HighThres)
{

       uint8_t NbObj, ValidRange, i;
       uint8_t objectInProximity;
       uint8_t RngSta;
       int16_t Range;


       NbObj = pMultiRangingData->NumberOfObjectsFound;
       ValidRange = 0;
       Range = 5000; /* arbitrary set no object range value to 5 meters */

       objectInProximity = prevObjectInProximity;

       i = 0;
       /* by default VL53L1_GetMultiRangingData gives objects from the nearest to the farest */
       while ((!ValidRange) && (i < NbObj)) {
             RngSta = pMultiRangingData->RangeData[i].RangeStatus;
             ValidRange = (RngSta == VL53L1_RANGESTATUS_TARGET_PRESENT_LACK_OF_SIGNAL);
             ValidRange = ValidRange || (RngSta == VL53L1_RANGESTATUS_RANGE_VALID);
             if (ValidRange) {
                    Range = pMultiRangingData->RangeData[i].RangeMilliMeter;
             }
             else
                    i++;
       }
       if (ValidRange) {
             /* We found one object, let's check its distance*/
             if (prevObjectInProximity==0 && Range < LowThres)
                    objectInProximity = 1;
             if (prevObjectInProximity==1 && Range > HighThres)
                    objectInProximity = 0;
       }
       return objectInProximity;
}

/*
* LowThres & HighThres limits expressed in millimeters
*/
int  RunProxyDetectionLoop(VL53L1_DEV dev, int LowThres, int HighThres)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_MultiRangingData_t MultiRangingData;
	uint8_t NbObj, ValidRange, i;
	uint8_t objectInProximity, prevpyt, pyt;
	uint8_t RngSta;
	int16_t Range;

	Status = VL53L1_StartMeasurement(dev);
	/* Very first ranging measurement completion interrupt must be ignored */
	Status = VL53L1_WaitMeasurementDataReady(dev);
	Status = VL53L1_GetMultiRangingData(dev, &MultiRangingData);
	Status = VL53L1_ClearInterruptAndStartMeasurement(dev);

	objectInProximity = 0;
	prevpyt = pyt = 0;

	/* Actual measurement loop starts here... */
	while (1)
	{
		/* Wait for range completion */
		Status = VL53L1_WaitMeasurementDataReady(dev);
		if (Status == VL53L1_ERROR_NONE)
		{
			Status = VL53L1_GetMultiRangingData(dev, &MultiRangingData);
			Status = VL53L1_ClearInterruptAndStartMeasurement(dev);

#if 0
			/* Display all measurements whatever the range status */
			NbObj = MultiRangingData.NumberOfObjectsFound;
			if (NbObj>0){
				printf("#objs %d ", NbObj);
				for (i=0; i<NbObj; i++) {
					printf("rng %d st %d ", MultiRangingData.RangeData[i].RangeMilliMeter,
						MultiRangingData.RangeData[i].RangeStatus);
				}
				printf("\n");
			}
#endif

			if (Status == VL53L1_ERROR_NONE) {
				NbObj = MultiRangingData.NumberOfObjectsFound;
				ValidRange = 0;
				Range = 5000; /* arbitrary set no object range value to 5 meters */
				i = 0;
				/* by default VL53L1_GetMultiRangingData gives objects from the nearest to the farest */
				while ((!ValidRange) && (i < NbObj)) {
					RngSta = MultiRangingData.RangeData[i].RangeStatus;
					ValidRange = (RngSta == VL53L1_RANGESTATUS_TARGET_PRESENT_LACK_OF_SIGNAL);
					ValidRange = ValidRange || (RngSta == VL53L1_RANGESTATUS_RANGE_VALID);
					if (ValidRange) {
						Range = MultiRangingData.RangeData[i].RangeMilliMeter;
					}
					else
						i++;
				}
				if (ValidRange) {
					/* We found one object, let's check its distance*/
					if (Range < LowThres)
						objectInProximity = 1;
					if (Range > HighThres)
						objectInProximity = 0;
					printf("Nearest object distance %d \t", Range);
					if (objectInProximity)
						printf("[PROXYMITY_FLAG]");
//					printf("\n");
//					fflush(stdout);
				}

			prevpyt = pyt;
			pyt = SD_Proxy(&MultiRangingData, prevpyt, LowThres, HighThres);
			printf(" SD_Proxy() result %d previous %d\n", pyt, prevpyt);
			fflush(stdout);
			}
		}
	}

	// Call StopMeasurement whatever the previous status but take into account its own error as final result
	VL53L1_StopMeasurement(dev);
	return Status;
}




int main(int argc, char **argv)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Dev_t                   dev;
	VL53L1_DEV                     Dev = &dev;
	VL53L1_PresetModes             PresetMode;
	VL53L1_Version_t               Version;
	VL53L1_ll_version_t            llVersion;
	VL53L1_RoiConfig_t OneRoi4_3 = {1, .UserRois = {{0, 14, 15, 1}}};
#ifdef VL53L1_LOG_ENABLE
	char filename[] = "./trace.log";
#endif

	SUPPRESS_UNUSED_WARNING(argc);
	SUPPRESS_UNUSED_WARNING(argv);

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	/*
	* Initialize the platform interface
	*/
	if (Status == VL53L1_ERROR_NONE)
	Status = VL53L1_platform_init(
		Dev,
		(0x29 << 1), /* EVK requires 8-bit I2C */
		1, /* comms_type  I2C*/
		400);       /* comms_speed_khz - 400kHz recommended */

	/*
	*  Wait for firmware to finish booting
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_WaitDeviceBooted(Dev);

	printf ("VL53L1 Proxy detection example\n");

	VL53L1_GetVersion(&Version);
	printf("driver version\t %d.%d.%d rev %d\n",
			Version.major, Version.minor, Version.build, Version.revision);
	VL53L1_get_version(Dev, &llVersion);
	printf("lld    version\t %d.%d.%d rev %d\n",
			llVersion.ll_major, llVersion.ll_minor, llVersion.ll_build, llVersion.ll_revision);
	printf("\n");
	/*
	* Configure logging - turn everything on
	*/

#if defined(VL53L1_LOG_ENABLE) && defined(_WIN64)
		Status = VL53L1_trace_config(
			filename,
			VL53L1_TRACE_MODULE_ALL,
			VL53L1_TRACE_LEVEL_ALL,
			VL53L1_TRACE_FUNCTION_ALL);
#elif defined(VL53L1_LOG_ENABLE) && !defined(_WIN64)
		Status = VL53L1_trace_config(
			NULL,
			VL53L1_TRACE_MODULE_NONE,
			VL53L1_TRACE_LEVEL_NONE,
			VL53L1_TRACE_FUNCTION_NONE);
#endif
	Status = 0;

	/*
	* Initialise Dev data structure
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_DataInit(Dev);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_StaticInit(Dev);

	/*
	* Run reference SPAD characterisation
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_PerformRefSpadManagement(Dev);

	/*
	* Example for standard offset calibration
	* value 0x50000 for a 5% reflective target (16.16 fixed point format)
	* The target shall be located at 140 mm from the device
	*/
	/*
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_SetOffsetCalibrationMode(Dev,
				VL53L1_OFFSETCALIBRATIONMODE_STANDARD);
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_PerformOffsetCalibration(Dev, 140, 0x50000);
	 */

	/*
	* Example for standard crosstalsk calibration
	* Assuming there is no target lower than 80 cm from the device
	*/
	/*
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_PerformXTalkCalibration(Dev,
				VL53L1_XTALKCALIBRATIONMODE_NO_TARGET);
	*/

	/*
	* Initialize configuration data structures for the
	* given preset mode. Does *not* apply the settings
	* to the device just initializes the data structures
	*/
	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = VL53L1_PRESETMODE_PROXY_RANGING_MODE;
		Status = VL53L1_SetPresetMode(Dev, PresetMode);
	}

	/*
	* configure a 4:3 Region Of Interest
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_SetROI(Dev, &OneRoi4_3);

	/*
	* configure timing budget for about 30fps -> 33ms between each ranging
	*/
	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev, 16000);
	}
	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetInterMeasurementPeriodMilliSeconds(Dev, 16);
	}

	/*
	* Launch the main proxy detection loop
	*/
	if (Status == VL53L1_ERROR_NONE) {
		Status = RunProxyDetectionLoop(Dev, 50, 70);
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_platform_terminate(Dev);


	print_pal_error(Status);

	return (Status);
}

