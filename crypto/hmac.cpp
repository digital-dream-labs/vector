#include <string.h>

#include "md5.h"
#include "hmac.h"

static const int BLOCK_LENGTH = 64;

bool test_hmac(const uint8_t* hmac, const uint8_t* nonce, int nonce_length, const uint8_t* data, int data_length) {
  uint8_t cmp[HMAC_LENGTH];

  create_hmac(cmp, nonce, nonce_length, data, data_length);

  return memcmp(hmac, cmp, HMAC_LENGTH) == 0;
}

void create_hmac(uint8_t* hmac, const uint8_t* nonce, int nonce_length, const uint8_t* data, int data_length) {
  MD5_CTX ctx;
  uint8_t pad[BLOCK_LENGTH];

  memset(pad, 0x36, sizeof(pad));
  for (int i = 0; i < nonce_length; i++) {
    pad[i] ^= nonce[i];
  }

  MD5_Init(&ctx);
  MD5_Update(&ctx, pad, sizeof(pad));
  MD5_Update(&ctx, data, data_length);
  MD5_Final(hmac, &ctx);

  memset(pad, 0x5C, sizeof(pad));
  for (int i = 0; i < nonce_length; i++) {
    pad[i] ^= nonce[i];
  }

  MD5_Init(&ctx);
  MD5_Update(&ctx, pad, sizeof(pad));
  MD5_Update(&ctx, hmac, HMAC_LENGTH);
  MD5_Final(hmac, &ctx);
}
