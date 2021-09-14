# How to Reset Victor to Recovery (unbrick) mode

Created by Gary Darer Last updated Apr 16, 2018

1. set on charger
2. press and hold button for 20sec
3. keep holding until lights turn off & back on again

To erase all faces, open your terminal and paste the following:
`curl "robotip:8888/sendAppMessage?type=EraseAllEnrolledFaces"`

to get messages that the robot would be sending to the app, including error codes:
`curl "robotip:8888/getAppMessages"`

to get a list of names the robot knows
```curl "robotip:8888/sendAppMessage?type=RequestEnrolledNames"
curl "robotip:8888/getAppMessages"```