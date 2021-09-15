#!/bin/bash

# Tool for clearing shared memory segments that sometimes get orphaned occur when using Webots simulator.
# If you are see red warning when you start the simulator mentioning some problem about shared memory
# this will probably fix it.
#
# NOTE: THIS REMOVES ALL SHARED MEMORY SEGMENTS, SEMAPHORES AND QUEUES!!!
# Make sure you know that you aren't using shared memory for any other active programs.


ME=`/usr/bin/whoami`

IPCS_S=`/usr/bin/ipcs -s | /usr/bin/grep $ME | /usr/bin/tr -s " " | cut -f2 -d" "`
IPCS_M=`/usr/bin/ipcs -m | /usr/bin/grep $ME | /usr/bin/tr -s " " | cut -f2 -d" "`
IPCS_Q=`/usr/bin/ipcs -q | /usr/bin/grep $ME | /usr/bin/tr -s " " | cut -f2 -d" "`

#echo memory
for id in $IPCS_M; do
  echo killing shared memory $id
  ipcrm -m $id;
done

#echo semaphore
for id in $IPCS_S; do
  echo killing semaphore $id
  ipcrm -s $id;
done

#echo queue
for id in $IPCS_Q; do
  echo killing queue $id
  ipcrm -q $id;
done

/usr/bin/pkill webots

# Kill parent processes if there are any
CPID=`/usr/bin/ipcs -p | /usr/bin/egrep "[0-9a-f]+ [0-9]+" | /usr/bin/grep $ME | /usr/bin/awk -F" " '{print $7}'`

for id in $LPID; do
  echo killing cpid $id
  kill -9 $id;
done

LPID=`/usr/bin/ipcs -p | /usr/bin/egrep "[0-9a-f]+ [0-9]+" | /usr/bin/grep $ME | /usr/bin/awk -F" " '{print $8}'`

for id in $LPID; do
  echo killing lpid $id
  kill -9 $id;
done


IPCS_ANY=`/usr/bin/ipcs | /usr/bin/grep $ME | /usr/bin/tr -s " " | cut -f2 -d" "`
if [ -n "$IPCS_ANY" ]; then
  echo not all ipc facilities were killed
  /usr/bin/ipcs -a
  exit 1
fi

