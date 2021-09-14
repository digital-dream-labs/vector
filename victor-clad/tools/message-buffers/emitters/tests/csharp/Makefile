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

#
# CLAD makefile for C#
#

OUTPUT_DIR=./build

PYTHON?=python
PYTHONFLAGS?=
EMITTER=../../CSharp_emitter.py
CLAD=$(PYTHON) $(PYTHONFLAGS) $(EMITTER)
CS_TEST_SRC=csharptest.cs
CS?=mcs
CSFLAGS?=-debug+

TREE_INPUT_DIR=../src/DirTreeTests
TREE_OUTPUT_DIR=$(OUTPUT_DIR)/tree

INPUT_FILES=$(subst $(TREE_INPUT_DIR)/, ,$(shell find $(TREE_INPUT_DIR) -type f -iname '*.clad'))
OUTPUT_CS=$(patsubst %.clad, $(TREE_OUTPUT_DIR)/%.cs, $(INPUT_FILES))
OUTPUT_FILES=$(OUTPUT_CS)
TEST_BIN=$(TREE_OUTPUT_DIR)/testCSharp.exe

ALIGNED_TREE_INPUT_DIR=../src/AlignedDirTreeTests
ALIGNED_TREE_OUTPUT_DIR=$(OUTPUT_DIR)/aligned-tree

ALIGNED_INPUT_FILES=$(subst $(ALIGNED_TREE_INPUT_DIR)/, ,$(shell find $(ALIGNED_TREE_INPUT_DIR) -type f -iname '*.clad'))
ALIGNED_OUTPUT_CS=$(patsubst %.clad, $(ALIGNED_TREE_OUTPUT_DIR)/%.cs, $(ALIGNED_INPUT_FILES))
ALIGNED_OUTPUT_FILES=$(ALIGNED_OUTPUT_CS)

vpath %.clad $(TREE_INPUT_DIR)
vpath %.cs $(TREE_OUTPUT_DIR)

vpath %.clad $(ALIGNED_TREE_INPUT_DIR)
vpath %.cs $(ALIGNED_TREE_OUTPUT_DIR)

.PHONY: simple big tree random_fuzz fixed_fuzz show clean always

default: simple tree fixed_fuzz

all: default big

random_fuzz:
	@echo "No random_fuzz C# test yet"

fixed_fuzz:
	@echo "No fixed_fuzz C# test yet"

simple:
	OUTPUT_DIR="$(OUTPUT_DIR)/simple" PYTHON="$(PYTHON)" PYTHONFLAGS="$(PYTHONFLAGS)" EMITTER="$(EMITTER)" ./simpletest.sh

big:
	@echo "No big C# test yet"

tree: tree-aligned $(OUTPUT_FILES) test

$(TREE_OUTPUT_DIR)/%.cs : %.clad always
	mkdir -p $(TREE_OUTPUT_DIR)/$(shell dirname $*.clad)
	$(CLAD) -C $(TREE_INPUT_DIR) -o $(TREE_OUTPUT_DIR) $*.clad

$(TEST_BIN): $(OUTPUT_CS) $(CS_TEST_SRC)
	$(CS) $(CSFLAGS) -out:$@ $^

test: $(TEST_BIN) always
	mono $(TEST_BIN)

show:
	@echo $(INPUT_FILES) $(ALIGNED_INPUT_FILES)

tree-aligned: $(ALIGNED_OUTPUT_FILES)

$(ALIGNED_TREE_OUTPUT_DIR)/%.cs : %.clad always
	mkdir -p $(ALIGNED_TREE_OUTPUT_DIR)/$(shell dirname $*.clad)
	$(CLAD) -C $(ALIGNED_TREE_INPUT_DIR) -o $(ALIGNED_TREE_OUTPUT_DIR) $*.clad

clean:
	rm -rf $(OUTPUT_DIR)
