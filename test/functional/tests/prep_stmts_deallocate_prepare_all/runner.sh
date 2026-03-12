#!/bin/bash
#
# Test: Verify Odyssey clears prepared statement cache on DEALLOCATE PREPARE ALL
#

set -ex

# Start Odyssey with prepared statement support
/usr/bin/odyssey /tests/prep_stmts_deallocate_prepare_all/odyssey.conf
sleep 1

# Run the test
python3 /tests/prep_stmts_deallocate_prepare_all/test.py

ody-stop

