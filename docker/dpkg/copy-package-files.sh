#!/bin/bash

set -eux

mkdir -p /packages

for result in $(find / -maxdepth 1 -name "odyssey*" -type f);
do
    cp $result /packages
done
