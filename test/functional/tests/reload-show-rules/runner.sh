#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
timeout 90 python3 "$script_dir/check_reload_show.py"
