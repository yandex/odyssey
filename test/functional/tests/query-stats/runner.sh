#!/bin/bash
set -euo pipefail

python3 "$(dirname "$0")/query_stats.py" "$@"
