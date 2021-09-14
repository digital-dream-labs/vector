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

HILITE_TEXT = "\x1b[7m" $(1) "\x1b[0m"

default all simple big tree random_fuzz fixed_fuzz clean:
	@echo ""
	@echo $(call HILITE_TEXT,"C++ Lite $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C cpplite $@
	@echo ""
	@echo $(call HILITE_TEXT,"C# $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C csharp $@
	@echo ""
	@echo $(call HILITE_TEXT,"PYTHON $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C python $@
	@echo ""
	@echo $(call HILITE_TEXT,"C++ $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C cpp $@
	@echo ""
	@echo $(call HILITE_TEXT,"C $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C c $@
	@echo ""
	@echo $(call HILITE_TEXT,"GO $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	$(MAKE) -C go $@
	@echo ""
	@echo $(call HILITE_TEXT,"DONE $@ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
	@echo ""

help:
	@echo ""
	@echo "CLAD Tests"
	@echo "Available targets (for all platforms): default all simple big tree clean"
	@echo ""
	@echo "simple: a quick test to make sure most things are working OK"
	@echo "big: similar to simple, but with multiple compilers and valgrind, so it takes a little longer"
	@echo "tree: test out the inclusion tree commands"
	@echo ""
	@echo "random_fuzz: random seed fuzz testing"
	@echo "fixed_fuzz: fixed seed fuzz testing"
	@echo ""
	@echo "default (no need to specify): both simple and tree tests"
	@echo "all: run all tests (not random_fuzz)"
	@echo "clean: delete all output--you may need to clean between tests to discover certain problems or regressions"
	@echo ""
	@echo "To use python 3, just add PYTHON=python3 (or the path to the python 3 binary) to the end of your command."
	@echo ""
	@echo "You may also run these commands in a specific platform directory to only run tests on that platform."
	@echo ""
