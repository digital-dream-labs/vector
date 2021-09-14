# DAS Geolocation Fields for Victor Robot

## Table (victor_geo) Fields

|Field          | Sample Data | Usage |
|---------------|-------------|-------|
|boot_id        |GUID         |used to keep track when robot reboots.|
|robot_id       |8 digit hex  |ESN (NOT the old "beefcode")|
|connection_type|||
|postalcode     |||
|lookup_source  |||
|country        |||
|city           |||
|region         |||
|latitude       |||
|longitude      |||

## Notes
boot_id generation:
* DAS reads the current boot ID from /proc/sys/kernel/random/boot_id when the service starts up.
* As is, Boot ID is not guaranteed to be unique. Two robots could generate the same random number and a single robot can generate the same random number twice.
* We could combine ESN + boot_id to prevent two robots from generating the same value.
* We could combine timestamp + boot_id to prevent a robot from generating the same value twice, but timestamp is not 100% reliable on our robots.

robot_id is added per Analytics request to avoid joining the raw table to find boot_id to robot_id mapping