# Victor Web Server

Created by Paul Dimailig Last updated Jun 07, 2018

ReadMe: https://github.com/anki/victor/blob/master/docs/development/web-server.md

We have a Web Server view for Victor that displays information from our robot as well as giving the ability to view variables and call out functions that normally would need to be run in other software - e.g. Webots was previously the only way to run some of the animation/behaviors until a tool was made.

Note: some guest networks block video streaming which seems to block this webserver from being connected to. 

You can access them by just inputting your robot's ip in your browser, plus a port based on which process you wish to view.

NOTE: If you get prompted for a username and password when accessing: anki / overdrive. Also, this does not require any sort of manual connection from you other than your robot just needs to be connected to wifi and have an IP.

Process: <robot ip>:8887 - A standalone web server (currently not working).

Engine Process: <robot ip>:8888 - This is where you can view some of the engine-related variables and functions.

Animation Process: <robot ip>:8889 - This is where you can view some of the animation-related variables and functions. This is also where you can view microphone directional confidence as well as set the robot's volume.

If you happen to be running Webots and only using the robot simulator (Cozmo2World.wbt) for whatever reason, you use the ip 127.0.0.1 in place of the robot ip.

## Main Victor Web Server
### Main

This tab shows info about the robot.

![](images/Screen%20Shot%202018-03-06%20at%202.48.18%20PM.png)

### Console Vars/Funcs
Console Vars is a tool where you can modify known variables within the robot as well as call functions that it knows. This is the default tab you land on when you input your robot's ip into your browser.

![](images/Screen%20Shot%202018-03-06%20at%202.49.04%20PM.png)

Note: If the above screen is blank with just a "title" in it, then it may be slowly loading up (sometimes happens if you had a WebViz Behavior tab open for a long time, or possibly affected if you're building?). If nothing still shows up, just power cycle the robot and try refreshing the page.

/consolevars - Displays all variables and functions in the robot that you can modify.

/consolevarlist - Displays a list of all variable names that exist in the robot.

/consolevarlist?key=search_key - Returns a list of variables, optionally you can add a filtering parameter that contains 'search_key' - e.g. if you search for 'sound' it will return a list of variables with the part 'sound' in their name

/consolevarset?key=name_of_variable&value=new_value_of_variable - Allows you to modify a variable to contain a new value.

/consolevarget?key=name_of_variable - Returns whatever value the 'name_of_variable' is set to.

/consolefunclist?key=search_key - Returns a list of functions, optionally you can add a filtering parameter that contains 'search_key'

/consolefunccall?func=name_of_function&args=arguments - Allows you to call a function along with its arguments.

WebViz - Brings you to a separate page where you can monitor/force behaviors, send intents, and view microphone directional confidence. Separate section to explain its features.

Tip: These can be run in a Terminal in the form of a curl command. This is an alternative to having to open a browser tab just to call the function.

Example to get a list of animations: curl "http://<robot's ip>:8889/consolefunccall?func=ListAnimations"

### Perf
This tab displays the performance of the robot.

![](images/Screen%20Shot%202018-03-06%20at%202.55.19%20PM.png);

Update NOW - Updates the chart with the current stats at the time the button was clicked.

Set update period (ms) - Set an interval as to how often to update if auto-update is toggled ON.

Toggle Auto Update - Auto updates the chart at the rate of the above intervals.

Stats:

* CPU freq: 
* Temperature: Displays the current temperature of the robot in Celsius.
* Battery: Displays the current voltage of the battery, ranging from 3.6-5.0
* Uptime: This is the number of real-time seconds since the robot was booted
* Idle time: This is the number of accumulated 'seconds' of idle time since the robot was booted. It often increased FASTER than real time, because it's the accumulation of idle time on all FOUR cpu cores. (So for example if all 4 cores were idle, it would increase at 4x real time rate.)
* Real time clock: This is the date/time on the robot. Currently it appears in UTC time, so 8 hours ahead of PST.
* Memory: Shows the amount of used and free user memory, in kB
* Overall CPU: Shows the % of overall CPU use, since the last time it was queried by the client (browser) you're using.
* CPU0, 1, 2, 3: Shows the % of CPU use by each CPU, since the last time it was queried by the client (browser) you're using. 

### Files
### Processes
Info:  VIC-1005 - Web interface: Basic process control CLOSED
This page displays all the processes that live in the robot and indicate what status they are currently in.

![](images/Screen%20Shot%202018-03-09%20at%206.25.27%20PM.png)

You have the ability to Stop, Start, or Restart individual processes.

This is a good place to check in case you see issues.

* Cloud: Issues around intents?
* Engine: 
* Anim: Issues around animations? Maybe sounds?
* Robot:

Webserver is the only process that will stay as Unknown.

Note: If you turn on Auto Updates, but you Stop/Restart a process that you're currently viewing (8888 for Engine,8889 for Anim), you won't see an update since you're basically restarting that process.

### Engine
This tab displays more detailed info around the Engine processes.

![](images/Screen%20Shot%202018-03-14%20at%204.26.15%20PM.png)

Currently only displays battery info, but more to come.

## Victor Web Viz

![](images/Screen%20Shot%202018-03-09%20at%206.26.35%20PM.png)

This page has tabs that displays data for certain modules running for whichever process you're viewing.

The default Overview page merely introduces what the tool is about.

Tip: Other tabs will have a Pin icon located at the top right corner, which can be used to Pin that tab's view onto the page should you want to have multiple tabs open at a time.

At the bottom right corner, you will see whether you are connected to the robot or not.

* Not Connected to Robot - First seen when you load before it was ever connected to the robot, or when you reload the page. If your robot is on, it may take a few seconds to connect after loading/reloading the page.
* Connected - Seen once it has connected to the robot, sometimes first seen after a page refresh if it was already in Connected state.
* Disconnected - Seen if it was connected to the robot, but either you had power cycled him or carried him out of range and back. Page will have to be refreshed to reestablish connection.
NOTE: Refreshing the page will clear all info that has been recorded in any tabs.

## 8888 (Engine Process)
### BehaviorConds
This tab displays conditions that are blocking transitions to the next behavior.

### Behaviors
This tab displays a hierarchy of behaviors that the robot is doing.

![](images/Screen%20Shot%202018-03-14%20at%202.51.58%20PM.png)

The texts that are highlighted in red are the lines of behaviors that Victor is currently running.

The bars to the right indicates a timer as to how long the behavior is being performed. This gets organized into a graph that visually displays when each behavior occurred:

![](images/Screen%20Shot%202018-03-14%20at%202.56.46 PM.png)

NOTE: Top of the graph displays the elapsed time since the server connected to the robot. Refeshing the page as mentioned will clear the data, but will not reset the elapsed timer unless the robot was restarted/power cycled (notice the empty area starting from 0 seconds in the above screenshot, because the page was refreshed around after 500 seconds). If the elapsed time gets too long (say 2000+ seconds), it can slow down the browser, so it may be recommended to restart/power cycle your Victor and then refresh the page.

The dropdown button at the bottom allows you to force a behavior. Many do not work as some come directly from Cozmo, which are either invalid or will be reworked for Victor.

### CloudIntents

This tab can be used to see what utterance was picked by by the robot (and the cloud), and what the cloud processing thought your utterance was, what intent it tried to match it to, as well as the confidence in matching your utterance to the intent. Additionally, you can access and download the audio link of what your utterance was that was processed.

![](images/Screen%20Shot%202018-03-28%20at%205.37.38%20PM.png)

### Intents
This tab can be used to send intents after calling Victor's trigger word.

![](images/Screen%20Shot%202018-03-14%20at%203.27.42%20PM.png)

Any intents sent and recognized by the cloud while the tab was open can be resent.

### ObservedObjects
This tab displays all the Faces and Cubes that Victor has seen since the tab was opened.

![](images/Screen%20Shot%202018-03-14%20at%202.52.56%20PM.png)


### VisionScheduleMediator

## 8889 (Animation Process)
### Animations
This tab displays all animations that Victor plays through since the time the tab is viewed.

![](images/Screen%20Shot%202018-03-14%20at%202.47.37%20PM.png)

This is a good place to note all the sequence of animations that are playing in case you run into some strange behavior.

### MicData
This tab displays the audio directional confidence levels on the robot.

![](images/Screen%20Shot%202018-03-14%20at%202.46.38%20PM.png)


## Useful Functions
### Set robot's volume:
Curl: curl "http://robotip:8889/consolefunccall?func=SetRobotMasterVolume&args=<volume range from 0.0 to 1.0>"

URL: http://robotip:8889/consolefunccall?func=SetRobotMasterVolume&args=<volume range from 0.0 to 1.0>

### Faces
See what faces are enrolled. Run the below first. Won't return any input but will return info for the next functions

Curl: curl "http://robotip:8888/sendAppMessages?type=RequestEnrolledNames"

URL: http://robotip:8888/sendAppMessages?type=RequestEnrolledNames

The below functions will display only enrolled faces at the time it was connected to the robot. It will appear empty after power cycling (showing just an empty box []). The running above commands beforehand will populate it with all-time data.

Curl: curl "http://robotip:8888/getAppMessages"

URL: http://robotip:8888/getAppMessages

Erase all enrolled faces

Curl: curl "http://robotip:8888/sendAppMessage?type=EraseAllEnrolledFaces" 

URL: http://robotip:8888/sendAppMessage?type=EraseAllEnrolledFaces