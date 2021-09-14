#!/usr/bin/env bash
#
# Copyright 2015-2018 Anki Inc.
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

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../JS_emitter.py}"
CLAD_CPP="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER_CPP:=../../CPP_emitter.py}"
CLADSRC=../src/js-simple
OUTPUT_DIR=${OUTPUT_DIR:-./build/simple}

for file in $CLADSRC/*.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    $CLAD -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file);
    $CLAD_CPP -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file);
done

clang++ -Wall -std=c++11 clad_test.cpp build/simple/Javascript.cpp ../../../support/cpp/source/SafeMessageBuffer.cpp -I../../../support/cpp/include -o build/simple/cpp_test;

cp cladConfig.js ./build/simple/cladConfig.js

node clad_test.js
./build/simple/cpp_test

rm buffer.tmp