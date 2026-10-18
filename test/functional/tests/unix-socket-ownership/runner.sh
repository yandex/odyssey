#!/bin/bash
set -euo pipefail
ulimit -c 0
python3 "$(dirname "$0")/check_unix_socket.py"
