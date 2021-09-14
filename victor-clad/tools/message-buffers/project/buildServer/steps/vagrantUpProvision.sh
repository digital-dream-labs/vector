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

# change dir to the project dir, no matter where script is executed from
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VAGRANT=`which vagrant`

VAGRANT_RUNNING=$(vagrant status | grep running | grep virtualbox | wc -l)
VAGRANT_EXISTS=$(vagrant status | grep poweroff | grep virtualbox | wc -l)

if [ "$VAGRANT_RUNNING" -gt 0 ];then
    echo ">>> Vagrant VM already running, using existing VM"
    cd $DIR ; $VAGRANT provision

elif [ "$VAGRANT_EXISTS" -gt 0 ];then
    echo ">>> Vagrant VM exists but is not running"
    cd $DIR ; $VAGRANT up ; $VAGRANT provision

else
    echo ">>> No Vagrant VM exists, starting a new one"
    cd $DIR ; $VAGRANT up
fi

EXIT_STATUS=$?

echo ">>> Stopping Vagrant instance"
$VAGRANT halt

# exit
echo "EXIT_STATUS ${EXIT_STATUS}"
exit $EXIT_STATUS
