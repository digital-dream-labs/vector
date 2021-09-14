#ifndef __BLE_PACKET_H
#define __BLE_PACKET_H

void aes_message_encode(const uint8_t* key, const uint8_t* nonce, int nonce_length, uint8_t* data, int& data_length);
bool aes_message_decode(const uint8_t* key, const uint8_t* nonce, int nonce_length, uint8_t* data, int& data_length);

#endif
