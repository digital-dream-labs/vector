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

![](images/VictorSDK_HighLevelOverview.png)


## Initial Connection Handshake (for versioning)

![](images/VictorSDKMessageLayer_Handshake.png)


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
        * size_t Foo::Pack(CLAD::SafeMessageBuffer& buffer, int cladVersion=0) const 
        * Pros:
            * Simpler to maintain communication with multiple version simultaneously if necessary
            * Defaulting them to zero would allow this code to generate against our existing clad domains without breaking functionality
        * Cons:
            * Modifying these function prototypes introduces necessary code delta
            * Even with defaulting zero, we should at least unit test that areas which don't need versioning don't use it
            * I'm less clear on how to deal with the ==, != operators.  Perhaps unpack / constructors could just inject 0 into any field not covered by current version?
    * Proposal B) A static variable exposed to all messages, such as implied with _sendVersion above
        * Pros:
            * No overhead on the messages - SDK/Switchboard just needs to track the version from the handshake
        * Cons:
            * Much harder to maintain multiple connections if thats ever needed
            * Enforces that the switchboard clad needs to be generated seperately from general clad generation
* What exactly is the ultimate purpose of the cpp hash_str / union_hash / VersionHash[16] that shows up in many cpp classes
    * What version does this refer to, and is it still relevant across switchboard if we are more actively versioning the communication
* What exactly constitutes a breaking over-the-wire change?
    * If I create separate emitters for switchboard, does that make it ok for me to do anything I deem appropriate with those emmitters without breaking any other parts of the project?
        * Is it sufficient to add configuration flags so that only certain things utilize certain parts of the emmitters (i.e. the versioning?)
        * Is creating seperate emitters for switchboard an ok thing to do, or is that kind of duplication/forking frowned upon with clad? 
        * It already seems like certain things are built for c++Lite, while others aren't, but i'm still trying to work out if thats a config-space thing, a clad source-file thing, or a message-buffer-internals things

It strikes me that this probably means we want to add the firmware version to the Victor Robot DAS events. We have a version field for the app, but not for the robot. 

definitely. There's a good chance we'll update robot code in the future without updating the app, or one day we might release out-of-sync (although my guess would be that the first several releases would include both)

Just a heads up that this implies some sort of release version process just like with app updates, unless the robot authentication work provides a better way of preventing unreleased builds from sending test data in production (cc @Tom Eliaz @Gareth Watts - any thoughts on that? We've been discussing authenticating robots - what about authenticating builds?)

That's a fantastic question but a bit outside my scope – I have no idea what versioning on robot will look like. Since it's not (normally) user visible I could imagine we might just go with a git sha or build number, but definitely don't quote me on that, it's a question for production & the build "team"

I am interested if there are specific reasons why it would be advantageous for us to use generated hashes / commit based tracking.  My current understanding/proposal is that the versioning surfaced in these messages will be tied to releases.   

 - i.e. "I am a robot that speaks 1.0.0 through 2.2.0", and an app saying "I speak 1.0.0 through 2.5.0 - we will henceforth speak 2.2.0"

i think we will have to use both. 
In internal development, specifically on an internal message pipeline we should use commit hash or build number - just to be able to signal to any developer that clad rebuild is needed. (the old clad version mismatch).
However for external messages it should be release version like Nic said.

External messages are the only ones I'm concerned with in terms of this particular versioning work.

But what do those "2.2.0" version numbers mean? I think we need to have a much broader discussion about versioning in victor in general, but my guess is that we'll actually want to use a protocol version for this, rather than "app" version or "robot" version. So the app might be versioned like "2.1.3" and the robot might be something like "2.145321" or even a sha or something like that, and then both of them can say they speak protocol versions between 1 and 3 (or maybe 1.0-2.0). Alternatively, we could decide to just version the robot, and the robot and app both say the same things like "I can speak robot version 1.2.0 - 1.3.0" but then we have to be much more careful about hot-fixes and updates and the like.

I agree, this should be protocol version. I envision it as an integer, and we bump the version up 1 for every update to the external interface. There'd be no semantics captured in the version (in a good way) because we can break off support for old versions when we feel the gap of time is appropriate instead of needing to bump from like 1.2.0 to 2.0.0 to indicate that bump.

I think the answer to the first big question in the open questions section (about where the version lives after handshake) is it lives in the client, and gets passed to each message when that message is constructed. This makes it easy for multiple connections because the client can open each connection and provide different versions to their clad message constructions, and (with sane defaults) it prevents breaking old clad interfaces.

For the second question, I think those aren't a big deal in a versioned clad world, but probably could be useful for somebody working inside the engine. Say they update one process with a new version of clad, but not another, they should be able to see that those clad hashes are mismatched.

