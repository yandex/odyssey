#!/usr/bin/env bash

set -u

BUILD_DIR=$(realpath -m "${TSAN_BUILD_DIR:-build-tsan}")
OUT_DIR=$(realpath -m "${TSAN_OUT_DIR:-tsan-out}")
JOBS=${TSAN_JOBS:-$(nproc)}

mkdir -p "${OUT_DIR}"

{
    printf 'commit='; git rev-parse HEAD
    printf 'version='; cat VERSION
    printf 'compiler='; "${CC:-cc}" --version | sed -n '1p'
    printf 'cmake='; cmake --version | sed -n '1p'
} > "${OUT_DIR}/environment.txt"

if ! cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=TSAN -DCC_FLAGS="" \
    > "${OUT_DIR}/configure.log" 2>&1; then
    echo 'TSan configure failed' >&2
    exit 1
fi

if ! cmake --build "${BUILD_DIR}" --parallel "${JOBS}" \
    > "${OUT_DIR}/build.log" 2>&1; then
    echo 'TSan build failed' >&2
    exit 1
fi

set +e
(
    cd "${BUILD_DIR}/sources" || exit 1
    TSAN_OPTIONS="halt_on_error=1 exitcode=66 log_path=${OUT_DIR}/tsan-report history_size=7" \
        ./odyssey_test
) > "${OUT_DIR}/test.log" 2>&1
TEST_STATUS=$?
set -e

echo "${TEST_STATUS}" > "${OUT_DIR}/test.exit-code"

if compgen -G "${OUT_DIR}/tsan-report.*" > /dev/null; then
    echo 'TSan report found' >&2
    exit 1
fi

if [ "${TEST_STATUS}" -ne 0 ]; then
    echo "TSan tests failed with exit code ${TEST_STATUS}" >&2
    exit "${TEST_STATUS}"
fi

echo 'TSan tests passed'
