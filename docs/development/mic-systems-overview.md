# Mic Systems Overview
Breakdown of the processing we do for microphones on Victor, starting with the smallest code modules and working larger.

## AudioUtil
The shared util called [audioUtil](/lib/util/source/anki/audioUtil) defines the base audio types we use. It has defines for samplerate, type of audio samples, and defines the interface for doing speech recognition (with the actual implementation to be supplied in a subclass).

## SpeechRecognizerTHF and Sensory
* The SpeechRecognizerTHF class is a subclass of AudioUtil::SpeechRecognizer (an implementation-agnostic interface), that uses the Sensory TrulyHandsFree(THF) API Version 4.4 as its implementation.
* The THF implementation is provided as a binary that runs on our platform, along with header files. The binary is initially delivered with a built-in expiration date. In order to get the non-expiring version of the binaries, we have to unpack the expiring versions, upload them to a shared directory with Sensory, then request that Sensory strip out the expiration. Then we can use these nonexpiring versions in production on the actual robot.
  * The version of the Sensory API has two modes for doing speech detection, "wordspotting" and "phrasespotting". "Wordspotting" is good for trying to recognize more complicated grammars, where you end up trying to match against individual words in a phrase and maybe parse out some arguments, while "Phrasespotting" is good for matching against a whole phrase in one go. "Phrasespotting" is better for our use case, and I think it's the technology that Sensory is generally working on more these days overall.
  * The `allowsFollowUpRecog` arg is for the wrapper class SpeechRecognizerTHF, so it knows whether this new "search" is intended to run immediately after a trigger. The reason being that different initialization args are used for the followup searches, so that the audio buffer on the first-recognized audio is handed off to the second search when you switch to it, so that any syllables immediately following the thing that was recognized first are still detected against.
  * This was part of the setup for voice commands for Cozmo, when there were going to be a pre-trained set of commands that we had as one "search", and we switched to it immediately after the recognition happened on the initial search, which was the HeyCozmo trigger.

**NOTE:** This wrapper around the THF API is used both in Victor and in a standalone python module that we use for QA testing, found here: https://github.com/anki/triggerword.
The python module only builds for mac (using the thf binary for osx), for both python2 and python3.

**When we get delivery of a new model and/or searchfiles to be used by the THF API, here are the steps for integrating:**
1. Add the new data to the trunk of the anki-thirdparty svn "sensory" directory (ask dwoods or dmudie for details if unfamiliar) (https://svn.ankicore.com/svn/anki-thirdparty/trunk/sensory/)
1. Merge the new data to the victor branch (https://svn.ankicore.com/svn/anki-thirdparty/branches/victor/sensory/)
1. Update the DEPS file in the victor repo to use the new version of the svn repo that contains the new data in the victor branch
1. Update the resources/config/micData/micTriggerConfig.json file with the new data files
1. (optional) Update the SupportedLocales and `kTriggerModelDataList` and `kMicData_NextTriggerIndex` variables in [micDataProcessor.cpp](/animProcess/src/cozmoAnim/micData/micDataProcessor.cpp) to easily access the new data with a console var

The THF models we've been getting from Sensory have generally been delivered in the most accurate model size of 1mb. Sensory also can provide 500kb and 250kb models, with significant dropoffs in accuracy. We requested all 3 sizes for HeyVector in en-US, and the accuracy stats provided by Sensory can be found in the notes accompanying the file delivery, in EXTERNALS/anki-thirdparty/sensory. I did a quick measurement of CPU performance differences between the model sizes in the sample Alexa model Sensory gave us, and got these results on average(with a vector robot running at 533 megahertz):
* 1mb trigger: 3.965ms per 10ms aka 40% of a core 
* 500K trigger: 2.000ms per 10ms aka 20% of a core
* 250K trigger: 1.137ms per 10ms aka 11% of a core

## SignalEssence
* The SignalEssence API allows us to both combine the 4 channels of raw microphone data into a single channel that's "focused" on the best direction, as well as get information on the confidence scores of the 12 clock face directions around the robot. This happens on every new block of audio data, 10ms at a time. Their API includes some code that acts as the glue between their proprietary implementation and ours (see policy_actions.h, mmif_proj.h), and the precompiled binary that contains their proprietary implementation. Both are stored in the svn anki-thirparty repo (https://svn.ankicore.com/svn/anki-thirdparty/trunk/signalEssence/)
* The API exposes some data through the SEDiag... interface that we use in MicDataProcessor to get information after processing each audio block.
* There is also an anki_victor_vad project that SignalEssence created for us to do simple activity detection. The implementation can be found in svad.c/h. We use it in MicDataProcessor::ProcessMicrophonesSE with the call DoSVad(). See the MicDataProcessor section for more info on how that information is used.

## MicDataProcessor
* The MicDataProcessor class is the first Victor-specific piece of functionality in the system. Its responsibility is to use SpeechRecognizerTHF and the SignalEssence API on the incoming micdata stream to combine mic channels, extract signal direction and other data, and respond when a triggerword is detected.
* It is organized expecting that `MicDataProcessor::ProcessMicDataPayload` is called initially with new data, from the main thread of the anim process. From there, the audio is first combined and direction information retrieved in another thread, running in `MicDataProcessor::ProcessRawLoop`. Next the combined mic data (4 channels to 1) is put through the trigger recognizer on a different thread, running in `MicDataProcessor::ProcessTriggerLoop`.
* The MicDataProcessor tries not to do unnecessary work. Inside `MicDataProcessor::ProcessMicrophonesSE`, before the more expensive mic combining and direction finding occurs, we use a simple VAD (voice activity detector) functionality to decide if there is relevant activity in the current audio block. There is logic combining that activity check, some history, whether motors are moving (which could obscure activity therefore we should assume there was activity, so as to not miss it), whether the speakers are playing audio, etc. This activity flag is also passed along and used to decide whether we should do trigger recognition on this block of audio.
* As part of owning the SpeechRecognizerTHF instance, the MicDataProcessor also has the functionality for changing the model used for trigger recogntion when requested.
* MicDataProcessor is responsible for the model configuration to be used by SpeechRecognizerTHF. To do this, it uses the MicTriggerConfig class, which holds the mapping of locale to model and search file for that locale. The data for this configuration is loaded during initialization. 

## MicDataSystem
* The MicDataSystem class provides the API for other parts of the anim process to interact with the mic system, including the main Update() call from the anim process update loop. It also handles the logic for mic data recording and streaming jobs, communicating with the cloud process to send streaming data to the cloud, and sending out messages to the engine based on events in the mic data system.
* On non-shipping builds, both the trigger and the subsequent intent are saved to the robot by default (the most recent 100 files are saved of each). These files are accessible either through the web server for the robot (http://ROBOT_IP:8889/cache/micdata/), or using ssh on the filesystem (/data/data/com.anki.victor/cache/micdata/). For convenience, there are scripts to automatically pull the micdata using either method. See [ear_puller](/tools/ear_puller.py) to get the files over http or [pull_micdata](/project/victor/scripts/pull_micdata.sh) to get the files over ssh. 

## Misc
* There is a consolevar button that can be used to trigger a recording of the micdata for a variable length of time. It will record both the raw (4-channel unmodified) and processed (combined and gained) micdata for the given number of seconds (default 4, see `kMicData_ClipRecordTime_ms`). The button can be found in the anim process consolevars, in the MicData tab, and is called RecordMicDataClip.
* There is a commit I've used to create a one-off build of the executables, where pressing the button on Vector will start the same recording as above. See https://github.com/anki/victor/commit/732355eb453a15dd06b1231297ff03d7db46e1d6, part of the branch lee/button-record-audio-new. It should be easy to cherry-pick this commit ontop of master if the same functionality needs to be recreated again.
