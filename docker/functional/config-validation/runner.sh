#!/usr/bin/env bash

set -ex

ody-start
sleep 1
/config-validation
ody-stop
