/**
 * File: keyExchange.h
 *
 * Author: paluri
 * Created: 1/16/2018
 *
 * Description: Class for interfacing with libsodium for ankiswitchboardd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#pragma once

#include <string>
#include <sodium.h>

namespace Anki {
namespace Switchboard {
  class KeyExchange {
  public:
    KeyExchange(uint8_t numPinDigits) :
    _numPinDigits(numPinDigits)
    {
    }
    
    // Getters
    uint8_t* GetEncryptKey() {
      return _encryptKey;
    }
    
    uint8_t* GetDecryptKey() {
      return _decryptKey;
    }
    
    uint8_t* GetPublicKey() {
      return _publicKey;
    }

    uint8_t* GetPrivateKey() {
      return _secretKey;
    }

    void SetKeys(uint8_t* publicKey, uint8_t* privateKey) {
      memcpy(_secretKey, privateKey, sizeof(_secretKey));
      memcpy(_publicKey, publicKey, sizeof(_publicKey));
    }
    
    uint8_t* GetPinLengthPtr() {
      return &_numPinDigits;
    }
    
    uint8_t* GetToRobotNonce() {
      return _initialToRobotNonce;
    }

    uint8_t* GetToDeviceNonce() {
      return _initialToDeviceNonce;
    }
    
    uint8_t* GetVerificationHash() {
      crypto_generichash(_hashedKey, crypto_kx_SESSIONKEYBYTES, _encryptKey, crypto_kx_SESSIONKEYBYTES, nullptr, 0);
      
      return _hashedKey;
    }
    
    // Method Declarations
    uint8_t* GenerateKeys();

    std::string GeneratePin(int digits) const;
    std::string GeneratePin() const;
    void Reset();
    void SetRemotePublicKey(const uint8_t* pubKey);
    bool CalculateSharedKeysServer(const uint8_t* pin);
    bool CalculateSharedKeysClient(const uint8_t* pin);
    bool ValidateKeys(uint8_t* publicKey, uint8_t* privateKey);
    
  private:
    // Variables
    uint8_t _secretKey [crypto_kx_SECRETKEYBYTES];   // our secret key
    uint8_t _decryptKey [crypto_kx_SESSIONKEYBYTES]; // rx
    uint8_t _encryptKey [crypto_kx_SESSIONKEYBYTES]; // tx
    uint8_t _remotePublicKey [crypto_kx_PUBLICKEYBYTES];  // partner's public key
    uint8_t _publicKey [crypto_kx_PUBLICKEYBYTES];   // our public key
    uint8_t _hashedKey [crypto_kx_SESSIONKEYBYTES];  // size of code
    uint8_t _initialToRobotNonce [crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    uint8_t _initialToDeviceNonce [crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    uint8_t _numPinDigits = 6;
  };
} // Switchboard
} // Anki
