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

/stester -dsn 'postgres://tuser:postgres@localhost:6432/postgres?sslmode=disable' \
  -duration 10m \
  -startup-stagger-max 10s \
  -fail-fast=true \
  -connect-timeout 1s \
  -reconnect-prob 0.5 \
  -small-clients 100 \
  -small-think-min 20ms \
  -small-think-max 500ms \
  -small-timeout 2s \
  -small-max-latency 1s \
  -prepared-clients 50 \
  -prepared-rows 10 \
  -prepared-think-min 20ms \
  -prepared-think-max 500ms \
  -prepared-timeout 2s \
  -prepared-max-latency 2s \
  -tx-clients 4 \
  -tx-sleep 1s \
  -tx-think-min 100ms \
  -tx-think-max 200ms \
  -tx-timeout 2s \
  -tx-max-latency 2s \
  -elephant-clients 4 \
  -elephant-chunk-bytes 65536 \
  -elephant-chunks 20 \
  -elephant-row-delay 1s \
  -elephant-drop-prob 0.15 \
  -elephant-timeout 60s \
  -elephant-max-factor 1.2 \
  -elephant-max-duration 50s || {
    cat /odyssey.log
    for i in /asan-output*; do
      cat $i
    done
    exit 1
}

ody-stop
