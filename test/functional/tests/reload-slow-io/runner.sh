#!/usr/bin/env bash

set -euo pipefail

python3 "$(dirname "$0")/test_reload.py" --odyssey "${ODYSSEY_BIN:-/usr/bin/odyssey}"
python3 "$(dirname "$0")/test_regression.py" --odyssey "${ODYSSEY_BIN:-/usr/bin/odyssey}"
