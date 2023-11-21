#!/bin/bash

set -ex

setup

ody-start
/ody-integration-test
ody-stop

teardown
