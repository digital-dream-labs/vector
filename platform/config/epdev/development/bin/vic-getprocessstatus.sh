#!/bin/sh
# Args:  port, followed by a variable number of process names

port=$1
shift

while [ "$1" != "" ]; do
    processName=$1
    shift
    result=`/bin/systemctl is-active $processName`
    allResults=$allResults$processName=$result\&
done
# Remove the last character (the last ampersand)
allResults=${allResults%?};
# Now send the results to the webserver:
/usr/bin/curl localhost:$port/processstatus?$allResults
