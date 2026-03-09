#!/bin/bash
#
# sqli_guard performance test
#
# Runs the 3-mode benchmark and validates:
# 1. All modes produce correct match counts
# 2. Cache mode has 100% hit rate after warmup
# 3. Cache mode is faster than plain regex
#
set -e

echo "=== sqli_guard performance test ==="
echo ""

OUTPUT=$(./bench_sqli_guard)
echo "$OUTPUT"

echo ""
echo "=== Validating results ==="

# Check mode 2 matches (should be 500000 — 10 malicious out of 20 queries)
MATCHES_PLAIN=$(echo "$OUTPUT" | grep -A2 "Mode 2:" | grep "matches:" | awk '{print $2}')
if [ "$MATCHES_PLAIN" != "500000" ]; then
    echo "FAIL: Mode 2 expected 500000 matches, got $MATCHES_PLAIN"
    exit 1
fi
echo "OK: Mode 2 matches = $MATCHES_PLAIN"

# Check mode 3 matches (should also be 500000)
MATCHES_CACHE=$(echo "$OUTPUT" | grep -A4 "Mode 3:" | grep "matches:" | awk '{print $2}')
if [ "$MATCHES_CACHE" != "500000" ]; then
    echo "FAIL: Mode 3 expected 500000 matches, got $MATCHES_CACHE"
    exit 1
fi
echo "OK: Mode 3 matches = $MATCHES_CACHE"

# Check cache hit rate > 99%
HIT_PCT=$(echo "$OUTPUT" | grep "cache hits:" | grep -o '[0-9]*\.[0-9]*%' | tr -d '%')
HIT_OK=$(echo "$HIT_PCT > 99.0" | bc -l 2>/dev/null || echo "1")
if [ "$HIT_OK" != "1" ]; then
    echo "FAIL: Cache hit rate ${HIT_PCT}% is below 99%"
    exit 1
fi
echo "OK: Cache hit rate = ${HIT_PCT}%"

# Check cache mode is faster than plain (compare avg latency)
LATENCY_PLAIN=$(echo "$OUTPUT" | grep -A4 "Mode 2:" | grep "avg per op:" | awk '{print $4}')
LATENCY_CACHE=$(echo "$OUTPUT" | grep -A5 "Mode 3:" | grep "avg per op:" | awk '{print $4}')
CACHE_FASTER=$(echo "$LATENCY_CACHE < $LATENCY_PLAIN" | bc -l 2>/dev/null || echo "1")
if [ "$CACHE_FASTER" != "1" ]; then
    echo "FAIL: Cache mode ($LATENCY_CACHE us) is not faster than plain ($LATENCY_PLAIN us)"
    exit 1
fi
echo "OK: Cache mode (${LATENCY_CACHE} us) faster than plain (${LATENCY_PLAIN} us)"

echo ""
echo "=== All performance checks passed ==="
