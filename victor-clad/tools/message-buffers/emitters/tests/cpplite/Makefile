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
# CLAD makefile for C++ Lite
# Requires GNU make, probably a fairly recent version
#

###############################################
# Editable Options                            #
###############################################

# Can override from command line with constructs like:
# make PYTHON=python3

OUTPUT_DIR=./build

CPPLITE_TEST_DIR?=.
CPPLITE_BUILD_DIR?=$(CPPLITE_TEST_DIR)/build
CPPLITE_ROOT_DIR?=$(CPPLITE_TEST_DIR)/../../..

CPPLITE_COMPILER?=clang++
CPPLITE_COMPILERFLAGS?=-DCLAD_DEBUG -c -Wall -Wextra -std=c++11 -stdlib=libc++ -Os -g
CPPLITE_LINKER?=clang++
CPPLITE_LINKERFLAGS?=-Wall -Wextra -std=c++11 -stdlib=libc++ -Os -g

PYTHON?=python
PYTHONFLAGS?=
CPPLITE_EMITTER?=$(CPPLITE_ROOT_DIR)/emitters/CPPLite_emitter.py
CPPLITE_EMITTERFLAGS?=


###############################################
# Variables to Supply to test.make            #
###############################################

export BUILD_MODE=1

export CLAD_DIR:=$(CPPLITE_ROOT_DIR)
#INPUT_DIR varies
#OUTPUT_DIR varies

export GENERATED_SOURCE:=%.cpp
export GENERATED_HEADER:=%.h
export GENERATED_INTERMEDIATE:=%.o

export SUPPORT_SOURCE:=
export SUPPORT_HEADERS:=
export SUPPORT_INTERMEDIATE:=

#TEST_SOURCE varies
#TEST_HEADERS vary
export TEST_INTERMEDIATE:=cpplitetest.o
export TEST_BINARY:=cpplitetest

export PYTHON
export PYTHONFLAGS
export EMITTER:=$(CPPLITE_EMITTER)
export EMITTERFLAGS:=$(CPPLITE_EMITTERFLAGS)

export COMPILER:=$(CPPLITE_COMPILER)
export COMPILERFLAGS:=$(CPPLITE_COMPILERFLAGS) -I$(CPPLITE_SUPPORT_DIR)/include
export LINKER:=$(CPPLITE_LINKER)
export LINKERFLAGS:=$(CPPLITE_LINKERFLAGS)
#RUNNER varies
#RUNNERFLAGS vary


###############################################
# Per-Target Variables to Supply to test.make #
###############################################

FUZZ_INPUT_DIR:=$(CLAD_DIR)/emitters/tests/src/fuzz-lite
FUZZ_OUTPUT_DIR:=$(CPPLITE_BUILD_DIR)/fuzz
FUZZ_TEST_SOURCE:=$(CPPLITE_TEST_DIR)/litefuzztest.cpp
FUZZ_TEST_HEADERS=$(FUZZ_GENERATED_HEADER)
FUZZ_RUNNER:=$(CLAD_DIR)/emitters/tests/fuzz_runner.sh

FIXED_FUZZ_RUNNERFLAGS="--fixed $(FUZZ_TYPE) $(FUZZ_INPUT_DIR)"
RANDOM_FUZZ_RUNNERFLAGS="--random $(FUZZ_TYPE) $(FUZZ_INPUT_DIR)"

# Type to fuzz
FUZZ_TYPE?=n1::ComplexUnion
FUZZ_GENERATED_HEADER:=$(FUZZ_OUTPUT_DIR)/fuzzdef.h

FUZZ_TYPE_TARGET:=$(FUZZ_OUTPUT_DIR)/$(subst :,-,$(FUZZ_TYPE)).fuzztype

# function that calls an inner test based upon a variable name prefix
invoke_test=$(MAKE) -f $(CLAD_DIR)/emitters/tests/test.make $(2) \
    INPUT_DIR="$($(1)_INPUT_DIR)" \
    OUTPUT_DIR="$($(1)_OUTPUT_DIR)" \
    TEST_SOURCE="$($(1)_TEST_SOURCE)" \
    TEST_HEADERS="$($(1)_TEST_HEADERS)" \
    COMPILERFLAGS="-I$($(1)_OUTPUT_DIR) $(COMPILERFLAGS)" \
    LINKERFLAGS="-I$($(1)_OUTPUT_DIR) $(LINKERFLAGS)" \
    RUNNER="$($(1)_RUNNER)"


###############################################
# Targets                                     #
###############################################

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.PHONY: simple big tree random_fuzz fixed_fuzz show clean default

default: simple tree

all: default big

simple:
	OUTPUT_DIR="$(OUTPUT_DIR)/simple" ./simpletest.sh

big:
	@echo "No big C++ Lite test yet"

tree:
	@echo "No tree C++ Lite test yet"

random_fuzz: $(FUZZ_GENERATED_HEADER)
	$(call invoke_test,FUZZ) RUNNERFLAGS=$(RANDOM_FUZZ_RUNNERFLAGS)

fixed_fuzz: $(FUZZ_GENERATED_HEADER)
	$(call invoke_test,FUZZ) RUNNERFLAGS=$(FIXED_FUZZ_RUNNERFLAGS)

$(FUZZ_GENERATED_HEADER): $(FUZZ_TYPE_TARGET) | $(FUZZ_OUTPUT_DIR)
	echo "typedef $(FUZZ_TYPE) MessageType;\nconst char *MessageTypeName = \"$(FUZZ_TYPE)\";\n" > $(FUZZ_GENERATED_HEADER)

# weird construct that allows you to change message types in the makefile without rebuilding everything
$(FUZZ_TYPE_TARGET): | $(FUZZ_OUTPUT_DIR)
	rm -f $(FUZZ_OUTPUT_DIR)/*.fuzztype
	touch $(FUZZ_TYPE_TARGET)

$(FUZZ_OUTPUT_DIR):
	mkdir -p $(FUZZ_OUTPUT_DIR)

show:
	$(call invoke_test,FUZZ,show) RUNNERFLAGS=$(TREE_RUNNERFLAGS)

clean:
	rm -rf $(CPPLITE_BUILD_DIR)
