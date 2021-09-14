# PerfMetric Tool

PerfMetric is a tool that is compiled into Victor's engine and anim processes, and records information about each tick in a circular buffer. After a short recording session, one can examine the buffer of recorded tick information. PerfMetric's CPU overhead is minimal. PerfMetric is compiled out of shipping builds.

Some uses:
* See how spikey or smooth the engine or anim tick rate is
* See that certain behaviors or activities use more or less CPU
* Measure before-and-after average tick rates, for optimization attempts
* Compare Debug vs. Release performance
* For automated performance testing

What is recorded for each vic-engine tick:
* Engine duration: The time the engine tick took to execute, in ms
* Engine frequency: The total time of the engine tick, including sleep, in ms
* Sleep intended: The duration the engine intended to sleep, in ms
* Sleep actual: The duration the engine actually slept, in ms; this is often more than what was intended
* Oversleep: How much longer the engine slept than intended, in ms
* RtE Count: How many robot-to-engine messages were received
* EtR Count: How many engine-to-robot messages were sent
* GtE Count: How many game-to-engine messages were received
* EtG Count: How many engine-to-game messages were sent
* GWtE Count: How many gateway-to-engine messages were received
* EtGW Count: How many engine-to-gateway messages were sent
* Viz Count: How many vizualization messages were sent
* Battery Voltage: Current battery voltage (at one point we thought this might be helpful in debugging)
* CPU Freq: CPU frequency (Note: not updated every tick, so a change typically appears several ticks later)
* Active Feature: The current 'active feature'
* Behavior: The current top-of-stack behavior

What is recorded for each vic-anim tick:
* Anim duration: The time the anim tick took to execute, in ms
* Anim frequency: The total time of the anim tick, including sleep, in ms
* Sleep intended: The duration the anim process intended to sleep, in ms
* Sleep actual: The duration the anim process actually slept, in ms; this is often more than what was intended
* Oversleep: How much longer the anim process slept than intended, in ms
* (more to come later)

When results are dumped, a summary section shows extra information, including the mininum, maximum, average, and standard deviation for each of the appropriate stats. This allows you to see, for example, the average tick rate, or the longest tick.

### Sample output (for vic-engine)

```
         Engine   Engine    Sleep    Sleep     Over      RtE   EtR   GtE   EtG   Viz  Battery
       Duration     Freq Intended   Actual    Sleep    Count Count Count Count Count  Voltage  Active Feature/Behavior
     0   25.839   59.744   33.591   33.904    0.313       24     0     0     7     0    3.970  Observing  LookInPlaceHeadUp
     1   10.074   59.930   49.611   49.855    0.244       13     0     0     5     0    3.970  Observing  LookInPlaceHeadUp
     2   13.431   59.941   46.324   46.509    0.185       25     0     0     4     0    3.970  Observing  LookInPlaceHeadUp
     3   39.082   60.016   20.732   20.933    0.201       26     1     0     8     0    3.970  Observing  LookInPlaceHeadUp
     4   12.754   59.972   47.044   47.217    0.173       24     1     0     6     0    3.970  Observing  LookInPlaceHeadUp
     5   14.375   60.007   45.451   45.632    0.181       22     0     0     5     0    3.970  Observing  LookInPlaceHeadUp
     6   17.166   59.973   42.652   42.806    0.154       28     1     0     4     0    3.970  Observing  LookInPlaceHeadUp
     7   16.485   60.175   43.360   43.689    0.329       14     0     0     4     0    3.970  Observing  LookInPlaceHeadUp
     8   30.402   59.971   29.268   29.569    0.301       23     0     0    10     0    3.970  Observing  LookInPlaceHeadUp
     9   16.012   60.048   43.686   44.036    0.350       26     0     0     5     0    3.970  Observing  LookInPlaceHeadUp
    10   18.250   59.882   41.399   41.631    0.232       24     0     0     7     0    3.970  Observing  LookInPlaceHeadUp
    11   34.827   64.321   24.939   29.493    4.554       23     0     0     5     0    3.970  Observing  LookInPlaceHeadUp
    12   14.444   55.636   41.001   41.191    0.190       24     0     0     7     0    3.970  Observing  LookInPlaceHeadUp
    13   18.323   60.091   41.486   41.768    0.282       14     0     0     5     0    3.970  Observing  LookInPlaceHeadUp
    14   24.480   59.900   35.238   35.420    0.182       27     0     0     9     0    3.970  Observing  LookInPlaceHeadUp
    15   27.245   59.978   32.572   32.733    0.161       24     1     0     4     0    3.970  Observing  LookInPlaceHeadUp
    16   11.492   60.740   48.347   49.247    0.900       24     0     0     4     0    3.969  Observing  LookInPlaceHeadUp
    17   17.585   59.299   41.514   41.713    0.199       25     0     0     6     0    3.969  Observing  LookInPlaceHeadUp
    18   11.974   59.962   47.825   47.987    0.162       13     0     0     7     0    3.969  Observing  LookInPlaceHeadUp
    19   29.614   64.440   30.223   34.825    4.602       24     0     0     4     0    3.969  Observing  LookInPlaceHeadUp
 Summary:  (RELEASE build; VICOS; 20 engine ticks; 1.204 seconds total)
         Engine   Engine    Sleep    Sleep     Over      RtE   EtR   GtE   EtG   Viz  Battery
       Duration     Freq Intended   Actual    Sleep    Count Count Count Count Count  Voltage
  Min:   10.074   55.636   20.732   20.933    0.154     13.0   0.0   0.0   4.0   0.0    3.969
  Max:   39.082   64.440   49.611   49.855    4.602     28.0   1.0   0.0  10.0   0.0    3.970
 Mean:   20.193   60.201   39.313   40.008    0.695     22.4   0.2   0.0   5.8   0.0    3.970
  Std:    8.161    1.700    8.063    7.553    1.304      4.6   0.4   0.0   1.7   0.0    0.000
```

### Prerequisites
PerfMetric is compiled into the engine and anim processes by default for Debug and Release builds, but not for Shipping builds. To override the default, use

```-DANKI_PERF_METRIC_ENABLED=1```

(or 0 to disable)

### Use from command line
The interface to PerfMetric is through Victor's embedded web server. From the command line, this will start a recording session for vic-engine, assuming ANKI_ROBOT_HOST is set to your robot IP (use port 8889 for vic-anim):

```curl ${ANKI_ROBOT_HOST}':8888/perfmetric?start'```

You can also use your robot IP directly, this way:

```curl '192.168.42.82:8888/perfmetric?start'```

Now some time later, one can dump the results with:

```curl ${ANKI_ROBOT_HOST}':8888/perfmetric?stop&dumplogall'```

Note that multiple commands can be entered at the same time, separated by ampersands. This includes some 'wait' commands that allow you to do a recording session with one single command:

```curl ${ANKI_ROBOT_HOST}':8888/perfmetric?start&waitseconds30&stop&dumplogall'```

Here is the complete list of commands and what they do:
* "status" returns status (recording or stopped, and number of frames in buffer)
* "start" starts recording; if a recording was in progress, the buffer is reset before re-starting
* "stop" stops recording
* "dumplog" dumps the summary of results to the log
* "dumplogall" dumps the entire recorded tick buffer, along with the summary, to the log
* "dumpresponse" returns summary as HTTP response
* "dumpresponseall" returns all info as HTTP response
* "dumpfiles" writes all info to two files on the robot: One is a formatted txt file, and the other a csv file. These go in /data/data/com.anki.victor/cache/perfMetricLogs. The filename has the time of the file write baked in, as well as "R" or "D" to indicate Release or Debug build, and "Eng" or "Anim" to indicate vic-engine or vic-anim. An example: "perfMetric_2018-11-29_17-41-05_R_Eng.txt"

### Use from webserver page in a browser
The engine (port 8888) and anim (port 8889) webserver pages have a "PERF METRIC" page button. This brings you to a page with all of the PerfMetric controls, including the ability to dump the formatted output to the page itself.

### Helper script
A script has been created for convenience and as an example:

```tools/perfMetric/autoPerfMetric.sh```

### Use with webots (pure simulator)
When using with webots pure simulator, use 'localhost' as your IP. Also note that webots pure simulator does NOT sleep between engine ticks, so the output will reflect that.

### Future features
* Create a bucket-based histogram feature that can help us quickly find code issue culprits
* Record visual schedule mediator info
* More vic-anim stats
