#!/usr/bin/env bash

set -ex

ody-start
/tests/config-validation/config-validation
ody-stop
