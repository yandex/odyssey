#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C
umask 077

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$test_dir/../.." && pwd)
odyssey=${ODYSSEY_BIN:-"$repo_root/build/sources/odyssey"}
if [[ "$odyssey" != /* ]]; then
    odyssey="$PWD/$odyssey"
fi
if [[ ! -x "$odyssey" ]]; then
    printf 'Odyssey binary is not executable: %s\n' "$odyssey" >&2
    exit 1
fi

work_root=$(mktemp -d "${TMPDIR:-/tmp}/odyssey-pgbouncer.XXXXXX")
process_pid=

stop_odyssey() {
    local attempt
    if [[ -n "$process_pid" ]]; then
        kill "$process_pid" 2>/dev/null || true
        for ((attempt = 0; attempt < 100; attempt++)); do
            if ! kill -0 "$process_pid" 2>/dev/null; then
                break
            fi
            sleep 0.05
        done
        kill -KILL "$process_pid" 2>/dev/null || true
        wait "$process_pid" 2>/dev/null || true
        process_pid=
    fi
}

cleanup() {
    stop_odyssey
    rm -rf -- "$work_root"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

check_startup() {
    local directory=$1 port=$2 attempt
    (cd -- "$directory" && exec "$odyssey" --log_to_stdout actual.conf) \
        > "$directory/startup.log" 2>&1 &
    process_pid=$!
    for ((attempt = 0; attempt < 100; attempt++)); do
        if ! kill -0 "$process_pid" 2>/dev/null; then
            break
        fi
        if [[ -S "$directory/.s.PGSQL.$port" ]] && \
            (exec 3<> "/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
            return 0
        fi
        sleep 0.05
    done
    printf 'Odyssey did not start listening:\n' >&2
    cat "$directory/startup.log" >&2
    return 1
}

run_case() {
    local fixture=$1 name directory source port status expected_status expected_conf expected_stderr file
    name=${fixture##*/}
    directory="$work_root/$name"
    mkdir -p -- "$directory" || return 1
    cp -R -- "$fixture/." "$directory/" || return 1
    source=pgbouncer.ini
    if [[ ! -f "$directory/$source" ]]; then
        source=source/pgbouncer.ini
    fi

    port=$((30000 + RANDOM % 30000))
    if [[ -f "$directory/startup" ]]; then
        for file in "$source" expected.conf; do
            sed "s/@PORT@/$port/g" "$directory/$file" > "$directory/rendered" || return 1
            mv -- "$directory/rendered" "$directory/$file" || return 1
        done
    fi

    expected_status=0
    expected_conf="$directory/expected.conf"
    if [[ ! -f "$expected_conf" ]]; then
        expected_status=1
        expected_conf=/dev/null
    fi
    expected_stderr="$directory/expected.stderr"
    if [[ ! -f "$expected_stderr" ]]; then
        expected_stderr=/dev/null
    fi
    status=0
    (cd -- "$directory" && "$odyssey" --convert-pgbouncer "$source") \
        > "$directory/actual.conf" 2> "$directory/actual.stderr" || status=$?
    if [[ "$status" -ne "$expected_status" ]]; then
        printf 'Expected exit %s, got %s\n' "$expected_status" "$status" >&2
        cat "$directory/actual.stderr" >&2
        return 1
    fi
    diff -u -- "$expected_conf" "$directory/actual.conf" || return 1
    diff -u -- "$expected_stderr" "$directory/actual.stderr" || return 1
    if [[ "$status" -eq 0 ]]; then
        if ! "$odyssey" --test "$directory/actual.conf" > "$directory/validation.log" 2>&1; then
            cat "$directory/validation.log" >&2
            return 1
        fi
    fi
    if [[ -f "$directory/startup" ]]; then
        if ! check_startup "$directory" "$port"; then
            stop_odyssey
            return 1
        fi
        stop_odyssey
    fi
}

if [[ "$#" -gt 0 ]]; then
    cases=()
    for name in "$@"; do
        cases+=("$test_dir/cases/$name")
    done
else
    cases=("$test_dir"/cases/*)
fi

passed=0
failed=0
for fixture in "${cases[@]}"; do
    if [[ ! -d "$fixture" ]]; then
        printf 'Missing test case: %s\n' "$fixture" >&2
        exit 1
    fi
    if run_case "$fixture"; then
        printf 'PASS %s\n' "${fixture##*/}"
        passed=$((passed + 1))
    else
        printf 'FAIL %s\n' "${fixture##*/}" >&2
        failed=$((failed + 1))
    fi
done
printf '%s passed, %s failed\n' "$passed" "$failed"
[[ "$failed" -eq 0 ]]
