#!/bin/bash

set -e

sleep 2

for run in {1..1000}; do
    kill -s HUP $(pidof odyssey) || exit 1
done
