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

# autoconf: <config>.conf.autoconf overrides global options and must be
# validated by --test just like the base config.
script_dir=$(cd "$(dirname "$0")" && pwd)

# shared config base: /tests copy inside the functional test image, repo
# test/pg_regress tree when running the runner locally
base_conf=/tests/config-errors/_base
if [ ! -f "$base_conf" ]; then
    base_conf="$script_dir/../../../pg_regress/conf/_base"
fi

assert_config_valid() {
    local status
    if timeout 10 "$odyssey_bin" --test "$test_dir/config.conf" >"$test_dir/output.log" 2>&1; then
        status=0
    else
        status=$?
    fi

    if [[ "$status" != 0 ]]; then
        echo "Expected config test success, got $status"
        cat "$test_dir/output.log"
        exit 1
    fi
    if ! grep -q 'config is valid' "$test_dir/output.log"; then
        echo "Missing 'config is valid' diagnostic"
        cat "$test_dir/output.log"
        exit 1
    fi
}

# valid base config without autoconf passes
rm -f "$test_dir/config.conf.autoconf"
cp "$base_conf" "$test_dir/config.conf"
assert_config_valid

# valid autoconf overriding a global option passes
cp "$script_dir/autoconf_valid.conf" "$test_dir/config.conf.autoconf"
assert_config_valid

# unknown directive in autoconf fails and points at the autoconf file
cp "$script_dir/autoconf_bad_directive.conf" "$test_dir/config.conf.autoconf"
assert_config_error --test
if ! grep -q 'config.conf.autoconf' "$test_dir/output.log"; then
    echo "Expected autoconf file name in error diagnostics"
    cat "$test_dir/output.log"
    exit 1
fi

# bad option value in autoconf fails
cp "$script_dir/autoconf_bad_value.conf" "$test_dir/config.conf.autoconf"
assert_config_error --test

# include directives are forbidden in autoconf
cp "$script_dir/autoconf_bad_include.conf" "$test_dir/config.conf.autoconf"
assert_config_error --test
if ! grep -q 'includes are forbidden in this context' "$test_dir/output.log"; then
    echo "Expected 'includes are forbidden in this context' in error diagnostics"
    cat "$test_dir/output.log"
    exit 1
fi

rm -f "$test_dir/config.conf" "$test_dir/config.conf.autoconf"
