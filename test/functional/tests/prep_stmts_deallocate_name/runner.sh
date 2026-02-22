#!/bin/bash
#
# Test: Verify Odyssey handles DEALLOCATE name (specific statement)
#

set -ex

# Start Odyssey with prepared statement support
/usr/bin/odyssey /tests/prep_stmts_deallocate_name/odyssey.conf
sleep 1

# Run the test
python3 /tests/prep_stmts_deallocate_name/test.py

ody-stop

