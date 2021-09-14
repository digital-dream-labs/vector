#ifndef __AES_H
#define __AES_H

#include <stdint.h>
#include <stddef.h>

static const int AES_KEY_LENGTH = 16;

void AES128_ECB_encrypt(const uint8_t* input, const uint8_t* key, uint8_t *output);
void AES128_ECB_decrypt(const uint8_t* input, const uint8_t* key, uint8_t *output);

void aes_fix_block(uint8_t* data, int& length);
void aes_cfb_encode(const void* key, void* iv, const uint8_t* data, uint8_t* output, int length);
void aes_cfb_decode(const void* key, const void* iv, const uint8_t* data, uint8_t* output, int length, void* iv_out = NULL);

#endif
