#!/bin/bash

set -eux

mkdir -p /odyssey-packages

for result in $(find / -maxdepth 1 -name "odyssey*" -type f);
do
    cp $result /odyssey-packages
done
