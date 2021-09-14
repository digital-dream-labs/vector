# DAS Protocol Changes for Victor

Created by Adam Alpern Last updated Mar 13, 2018


## Overview
This spec describes changes to the low-level DAS event protocols to support explicit versioning, data compression, and potentially other minor changes. If approved, this will constitute version 2 of the DAS transport protocol and version 3 of the DAS data protocol. 

## TL;DR
* Indicate version 2 of the transport protocol by adding the message attribute DAS-Transport-Version: 2
* To sent compressed data, add the message attribute Content-Encoding: gzip, base64, and gzip the JSON data before Base64 encoding it. 
* Specify product in the Content-Type message attribute to aid server side parsing

## Transport Protocol
### Explicit Versioning
DAS SQS messages should include a message attribute indicating the version of the transport protocol. To indicate the transport protocol version, add a message attribute of type `Number.int`, with the attribute name `DAS-Transport-Version` and the protocol version as the value. Any message received without this attribute will be assumed to be using version 1. 

### Content Metadata
Information which is necessary to decode the payload before parsing and loading will be encoded in SQS message attributes which mirror HTTP's Content-Type and Content-Encoding headers.

#### Content-Type
The media type for Anki DAS data will be `application/vnd.anki+json` with additional required information about product specific data format in media type directives. The sole required field is product, which is the same as the product field in the body data in the legacy protocol. 

#### Example
Messages from the Victor apps and robot will should use one of these values for Content-Type:

```
Content-Type: application/vnd.anki+json; product=vic
Content-Type: application/vnd.anki+json; product=vicapp
```

The JSON data will be assumed to be in normalized format in these cases. 

#### Content-Encoding
Content-Encoding is used to indicate how the JSON data in the message body has been encoded for transmission, including compression and base64 encoding. 

#### Examples
A message containing Victor data in normalized format (see below) with compression should add the following SQS message attributes:

```
Content-Type: application/vnd.anki+json; format=normal,product=victor
Content-Encoding: gzip, base64
```

### Data Compression
DAS data in JSON format typically compresses at a ratio of 20:1 (using standard gzip compression). By transmitting the data compressed, we can save 95% of the bandwidth currently used for DAS uploads, allowing them to complete faster and more reliably. Compressed event data should be compressed with gzip, then Base64 encoded and stored in the SQS message body (the same as uncompressed JSON is currently bas64 encoded for inclusion in the message body). To indicate that the body is compressed, the client must add the Content-Encoding message attribute, as specified above. 

The values in Content-Encoding must be in the order they were applied to the data. So a value of "gzip, base64" means that the raw JSON data was first compressed with gzip, then base64-encoded. 

## Data Protocol
### Normalized Format
Rather than sending events from the client in the current raw syntax, send them in the normalized syntax.  If the message is sent withDAS-Transport-Version=2 or higher,the data will be assumed to be in normalized format.

#### Raw Format
The raw syntax will be deprecated, and should never be sent by any component of Victor. 

## Event Field Changes
All changes to the product-specific event fields are now being tracked in separate documents for the companion app and the robot.  

DAS Event Fields for Victor Companion App

DAS Event Fields for Victor Robot 

## Sample Flows
Sample das event flows can be found here. 

## Open Questions
The following issues are still to be resolved. 

Cloud events - what's up with that? 

## Appendix - Client Implementations
Documenting where the communication with SQS occurs. 

Victor Xamarin Mobile App - https://github.com/aws/aws-sdk-net/
Golang Client - https://github.com/anki/sai-das-client/
age table
join with robot & app events based on user ID