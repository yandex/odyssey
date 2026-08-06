#!/usr/bin/env bash

# TODO: create fuzzing tests here

set -eux

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

until pg_isready -h replica -p 5432 -U postgres -d postgres; do
  echo "Wait for replica..."
  sleep 1
done

/odyssey /odyssey.conf
sleep 1

# Under sanitizers (ASAN/TSAN) Odyssey runs 2-3x slower, so we relax
# latency limits to avoid flaky failures on CI.
if [ -n "${ASAN_OPTIONS:-}" ] || [ -n "${TSAN_OPTIONS:-}" ]; then
  SMALL_MAX_LATENCY=3s
  PREPARED_MAX_LATENCY=5s
  TX_MAX_LATENCY=5s
  SMALL_TIMEOUT=5s
  PREPARED_TIMEOUT=5s
  TX_TIMEOUT=5s
  ELEPHANT_TIMEOUT=90s
  ELEPHANT_MAX_DURATION=70s
else
  SMALL_MAX_LATENCY=1s
  PREPARED_MAX_LATENCY=2s
  TX_MAX_LATENCY=2s
  SMALL_TIMEOUT=2s
  PREPARED_TIMEOUT=2s
  TX_TIMEOUT=2s
  ELEPHANT_TIMEOUT=60s
  ELEPHANT_MAX_DURATION=50s
fi

/stester -dsn 'postgres://tuser:postgres@localhost:6432/postgres?sslmode=disable' \
  -duration 10m \
  -startup-stagger-max 30s \
  -fail-fast=true \
  -connect-timeout 1s \
  -reconnect-prob 0.5 \
  -small-clients 100 \
  -small-think-min 20ms \
  -small-think-max 500ms \
  -small-timeout "$SMALL_TIMEOUT" \
  -small-max-latency "$SMALL_MAX_LATENCY" \
  -prepared-clients 50 \
  -prepared-rows 10 \
  -prepared-think-min 20ms \
  -prepared-think-max 500ms \
  -prepared-timeout "$PREPARED_TIMEOUT" \
  -prepared-max-latency "$PREPARED_MAX_LATENCY" \
  -tx-clients 4 \
  -tx-sleep 1s \
  -tx-think-min 100ms \
  -tx-think-max 200ms \
  -tx-timeout "$TX_TIMEOUT" \
  -tx-max-latency "$TX_MAX_LATENCY" \
  -elephant-clients 4 \
  -elephant-chunk-bytes 65536 \
  -elephant-chunks 20 \
  -elephant-row-delay 1s \
  -elephant-drop-prob 0.15 \
  -elephant-timeout "$ELEPHANT_TIMEOUT" \
  -elephant-max-factor 1.2 \
  -elephant-max-duration "$ELEPHANT_MAX_DURATION" || {
    sleep 1
    cat /odyssey.log
    for i in /asan-output*; do
      cat $i
    done
    exit 1
}

ody-stop
