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

CLAD="${PYTHON:=python} ${PYTHONFLAGS:=} ${EMITTER:=../../CSharp_emitter.py}"
CLADSRC=../src
OUTPUT_DIR=${OUTPUT_DIR:-./build/simple}

for file in $CLADSRC/*.clad $CLADSRC/aligned/*.clad; do
    OUTPUT_DIR_PARAM=$(dirname $OUTPUT_DIR/${file#$CLADSRC/};)
    mkdir -p ${OUTPUT_DIR_PARAM}
    $CLAD -o ${OUTPUT_DIR_PARAM} -C $(dirname $file) $(basename $file);
done

cp -f csharptest.cs $OUTPUT_DIR

mcs -warnaserror -warn:4 -debug+ -out:$OUTPUT_DIR/csharptest.exe \
    $OUTPUT_DIR/csharptest.cs \
    $OUTPUT_DIR/SimpleTest.cs \
    $OUTPUT_DIR/aligned/AnkiEnum.cs \
    $OUTPUT_DIR/Bar.cs \
    $OUTPUT_DIR/Foo.cs \
    $OUTPUT_DIR/UnionOfUnion.cs \
    $OUTPUT_DIR/DefaultValues.cs \
    $OUTPUT_DIR/TestEnum.cs

time mono --debug $OUTPUT_DIR/csharptest.exe
