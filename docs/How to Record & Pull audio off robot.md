# How to Record & Pull audio off robot

Created by Rachel Lewis Jun 27, 2018

## RECORD : 

* Using the button in the 8889 console vars will start a recording (MicData -> RecordMicDataClip)
* A 3..2..1… will show on his face : he is recording!
* When the robot is recording this way, it won’t respond to the trigger as normal, and won’t stream to cloud 

![](imagesScreenshot%202018-06-27%2015.12.49.png)


## PULL AUDIO : 

* Files captured this way end up in the folder called “debugCapture” instead of “triggeredCapture”, and will store up to 100 recordings per robot

* Go To :

ip:8889/cache/micdata 

DebugCapture : This is for audio that is force triggered by using the RecordMicDataClip as mentioned above.  

TriggeredCapture : This is the audio captured after trigger word - would be the Voice Command space 

TriggerOnly : Trigger Word (including false positives that set off robot) 

![](images/Screenshot%202018-06-27%2015.14.30.png)

