#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
odyssey_bin=${1:-"$repo_root/build/sources/odyssey"}
trap 'ody-stop || exit 1' EXIT

ODYSSEY_BIN="$odyssey_bin" ody-start "$repo_root/test/injection-points-off/odyssey.conf" console

output=$(timeout 5 psql -XAt -v ON_ERROR_STOP=0 -v VERBOSITY=verbose \
    -h 127.0.0.1 -p 6432 -U console -d console \
    -c 'LOAD injection_test' -c 'SHOW IS_PAUSED' 2>&1)
echo "$output"
grep -q 'ERROR:.*0A000:.*LOAD is disabled' <<< "$output"
grep -qx f <<< "$output"
