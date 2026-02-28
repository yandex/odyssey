#!/bin/bash
#
# Test: Verify Odyssey handles Close message with 'S' option (prepared statement close)
#
# This test uses raw PostgreSQL protocol to send Close message with type 'S'
# and verifies that Odyssey properly removes the statement from its cache.
#

set -ex

# Start Odyssey with prepared statement support
/usr/bin/odyssey /tests/prep_stmts_close_statement/odyssey.conf
sleep 1

# Run the test
python3 /tests/prep_stmts_close_statement/test.py

ody-stop

