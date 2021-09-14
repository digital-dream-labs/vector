# Write a Bug Report

Something went wrong!  You want to get it fixed.  You want the team to operate more efficiently.  You want to reduce duplicate work.  You want to win! :)

## Step-by-step guide

1.	Go To Jira : 
2.	Click the big '+' on the left hand side and select Victor Development for the Project and Bug for the Issue Type
3.	Summary - A simple plain English summary of what went wrong.  Minimize technical terms and details.  State what went wrong, not what you think the root cause is.
4.	Assignee - Leave it set to Automatic unless you were asked to assign it to someone in particular
5.	Description - Your description should have enough detail to help someone else reproduce it.  Here is a template.  Good Examples are below: 
6. Attachment - Provide logs, tombstones, videos, screenshots, etc. that you have.  If you attach a file like screenshot.png, you can show it inline like the example above.  This makes it easier for people to quickly see what the bug is.  Remember to restrict the width to something reasonable like 480 or 640. See   VIC-6317 - [Chewie/Login/Motorola G5/SamSung S7/Google Pixel] Password text is overlapped to the Email field and Login button is cut off. Closed  as an example.
7. Leave all other fields blank for the producers to fill in and modify


*+Environment+*
* Victor Hardware :
* Victor ESN : 
* Victor OS:
* Location :
* WiFi :
* Chewie Version : 
* Chewie Hardware:
* Reproducibility Rate :
    
*+Steps to Reproduce+*
1. 
2.

*+Expected Results+*
*+Actual Results+*
!screenshot.png|width=480!
*+Notes/Workarounds+*

## Examples of good bug reports

* Summary: Chewie (iOS) can't find my Vector
* Description:
* Environment
* Victor Hardware : PVT
* Victor ESN : 00e20187
* Victor OS: 0.9.0 
* Location : Anki SF office
* WiFi : AnkiRobits
* Chewie Version : 282
* Chewie Hardware: iPhone 8+ running iOS 11.4
* Reproducibility Rate : 5 out of 5 tries

Steps to Reproduce
    1.  Place the robot on the charger
    2.  Hold the back pack button to turn him off
    3.  Keep holding the backpack button to reboot him into recovery
    4.  Launch Chewie on the iPhone and watch it search for Vector
    5.  Wait 30 seconds for it to search

Expected Results
* My Vector shows up in the list

Actual Results
* The list on Chewie is empty

Notes/Workarounds
* I used the LightBlue app on iOS to do a Bluetooth Low Energy scan and it found my Victor robot.


* Summary: Face shows 915 shortly after saying "Hey Vector"
* Description:
* Environment
* Victor Hardware : PVT
* Victor ESN : 00e20187
* Victor OS: 0.12.1402
* Location : Anki SF office
* WiFi : AnkiRobits
* Chewie Version : n/a
* Chewie Hardware: n/a
* Reproducibility Rate : 2 out of 3 tries

Steps to Reproduce
    1.  Place the robot on the charger
    2.  Say "Hey Vector"

Expected Results
* The 3 backpack lights light up and the earcon plays audibly

Actual Results
* Within 2 seconds the face shows "915"

Notes/Workarounds
* It worked the 1st time and then failed with "915" the next 2 times.  See my attached logcat.txt file.
* I tested this yesterday several times with build 0.12.1393 on the same robot in the same environment and it never happened.

## Examples of bad bug reports

* Summary: On latest master, Victor doesn't work
* Description: I said "Hey Vector" and he didn't hear me

The summary is terrible because "latest master" is a moving target and "doesn't work" isn't specific.  Take the time to look up the version you have. The description doesn't provide the details listed above.

* Summary: Victor's SSL certs are out of date causing the cloud to fail

The summary is bad because it assumes the root cause for "the cloud" failing.  Also, what does "the cloud to fail" mean?  A problem uploading DAS logs?  A problem with knowledge graph?  A problem with the weather?




