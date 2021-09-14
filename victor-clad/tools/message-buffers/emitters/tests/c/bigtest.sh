#!/usr/bin/env bash
#
# Copyright 2015-2016 Anki Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e
set -u

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../C_emitter.py}"
CLADSRC=../src
OUTPUT_DIR=${OUTPUT_DIR:-./build/big}

echo "Generating output files"
for file in $CLADSRC/aligned/*.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    COMMAND="$CLAD -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file)"
    echo $COMMAND
    time $COMMAND
done
echo "*********"
echo ""

COMMON_CFLAGS="-Wall -Wextra -Os -g"

cp -f ctest.c $OUTPUT_DIR

SRCFILES="$OUTPUT_DIR/ctest.c"
STDS="gnu89 c99 gnu99 c11 gnu11"
#c89 is really rough and c94 doesn't seem to be valid
#STDS="c89 gnu89 c94 c99 gnu99 c11 gnu11"

echo "Compiling under many standards to ensure compatibility"
for std in $STDS; do
    COMMAND="clang -std=$std $COMMON_CFLAGS -o $OUTPUT_DIR/ctest_$std.out $SRCFILES"
    echo $COMMAND
    time $COMMAND
done
echo "*********"
echo ""

echo "Testing"
for std in $STDS; do
    COMMAND="$OUTPUT_DIR/ctest_$std.out"
    echo $COMMAND
    time $COMMAND
done
echo "*********"
echo ""

if `which valgrind > /dev/null`; then
    echo "Memcheck"
    for std in $STDS; do
        COMMAND="valgrind $OUTPUT_DIR/ctest_$std.out"
        echo $COMMAND
        eval $COMMAND
    done
    echo "*********"
    echo ""
fi
