#!/usr/bin/env bash

set -ex

ody-start
sleep 1
/tests/config-validation/config-validation
ody-stop
