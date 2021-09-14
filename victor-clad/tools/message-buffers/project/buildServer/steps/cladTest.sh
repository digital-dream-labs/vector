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

echo "cladTest.sh: Entering directory /vagrant"
cd /vagrant

# PYTHON=2.7 or 3
PYTHON=$1

if [ "${1}" == "3" ]; then
    PYTHON_VERSION="PYTHON=python${PYTHON}"
else
    PYTHON_VERSION=""
fi

# clean
make -C emitters/tests clean

# build
make $PYTHON_VERSION OUTPUT_DIR=build -C emitters/tests

EXIT_STATUS=$?
set -e

# exit
exit $EXIT_STATUS
