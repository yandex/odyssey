#!/bin/bash

set -euo pipefail

config=$(mktemp)
trap 'ody-stop; rm -f "$config"' EXIT

for async in no yes; do
    sed "s/log_async yes/log_async $async/" \
        /tests/log-escaping/config.conf > "$config"
    : > /var/log/odyssey.log
    /usr/bin/odyssey "$config"
    python3 /tests/log-escaping/test_log_escaping.py
    ody-stop
done
