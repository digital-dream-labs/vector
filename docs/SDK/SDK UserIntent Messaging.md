# SDK UserIntent Messaging

Created by Bruce vonKugelgen Last updated Mar 28, 2019

The SDK will send UserIntents to be consumed by SDK scripts when they have behavior control.  VIC-3682 - Receive Voice Intent events (for existing intents only) CLOSED
See https://github.com/anki/victor/blob/master/docs/architecture/behaviors_intents.md for a detailed explanation of how the Robot handles UserIntents.

In deciding which UserIntents to send to the SDK, not all of them are appropriate for user consumption. 

Our current criteria for including userIntents:

* Nothing that a higher-level Behavior consumes, because we can never get it in behaviorSDKInterface (like greeting_goodnight that puts him to sleep)
* Nothing that can't be tested because it is a secondary prompt in a behavior (like blackjack_hit, knowledge_response), or because it is not released (message_playback)
* No Amazon/Alexa
* No onboarding intents
* No testing intents

Here is our pick list for discussion and implementation (drawn from userIntent.clad).

This is not intended as literal reflection of the current state of SDK UserIntents.  For that, refer to the .cpp and .py code.


| Intent (from userIntentTag.h)|Send?|Reasoning|
|------------------------------|-----|---------|
|unmatched_intent              |No	 |Not a valid UserIntent|
|amazon_signin                 |No/NA|Higher-priority Amazon behavior will get this before SDK can. Also, bad form to mess with a partner's feature|
|amazon_signout                |No/NA|Higher-priority Amazon behavior will get this before SDK can. Also, bad form to mess with a partner's feature|
|blackjack_hit                 |No/NA|Can't get to this since the SDK behavior blocks the Blackjack behavior|
|blackjack_stand               |No/NA|Can't get to this since the SDK behavior blocks the Blackjack behavior|
|blackjack_playagain           |No/NA|Can't get to this since the SDK behavior blocks the Blackjack behavior|
|character_age                 |Yes	 ||
|check_timer                   |Yes	 ||
|explore_start                 |Yes	 ||
|global_stop                   |Yes	 ||
|global_delete                 |No/NA||
|greeting_goodbye              |Yes	 ||
|greeting_goodmorning          |Yes	 ||
|greeting_goodnight            |No/NA|A higher priority behavior will get this before the SDK|
|greeting_hello                |Yes  ||
|imperative_abuse              |Yes	 ||
|imperative_affirmative        |Yes	 ||
|imperative_apology            |Yes  ||
|imperative_come               |Yes  ||
|imperative_dance              |Yes  ||
|imperative_fetchcube          |Yes  ||
|imperative_findcube           |Yes  ||
|imperative_lookatme           |Yes  ||
|imperative_lookoverthere      |No/NA||currently behind a feature flag.  Excluding for now.|
|imperative_love               |Yes  ||
|imperative_praise             |Yes ||
|imperative_negative           |Yes ||
|imperative_scold              |Yes  ||
|imperative_shutup             |No/NA|A higher-priority behavior will get this before the SDK|
|imperative_quiet              |No/NA|A higher-priority behavior will get this before the SDK|
|imperative_volumelevel        |Yes  ||
|imperative_volumeup           |Yes  ||
|imperative_volumedown         |Yes  ||
|movement_forward              |Yes  ||
|movement_backward             |Yes  ||
|movement_turnleft             |Yes  ||
|movement_turnright            |Yes  ||
|movement_turnaround           |Yes  ||
|knowledge_question            |Yes  ||
|knowledge_response            |No/NA|Can't get to this since the SDK behavior blocks the Knowledge Graph behavior|
|knowledge_unknown             |No/NA|Can't get to this since the SDK behavior blocks the Knowledge Graph behavior|
|meet_victor                   |No   |onboarding|
|message_playback              |No   |Unreleased feature|
|message_record                |No   |Unreleased feature|
|names_ask                     |Yes  ||
|play_anygame                  |Yes  ||
|play_anytrick                 |Yes  ||
|play_blackjack                |Yes  ||
|play_fistbump                 |Yes  ||
|play_pickupcube               |Yes  ||
|play_popawheelie              |Yes  ||
|play_rollcube                 |Yes  ||
|seasonal_happyholidays        |Yes  ||
|seasonal_happynewyear         |Yes  ||
|set_timer                     |Yes  ||
|show_clock                    |Yes  ||
|silence                       |No/NA|A higher-priority behavior will get this before the SDK|
|status_feeling                |No/NA|Not currently used by Robot|
|system_charger                |No/NA|A higher-priority behavior will get this before the SDK|
|system_sleep                  |No/NA|A higher-priority behavior will get this before the SDK|
|take_a_photo                  |Yes  ||
|weather_response              |Yes  ||
|test_SEPARATOR                |No   |Test code|
|test_user_intent_1            |No   |Test code|
|test_user_intent_2            |No   |Test code|
|test_name                     |No   |Test code|
|test_timeWithUnits            |No   |Test code|
|INVALID                       |No   |Not a valid UserIntent|

Even though constructing a blacklist of UserIntents not to be sent to the SDK would be simpler, we will maintain a whitelist of valid SDK UserIntents to avoid unexpected additions.

In addition, the allowed UserIntents will be maintained enumerated in the SDK so that there is no confusion for users (waiting for UserIntents that can never be sent, for example).