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

# CLAD makefile for C++
# Requires GNU make, probably a fairly recent version
#

###############################################
# Editable Options                            #
###############################################

# Can override from command line with constructs like:
# make PYTHON=python3

CPP_TEST_DIR?=.
CPP_BUILD_DIR?=$(CPP_TEST_DIR)/build
CPP_ROOT_DIR?=$(CPP_TEST_DIR)/../../..
CPP_SUPPORT_DIR=$(CPP_ROOT_DIR)/support/cpp

CPP_COMPILER?=clang++
CPP_COMPILERFLAGS?=-c -Wall -Wextra -std=c++11 -stdlib=libc++ -Os -g
CPP_LINKER?=clang++
CPP_LINKERFLAGS?=-Wall -Wextra -std=c++11 -stdlib=libc++ -Os -g

PYTHON?=python
PYTHONFLAGS?=
CPP_EMITTER?=$(CPP_ROOT_DIR)/emitters/CPP_emitter.py
CPP_EMITTERFLAGS?=--output-json --output-properties


###############################################
# Variables to Supply to test.make            #
###############################################

export BUILD_MODE=1

export CLAD_DIR:=$(CPP_ROOT_DIR)
#INPUT_DIR varies
#OUTPUT_DIR varies

INPUT_DIR ?= ./
OUTPUT_DIR ?= ./build

export GENERATED_SOURCE:=%.cpp
export GENERATED_HEADER:=%.h
export GENERATED_INTERMEDIATE:=%.o

export SUPPORT_SOURCE:=$(CPP_SUPPORT_DIR)/source/SafeMessageBuffer.cpp
export SUPPORT_HEADERS:=$(CPP_SUPPORT_DIR)/include/CLAD/SafeMessageBuffer.h
export SUPPORT_INTERMEDIATE:=SafeMessageBuffer.o

#TEST_SOURCE varies
#TEST_HEADERS vary
export TEST_INTERMEDIATE:=cpptest.o
export TEST_BINARY:=cpptest

export PYTHON
export PYTHONFLAGS
export EMITTER:=$(CPP_EMITTER)
export EMITTERFLAGS:=$(CPP_EMITTERFLAGS)

export COMPILER:=$(CPP_COMPILER)
export COMPILERFLAGS:=$(CPP_COMPILERFLAGS) -I$(CPP_SUPPORT_DIR)/include
export LINKER:=$(CPP_LINKER)
export LINKERFLAGS:=$(CPP_LINKERFLAGS)
#RUNNER varies
#RUNNERFLAGS vary

###############################################
# Targets                                     #
###############################################

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.PHONY: simple big show clean default

default: simple big

all: default

simple:
	OUTPUT_DIR="$(OUTPUT_DIR)/simple" ./simpletest.sh

big:
	OUTPUT_DIR="$(OUTPUT_DIR)/big" ./bigtest.sh

clean:
	rm -rf $(CPP_BUILD_DIR)
