#!/usr/bin/env bash

set -ex

/odyssey/build/sources/odyssey /odyssey/docker/prometheus-legacy/config.conf
ps aux | grep odyssey

kill -s TERM $(pidof odyssey)
sleep 3
if ps aux | grep -q '[o]dyssey'; then
  echo "Can't finish odyssey after sigterm"
fi