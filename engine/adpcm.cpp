#include "adpcm.h"

static uint8_t MuLawCompressTable[] =
{
     0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,
     5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

// Resulting values are the state for the next block
// Length is in input samples

void encodeMuLaw(int16_t *in, uint8_t *out, int length) {
  for ( ; length > 0; length--) {
    int16_t sample = *(in++);
    const bool sign = (sample < 0);

    if (sign) {
      sample = ~sample;
    }

    const uint8_t exponent = MuLawCompressTable[sample >> 8];
    uint8_t mantissa;

    if (exponent) {
      mantissa = (uint8_t)((sample >> (exponent + 3)) & 0xF);
    } else {
      mantissa = (uint8_t)(sample >> 4);
    }

    *(out++) = (uint8_t)((sign ? 0x80 : 0) | (exponent << 4) | mantissa);
  }
}
