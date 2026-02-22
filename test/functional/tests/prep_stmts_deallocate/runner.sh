#!/bin/bash
#
# Test: Verify Odyssey clears prepared statement cache on DEALLOCATE
#
# This test ensures that when a client sends DEALLOCATE ALL,
# Odyssey invalidates its server->prep_stmts cache.
#
# Uses psycopg3 which uses extended query protocol (Parse/Bind/Execute).
#

set -ex

# Start Odyssey with prepared statement support
/usr/bin/odyssey /tests/prep_stmts_deallocate/odyssey.conf
sleep 1

# Run the test
python3 /tests/prep_stmts_deallocate/check_deallocate.py

ody-stop
