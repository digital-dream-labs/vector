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
# CLAD makefile for Go
#

OUTPUT_DIR=./build
OUTPUT_GO_DIR=$(OUTPUT_DIR)/src/build
OUTPUT_CPP_DIR=$(OUTPUT_DIR)/cpp
SRC_DIR=./DirTreeTests

PYTHON?=python
PYTHONFLAGS?=
EMITTER=../../Go_emitter.py
C_EMITTER=../../CPP_emitter.py
CLAD=$(PYTHON) $(PYTHONFLAGS) $(EMITTER)
CLAD_C=$(PYTHON) $(PYTHONFLAGS) $(C_EMITTER)
GO_TEST_SRC=clad_test.go interop.go interop_test.go
GO?=go
GOFLAGS?=
GOPATH?=`pwd`/build:`pwd`/../../../support/go
TEST_BIN=$(OUTPUT_DIR)/testGo
CXXFLAGS="-I ./build/cpp -I ../../../support/cpp/include -std=c++11"

CLAD_INPUT_DIRS=$(shell find $(SRC_DIR) -type d)
CLAD_INPUT_SRC=$(subst $(SRC_DIR)/, ,$(shell find $(SRC_DIR) -type f -iname '*.clad'))
IGNORED_C_FILES=
CLAD_C_INPUT_SRC=$(filter-out $(IGNORED_C_FILES),$(CLAD_INPUT_SRC))
GENERATED_GO=$(patsubst %.clad, %.go, $(addprefix $(OUTPUT_GO_DIR)/,$(shell basename -a $(CLAD_INPUT_SRC))))
GENERATED_C=$(patsubst %.clad, $(OUTPUT_CPP_DIR)/%.cpp, $(CLAD_C_INPUT_SRC))

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
EXTRA_ENV?=CC=clang CXX=clang++
endif
EXTRA_ENV?=

vpath %.clad $(CLAD_INPUT_DIRS)

.PHONY: test clean always
default: test
all: default

$(OUTPUT_GO_DIR)/%.go : %.clad $(EMITTER)
	mkdir -p $(OUTPUT_GO_DIR)
	$(eval DEP=$(filter %.clad,$^))
	$(CLAD) -C $(shell dirname $(DEP)) -I $(SRC_DIR) -o $(OUTPUT_GO_DIR) $(notdir $(DEP))

$(OUTPUT_CPP_DIR)/%.cpp : %.clad $(C_EMITTER)
	mkdir -p $(OUTPUT_CPP_DIR)
	$(eval DEP=$(filter %.clad,$^))
	$(CLAD_C) -C $(SRC_DIR) -o $(OUTPUT_CPP_DIR) $(subst $(SRC_DIR)/, ,$(DEP))

$(TEST_BIN): $(GENERATED_GO) $(GO_TEST_SRC) $(GENERATED_C)
	GOPATH=$(GOPATH) $(EXTRA_ENV) CGO_CXXFLAGS=$(CXXFLAGS) $(GO) test $(GOFLAGS) -v -o $@

test: $(TEST_BIN)

clean:
	rm -rf $(OUTPUT_DIR)
