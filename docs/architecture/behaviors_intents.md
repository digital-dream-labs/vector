### User/Cloud/App Intents in the Behavior System

There are three types of intents. Each represents a user's intention for the robot to perform some action, such as a fist bump. 

* **User intents** are a CLAD union of structs, where each struct represents a different user request and its data. User intents are not exposed outside of the behavior system, but can be requested either by voice or through the companion app. In other words, any cloud or app intent is translated into a user intent before being handled by the behavior system. 
* **Cloud intents** are received from the cloud process containing the intent type along with any parameters. The behavior system is not aware of cloud intents beyond how to translate them into user intents.
* [DEPRECATED] **App Intents** are messages received from the companion app containing the intent type along with any parameters. The behavior system is not aware of app intents beyond how to translate them into user intents. (Note: app intents were cut from the chewie design)


#### Simple Animated Voice responses

If all you need to do is create a voice command (with no parameters / extensions) that in result causes the robot to play an animation from a single animation group, there is a shortcut to the larger process below called "simple voice responses". You can simply add your new mapping to the top of [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) under the `simple_voice_responses` section. In this section, you specify a cloud intent string and optionally a feature flag (see below) and then a response. The response must contain an animation group string (checked at load time) and may optionally specify emotion events or active features (used for DAS and the app string). None of this requires touching any code or CLAD files, or even a full build (with the exception, currently, of adding a new `ActiveFeature`, if that is needed).

If you need any parameters, or want a more complex / custom behavior to run, or it needs to run at a different level in the tree with a different priority, you'll need to create a new intent.

#### Creating a new intent

In [`userIntent.clad`](/clad/src/clad/types/behaviorComponent/userIntent.clad), create a new CLAD `structure` containing the data associated with your user intent, or use `UserIntent_Void` if there are no params. Add an entry to [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) that defines the mapping from cloud and app intents to user intents.

The names of the user intent parameters should, when possible, match the names of the cloud/app intent parameters. Since user intents are CLAD, the parameter names must be valid C++. However, the parameters associated with cloud intents are based on cloud service requirements, and their names may not be valid C++ (e.g., a parameter may be called `time.duration-s`). In this case, you may define a substitution of a cloud variable name to a CLAD field name using the `cloud_substitutions` JSON field.

Additionally, in the JSON we receive from the cloud, all parameters are strings. For example, the integer 10 is received as `{"paramName": "10"}`. To help parse cloud intents, if the user intent parameter is a numeric type, you should specify that in the `cloud_numerics` list.

#### Handling intents

When a cloud or app intent arrives, it gets translated into a user intent by the `UserIntentComponent`, which also marks that intent as _pending_. Only one user intent can be pending at any time, so receipt of a new cloud or app intent will replace the last user intent with a new one. Once a user intent is pending, it must be activated (handled) within a small number (2-4) of engine ticks, or a warning is printed and it is cleared automatically. Similarly, there can only be a single _active_ user intent. In general, this intent should stay active during the time that the behavior responding to it is active. To make this easier, you can use `ICozmoBehavior::SmartActivateUserIntent()` from inside a behavior. Note that it is possible to simultaneously have a pending and active intent that are different if you activate one intent, then while that is active another intent comes in. This is unlikely because in the common case the trigger word reaction will deactivate the "old" intent, but it is possible (e.g. a "voice explorer mode" behavior might stay active and handle multiple intents by dispatching to actions as the intents come in). If you try to activate an intent while one is already active, warnings will print (this would be a bug).

When writing a behavior, you have a few options to handle an intent. Use of the JSON behavior parameter `respondToUserIntents` will block activation of that behavior until one of the specified user intents is pending, similar to how `wantsToBeActivatedConditions` works. If the user intent is set as pending, then upon behavior activation, that intent will be automatically activated during the lifetime of your behavior. Your behavior class may also request to block activation until a user intent is pending by calling `AddWaitForUserIntent`.

The activation conditions in `respondToUserIntents` or `AddWaitForUserIntent` will compare the pending user intent to the `UserIntentTag` that you provide, which is the autogenerated tag for the `UserIntent` union. It's also possible to match a pending intent against its parameters, and if you need something even more complicated, you may write a lambda to check the `UserIntent` struct directly.

All pending user intents must be activated shortly after they are pending. If you need to access the user intent data after activating it, you can use `GetActiveUserIntent()` to get the full data (not just the intent tag) of the intent.

Note: Don't include the CLAD-generated files `userIntent.h`/`.cpp` in your behavior unless it's absolutely necessary. Instead, the file [`userIntents.h`](/engine/aiComponent/behaviorComponent/userIntents.h) provides most methods you will need for handling user intents.

#### Unmatched and unclaimed intents

Sometimes, an incoming cloud intent does not have a matching user intent. This most frequently happens when the cloud cannot understand the voice command and issues an `intent_system_unsupported`, which is purposefully not listed in [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json). It could also occur if the version of [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) is not the same as that of the cloud, i.e., if the robot is out of date. If there is no match, the `unmatched_intent` user intent is set as pending. Currently, when the `unmatched_intent` user intent is pending, it is activated, a "huh?!" animation plays, and then intent is deactivated.

If a user intent goes unclaimed for more engine ticks than we allow (2-4), it means the behavior tree is not configured to handle the intent based on its current state. This may be because we forgot to implement the feature, or it may be because the intent was understood but can't be carried out (e.g. you say "come here" but the robot is being held in the air). In these cases, a behavior `BehaviorReactToUnclaimedIntent` will play an animation that is meant to communicate "I can't do that" if an intent goes unclaimed.

#### Feature flags

If the new voice feature is feature flagged, you must specify such in [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) rather than just in the behavior instance json. This way, if the intent does come in from cloud, the user intent map will map it to "unmatched" with a negative earcon, rather than _successfully_ matching the intent, but then refusing to run the behavior which results in an unclaimed intent ("I don't know") rather than the desired "huh" reaction. The way to think about this is that disabling a feature flag should have the equivalent result (as far us user-visible behavior) as saying something the robot doesn't understand.

#### Unit tests

When you add a new intent and believe that it is properly hooked up,  we force you to write some unit tests for it (sorry not sorry). 

Since user/cloud/app intents are in active development, it should be possible to add handlers in behaviors and entries in [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) that don't encompass all possible uses of the intent. Eventually, the entries in [`user_intent_map.json`](/resources/config/engine/behaviorComponent/user_intent_map.json) should be considered as complete. For now, we have a separate file that lists those intents that we believe to be fully handled.

In [`completedUserIntents.json`](/resources/test/aiTests/completedUserIntents.json), add a JSON entry for the user intent along with a label, which can be used by any unit test to grab the completed intent by name. 

If the intent is handled as part of the HighLevelAI behavior, you must also add an entry to the `LabeledExceptions exceptionsList` in [`testBehaviorHighLevelAI.cpp`](/test/engine/behaviorComponent/testBehaviorHighLevelAI.cpp). In this entry, you should include the intent label from [`completedUserIntents.json`](/resources/test/aiTests/completedUserIntents.json) and a list of states of HighLevelAI that should _not_ handle the intent. The test checks that your assumptions match. 

If the intent is handled elsewhere, a unit test will force you to write a test in [`testUserIntentTransitions.cpp`](/test/engine/behaviorComponent/testUserIntentTransitions.cpp). 

#### Trigger words
We handle trigger words in a similar manner to user intents. When the trigger word is detected, the `UserIntentComponent` marks it as pending, and it must be cleared (handled) by some behavior within some number of ticks. Otherwise, a warning is printed and the trigger word is cleared automatically. However, the trigger word is _not_ a user intent (as it was previously under cozmo / early victor days).

#### Cloud intent details

In normal operation, `vic-cloud` sends cloud intents through the BehaviorComponentCloudServer as CLAD messages of type [`CloudMic::Message`](/clad/src/clad/cloud/mic.clad). These messages can be actual intents, where the type is `result`, or other information such as an error, a "stream open" message, or debug info.

If the message is a result, it contains an intent, a string of "parameters" and a string of "metadata". The parameters contain all of the extended info from cloud, e.g. the weather details, or the name of the person meeting Vector. These are sent as stringified JSON, and may contain newlines, escaped quotes, etc.

The UserIntentComponent then turns this into an "expanded" JSON value, where we unpack and parse the parameters string (into a json key called "params" instead of "parameters"). This expanded JSON is then matched against user intents in the user intent map as described above.
