#!/usr/bin/env bash
set -euo pipefail

python3 "$(dirname "$0")/check_startup.py" "${ODYSSEY_BIN:-/usr/bin/odyssey}"
