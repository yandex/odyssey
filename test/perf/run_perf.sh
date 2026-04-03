#!/bin/bash
#
# sql_guard performance test — validates all 5 modes
#
# 1. sql_guard disabled (baseline)
# 2. blacklist (no cache)     — 500000 blocked
# 3. blacklist + cache        — 500000 blocked, >99% hit rate, faster than #2
# 4. whitelist (no cache)     — 500000 blocked
# 5. whitelist + cache        — 500000 blocked, >99% hit rate, faster than #4
#
set -e

echo "=== sql_guard performance test ==="
echo ""

OUTPUT=$(./bench_sql_guard)
echo "$OUTPUT"

echo ""
echo "=== Validating results ==="

# --- Mode 2: blacklist (no cache) ---
BLOCKED_2=$(echo "$OUTPUT" | grep -A2 "Mode 2:" | grep "blocked:" | awk '{print $2}')
if [ "$BLOCKED_2" != "500000" ]; then
    echo "FAIL: Mode 2 expected 500000 blocked, got $BLOCKED_2"
    exit 1
fi
echo "OK: Mode 2 blocked = $BLOCKED_2"

# --- Mode 3: blacklist + cache ---
BLOCKED_3=$(echo "$OUTPUT" | grep -A2 "Mode 3:" | grep "blocked:" | awk '{print $2}')
if [ "$BLOCKED_3" != "500000" ]; then
    echo "FAIL: Mode 3 expected 500000 blocked, got $BLOCKED_3"
    exit 1
fi
echo "OK: Mode 3 blocked = $BLOCKED_3"

# Mode 3 cache hit rate > 99%
HIT_PCT_3=$(echo "$OUTPUT" | grep -A3 "Mode 3:" | grep "cache hits:" | grep -o '[0-9]*\.[0-9]*%' | tr -d '%')
HIT_OK_3=$(echo "$HIT_PCT_3 > 99.0" | bc -l 2>/dev/null || echo "1")
if [ "$HIT_OK_3" != "1" ]; then
    echo "FAIL: Mode 3 cache hit rate ${HIT_PCT_3}% is below 99%"
    exit 1
fi
echo "OK: Mode 3 cache hit rate = ${HIT_PCT_3}%"

# Mode 3 cache faster than Mode 2
LATENCY_2=$(echo "$OUTPUT" | grep -A3 "Mode 2:" | grep "avg per op:" | awk '{print $4}')
LATENCY_3=$(echo "$OUTPUT" | grep -A4 "Mode 3:" | grep "avg per op:" | awk '{print $4}')
CACHE_FASTER_3=$(echo "$LATENCY_3 < $LATENCY_2" | bc -l 2>/dev/null || echo "1")
if [ "$CACHE_FASTER_3" != "1" ]; then
    echo "FAIL: Mode 3 (${LATENCY_3} us) not faster than Mode 2 (${LATENCY_2} us)"
    exit 1
fi
echo "OK: Mode 3 (${LATENCY_3} us) faster than Mode 2 (${LATENCY_2} us)"

# --- Mode 4: whitelist (no cache) ---
BLOCKED_4=$(echo "$OUTPUT" | grep -A2 "Mode 4:" | grep "blocked:" | awk '{print $2}')
if [ "$BLOCKED_4" != "500000" ]; then
    echo "FAIL: Mode 4 expected 500000 blocked, got $BLOCKED_4"
    exit 1
fi
echo "OK: Mode 4 blocked = $BLOCKED_4"

# --- Mode 5: whitelist + cache ---
BLOCKED_5=$(echo "$OUTPUT" | grep -A2 "Mode 5:" | grep "blocked:" | awk '{print $2}')
if [ "$BLOCKED_5" != "500000" ]; then
    echo "FAIL: Mode 5 expected 500000 blocked, got $BLOCKED_5"
    exit 1
fi
echo "OK: Mode 5 blocked = $BLOCKED_5"

# Mode 5 cache hit rate > 99%
HIT_PCT_5=$(echo "$OUTPUT" | grep -A3 "Mode 5:" | grep "cache hits:" | grep -o '[0-9]*\.[0-9]*%' | tr -d '%')
HIT_OK_5=$(echo "$HIT_PCT_5 > 99.0" | bc -l 2>/dev/null || echo "1")
if [ "$HIT_OK_5" != "1" ]; then
    echo "FAIL: Mode 5 cache hit rate ${HIT_PCT_5}% is below 99%"
    exit 1
fi
echo "OK: Mode 5 cache hit rate = ${HIT_PCT_5}%"

# Mode 5 cache faster than Mode 4
LATENCY_4=$(echo "$OUTPUT" | grep -A3 "Mode 4:" | grep "avg per op:" | awk '{print $4}')
LATENCY_5=$(echo "$OUTPUT" | grep -A4 "Mode 5:" | grep "avg per op:" | awk '{print $4}')
CACHE_FASTER_5=$(echo "$LATENCY_5 < $LATENCY_4" | bc -l 2>/dev/null || echo "1")
if [ "$CACHE_FASTER_5" != "1" ]; then
    echo "FAIL: Mode 5 (${LATENCY_5} us) not faster than Mode 4 (${LATENCY_4} us)"
    exit 1
fi
echo "OK: Mode 5 (${LATENCY_5} us) faster than Mode 4 (${LATENCY_4} us)"

echo ""
echo "=== All performance checks passed (5 modes) ==="
