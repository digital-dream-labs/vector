#!/bin/sh

LD_LIBRARY_PATH="/anki/lib"
export LD_LIBRARY_PATH

export ANDROID_DATA=/data ANDROID_ROOT=/anki

echo "Running crash tests..."

./vic-testcrash SIGABRT 0.25 2
./vic-testcrash SIGFPE 0.25 2
./vic-testcrash SIGILL 0.25 2
./vic-testcrash SIGSEGV 0.25 2

./vic-testcrash null 0.5 10

./vic-testcrash abort 1 10

./vic-testcrash stackoverflow 0.25 2 

# Normal exit:  Times out before crashing
./vic-testcrash null 999 0.5

echo "Done running crash tests."
