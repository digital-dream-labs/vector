#ifndef __HMAC_C
#define __HMAC_C

#include <stdint.h>

static const int HMAC_LENGTH = 16;

bool test_hmac(const uint8_t* hmac, const uint8_t* nonce, int nonce_length, const uint8_t* data, int data_length);
void create_hmac(uint8_t* hmac, const uint8_t* nonce, int nonce_length, const uint8_t* data, int data_length);

#endif
