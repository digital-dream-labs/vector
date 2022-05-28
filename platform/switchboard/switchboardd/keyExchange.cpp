/**
 * File: keyExchange.cpp
 *
 * Author: paluri
 * Created: 1/16/2018
 *
 * Description: Class for interfacing with libsodium for ankiswitchboardd
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <random>
#include "switchboardd/log.h"
#include "util/fileUtils/fileUtils.h"
#include "auto-test/autoTest.h"
#include "switchboardd/keyExchange.h"

namespace Anki {
namespace Switchboard {

uint8_t* KeyExchange::GenerateKeys() {
  crypto_kx_keypair(_publicKey, _secretKey);
  return _publicKey;
};

void KeyExchange::Reset() {
  // set all keys to 0
  memset(_secretKey, 0, sizeof(_secretKey));
  memset(_decryptKey, 0, sizeof(_decryptKey));
  memset(_encryptKey, 0, sizeof(_encryptKey));
  memset(_remotePublicKey, 0, sizeof(_remotePublicKey));
  memset(_publicKey, 0, sizeof(_publicKey));
}

bool KeyExchange::ValidateKeys(uint8_t* publicKey, uint8_t* privateKey) {
  uint8_t encryptServer[crypto_kx_SESSIONKEYBYTES];
  uint8_t decryptServer[crypto_kx_SESSIONKEYBYTES];
  uint8_t encryptClient[crypto_kx_SESSIONKEYBYTES];
  uint8_t decryptClient[crypto_kx_SESSIONKEYBYTES];
  uint8_t remotePub[crypto_kx_PUBLICKEYBYTES];
  uint8_t remotePriv[crypto_kx_SECRETKEYBYTES];

  // generate fake key pair
  crypto_kx_keypair(remotePub, remotePriv);

  int serverStatus = crypto_kx_server_session_keys(decryptServer, encryptServer, publicKey, privateKey, remotePub);
  int clientStatus = crypto_kx_client_session_keys(decryptClient, encryptClient, remotePub, remotePriv, publicKey);

  if(serverStatus != 0 || clientStatus != 0) {
    return false;
  }

  return (memcmp(decryptServer, encryptClient, crypto_kx_SESSIONKEYBYTES) == 0) &&
    (memcmp(decryptClient, encryptServer, crypto_kx_SESSIONKEYBYTES) == 0);
}

std::string KeyExchange::GeneratePin() const {
  std::string pin { "" };

  if(AutoTest::IsAutoTestBot()) {
    Log::Write("Test pin file exists.");
    // Check if pin file exists--if so, use
    // as pin instead of randomly generated
    
    std::vector<uint8_t> pinBytes = Anki::Util::FileUtils::ReadFileAsBinary(AutoTest::kTestPinFilePath, 0, _numPinDigits);
    
    if(pinBytes.size() == _numPinDigits) {
      pin = std::string((char*)pinBytes.data(), _numPinDigits);
    } else {
      Log::Write("Error reading test pin file. File [%s] has contents of size [%d] but was expecting at least [%d] bytes.", AutoTest::kTestPinFilePath.c_str(), pinBytes.size(), _numPinDigits);
    }
  } 

  if(pin.empty()) {
    pin = GeneratePin(_numPinDigits);
  }

  return pin;
}

std::string KeyExchange::GeneratePin(int digits) const {
  if(digits == 0) {
    return "";
  }

  std::random_device rd;
  std::mt19937 gen(rd());

  // min (inclusive) value
  int minValue = 1;

  // add _numPinDigits 0s to minValue
  for(int i = 0; i < digits - 1; i++) {
    minValue *= 10;
  }

  // max (inclusive) value. append a 0, and subtract 1
  int maxValue = (minValue * 10) - 1;

  std::uniform_int_distribution<> dis(minValue, maxValue);
  return std::to_string(dis(gen));
}

void KeyExchange::SetRemotePublicKey(const uint8_t* pubKey) {
  // Copy in public key
  memcpy(_remotePublicKey, pubKey, crypto_kx_PUBLICKEYBYTES);
}

bool KeyExchange::CalculateSharedKeysServer(const uint8_t* pin) {
  //
  // Messages from the robot will be encrypted with a hash that incorporates
  // a random pin
  // server_tx (encryptKey) needs to be sha-256'ed
  // client_rx (client's decrypt key) needs to be sha-256'ed
  //
  bool success = crypto_kx_server_session_keys(
    _decryptKey, _encryptKey, _publicKey, _secretKey, _remotePublicKey) == 0;
  
  // Save tmp version of encryptKey
  std::vector<uint8_t> tmpEncryptKey(_encryptKey, _encryptKey + sizeof(_encryptKey));

  // Save tmp version of encryptKey
  std::vector<uint8_t> tmpDecryptKey(_decryptKey, _decryptKey + sizeof(_decryptKey));
  
  // Hash mix of pin and encryptKey to form new encryptKey
  crypto_generichash(_encryptKey, crypto_kx_SESSIONKEYBYTES, 
    tmpEncryptKey.data(), crypto_kx_SESSIONKEYBYTES, 
    pin, _numPinDigits);

  // Hash mix of pin and decryptKey to form new decryptKey
  crypto_generichash(_decryptKey, crypto_kx_SESSIONKEYBYTES, 
    tmpDecryptKey.data(), crypto_kx_SESSIONKEYBYTES, 
    pin, _numPinDigits);
  
  return success;
}

bool KeyExchange::CalculateSharedKeysClient(const uint8_t* pin) {
  //
  // Messages from the robot will be encrypted with a hash that incorporates
  // a random pin
  // server_tx (encryptKey) needs to be sha-256'ed
  // client_rx (client's decrypt key) needs to be sha-256'ed
  //
  bool success = crypto_kx_client_session_keys(
    _decryptKey, _encryptKey, _publicKey, _secretKey, _remotePublicKey) == 0;
  
  // Save tmp version of encryptKey
  std::vector<uint8_t> tmpEncryptKey(_encryptKey, _encryptKey + sizeof(_encryptKey));

  // Save tmp version of encryptKey
  std::vector<uint8_t> tmpDecryptKey(_decryptKey, _decryptKey + sizeof(_decryptKey));
  
  // Hash mix of pin and encryptKey to form new encryptKey
  crypto_generichash(_encryptKey, crypto_kx_SESSIONKEYBYTES, 
    tmpEncryptKey.data(), crypto_kx_SESSIONKEYBYTES, 
    pin, _numPinDigits);

  // Hash mix of pin and decryptKey to form new decryptKey
  crypto_generichash(_decryptKey, crypto_kx_SESSIONKEYBYTES, 
    tmpDecryptKey.data(), crypto_kx_SESSIONKEYBYTES, 
    pin, _numPinDigits);
  
  return success;
}

} // Switchboard
} // Anki