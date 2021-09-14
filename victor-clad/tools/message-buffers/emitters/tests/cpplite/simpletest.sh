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

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../CPPLite_emitter.py}"
CLADSRC=../src
OUTPUT_DIR=${OUTPUT_DIR:-./build/simple}

for file in $CLADSRC/aligned-lite/*.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    $CLAD --max-message-size 1420 -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file);
done

cp -f cpplitetest.cpp $OUTPUT_DIR

clang++ -DCLAD_DEBUG -Wall -Wextra --std=c++11 -g -Os -o $OUTPUT_DIR/cpplitetest.out $OUTPUT_DIR/cpplitetest.cpp $OUTPUT_DIR/aligned-lite/CTest.cpp
$OUTPUT_DIR/cpplitetest.out
