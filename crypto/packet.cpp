#include <stdint.h>

#include "hmac.h"
#include "aes.h"

void aes_message_encode(const uint8_t* key, const uint8_t* nonce, int nonce_length, uint8_t* data, int& data_length) {
  // Round up to 16-bytes
  aes_fix_block(data, data_length);

  // Append the HMAC for the message
  create_hmac(data + data_length, nonce, nonce_length, data, data_length);
  data_length += HMAC_LENGTH;
  
  // Encode the message
  aes_cfb_encode(key, data, data, data + AES_KEY_LENGTH, data_length);
  data_length += AES_KEY_LENGTH;
}

bool aes_message_decode(const uint8_t* key, const uint8_t* nonce, int nonce_length, uint8_t* data, int& data_length) {
  // Decode the message
  data_length -= AES_KEY_LENGTH;
  aes_cfb_decode(key, data, data + AES_KEY_LENGTH, data, data_length);  

  // Test the HMAC
  data_length -= HMAC_LENGTH;
  return test_hmac(data + data_length, nonce, nonce_length, data, data_length);
}
