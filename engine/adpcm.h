// First block should starts with index + predictor = 0
// Resulting values are the state for the next block
// Length is in input samples
#include <stdint.h>

void encodeMuLaw(int16_t *in, uint8_t *out, int length);
