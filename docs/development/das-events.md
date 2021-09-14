# Victor DAS Events

## Viewing
In order to view all current DAS events join the `#vic-nightly-tests` and look for the DAS Documentation message.
The link will take you to the correct teamcity build page

If needed you can manually kick off the DAS Documentation build on teamcity to get these results

If you are looking for local results you can run Doxygen against `./project/doxygen/Doxyfile`

## Creation
In order to send and create documentation for DAS events you will need 3+ macros in order:
```
DASMSG(unique reference variable, das event string, class documentation)
DASMSG_SET(das message variable, sent string, variable documentation)
DASMSG_SEND()
```

### DASMSG()
#### Unique reference variable
  * A simple variable which will be used as a unique reference name for quick searching in the doxygen documentation

#### Das Event String
  * This is the string which will be used to reference this event once sent up to the cloud

#### Class Documentation
  * The information which will be shown about the class in the Doxygen documentation
  * This will support full doxygen markup

### DASMSG_SET()
#### Das Message Variable
  * Either `s1-s4` or `i1-i4`
  * Use `s1-s4` to send max 256 character string messages
  * Use `i1-i4` to send 32 bit integers

#### Sent Variable
  * This is the string which will be sent up in this variable's field

#### String Documentation
  * The information which will be shown under this variable in the Doxygen documentation
  * This will support full doxygen markup

### DASMSG_SEND()
  * Simply call this macro at the end once everything is set

### Example
```cpp
     DASMSG(engine_main_hello, "engine.main.hello", "Example event");
     DASMSG_SET(s1, "str1", "Example string 1");
     DASMSG_SET(s2, "str2", "Example string 2");
     DASMSG_SET(s3, "str3", "Example string 3");
     DASMSG_SET(s4, "str4", "Example string 4");
     DASMSG_SET(i1, 1, "Example int 1");
     DASMSG_SET(i2, 2, "Example int 2");
     DASMSG_SET(i3, 3, "Example int 3");
     DASMSG_SET(i4, 4, "Example int 4");
     DASMSG_SEND();
```
