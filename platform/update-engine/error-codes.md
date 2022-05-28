# OTA / Recovery Error Codes
Range from 200-219 inclusive.

Codes in this document must not change after release, they are add only.

| Code | Reason                                |
|:----:|---------------------------------------|
|   1  | Switchboard: unknown status           |
|   2  | Switchboard: OTA in progress          |
|   3  | Switchboard: OTA completed            |
|   4  | Switchboard: rebooting                |
|   5  | Switchboard: Other OTA error          |
|  10  | OS: Unknown system error              |
| 200  | Unexpected .tar contents              |
| 201  | Unhandled manifest version or feature |
| 202  | Boot Control HAL failure              |
| 203  | Could not open URL                    |
| 204  | URL not a TAR file                    |
| 205  | Decompressor error                    |
| 206  | Block error                           |
| 207  | Imgdiff error                         |
| 208  | I/O error                             |
| 209  | Signature validation error            |
| 210  | Decryption error                      |
| 211  | Wrong base version                    |
| 212  | Subprocess exception                  |
| 213  | Wrong serial number                   |
| 214  | Dev / Prod mismatch                   |
| 215  | Socket Timeout (network stall)        |
| 216  | Downgrade not allowed                 |
| 217  |                                       |
| 218  |                                       |
| 219  | Other Exception                       |
