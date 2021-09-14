# Android Profiling
Created by Daniel Casner Last updated Sep 25, 2017

Android has a low level profiling API based on the Linux kernel's ftrace mechanism.

Uses atrace utility on device and systrace utility on host.

https://developer.android.com/studio/profile/systrace-commandline.html

https://developer.android.com/studio/profile/systrace.html



In C source
```
#define ATRACE_TAG ATRACE_TAG_ALWAYS
...
#include <cutils/trace.h>
...
fct() {
    ...
    ATRACE_BEGIN()
    ...
    ATRACE_END()
    ...
} 
```

In C++ source

```
#define ATRACE_TAG ATRACE_TAG_ALWAYS
...
#include <utils/Trace.h>
...
fct() {
    ATRACE_CALL()
    ...
} 
```

In Java Source

```
Trace.traceBegin()
Trace.traceEnd()
```

Enabling on the command line:

```
# setprop debug.atrace.tags.enableflags
```

Make sure the trace marker file is writeable (/sys/kernel/debug/tracing/trace_marker):

Either mount debugfs at startup Or:

```
# chmod 222 /sys/kernel/debug/tracing/trace_marker
```