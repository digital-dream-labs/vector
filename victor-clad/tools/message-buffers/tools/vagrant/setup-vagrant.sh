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

if [ ! -h /usr/bin/clang ] && [ ! -h /usr/bin/clang++ ]; then
  echo ">>> Updating apt-get"
  # for golang
  sudo add-apt-repository -y ppa:gophers/archive
  sudo apt-get update --fix-missing

  echo ">>> Installing Valgrind v3.10.1"
  cd /opt/
  sudo apt-get install valgrind -y --fix-missing

  echo ">>> Installing Valgrind Dependencies"
  sudo apt-get -y install \
  curl \
  tree \
  build-essential \
  gyp \
  ninja-build \
  clang-3.6 \
  libc++-dev \
  mono-mcs \
  git \
  libboost1.55-dev \
  cmake \
  libreadline-dev \
  libglib2.0-dev \
  libuv-dev \
  dbus*-dev \
  libbluetooth-dev \
  python2.7 \
  python-pip \
  golang-1.10-go \
  unzip

  sudo ln -s /usr/bin/clang-3.6 /usr/bin/clang
  sudo ln -s /usr/bin/clang++-3.6 /usr/bin/clang++
  sudo ln -s /usr/lib/go-1.10/bin/go /usr/bin/go

else
  echo ">>> Already privisioned Vagrant for Linux Valgrind run"
fi
