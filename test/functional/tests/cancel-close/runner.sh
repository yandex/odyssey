#!/bin/bash
set -euo pipefail
ulimit -c 0

cd /tests/cancel-close
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.pem \
	-days 2 -nodes -subj "/CN=localhost"

odyssey /tests/cancel-close/config.conf
trap 'ody-stop || exit 1' EXIT
sleep 1

python3 check_cancel_close.py
