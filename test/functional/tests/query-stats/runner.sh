#!/bin/bash
set -euo pipefail

python3 "$(dirname "$0")/test_query_stats.py"
