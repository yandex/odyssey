#!/usr/bin/env bash

set -ex

/usr/bin/odyssey /tests/prep_stmts/pstmts.conf
sleep 1

/tests/prep_stmts/pstmts-test

ody-stop