#!/bin/bash

set -ex

setup

#prepared statements in transaction pooling
/usr/bin/odyssey /etc/odyssey/pstmts.conf
sleep 1
/pstmts-test

ody-stop

teardown
