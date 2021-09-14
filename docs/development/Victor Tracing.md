# Victor Tracing

Created by Mathew Prokos

Victor tracing is done using the lttng (Linux Trace Toolkit Next Gen) toolset. This page gives an overview of how to capture and graph a trace.

## Babeltrace

Babeltrace is a utility for inspecting CTF (common trace format) traces. This package includes python modules for reading and inspecting traces as well as a command line utility. This document details how to use scripts using the python modules to graph measurements taken on a victor robot.

### Setup
Victor tracing works on a MAC using docker to convert the trace to a python pickle file. If you are using Ubuntu you don't need to setup docker. This is necessary because there is currently no homebrew package for babeltrace and installing the python modules manually on a MAC is not trivial.

#### Steps for a MAC OSX based host
1. Install homebrew
    a. brew https://brew.sh/
2. Install supporting python modules
    a. brew install python3
    b. if pip --version tells you that its associated with python3:
        i. pip install matplotlib numpy pandas seaborn
    c. otherwise:
        i. python3 -m  pip install matplotlib numpy pandas seaborn
3. Install Docker
    a. https://docs.docker.com/docker-for-mac/install/

#### Steps for an Ubuntu 18LTS based host

1. Install supporting python modules and babeltrace
    a. apt-get install python3-babeltrace python3-numpy python3-pandas python3-seaborn python3-matplotlib babeltrace

## Capturing a Trace
1. host:ssh onto the robot you would like to take a trace on.
2. robot:run "ankitrace -c" to clear any existing traces
3. robot:run "ankitrace -r" to take a snapshot of the currently running trace
4. host:scp the /data/ankitrace/trace/ankitrace-* directory from the robot to ~/trace/

## Generating Graphs
After capturing a trace use these steps to graph the results.

Note: If using ubuntu the docker steps can be preformed on host.

### Process trace and convert to python pickle 
1. host: Change directories to the victor repository
2. host:run "./docker/run.sh /bin/bash" to get a prompt in the docker container
3. docker: Change directories to the victor repository
4. docker:run "./project/victor/scripts/lttng/convert.py -t ~/trace/ankitrace-* -f ~/trace.pickle"

Note: You can pass one or more trace directories to the convert.py script. The above example with shell expand all of the traces in the directory. You can also select specific ones giving the full path with the -t, --trace option.
    * e.g: ./project/victor/scripts/lttng/convert.py -t ~/trace/ankitrace-20190227-092122-0 -t ankitrace-20190227-092122-1 -f ~/trace.pickle

#### Graph latency using pickle file as input
1. host: Change directories to the victor repository
2. host:run "./project/victor/scripts/lttng/graph_latency.py -f ~/trace.pickle"

#### Graph loop duration using pickle file as input
1. host: Change directories to the victor repository
2. host:run "./project/victor/scripts/lttng/graph_durations.py -f ~/trace.pickle"

## Tracecompass
Tracecompass is an eclipse plugin (no one is perfect) that provides a rich set of tools to view lttng traces. The most helpful of these tools is control flow view.

https://help.eclipse.org/mars/index.jsp?topic=%2Forg.eclipse.tracecompass.doc.user%2Fdoc%2FLTTng-Kernel-Analysis.html

Go to https://www.eclipse.org/tracecompass/ to download tracecompass and get started.