#!/bin/bash
set -e

cd "$(dirname "$0")"

RESULTS_FILE="results.txt"

echo "=== Building benchmark container ==="
docker build -t bench-sqli-guard -f Dockerfile.cache .

echo ""
echo "=== Running benchmark ==="
docker run --rm bench-sqli-guard 2>&1 | tee "$RESULTS_FILE"

echo ""
echo "Results saved to $RESULTS_FILE"
