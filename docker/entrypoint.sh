#!/bin/bash
set -ex

setup

/usr/bin/odyssey /scram/config.conf
/scram/test_scram_backend.sh
/scram/test_scram_frontend.sh
ody-stop

/ody-intergration-test

ody-stop

/usr/bin/odyssey-asan /etc/odyssey/odyssey.conf

## /shell-test/test.sh

teardown
