#!/usr/bin/env bash
set -euo pipefail

/odyssey/test/injection-points-off/runner.sh
exec /odyssey/build/sources/odyssey_test "$@"
