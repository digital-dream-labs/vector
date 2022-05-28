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
#include <malloc.h>

#include "vl53l1_api.h"
#include "vl53l1_platform_init.h"

#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_CORE, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)

/* #define DETAILS_ON */

void print_pal_error(VL53L1_Error Status){
	char buf[VL53L1_MAX_STRING_LENGTH];
	VL53L1_GetPalErrorString(Status, buf);
	printf("API Status: %i : %s\n", Status, buf);
}

int  RunAutoDistanceLoop(VL53L1_DEV dev) {
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t no_of_object_found  = 0;
	int j;
	VL53L1_MultiRangingData_t MultiRangingData;
	VL53L1_DistanceModes CurrDistMode;
	VL53L1_DistanceModes NextInternal;
	const char *sModes[]={"SHORT ","MEDIUM","LONG  "};
#ifndef DETAILS_ON
	const char *sTabs []={"SHORT","\t\t\tMEDIUM","\t\t\t\t\t\tLONG  "};
#endif
	Status = VL53L1_StartMeasurement(dev);
	/* Very first ranging measurement completion interrupt must be ignored */
	Status = VL53L1_WaitMeasurementDataReady(dev);
	Status = VL53L1_GetMultiRangingData(dev, &MultiRangingData);
	Status = VL53L1_ClearInterruptAndStartMeasurement(dev);

	/* Actual measurement loop starts here... */
	while (1)
	{
		/* Wait for range completion */
		Status = VL53L1_WaitMeasurementDataReady(dev);
		if(Status == VL53L1_ERROR_NONE)
		{
			Status = VL53L1_GetMultiRangingData(dev, &MultiRangingData);

			no_of_object_found = MultiRangingData.NumberOfObjectsFound;
			Status = VL53L1_GetDistanceMode(dev, &CurrDistMode);
#ifdef DETAILS_ON
			printf("%s objects %d ", sModes[CurrDistMode-1], no_of_object_found);
#else
			printf("%s objects %d ", sTabs[CurrDistMode-1], no_of_object_found);
#endif
			if (no_of_object_found <= 1)
				no_of_object_found = 1;
			for(j = 0; j < no_of_object_found; j++) {
#ifdef DETAILS_ON
				printf("[ROI %d s_cnt %3d ",
						MultiRangingData.RoiNumber,
						MultiRangingData.RangeData[j].StreamCount);
						printf(" Rng %4d sigRate %f Rate %f sigma %f dmax_mm %4d Status %d] ",
						MultiRangingData.RangeData[j].RangeMilliMeter,
						MultiRangingData.RangeData[j].SignalRateRtnMegaCps/65536.0,
						MultiRangingData.RangeData[j].AmbientRateRtnMegaCps/65536.0,
						MultiRangingData.RangeData[j].SigmaMilliMeter/65536.0,
						MultiRangingData.DmaxMilliMeter,
						MultiRangingData.RangeData[j].RangeStatus);
#else
						printf(" Rng %4d] ",
						MultiRangingData.RangeData[j].RangeMilliMeter);
#endif
			}
			printf("\n");
			Status = VL53L1_ClearInterruptAndStartMeasurement(dev);

			if (CurrDistMode != MultiRangingData.RecommendedDistanceMode) {
				VL53L1_StopMeasurement(dev);
				NextInternal = MultiRangingData.RecommendedDistanceMode;
				printf("Change distance mode %s -> %s\n", sModes[CurrDistMode-1], sModes[NextInternal-1]);
				Status = VL53L1_SetDistanceMode(dev, NextInternal);
				Status = VL53L1_StartMeasurement(dev);
				/* Very first ranging measurement completion interrupt must be ignored */
				Status = VL53L1_WaitMeasurementDataReady(dev);
				Status = VL53L1_GetMultiRangingData(dev, &MultiRangingData);
				Status = VL53L1_ClearInterruptAndStartMeasurement(dev);
			}
			fflush(stdout);
		}
	}

	/* Call StopMeasurement whatever the previous status but take into account its own error as final result */
	VL53L1_StopMeasurement(dev);
	return Status;
}




int main(int argc, char **argv)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Dev_t                   dev;
	VL53L1_DEV                     Dev = &dev;
	VL53L1_PresetModes             PresetMode;
	VL53L1_DeviceInfo_t            DeviceInfo;
	VL53L1_Version_t               Version;
	VL53L1_ll_version_t            llVersion;
#ifdef VL53L1_LOG_ENABLE
	char filename[] = "./trace.log";
#endif
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf ("VL53L1 Auto distance adaptation example\n\n");
	SUPPRESS_UNUSED_WARNING(argc);
	SUPPRESS_UNUSED_WARNING(argv);

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

		VL53L1_GetVersion(&Version);
		printf("driver version\t %d.%d.%d rev %d\n",
				Version.major, Version.minor, Version.build, Version.revision);
		VL53L1_get_version(Dev, &llVersion);
		printf("lld    version\t %d.%d.%d rev %d\n",
				llVersion.ll_major, llVersion.ll_minor, llVersion.ll_build, llVersion.ll_revision);
		printf("\n");

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

	/*
	* Initialise Dev data structure
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_DataInit(Dev);

	if(Status == VL53L1_ERROR_NONE)
	{
		Status = VL53L1_GetDeviceInfo(Dev, &DeviceInfo);
		if(Status == VL53L1_ERROR_NONE)
		{
		    printf("VL53L1_GetDeviceInfo:\n");
		    printf("Device Name : %s\n", DeviceInfo.Name);
		    printf("Device Type : %s\n", DeviceInfo.Type);
		    printf("Device ID : %s\n", DeviceInfo.ProductId);
		    printf("ProductRevisionMajor : %d\n", DeviceInfo.ProductRevisionMajor);
		    printf("ProductRevisionMinor : %d\n", DeviceInfo.ProductRevisionMinor);

		    if ((DeviceInfo.ProductRevisionMajor != 1) || (DeviceInfo.ProductRevisionMinor != 1)) {
			printf("Error expected cut 1.1 but found cut %d.%d\n",
					DeviceInfo.ProductRevisionMajor, DeviceInfo.ProductRevisionMinor);
			Status = VL53L1_ERROR_NOT_SUPPORTED;
		    }
		}
		print_pal_error(Status);
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_StaticInit(Dev);

	/*
	* Run reference SPAD characterisation
	*/
#ifndef VL53L1_NOCALIB

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_PerformRefSpadManagement(Dev);
#endif

	/*
	* Initialize configuration data structures for the
	* given preset mode. Does *not* apply the settings
	* to the device just initializes the data structures
	*/

	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = VL53L1_PRESETMODE_RANGING;
		Status = VL53L1_SetPresetMode(Dev, PresetMode);
	}

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_LONG);
	}

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev, 16000);
	}
	if (Status == VL53L1_ERROR_NONE) {
		Status = RunAutoDistanceLoop(Dev);
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_platform_terminate(Dev);


	print_pal_error(Status);

	return (Status);
}

