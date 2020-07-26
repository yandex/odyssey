#!/bin/bash

mkdir -p /odyssey/build
cd /odyssey/build

set -ex

cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE ..
make

./sources/odyssey /etc/odyssey.conf
