#!/usr/bin/env bash
set -euo pipefail

echo "Waiting for odyssey at ${PG_HOST}:${PG_PORT} ..."
for i in $(seq 1 60); do
  if bash -c "echo > /dev/tcp/${PG_HOST}/${PG_PORT}" 2>/dev/null; then
    break
  fi
  sleep 1
done

echo "Starting userver service..."
/app/build/userver_notes --config /app/configs/config.yaml &
SERVICE_PID=$!

cleanup() {
  if kill -0 "${SERVICE_PID}" 2>/dev/null; then
    kill "${SERVICE_PID}" 2>/dev/null || true
    wait "${SERVICE_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "Waiting for HTTP service..."
READY=0
for i in $(seq 1 20); do
  if ! kill -0 "${SERVICE_PID}" 2>/dev/null; then
    echo "userver service exited prematurely"
    wait "${SERVICE_PID}" || true
    exit 1
  fi

  if curl -sf http://127.0.0.1:8080/ping >/dev/null; then
    READY=1
    break
  fi

  sleep 1
done

if [ "${READY}" -ne 1 ]; then
  echo "Service did not become ready in time"
  exit 1
fi

echo "Running pytest..."
pytest /app/tests -v
