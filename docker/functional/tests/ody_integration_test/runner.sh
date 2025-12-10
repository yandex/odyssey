#!/usr/bin/env bash

set -ex

ody-start

/tests/ody_integration_test/ody_integration_test
sleep 5
ody-stop