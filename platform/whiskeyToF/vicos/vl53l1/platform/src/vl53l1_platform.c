/** @brief VL53L1 Bare driver platform implimentation for use in Linux user space.
 * @copyright Anki 2019
 * @author Daniel Casner <daniel@anki.com>
 */


#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "vl53l1_platform.h"

#define FH(pdev) pdev->platform_data.i2c_file_handle
#define VL53L1_ERRNO (VL53L1_ERROR_PLATFORM_SPECIFIC_START - errno)
#define write_or_return(pdev, buffer, count) if (write(FH(pdev), buffer, count) != count) return VL53L1_ERRNO
#define  read_or_return(pdev, buffer, count) if (read(FH(pdev), buffer, count) != count) return VL53L1_ERRNO

static long g_timer_res = 0;

// I2C dev must already be open
VL53L1_Error VL53L1_CommsInitialise(VL53L1_Dev_t *pdev, int16_t address, uint8_t comms_type, uint16_t comms_speed_khz) {
  struct timespec ts;

  if (FH(pdev) < 0) {
    printf("I2C file handle needs to be initalized before calling __FUNCTION__ \n");
    return VL53L1_ERROR_INVALID_PARAMS;
  }

  pdev->platform_data.slave_address = address;

  if (ioctl(FH(pdev), I2C_SLAVE, address) < 0) {
    printf("Failed to acquire I2C bus access and / or talk to slave: %d\n", errno);
    return VL53L1_ERRNO;
  }

  if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) {
    printf("Failed to get clock timer precision: %d\n", errno);
    return VL53L1_ERRNO;
  }

  if (ts.tv_sec != 0) {
    printf("Clock resolution tv_sec = %ld!\n", ts.tv_sec);
    return VL53L1_ERROR_INVALID_PARAMS;
  }
  g_timer_res = ts.tv_nsec;

  return VL53L1_ERROR_NONE;
}

// Closes I2C file handle
VL53L1_Error VL53L1_CommsClose(VL53L1_Dev_t *pdev) {
  if (close(FH(pdev)) < 0) {
    return VL53L1_ERRNO;
  }

  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_platform_init(VL53L1_Dev_t *pdev,
                                  uint8_t i2c_slave_address,
                                  uint8_t comms_type,
                                  uint16_t comms_speed_khz) {
  return VL53L1_CommsInitialise(pdev, i2c_slave_address, comms_type, comms_speed_khz);
}


VL53L1_Error VL53L1_platform_terminate(VL53L1_Dev_t *pdev) {
  return VL53L1_CommsClose(pdev);
}


VL53L1_Error VL53L1_WriteMulti(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata, uint32_t count) {
  const uint32_t buffer_sz = count + sizeof(uint16_t);
  uint8_t* buffer = malloc(buffer_sz);
  if (buffer == NULL) return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

  // Assemble index little endian in buffer
  buffer[0] = (index >> 8) & 0xff;
  buffer[1] = (index >> 0) & 0xff;
  memcpy(buffer + 2, pdata, count);

  write_or_return(pdev, buffer, buffer_sz);

  return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_ReadMulti(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata, uint32_t count) {
  uint8_t index_buffer[] = { (index >> 8) & 0xff, (index >> 0) & 0xff };

  struct i2c_msg msg[] = {
    {
      .addr = pdev->platform_data.slave_address,
      .flags = 0,
      .len = 2,
      .buf = index_buffer,
    },
    {
      .addr = pdev->platform_data.slave_address,
      .flags = I2C_M_RD,
      .len = count,
      .buf = pdata,
    },
  };

  struct i2c_rdwr_ioctl_data payload = {
    .msgs = msg,
    .nmsgs = sizeof(msg) / sizeof(msg[0]),
  };

  if (ioctl(FH(pdev), I2C_RDWR, &payload) < 0) {
    return VL53L1_ERRNO;
  }

  return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_WrByte(VL53L1_Dev_t *pdev, uint16_t index, uint8_t data) {
  uint8_t buffer[] = { (index >> 8) & 0xff,
                       (index >> 0) & 0xff,
                       data };
  write_or_return(pdev, buffer, sizeof(buffer));
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_WrWord(VL53L1_Dev_t *pdev, uint16_t index, uint16_t data) {
  uint8_t buffer[] = { (index >> 8) & 0xff,
                       (index >> 0) & 0xff,
                       (data >> 8) & 0xff,
                       (data >> 0) & 0xff };
  write_or_return(pdev, buffer, sizeof(buffer));
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_WrDWord(VL53L1_Dev_t *pdev, uint16_t index, uint32_t data) {
  uint8_t buffer[] = { (index >> 8) & 0xff,
                       (index >> 0) & 0xff,
                       (data >> 24) & 0xff,
                       (data >> 16) & 0xff,
                       (data >>  8) & 0xff,
                       (data >>  0) & 0xff };
  write_or_return(pdev, buffer, sizeof(buffer));
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_RdByte(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata) {
  return VL53L1_ReadMulti(pdev, index, pdata, 1);
}

VL53L1_Error VL53L1_RdWord(VL53L1_Dev_t *pdev, uint16_t index, uint16_t *pdata) {
  uint8_t data[sizeof(uint16_t)];
  VL53L1_Error err = VL53L1_ReadMulti(pdev, index, data, sizeof(uint16_t));
  if (err != VL53L1_ERROR_NONE) return err;
  *pdata = data[0] << 8 | data[1];
  return VL53L1_ERROR_NONE;
}

VL53L1_Error VL53L1_RdDWord(VL53L1_Dev_t *pdev, uint16_t index, uint32_t *pdata) {
  uint8_t data[sizeof(uint32_t)];
  VL53L1_Error err = VL53L1_ReadMulti(pdev, index, data, sizeof(uint32_t));
  (void)err;
  *pdata = data[0] << 24 |
           data[1] << 16 |
           data[2] <<  8 |
           data[3];
  return VL53L1_ERROR_NONE;
}

#define MS_PER_SEC (1000)
#define US_PER_SEC (MS_PER_SEC * 1000)
#define NS_PER_SEC (US_PER_SEC * 1000)
#define NS_PER_MS (1000000)
#define NS_PER_US (1000)

VL53L1_Error VL53L1_WaitUs(VL53L1_Dev_t *pdev, int32_t wait_us) {
  struct timespec sleep_time = { wait_us / US_PER_SEC, (wait_us % US_PER_SEC) * NS_PER_US };
  if (nanosleep(&sleep_time, NULL) == 0) return VL53L1_ERROR_NONE;
  else return VL53L1_ERRNO;
}

VL53L1_Error VL53L1_WaitMs(VL53L1_Dev_t *pdev, int32_t wait_ms) {
  struct timespec sleep_time = { wait_ms / MS_PER_SEC, (wait_ms % MS_PER_SEC) * NS_PER_MS };
  if (nanosleep(&sleep_time, NULL) == 0) return VL53L1_ERROR_NONE;
  else return VL53L1_ERRNO;
}


VL53L1_Error VL53L1_GetTimerFrequency(int32_t *ptimer_freq_hz) {
  *ptimer_freq_hz = NS_PER_SEC / g_timer_res;
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_GetTimerValue(int32_t *ptimer_count) {
  struct timespec now;
  long long unscaled;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return VL53L1_ERRNO;
  unscaled = (long long)now.tv_sec * NS_PER_SEC + now.tv_nsec;
  *ptimer_count = (int32_t)(unscaled / g_timer_res);
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_GpioSetMode(uint8_t pin, uint8_t mode) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error VL53L1_GpioSetValue(uint8_t pin, uint8_t value) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error VL53L1_GpioGetValue(uint8_t pin, uint8_t *pvalue) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error VL53L1_GpioXshutdown(uint8_t value) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error VL53L1_GpioCommsSelect(uint8_t value) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error VL53L1_GpioPowerEnable(uint8_t value) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error  VL53L1_GpioInterruptEnable(void (*function)(void), uint8_t edge_type) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}

VL53L1_Error  VL53L1_GpioInterruptDisable(void) {
  return VL53L1_ERROR_NOT_IMPLEMENTED;
}


VL53L1_Error VL53L1_GetTickCount(uint32_t *ptime_ms) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return VL53L1_ERRNO;
  *ptime_ms = (uint32_t)now.tv_sec * MS_PER_SEC + now.tv_nsec / NS_PER_MS;
  return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_WaitValueMaskEx(VL53L1_Dev_t *pdev,
                                    uint32_t      timeout_ms,
                                    uint16_t      index,
                                    uint8_t       value,
                                    uint8_t       mask,
                                    uint32_t      poll_delay_ms) {
  uint32_t start_ms, now_ms;
  if (VL53L1_GetTickCount(&start_ms) != VL53L1_ERROR_NONE) return VL53L1_ERRNO;

  do {
    VL53L1_Error err;
    uint8_t data;
    err = VL53L1_RdByte(pdev, index, &data);
    if (err != VL53L1_ERROR_NONE) return err;

    if ((data & mask) == value) return VL53L1_ERROR_NONE;

    err = VL53L1_WaitMs(pdev, poll_delay_ms);
    if (err != VL53L1_ERROR_NONE) return err;

    err = VL53L1_GetTickCount(&now_ms);
    if (err != VL53L1_ERROR_NONE) return err;
  } while((now_ms - start_ms) < timeout_ms);

  return VL53L1_ERROR_TIME_OUT;
}
