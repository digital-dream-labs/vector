# DAS Protocol

Created by Adam Alpern Last updated Jan 22, 2018

## Overview

This document specifies the data and transport protocols for sending analytics event data from mobile apps into Anki's DAS ingestion pipeline. The specification for the current state of DAS protocols as of this writing (January, 2018) is purely descriptive - it describes what exists today, as that has never been formally specified prior.

## Transport Protocol
### Input Queue
Amazon's SQS (Simple Queue Service) is used for transport. Each environment (development, beta, production) uses a single standard queue for all inputs from all products. FIFO queues are not used. Devices connect directly to the SQS queue endpoint over standard HTTPS, and upload data using the SendMessage API. 

### Authentication
The transport protocol is currently un-authenticated.

### Versioning
The transport protocol is current not explicitly versioned. Messages sent in the current format, with no additional message attributes indicating version, are implicitly considered version 1. 

### Message Format
An SQS message consists of a message body and zero or more optional message attributes. Version 1 of the transport protocol does not make use of any message attributes, only the body.

#### Message Body
A single message may contain 1 or more discrete events formatted as a JSON array which is then Base64 encoded. The JSON array must be formatted according to the Data Protocol (Raw Syntax), and must always be an array, even when only one event is contained in the message.  

When more than one event is contained in the body of a message, every event MUST be in the same format (raw or normalized), using the same version of the data protocol (value for the messv field), and must have the same value for the product field. 

The maximum size of the Base64 encoded message body is 256KB, therefore the formatted JSON string should be no larger than 190KB. The number of events that can be transmitted in a single message is limited only by how many formatted events will fit within that limit. 

##### Message Body, pre-encoding

```
[{<event-data>},{<event-data>}]
```

##### Message Body, encoded
```
W3s8ZXZlbnQtZGF0YT59LHs8ZXZlbnQtZGF0YT59XQ==
```

## Data Protocol
DAS events are groups of key/value pairs represented as simple JSON objects. 

### Versioning
The version of the event data schema is indicated by the messv (for message version) field (or $messv in the raw syntax), which is an integer version. The current version used by Overdrive, Foxtrot, and Cozmo is 2. The format for Drive, which is not described here, is indicated by messv=1. 

### Event Fields

| Field Name | Type | Required? | Notes |
|------------|------|-----------|-------|
|messv	     | int  | Yes       |Data schema version. Current version for Overdrive, Foxtrot, and Cozmo is 2.|
|product	 |string| Yes       |Product identifier. One of od, ft, or cozmo|
|ts          | int	| Yes       |Event timestamp. The current time on the device, in UTC, when the even was generated, in UNIX timestamp format with millisecond resolution. |
|level       |string| Yes       |Simple logging level-like designation. One of debug, info, event, warn, error.|
|event       |string| Yes       |The name of the event.|
|apprun      |GUID (string)|Yes |Unique identifier for the mobile app process run. An apprun is a fairly arbitrary length of time denoting the lifetime of a process on the mobile device.|
|seq         |int   |Yes        |Sequence number for events, monotonically increasing with each successive event. The combination of apprun + seq constitutes a unique key for each message. |
|s_val       |string|           |Free-form text field for event-specific data. |
|data        |string|           |Free-form text field for event-specific data.  |
|phys        |string|           |A hexadecimal string code identifying the Overdrive vehicle or Cozmo unit involved in the event. |
|unit        |string|           |UUID identifying the mobile device that sent the event.|
|action_unit |string|           |UUID identifying the mobile device that originated the event in the case of multi-player games.|
|app         |string|           |Build string of the mobile app generating the event|
|platform    |string|           |Operating system platform of the mobile device originating the event. One of ios, android, or kindle|
|session_id  |string|           ||
|player_id   |string|           ||
|profile_id  |string|           ||
|group       |string|           ||
|duser       |string|           ||
|phone       |string|           |A string identifying the hardware model of the mobile device that generated the event. e.g. "iPhone OS-iPhone8,4-9.3.2-13F69".|
|game        |string|           |UUID of the game currently in progress at the time the event was generated, if there is one.|
|lobby       |string|           ||
|tag         |string|           ||
|user        |string|           ||

### Raw Syntax
The raw syntax for DAS events is used by the client to upload via the transport protocol. Each event consists a single flat JSON object, with all fields except the event name and and value prefixed with the '$' character. The event name is the key of the only field which does not begin with a '$' character, and the string value (s_val) of the event is the value of that field, which may be empty. 

The fields $seq, $messv, and $ts are JSON strings which must contain a base 10 formatted integer. 

#### Example
```
{
  "$lobby": "0078CCDC-D4D1-BAA9-00E8-CE596E4A61E2",
  "$phone": "iPhone OS-iPod5,1-9.3-13E233",
  "$platform": "ios",
  "$product": "od",
  "$app": "1.3.1.120.160328.1034.p.4eba95b",
  "$unit": "D0F19D20-364C-4C6A-9C66-7970A9A336B4",
  "BLEVehicleManager.disconnectVehicle": "0xbeef000808c0789a connectionState: 4",
  "$level": "event",
  "$ts": "1459468802114",
  "$seq": "538",
  "$apprun": "DF1D3C93-3187-4824-B8BB-5E7A977CF677",
  "$messv": "2"
}
```

### Normalized Syntax

The normalized syntax is a slight modification of the raw syntax that normalizes the JSON into a regular form that can be loaded into a database. In this format, the event and event value are given explicit keys, and the $ prefix is removed from all fields. Additionally, numbers are represented by JSON numbers, not strings. 

#### Example
```
{
  "lobby": "0078CCDC-D4D1-BAA9-00E8-CE596E4A61E2",
  "phone": "iPhone OS-iPod5,1-9.3-13E233",
  "platform": "ios",
  "product": "od",
  "app": "1.3.1.120.160328.1034.p.4eba95b",
  "unit": "D0F19D20-364C-4C6A-9C66-7970A9A336B4",
  "s_val": "0xbeef000808c0789a connectionState: 4",
  "event": "BLEVehicleManager.disconnectVehicle",
  "level": "event",
  "ts": 1459468802114,
  "seq": 538,
  "apprun": "DF1D3C93-3187-4824-B8BB-5E7A977CF677",
  "messv": 2
}
```
