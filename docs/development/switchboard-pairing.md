# RTS Version 5 Protocol #

**Author:** Paul Aluri

## The Layers ##

Right now this protocol is being used for secure BLE pairing. This general protocol hasn't changed since RTSv2.

During this pairing process, devices initially send each other a packed-byte handshake message. Then they move to sending CLAD messages. Once keys are exchanged, the pairing devices then send encrypted CLAD messages. An example top-to-bottom protocol stack for this final state is as follows:
* Clad Messages
* Encryption
* **BLE Framing Protocol**
* **BLE Transport**

## The BLE Framing Protocol ##

In order to support messages larger than 20 bytes, the robot currently expects messages to be sent and received over BLE to use this framing protocol. 

Messages are prepended by a `1 byte` header:
* 6 least significant bits denote `uint8_t` (with max value of `64`) value of size. This is the size of the payload for this particular message. E.g., if this value was `10`, that would mean the total BLE packet was ``11 bytes``, where the first byte was the header, and the next `10` were data payload bytes.
* The 2 most significant bits indicate whether the packet is `SOLO`, `FIRST`, `LAST`, or `CONTINUE`.
  * `SOLO` (`11`): This packet contains an entire message.
  * `FIRST` (`10`): The packet is the first of a multipart message.
  * `LAST` (`01`): The packet is the last of a multipart message.
  * `CONTINUE` (`00`): This packet is neither first nor last of a multipart message.

## The Pairing Protocol ##

_Devices expect hard coded packed-byte structure._

1. Robot sends `handshake` message.

2. Client sends `handshake` message.
    
    The version should be set with the same version as the Robot's handshake (assuming the client supports
    speaking this version), otherwise it should be set to the version that factory Robot's can speak (2).

_Devices now expect clad format._

3. Robot sends connection request message (`RtsConnRequest`).

    This message includes the robot's 32 byte public key generated by libsodium's `crypto_kx_keypair` method.

4. Client responds with connection response (`RtsConnResponse`) setting either "Initial Pair" or "Reconnection".

    The first bye of this message is the `RtsConnType` (either `FirstTimePair` or `Reconnection`). A `FirstTimePair` request can be successful only if the robot is in pairing mode (double pressed while on the charger). A `Reconnection` request can be successful only if the robot and client have previously paired and have each other's public keys and session keys stored.

5. Robot sends nonces (`RtsNonceMessage`) to client.

    Robot generates two nonces using libsodium's `randombytes_buf` method (with size of `crypto_aead_xchacha20poly1305_ietf_NPUBBYTES`)--one for _rx_ and one for _tx_.

6. Client sends ack (`RtsAck`) back to robot.

    Once the client receives the initial nonces, it should ack to the robot. The `uint_8` should be the **Tag** value of the `RtsNonceMessage` message type.

    Note that before acking, if this was a `FirstTimePair`, the robot will have communicated an OOB (out-of-band) 6 digit pin, which the client and robot will both use to seed their session keys which are generated based on their own public key, private key, and the other device's public key.

_Devices now expect encrypted clad format._

7. Robot sends challenge (`RtsChallengeMessage`).

    Robot generates a random `uint32_t` and sends to the client. Note that after each send/receive both the robot and client have to increment the appropriate nonce.

8. Client responds with challenge answer (`RtsChallengeMessage`).

    The client, after decrypting this message, will need to send back an answer `number + 1`.

9. Robot sends success message if challenge correct (`RtsChallengeSuccessMessage`).

    If the client returns the correct answer, the robot will send the success message. At this point, the client can be sure that he can securely send any of the API messages and receive responses.

## Pairing Messages ##

### The Handshake Message ###
```
packed_struct handshake {
  uint8_t   id = 1,
  uint32_t  version
}
```

### Clad Messages (unencrypted) ###
```
enum uint_8 RtsConnType {
  FirstTimePair,
  Reconnection
}
```
```
RtsConnRequest {
  uint_8        publicKey[32]
}
```
```
RtsConnResponse {
  RtsConnType   connectionType,
  uint_8        publicKey[32]
}
```
```
message RtsNonceMessage {
  uint_8        toRobotNonce[24],
  uint_8        toDeviceNonce[24],
}
```
```
message RtsAck {
  uint_8        rtsConnectionTag
}
```
### Clad Messages (secure) ###
```
message RtsChallengeMessage {
  uint_32       number
}
```
```
message RtsChallengeSuccessMessage { 
}
```
## Libsodium API ##

Both sides generate keys using:

```
uint8_t publicKey[crypto_kx_PUBLICKEYBYTES];
uint8_t privateKey[crypto_kx_SECRETKEYBYTES];
crypto_kx_keypair(publicKey, privateKey);
```

Nonces are generated using:

```
uint8_t toRobotNonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
uint8_t toClientNonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
randombytes_buf(
  toRobotNonce,
  sizeof(toRobotNonce));
  [crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
randombytes_buf(
  toClientNonce,
  sizeof(toClientNonce));
```

Session keys are generated using:

```
uint8_t toRobotNonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
uint8_t toClientNonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
randombytes_buf(
  toRobotNonce,
  sizeof(toRobotNonce));
  [crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
randombytes_buf(
  toClientNonce,
  sizeof(toClientNonce));
```

Session keys are generated on the server using:

```
uint8_t serverEncrypt[crypto_kx_SESSIONKEYBYTES];
uint8_t serverDecrypt[crypto_kx_SESSIONKEYBYTES];
uint8_t server_rx[crypto_kx_SESSIONKEYBYTES];
uint8_t server_tx[crypto_kx_SESSIONKEYBYTES];

// The pin is the ASCII 6 byte string
// from the robot's face. E.g., if robot
// shows "123456", then pin will be:
// [ 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 ];
const uint8_t* pin;

crypto_kx_server_session_keys(
  server_rx,
  server_tx,
  publicKey,
  privateKey,
  clientPublicKey);

crypto_generichash(
  serverEncrypt,
  crypto_kx_SESSIONKEYBYTES, 
  server_tx,
  crypto_kx_SESSIONKEYBYTES, 
  pin,
  NUM_PIN_DIGITS);

crypto_generichash(
  serverDecrypt,
  crypto_kx_SESSIONKEYBYTES, 
  server_rx,
  crypto_kx_SESSIONKEYBYTES, 
  pin,
  NUM_PIN_DIGITS);
```

And on the client:
```
uint8_t clientEncrypt[crypto_kx_SESSIONKEYBYTES];
uint8_t clientDecrypt[crypto_kx_SESSIONKEYBYTES];
uint8_t client_rx[crypto_kx_SESSIONKEYBYTES];
uint8_t client_tx[crypto_kx_SESSIONKEYBYTES];

// The pin is the ASCII 6 byte string
// from the robot's face. E.g., if robot
// shows "123456", then pin will be:
// [ 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 ];
const uint8_t* pin;

crypto_kx_client_session_keys(
  client_rx,
  client_tx,
  publicKey,
  privateKey,
  robotPublicKey);

crypto_generichash(
  clientEncrypt,
  crypto_kx_SESSIONKEYBYTES, 
  client_tx,
  crypto_kx_SESSIONKEYBYTES, 
  pin,
  NUM_PIN_DIGITS);

crypto_generichash(
  clientDecrypt,
  crypto_kx_SESSIONKEYBYTES, 
  client_rx,
  crypto_kx_SESSIONKEYBYTES, 
  pin,
  NUM_PIN_DIGITS);
```

Finally, encryption and decryption is done as follows:

```
// encrypt message to send to client
void encrypt(uint8_t* buffer, size_t length, uint8_t* output, uint64_t* outputLength) {
  int result = crypto_aead_xchacha20poly1305_ietf_encrypt(
    output,
    outputLength,
    buffer,
    length,
    nullptr,
    0,
    nullptr,
    toClientNonce,
    serverEncrypt);

  if(result == 0) {
    // increment nonce if successful
    sodium_increment(
      toClientNonce,
      crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  }
}

// decrypt message to send to client
void decrypt(uint8_t* buffer, size_t length, uint8_t* output, uint64_t* outputLength) {
  int result = crypto_aead_xchacha20poly1305_ietf_decrypt(
    output,
    outputLength,
    nullptr,
    buffer,
    length,
    nullptr,
    0,
    toRobotNonce,
    serverDecrypt);

  if(result == 0) {
    // increment nonce if successful
    sodium_increment(
      toRobotNonce,
      crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  }
}
```