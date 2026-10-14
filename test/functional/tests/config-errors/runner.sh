#!/usr/bin/env bash

set -euo pipefail
ulimit -c 0

odyssey_bin=${ODYSSEY_BIN:-/usr/bin/odyssey}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT

assert_config_error() {
    local status
    if timeout 10 "$odyssey_bin" "$@" "$test_dir/config.conf" >"$test_dir/output.log" 2>&1; then
        status=0
    else
        status=$?
    fi

    if [[ "$status" != 1 ]]; then
        echo "Expected config error exit code 1, got $status (options: $*)"
        cat "$test_dir/output.log"
        exit 1
    fi
    if ! grep -q 'error (config)' "$test_dir/output.log"; then
        echo "Missing config error diagnostic (options: $*)"
        cat "$test_dir/output.log"
        exit 1
    fi
}

for config in \
    'unknown_parameter yes' \
    'workres 1' \
    'listen { unknown_parameter yes }' \
    'storage "backend" { unknown_parameter yes }' \
    'database default { user default { unknown_parameter yes } }' \
    'workers unexpected' \
    'workers 0' \
    'listen {'; do
    printf '%s\n' "$config" >"$test_dir/config.conf"
    assert_config_error
    assert_config_error --test
done

rm "$test_dir/config.conf"
assert_config_error
assert_config_error --test
