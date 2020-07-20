#!/bin/bash
set -ex

cd /test_dir/test && ./odyssey_test || true
cd /
/test_dir/sources/odyssey /test_dir/test/odyssey-test.conf

/ody-load-producer

