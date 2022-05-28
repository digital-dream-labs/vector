
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





































#ifndef _VL53L1_PLATFORM_LOG_H_
#define _VL53L1_PLATFORM_LOG_H_


#ifdef VL53L1_LOG_ENABLE
	#include "vl53l1_platform_user_config.h"

	#ifdef _MSC_VER
	#   define EWOKPLUS_EXPORTS  __declspec(dllexport)
	#else
	#       define EWOKPLUS_EXPORTS
	#endif

	#include "vl53l1_types.h"

	#ifdef __cplusplus
	extern "C" {
	#endif

	#include <time.h>



































	#define		VL53L1_TRACE_LEVEL_NONE			0x00000000
	#define		VL53L1_TRACE_LEVEL_ERRORS		0x00000001
	#define		VL53L1_TRACE_LEVEL_WARNING		0x00000002
	#define		VL53L1_TRACE_LEVEL_INFO			0x00000004
	#define		VL53L1_TRACE_LEVEL_DEBUG		0x00000008
	#define		VL53L1_TRACE_LEVEL_ALL			0x00000010
	#define		VL53L1_TRACE_LEVEL_IGNORE		0x00000020

	#define		VL53L1_TRACE_FUNCTION_NONE		0x00000000
	#define		VL53L1_TRACE_FUNCTION_I2C		0x00000001
	#define		VL53L1_TRACE_FUNCTION_ALL		0x7fffffff

	#define		VL53L1_TRACE_MODULE_NONE		0x00000000
	#define		VL53L1_TRACE_MODULE_API			0x00000001
	#define		VL53L1_TRACE_MODULE_CORE		0x00000002
	#define		VL53L1_TRACE_MODULE_PROTECTED		0x00000004
	#define		VL53L1_TRACE_MODULE_HISTOGRAM		0x00000008
	#define		VL53L1_TRACE_MODULE_REGISTERS		0x00000010
	#define		VL53L1_TRACE_MODULE_PLATFORM		0x00000020
	#define		VL53L1_TRACE_MODULE_NVM			0x00000040
	#define		VL53L1_TRACE_MODULE_CALIBRATION_DATA	0x00000080
	#define		VL53L1_TRACE_MODULE_NVM_DATA		0x00000100
	#define		VL53L1_TRACE_MODULE_HISTOGRAM_DATA	0x00000200
	#define		VL53L1_TRACE_MODULE_RANGE_RESULTS_DATA	0x00000400
	#define		VL53L1_TRACE_MODULE_XTALK_DATA		0x00000800
	#define		VL53L1_TRACE_MODULE_OFFSET_DATA		0x00001000
	#define		VL53L1_TRACE_MODULE_DATA_INIT		0x00002000
    #define		VL53L1_TRACE_MODULE_REF_SPAD_CHAR	0x00004000
    #define		VL53L1_TRACE_MODULE_SPAD_RATE_MAP	0x00008000
	#ifdef PAL_EXTENDED
		#define	VL53L1_TRACE_MODULE_SPAD		0x01000000
		#define	VL53L1_TRACE_MODULE_FMT			0x02000000
		#define	VL53L1_TRACE_MODULE_UTILS		0x04000000
		#define	VL53L1_TRACE_MODULE_BENCH_FUNCS	0x08000000
	#endif
	#define		VL53L1_TRACE_MODULE_CUSTOMER_API	0x40000000
	#define		VL53L1_TRACE_MODULE_ALL			0x7fffffff


	extern uint32_t _trace_level;







	EWOKPLUS_EXPORTS int8_t VL53L1_trace_config(
		char *filename,
		uint32_t modules,
		uint32_t level,
		uint32_t functions);












	EWOKPLUS_EXPORTS void VL53L1_trace_print_module_function(
		uint32_t module,
		uint32_t level,
		uint32_t function,
		const char *format, ...);








	uint32_t VL53L1_get_trace_functions(void);








	void VL53L1_set_trace_functions(uint32_t function);










	uint32_t VL53L1_clock(void);

	#define LOG_GET_TIME() \
		((int)VL53L1_clock())

	#define _LOG_TRACE_PRINT(module, level, function, ...) \
		VL53L1_trace_print_module_function(module, level, function, ##__VA_ARGS__);

	#define _LOG_FUNCTION_START(module, fmt, ...) \
		VL53L1_trace_print_module_function(module, _trace_level, VL53L1_TRACE_FUNCTION_ALL, "%6ld <START> %s "fmt"\n", LOG_GET_TIME(), __FUNCTION__, ##__VA_ARGS__);

	#define	_LOG_FUNCTION_END(module, status, ...)\
		VL53L1_trace_print_module_function(module, _trace_level, VL53L1_TRACE_FUNCTION_ALL, "%6ld <END> %s %d\n", LOG_GET_TIME(), __FUNCTION__, (int)status, ##__VA_ARGS__)

	#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
		VL53L1_trace_print_module_function(module, _trace_level, VL53L1_TRACE_FUNCTION_ALL, "%6ld <END> %s %d "fmt"\n", LOG_GET_TIME(),  __FUNCTION__, (int)status, ##__VA_ARGS__)

	#define _LOG_GET_TRACE_FUNCTIONS()\
		VL53L1_get_trace_functions()

	#define _LOG_SET_TRACE_FUNCTIONS(functions)\
		VL53L1_set_trace_functions(functions)

	#define _LOG_STRING_BUFFER(x) char x[VL53L1_MAX_STRING_LENGTH]

	#ifdef __cplusplus
	}
	#endif

#else


	#define _LOG_TRACE_PRINT(module, level, function, ...)
	#define _LOG_FUNCTION_START(module, fmt, ...)
	#define _LOG_FUNCTION_END(module, status, ...)
	#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)
	#define _LOG_GET_TRACE_FUNCTIONS() 0
	#define _LOG_SET_TRACE_FUNCTIONS(functions)
	#define _LOG_STRING_BUFFER(x)

#endif


#endif


