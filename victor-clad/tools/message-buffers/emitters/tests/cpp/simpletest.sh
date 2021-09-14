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

set -eu

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../CPP_emitter.py}"
CLADSRC=../src
OUTPUT_DIR=${OUTPUT_DIR:-./build/simple}
SUPPORTDIR=../../../support/cpp
TEST_SUPPORTDIR=../support

for file in $CLADSRC/Foo.clad $CLADSRC/Bar.clad $CLADSRC/SimpleTest.clad \
                              $CLADSRC/ExplicitUnion.clad \
                              $CLADSRC/ExplicitAutoUnion.clad \
                              $CLADSRC/UnionOfUnion.clad \
                              $CLADSRC/DupesAllowedUnion.clad \
                              $CLADSRC/DupesAutoUnion.clad \
                              $CLADSRC/aligned/AutoUnionTest.clad \
                              $CLADSRC/aligned/AnkiEnum.clad \
                              $CLADSRC/DefaultValues.clad \
                              $CLADSRC/TestEnum.clad\
                              $CLADSRC/JsonSerialization.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    $CLAD --output-json --output-union-helper-constructors -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file);
done

cp -f cpptest.cpp $OUTPUT_DIR

clang++ -Wall -Wextra -std=c++11 -stdlib=libc++ \
    -I$TEST_SUPPORTDIR -I$SUPPORTDIR/include -g -os -o $OUTPUT_DIR/cpptest.out \
    -DHELPER_CONSTRUCTORS \
    $OUTPUT_DIR/cpptest.cpp \
    $OUTPUT_DIR/SimpleTest.cpp \
    $OUTPUT_DIR/aligned/AutoUnionTest.cpp \
    $OUTPUT_DIR/Foo.cpp \
    $OUTPUT_DIR/Bar.cpp \
    $OUTPUT_DIR/aligned/AnkiEnum.cpp \
    $OUTPUT_DIR/ExplicitUnion.cpp \
    $OUTPUT_DIR/UnionOfUnion.cpp \
    $OUTPUT_DIR/DupesAllowedUnion.cpp \
    $OUTPUT_DIR/DupesAutoUnion.cpp \
    $SUPPORTDIR/source/SafeMessageBuffer.cpp \
    $OUTPUT_DIR/DefaultValues.cpp \
    $OUTPUT_DIR/TestEnum.cpp \
    $SUPPORTDIR/source/jsoncpp.cpp \
    $OUTPUT_DIR/JsonSerialization.cpp
$OUTPUT_DIR/cpptest.out
