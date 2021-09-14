#include <stdint.h>

#include "crc32.h"

static const uint32_t polynomial = 0xedb88320L;

uint32_t calc_crc32(const uint8_t* data, int length) {
  uint32_t checksum = 0xFFFFFFFF;

  while (length-- > 0) {
    checksum ^= *(data++);
    for (int b = 0; b < 8; b++) {
      checksum = (checksum >> 1) ^ ((checksum & 1) ? polynomial : 0);
    }
  }

  return ~checksum;
}
