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

void print_pal_error(VL53L1_Error Status){
	char buf[VL53L1_MAX_STRING_LENGTH];
	VL53L1_GetPalErrorString(Status, buf);
	printf("API Status: %i : %s\n", Status, buf);
}

void print_multiranging_data(
		int i, int j,
		VL53L1_TargetRangeData_t *pRangeData) {

	printf("%d: SignalRateRtnMegaCps[%d]= %f\n",
			i, j, pRangeData->SignalRateRtnMegaCps/65536.0);
	printf("%d: AmbientRateRtnMegaCps[%d]= %f\n",
			i, j, pRangeData->AmbientRateRtnMegaCps/65536.0);
	printf("%d: SigmaMilliMeter[%d]= %f\n",
			i, j, pRangeData->SigmaMilliMeter/65536.0);
	printf("%d: RangeMilliMeter[%d]= %d\n",
			i, j, pRangeData->RangeMilliMeter);
	printf("%d: RangeMinMilliMeter[%d]= %d\n",
			i, j, pRangeData->RangeMinMilliMeter);
	printf("%d: RangeMaxMilliMeter[%d]= %d\n",
			i, j, pRangeData->RangeMaxMilliMeter);
	printf("%d: RangeStatus[%d]= %d\n",
			i, j, pRangeData->RangeStatus);
}

VL53L1_Error PrintROI(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_RoiConfig_t RoiConfig;
	uint8_t MaxNumberOfROI;
	int i;

	Status = VL53L1_GetMaxNumberOfROI(Dev, &MaxNumberOfROI);
	printf("MaxNumberOfROI : %d\n", MaxNumberOfROI);
	Status = VL53L1_GetROI(Dev, &RoiConfig);

	if (Status == VL53L1_ERROR_NONE) {
		for (i=0;i<RoiConfig.NumberOfRoi;i++) {
			printf("ROI number = %d\n",i);
			printf("TopLeftX = %d\n",RoiConfig.UserRois[i].TopLeftX);
			printf("TopLeftY = %d\n",RoiConfig.UserRois[i].TopLeftY);
			printf("BotRightX = %d\n",RoiConfig.UserRois[i].BotRightX);
			printf("BotRightY = %d\n",RoiConfig.UserRois[i].BotRightY);
		}
	}
	return Status;
}


int  RunRangingLoop(VL53L1_DEV Dev, int  no_of_measurements) {
	int Status = VL53L1_ERROR_NONE;
	int i,j,k;
	int no_of_object_found;
	VL53L1_TargetRangeData_t *pRangeData;
	VL53L1_MultiRangingData_t MultiRangingData;
	VL53L1_MultiRangingData_t *pMultiRangingData = &MultiRangingData;
//	VL53L1_RangingMeasurementData_t RangingMeasurementData;
	uint32_t MeasurementTimingBudgetMicroSeconds;

	if (Status == VL53L1_ERROR_NONE) {
		printf("run VL53L1_StartMeasurement\n");
		Status = VL53L1_StartMeasurement(Dev);
	}
	if( Status != VL53L1_ERROR_NONE){
		printf("fail to StartMeasurement\n");
		return -1;
	}
	if (Status == VL53L1_ERROR_NONE)
		Status = PrintROI(Dev);

	for (i = 0 ; i < no_of_measurements+1 ; i++) {

		Status = VL53L1_GetMeasurementTimingBudgetMicroSeconds(Dev,
			&MeasurementTimingBudgetMicroSeconds);

		printf("MeasurementTimingBudgetMicroSeconds: %d\n",
				MeasurementTimingBudgetMicroSeconds);
		/* Wait for range completion */
		if (Status == VL53L1_ERROR_NONE)
			Status = VL53L1_WaitMeasurementDataReady(Dev);

		if(Status == VL53L1_ERROR_NONE)
		{
			Status = VL53L1_GetMultiRangingData(Dev, pMultiRangingData);

			if (Status == VL53L1_ERROR_NONE)
				VL53L1_ClearInterruptAndStartMeasurement(Dev);

			no_of_object_found = pMultiRangingData->NumberOfObjectsFound;
			printf("Number of measurements = %d\n",i);
			printf("Number of Objects Found = %d\n",no_of_object_found);
			printf("%d: DmaxMilliMeter= %d\n",
					i, pMultiRangingData->DmaxMilliMeter);
			printf("%d: EffectiveSpadRtnCount= %d\n",
					i, pMultiRangingData->EffectiveSpadRtnCount);

			if (no_of_object_found <=1)
				k = 1;
			else
				k = no_of_object_found;

			for(j=0;j<k;j++) {
				pRangeData = &(pMultiRangingData->RangeData[j]);
				printf("RangingMeasurementData[%d]\n",j);

				printf("%d: Stream Count[%d]= %d\n",
						i, j, pMultiRangingData->StreamCount);
				print_multiranging_data(i, j, pRangeData);

			}
			printf("\n");


		} else {
			break;
		}
		if (Status == VL53L1_ERROR_NONE)
			Status = PrintROI(Dev);

		if (1)
			Status = VL53L1_WaitUs(Dev, 100000);
	}

	if (Status == VL53L1_ERROR_NONE) {
		printf("run VL53L1_StopMeasurement\n");
		Status = VL53L1_StopMeasurement(Dev);
	}

	return Status;
}


VL53L1_Error ROIExample(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_RoiConfig_t RoiConfig;
	uint8_t MaxNumberOfROI;

	Status = VL53L1_GetMaxNumberOfROI(Dev, &MaxNumberOfROI);
	printf("MaxNumberOfROI : %d\n", MaxNumberOfROI);


	if (Status == VL53L1_ERROR_NONE) {
		RoiConfig.NumberOfRoi = 1;
		RoiConfig.UserRois[0].TopLeftX = 2;
		RoiConfig.UserRois[0].TopLeftY = 14;
		RoiConfig.UserRois[0].BotRightX = 14;
		RoiConfig.UserRois[0].BotRightY = 2;
		Status = VL53L1_SetROI(Dev, &RoiConfig);
	}

	return Status;

}



VL53L1_Error TimingBudgetExample(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	uint32_t MeasurementTimingBudgetMicroSeconds = 12000;

	Status = VL53L1_GetMeasurementTimingBudgetMicroSeconds(Dev,
			&MeasurementTimingBudgetMicroSeconds);
	printf("Timing Budget is : %d us\n", MeasurementTimingBudgetMicroSeconds);

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev,
				MeasurementTimingBudgetMicroSeconds + 5000);
	}

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_GetMeasurementTimingBudgetMicroSeconds(Dev,
				&MeasurementTimingBudgetMicroSeconds);
		printf("New Timing Budget is : %d us\n", MeasurementTimingBudgetMicroSeconds);
	}

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
	uint8_t CalibrationOption = 0;
	uint8_t XTalkCompensationEnable;
#ifdef VL53L1_LOG_ENABLE
	char filename[] = "./trace.log";
#endif
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf ("VL53L1 Ranging example\n\n");
	SUPPRESS_UNUSED_WARNING(argc);
	SUPPRESS_UNUSED_WARNING(argv);

//	printf ("Press a Key to continue!\n\n");
//	getchar();

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
	* Wait 2 sec for supplies to stabilize
	*/

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_WaitMs(Dev, 2000);

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
	* Run Xtalk calibration
	*/
	if (0) {
		if (Status == VL53L1_ERROR_NONE) {
			Status = VL53L1_PerformXTalkCalibration(Dev, CalibrationOption);
			if (Status != VL53L1_ERROR_NONE) {
				print_pal_error(Status);
				Status = VL53L1_ERROR_NONE;
			}
		}

		if (Status == VL53L1_ERROR_NONE) {
			Status = VL53L1_GetXTalkCompensationEnable(Dev,
				&XTalkCompensationEnable);
			printf("VL53L1_GetXTalkCompensationEnable = %d\n", XTalkCompensationEnable);
		}

		/*
		* Run Offset calibration
		*/
		if (Status == VL53L1_ERROR_NONE) {
			Status = VL53L1_PerformOffsetCalibration(Dev, 600,
					(FixPoint1616_t)(5 * 65536));
			if (Status != VL53L1_ERROR_NONE) {
				print_pal_error(Status);
				Status = VL53L1_ERROR_NONE;
			}
		}
	}

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
//		Status = VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_AUTO);
//		Status = VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_MEDIUM);
//		Status = VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_SHORT);
		Status = VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_LONG);
	}

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetOutputMode(Dev, VL53L1_OUTPUTMODE_STRONGEST);
	}

	/*
	 * Set ROI Example before start
	*
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = ROIExample(Dev);
	if (Status == VL53L1_ERROR_NONE)
		Status = PrintROI(Dev);


	/* Example for timing budget before start */
	if (0) {
		if (Status == VL53L1_ERROR_NONE)
			Status = TimingBudgetExample(Dev);
	}

	/*
	 * Ranging LOOP
	 *
	 * Run two times the Ranging loop to test start stop
	*
	*/
	/* The following ranging loop will use Vl53L1_GetMultiRangingData*/
	if (Status == VL53L1_ERROR_NONE) {
		printf("*********************************************\n");
		printf("    RUN first RunRangingLoop\n");
		printf("*********************************************\n");
		Status = RunRangingLoop(Dev, 15);
	}

	if (0) {
		printf ("Press a Key to continue!\n\n");
		getchar();
		/* The following ranging loop will use Vl53L1_GetMultiRangingData*/
		if (Status == VL53L1_ERROR_NONE) {
			printf("*********************************************\n");
			printf("    RUN second RunRangingLoop\n");
			printf("*********************************************\n");
			Status = RunRangingLoop(Dev, 15);
		}
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_platform_terminate(Dev);


	print_pal_error(Status);

//	printf ("\nPress a Key to continue!");
//	getchar();

	return (Status);
}

