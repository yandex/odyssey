#!/bin/bash
set -euo pipefail

python3 "$(dirname "$0")/test.py" \
    --odyssey /usr/bin/odyssey --pg-bin /usr/lib/postgresql/18/bin
