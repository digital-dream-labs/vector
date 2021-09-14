# Victor Versioning Plan

Created by Mark Wesley Last updated Mar 12, 2018

## Summary
* The GameToEngine and EngineToGame CLAD protocol (used by both the Cozmo app and Cozmo SDK) is strongly versioned and does not allow any compatibility with even the tiniest engine changes. For Victor we need to do better. This document describes a way to implement an SDK that does not break so often.
* This is a remote SDK (does NOT provide any mechanism for running code on the robot).
* Used to build official Anki apps
    * Companion app
    * Code Lab app
* Used to interface with Code Lab extension (hosted at e.g. https://scratch.mit.edu/ but all code running on local computer and talking to Victor)
* Used internally for tools and testing
* Made publicly available for users to:
    * Program Victor in Python (with an SDK very similar to the Cozmo Python SDK)
    * Build their own 3rd party apps, which they are free to release on the iOS / Android / Kindle app stores.

## High level design
* NOTE: The exact way this interfaces with Security, Permissions + Pairing is TBD, and will depend on the designs and requirements from those once they are made available
* A new SDK message protocol is used for all communication (this is distinct from any internal CLAD messages within the robot)
* An SDK message translation layer runs on the robot, and handles:
    * Connection to remote computers / mobile devices
    * Initial handshake (with version exchange)
        * Format of initial connection message is locked (for all versions, forever) so that any 2 versions can read/write it correctly
        * A range of versions will be supported - at a minimum the robot should support the latest + previous version, otherwise robot updates will be out of sync with app updates.
        * Robot will refuse connection to apps that do not have an overlapping version range (allows us to deprecate code and systems if necessary, and not have to support every version forever)
    * Accepted connections will communicate over the highest common version of both sides - i.e. Min(Robot.Version, App.Version)
        * Older side (app or robot) communicates at it's current version (and does no translation)
        * Newer side (app or robot) automatically converts inbound and outbound messages from/to the other side's version
    * Receiving incoming messages and dispatching as relevant internal "Engine CLAD" messages
    * Receiving internal CLAD messages and dispatching external "SDK Message Protocol" messages
* A similar SDK message translation layer runs on the external device
* Client code, on device, talks to a C# or Python API, the details of the underlying messaging and versioning are completely hidden.
    * Specific attributes, for features added in later versions, can be queried - e.g. "CanVictorTapDance()"

![](VictorSDK_HighLevelOverview.png)


## Initial Connection Handshake (for versioning)

![](VictorSDKMessageLayer_Handshake.png)

## Versioned messages
* Handshake messages (just the initial 2 request, and notification of unsupported version messages) are completely final and can never change
* All Message IDs are static, i.e. new messages are always added at the end.
* Enum entries cannot be re-ordered, and new entries must be added to the end.
* Enum underlying type/size (e.g. uint8, uint16) cannot be changed.
* Changes of any messages will declare the version + allow both old and new
* The above will all be verified by unit tests to prevent accidental changes, and block PRs that break compatibility
* It is possible to completely deprecate and remove old messages and types (except for the handshake messages), especially prior to the first release:
    * Old messages etc. become deprecated when removed
    * When the mininim supported version is bumped past when something is deprecated, then it can safely be removed completely.
* Generally, this layer should stay as static as possible, which is why it's separated from the internal engine CLAD layer.

## Version-friendly messaging and extending CLAD


* Fields can optionally be marked with a min + max version (indicating when they were added, and when they were removed), along with optional default values - e.g. over time the SayText definition would change (but only one of these is ever present in the current code, so the end version looks like):

```
message SayText {
  string                  text,
  {version 1:1} bool      playAnim = true,
  {version 2:3} float_32  pitch = 0.0f,
  {version 3:}  float_32  speed = 1.0f
}
```

* Versioning is then handled entirely in the auto-generated pack and unpack methods - e.g. the auto-generated code would look something like:

```
// NOTE: The generated class only stores the latest version members (but knows defaults for legacy members)
class SayText {
  static const bool  kDefault_playAnim = true;
  static const float kDefault_pitch = 0.0f;
  static const float kDefault_speed = 1.0f;

  string  text;
  float   speed;
}


void SayText::Pack(outBuffer) {
  outBuffer.Pack(text);

  if (_sendVersion == 1) {
   outBuffer.Pack(kDefault_playAnim);
  }

  if ((_sendVersion >= 2) && (_sendVersion <= 3)) {
    outBuffer.Pack(kDefault_pitch);
  }

  if (_sendVersion >= 3) {
    outBuffer.Pack(speed);
  }
}


void SayText::UnPack(inBuffer) {
  inBuffer.UnPack(text);

  if (_sendVersion == 1) {
    // Only need to advance the buffer pointer - e.g:
    bool dummyPlayAnim;
    outBuffer.UnPack(dummyPlayAnim);
  }

  if ((_sendVersion >= 2) && (_sendVersion <= 3)) {
    // Only need to advance the buffer pointer - e.g:
    float dummyPitch;
    outBuffer.UnPack(dummyPitch);
  }

  if (_sendVersion >= 3) {
    outBuffer.UnPack(speed);
  } 
  else {
    // Use the default value (as one wasn't sent)
    speed = kDefault_speed;
  }
}
```

* Pros:
    * Automated - very little manual maintenance, and adding additional languages only requires writing a new code generator.
    * Scales better - only 1 definition per message, only 1-in memory class representation, compound types aren't affected by their internal members being versioned.
    * Minimal on-wire memory usage
* Cons:
    * Needs building into CLAD 
    * Cannot be done in-place (so works fine for C++ CLAD, but not usable for e.g. FlatBuffers), so there is a small performance hit when packing and unpacking (but basically the same as the exsting cost for CLAD on Cozmo)

## Open Questions
* After the handshake, Where exactly does the version live?
    * Proposal A) Explicit injection of version into all clad called (i.e. Pack, Unpack, Size...)
        * `size_t Foo::Pack(CLAD::SafeMessageBuffer& buffer, int cladVersion=0) const` 
        * `Pros:
            * `Simpler to maintain communication with multiple version simultaneously if necessary
            * Defaulting them to zero would allow this code to generate against our existing clad domains without breaking functionality
        * `Cons:
            * Modifying these function prototypes introduces necessary code delta
            * Even with defaulting zero, we should at least unit test that areas which don't need versioning don't use it
            * I'm less clear on how to deal with the ==, != operators.  Perhaps unpack / constructors could just inject 0 into any field not covered by current version?
    * Proposal B) A static variable exposed to all messages, such as implied with _sendVersion above
        * `Pros:
            * No overhead on the messages - SDK/Switchboard just needs to track the version from the handshake
        * `Cons:
            * Much harder to maintain multiple connections if thats ever needed
            * Enforces that the switchboard clad needs to be generated seperately from general clad generation
* What exactly is the ultimate purpose of the cpp hash_str / union_hash / VersionHash[16] that shows up in many cpp classes
    * What version does this refer to, and is it still relevant across switchboard if we are more actively versioning the communication
* What exactly constitutes a breaking over-the-wire change?
    * If I create separate emitters for switchboard, does that make it ok for me to do anything I deem appropriate with those emmitters without breaking any other parts of the project?
        * Is it sufficient to add configuration flags so that only certain things utilize certain parts of the emmitters (i.e. the versioning?)
        * Is creating seperate emitters for switchboard an ok thing to do, or is that kind of duplication/forking frowned upon with clad? 
        * It already seems like certain things are built for c++Lite, while others aren't, but i'm still trying to work out if thats a config-space thing, a clad source-file thing, or a message-buffer-internals things