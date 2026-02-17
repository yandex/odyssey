#!/bin/bash
#
# Test: Verify Odyssey handles DEALLOCATE PREPARE name (specific statement with PREPARE keyword)
#

set -ex

# Start Odyssey with prepared statement support
/usr/bin/odyssey /tests/prep_stmts_deallocate_prepare_name/odyssey.conf
sleep 1

# Run the test
python3 /tests/prep_stmts_deallocate_prepare_name/test.py

ody-stop

