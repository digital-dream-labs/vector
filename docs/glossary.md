Glossary

| Term               | Definition |
|--------------------|-------------------|
| Action             | A specific operation to be executed by the robot.|
| Animation          | A set of movements executed in sequence.|
| Animation Process  | Software layer between engine and robot process. Manages animations and audio sync. AKA 'vic-anim'.|
|ASR                |Automated Speech Recognition - A service/algorithm that turns waveform audio into text hypothesis. eg: google speech|
| Beamforming       |Roughly, using multiple microphones to determine the direction of a speaker And/Or isolate audio from a particular direction or speaker. https://en.wikipedia.org/wiki/Beamforming|
| Behavior          | A "state of mind" that determines what Vector will do next. |
|BLE                | Bluetooth Low Energy|
|BTLE                | Bluetooth low Energy|
|Chipper             | Current name for the service the Cloud team is writing that sits between the robot and the 3rd party cloud services that do ASR & NLP.|
|CLAD                | "C-Like Abstract Data" language. CLAD is used to define a message-passing protocol used between robot, engine, and application components.|
|Cozmo              | Victor's little brother, a little robot with a big heart|
|Cozmo 2            | Original codename for Victor|
| DAS                | Data Aggregation service. Log system used to report diagnostics and analytics. |
|DVT                | Design Validation Test |
| Engine Process    | Software layer between switchboard and animation process. Manages actions and behaviors. AKA 'vic-engine'.|
| EVT               | Engineering Validation Test |
| FTUE              | First Time User Experience, aka "Meet Victor"|
| IMU               | Inertial Measurement Unit. A device containing accelerometer and gyroscope. Cozmo contains an IMU to determine acceleration, turn rates, and positioning.(see also https://en.wikipedia.org/wiki/Inertial_measurement_unit)|
| Intent            | an intent is a desired action that is triggered in response to a collection of utterances. For example, "turn left", "go left" or "why don't you turn left"  are all valid utterances that might causes the "turn left" action (intent) to be executed. |
|Intent Latency	    |
|Natural Language Processing  | A class of algorithms that turn text into labelled information, such as parts of speech or intents and entities. In our context, natural language processing (NLP) is a software layer that takes the text output from voice recognition, and matches it against a set of possible actions or intents. NLP is the layer where we account for the large variety of ways a user might ask the robot to do something. For example, if the user says "how about we play speed tap?" or "let's play speed tap". These should map to the same action - specifically, that the robot should start playing speed tap. A strong NLP layer will use its knowledge of a specific language to create matches without being explicitly told of all the potential patterns. |
| Origin               | (0,0,0) of Vector's coordinate space. Vector's local origin between the front wheels. X-axis points out Vector’s front, y-axis to his left, and z-axis up.|
| OTA                  | "Over The Air", i.e. performing a wireless software & firmware upgrade |
| PID                  | Proportional Integral Derivative controller.  Vector's onboard processor uses a PID feedback loop for continuous update of tread & lift controls.  See also: https://en.wikipedia.org/wiki/PID_controller |
| Proto                | A webots file defining a single node in a simulation. See https://www.cyberbotics.com/doc/reference/proto |
| PVT                  | Production Validation Test (see also evt-dvt-pvt-decoded)|
| PWM                  | Pulse Width Modulation. Vector uses PWM to control the intensity of his lights.  See also: https://en.wikipedia.org/wiki/Pulse-width_modulation |
| Robot Process      | Software layer between animation process, syscon, and hardware. AKA 'vic-robot'.|
| SDK                | Software Development Kit, aka messaging interface between Victor and the outside world |
| Sensory            | Company providing wake-word technology for Anki. |
| Signal Essence     | Company providing Beam Forming technology for Anki.|
| SLAM                | Simultaneous Localization and Mapping. A technique where an agent can create a map and figure out where they are within it at the same time. |
| Slot                | a variable that is filled out when matching an intent. For example, if the intent is for turning left a certain number of degrees, e.g. "turn left X degrees", then X is called a slot variable that is filled out from the user's utterance. |
| Spine               |  Connection between Victor's head and body |
| Switchboard Process | Software layer between applications and engine process. AKA 'vic-switchboard'. |
| Syscon              | System Controller, aka Victor's body |
| Trigger Word        | The trigger (a.k.a wake word) is a voice command running locally on the robot (does not use the cloud) that is used to cause Victor to start streaming voice data to the cloud. For example, "Hey Cozmo", will trigger Victor to start it's cloud voice recognition capability, and start streaming data to the cloud so subsequent commands can be recognized with higher accuracy. |
| Utterance	          | an utterance is a piece of speech (audio) in which the user asks the robot to perform an action, e.g. "Hey Cozmo, pick up that block and bring it to me"  |
| VAD                 | Voice Activity Detection - The capability to detect when someone has stopped talking in an audio stream. |
| Vector              | alternate name for Victor |
| VicOS               | Victor's proprietary blend of operating system components |
| VicOS LA            | VicOS "Linux Android" flavor. We don't talk about this. |
| VicOS LE            | VicOS "Linux Embedded" flavor. |
| Victor              | alternate name for Vector |
| Voice Command        | at Anki, we've used the term "Voice Command" to refer to a limited type of voice recognition. Rather that convert speech to generic text, voice command refers to recognizing (from speech audio) a specific command from a small set of possible commands. |
| Voice Recognition	  | In our context, voice recognition means taking speech (audio) data and converting it to text. You may also see this referred to as "Speech-To-Text" (STT) or ASR (automated speech recognition).
| Wake word	          | See Trigger Word |
| Wwise    	          | Vector's sound engine.  Pronounced "wise".  See also: https://www.audiokinetic.com/products/wwise |

