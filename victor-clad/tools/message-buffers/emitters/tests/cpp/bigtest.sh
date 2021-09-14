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

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../CPP_emitter.py} ${EMITTERFLAGS}"
CLADSRC=../src
OUTPUT_DIR=${OUTPUT_DIR:-./build/big}
SUPPORTDIR=../../../support/cpp
TEST_SUPPORTDIR=../support

echo "Generating output files"
for file in $CLADSRC/*.clad $CLADSRC/aligned/*.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    COMMAND="$CLAD -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file)"
    echo $COMMAND
    time $COMMAND
done
echo "*********"
echo ""

COMMON_CFLAGS="-Wall -Wextra -Os -g -I$SUPPORTDIR/include -I$TEST_SUPPORTDIR"

cp -f cpptest.cpp $OUTPUT_DIR

echo "Compiling"
echo clang++ -std=c++11 -stdlib=libc++ $COMMON_CFLAGS -c -o $OUTPUT_DIR/SimpleTest.o $OUTPUT_DIR/SimpleTest.cpp
time clang++ -std=c++11 -stdlib=libc++ $COMMON_CFLAGS -c -o $OUTPUT_DIR/SimpleTest.o $OUTPUT_DIR/SimpleTest.cpp

SRCFILES="$OUTPUT_DIR/cpptest.cpp \
    $OUTPUT_DIR/SimpleTest.cpp \
    $OUTPUT_DIR/aligned/AnkiEnum.cpp \
    $OUTPUT_DIR/aligned/AutoUnionTest.cpp \
    $OUTPUT_DIR/Foo.cpp \
    $OUTPUT_DIR/Bar.cpp \
    $OUTPUT_DIR/ExplicitUnion.cpp \
    $OUTPUT_DIR/ExplicitAutoUnion.cpp \
    $OUTPUT_DIR/UnionOfUnion.cpp \
    $OUTPUT_DIR/DupesAllowedUnion.cpp \
    $OUTPUT_DIR/DupesAutoUnion.cpp \
    $OUTPUT_DIR/TestEnum.cpp \
    $SUPPORTDIR/source/SafeMessageBuffer.cpp \
    $OUTPUT_DIR/JsonSerialization.cpp \
    $SUPPORTDIR/source/jsoncpp.cpp"

echo "clang++ -std=gnu++11 $COMMON_CFLAGS -o $OUTPUT_DIR/cpptest_gnu.out $SRCFILES"
time clang++ -std=gnu++11 $COMMON_CFLAGS -o $OUTPUT_DIR/cpptest_gnu.out $SRCFILES
echo "clang++ -stdlib=libc++ $COMMON_CFLAGS -o $OUTPUT_DIR/cpptest_clang.out $SRCFILES"
time clang++ -std=c++11 -stdlib=libc++ $COMMON_CFLAGS -o $OUTPUT_DIR/cpptest_clang.out $SRCFILES
echo "*********"
echo ""

echo "Testing"
echo $OUTPUT_DIR/cpptest_gnu.out
time $OUTPUT_DIR/cpptest_gnu.out
echo $OUTPUT_DIR/cpptest_clang.out
time $OUTPUT_DIR/cpptest_clang.out
echo "*********"
echo ""

if `which valgrind > /dev/null`; then
   echo "Memcheck"
   valgrind --suppressions=./fprintf.supp --leak-check=full $OUTPUT_DIR/cpptest_clang.out
   valgrind --suppressions=./fprintf.supp --leak-check=full $OUTPUT_DIR/cpptest_gnu.out
fi
