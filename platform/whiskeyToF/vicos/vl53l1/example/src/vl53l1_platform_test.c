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

#define IDENTIFICATION__MODEL_ID_ADR 0x010F
#define IDENTIFICATION__MODEL_ID_VAL 0xEA

#define SPARE_ADR 0x64

int rd_write_verification(VL53L1_DEV Dev, uint16_t addr, uint32_t expected_value)
{
	uint8_t bytes[4],mbytes[4];
	uint16_t words[2];
	uint32_t dword;
	int i;

	VL53L1_ReadMulti(Dev, addr, mbytes, 4);
	for (i=0; i<4; i++){ VL53L1_RdByte(Dev, addr+i, &bytes[i]); }
	for (i=0; i<2; i++){ VL53L1_RdWord(Dev, addr+i*2, &words[i]); }
	VL53L1_RdDWord(Dev, addr, &dword);

	printf("expected   = %8x,\n",expected_value);
	printf("read_multi = %2x, %2x, %2x, %2x\n", mbytes[0],mbytes[1],mbytes[2],mbytes[3]);
	printf("read_bytes = %2x, %2x, %2x, %2x\n", bytes[0],bytes[1],bytes[2],bytes[3]);
	printf("read words = %4x, %4x\n",words[0],words[1]);
	printf("read dword = %8x\n",dword);

	if((mbytes[0]<<24 | mbytes[1]<<16 | mbytes[2]<<8 | mbytes[3]) != expected_value) return (-1);
	if((bytes[0]<<24 | bytes[1]<<16 | bytes[2]<<8 | bytes[3]) != expected_value) return (-1);
	if((words[0]<<16 | words[1]) != expected_value) return (-1);
	if(dword != expected_value) return(-1);

	return(0);

}

void i2c_test(VL53L1_DEV Dev)
{
	int err_count = 0;
	int expected_value = 0;

	uint8_t buff[4] = {0x11,0x22,0x33,0x44};
	uint8_t ChipID[4];
	int i=0;

	for (i=0; i<4; i++){ VL53L1_RdByte(Dev, (uint16_t)IDENTIFICATION__MODEL_ID_ADR+i, &ChipID[i]); }
#ifdef BIGENDIAN
	expected_value = ChipID[3]<<24 | ChipID[2]<<16 | ChipID[1]<<8 | ChipID[0];
#else
	expected_value = ChipID[0]<<24 | ChipID[1]<<16 | ChipID[2]<<8 | ChipID[3];
#endif

	if(rd_write_verification(Dev, (uint16_t)IDENTIFICATION__MODEL_ID_ADR, expected_value) <0) err_count++;	// check the chip ID

	VL53L1_WriteMulti(Dev, SPARE_ADR,  buff, 4);				// check WriteMulti
	if(rd_write_verification(Dev, SPARE_ADR, 0x11223344) <0) err_count++;

	VL53L1_WrDWord(Dev, SPARE_ADR, 0xffeeddcc);				// check WrDWord
	if(rd_write_verification(Dev, SPARE_ADR, 0xffeeddcc) <0) err_count++;


	VL53L1_WrWord(Dev, SPARE_ADR, 0x5566);					// check WrWord
	VL53L1_WrWord(Dev, SPARE_ADR+2, 0x7788);
	if(rd_write_verification(Dev, SPARE_ADR, 0x55667788) <0) err_count++;


	for (i=0; i<4; i++){ VL53L1_WrByte (Dev, SPARE_ADR+i, buff[i]); }
	if(rd_write_verification(Dev, SPARE_ADR,0x11223344) <0) err_count++;

	if(err_count>0)
	{
		printf("i2c test failed - please check it\n");
	} else {
		printf("i2c test succeeded !\n");
	}
}

int main(int argc, char **argv)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Dev_t                   dev;
	VL53L1_DEV                     Dev = &dev;
	VL53L1_Version_t               Version;
	VL53L1_ll_version_t            llVersion;

	/*
	* Initialize the platform interface
	*/
	if (Status == VL53L1_ERROR_NONE)
	Status = VL53L1_platform_init(
		Dev,
		(0x29 << 1), /* EVK requires 8-bit I2C */
		1, /* comms_type  I2C*/
		400);       /* comms_speed_khz - 400kHz recommended */

	printf("-------------------------------\n");
	printf("|Test of platform.c adaptation|\n");
	printf("-------------------------------\n");

	VL53L1_GetVersion(&Version);
	printf("driver version\t %d.%d.%d rev %d\n",
			Version.major, Version.minor, Version.build, Version.revision);
	VL53L1_get_version(Dev, &llVersion);
	printf("lld    version\t %d.%d.%d rev %d\n",
			llVersion.ll_major, llVersion.ll_minor, llVersion.ll_build, llVersion.ll_revision);
	printf("\n");
	printf("Waiting for firware boot...\n");

	/*
	* Wait 0.5 sec for supplies to stabilize
	*/

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_WaitMs(Dev, 500);

	/*
	*  Wait for firmware to finish booting
	*/
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_WaitDeviceBooted(Dev);


	printf("\nTest of i2c access functions\n");
	i2c_test(Dev);

	return (Status);
}




/*
 * vl53l1_platform_test.c
 *
 *  Created on: 16 janv. 2018
 *      Author: taloudpy
 */

#include "vl53l1_platform.h"

VL53L1_Dev_t MyDevice;




