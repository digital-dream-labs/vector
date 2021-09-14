#pragma once

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  Type_Funky,
  Type_Monkey,
  Type_Music,
  Type_Fire,
  Type_FunkyMessage,
  Type_UnionOfUnion,
  Type_MessageOfUnion
};

// Take the given buffer, unpack into the C version of the struct, and re-pack
// into the out buffer so Go can verify the output on the other end is identical
size_t RoundTrip(int type, const uint8_t* inBuf, size_t inLen, uint8_t* outBuf, size_t outLen);

#ifdef __cplusplus
};
#endif