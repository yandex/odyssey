#!/bin/bash

set -ex

# Wait for PostgreSQL
until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

# Wait for Odyssey
until pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
  echo "Wait for odyssey..."
  sleep 1
done

# Wait for odyssey log file to appear
LOG_FILE="/var/log/odyssey/odyssey.log"
for i in $(seq 1 30); do
  if [ -f "$LOG_FILE" ]; then
    break
  fi
  echo "Wait for log file..."
  sleep 1
done

if [ ! -f "$LOG_FILE" ]; then
  echo "FAIL: log file $LOG_FILE not found"
  exit 1
fi

# Generate a marker that is exactly 2048 characters long (well above default 1024)
# Use a known pattern: MARKER_START + 2000 x 'A' + MARKER_END
MARKER_START="LOGMAXTEST_BEGIN_"
MARKER_END="_LOGMAXTEST_END"
FILL=$(printf '%0.sA' $(seq 1 2000))
LONG_VALUE="${MARKER_START}${FILL}${MARKER_END}"
LONG_VALUE_LEN=${#LONG_VALUE}

echo "Generated test value length: $LONG_VALUE_LEN"

# Execute a query containing the long string through odyssey
# log_query is enabled, so odyssey will log this SQL
psql "host=odyssey port=6432 user=postgres dbname=postgres" \
  -c "SELECT '${LONG_VALUE}'" > /dev/null 2>&1 || {
    echo "FAIL: query execution failed"
    exit 1
}

# Give odyssey time to flush the log
sleep 2

# -------------------------------------------------------
# Test 1: log_max_msg_size 4096 — message must NOT be truncated
# The full MARKER_END must appear in the log
# -------------------------------------------------------
echo "=== Test 1: Verify long message is NOT truncated ==="

if grep -q "${MARKER_END}" "$LOG_FILE"; then
  echo "PASS: full message found in log (MARKER_END present)"
else
  echo "FAIL: message was truncated (MARKER_END not found in log)"
  echo "--- Log tail ---"
  tail -20 "$LOG_FILE"
  exit 1
fi

# -------------------------------------------------------
# Test 2: Verify MARKER_START is also present (sanity check)
# -------------------------------------------------------
echo "=== Test 2: Verify marker start present ==="

if grep -q "${MARKER_START}" "$LOG_FILE"; then
  echo "PASS: MARKER_START found in log"
else
  echo "FAIL: MARKER_START not found in log"
  echo "--- Log tail ---"
  tail -20 "$LOG_FILE"
  exit 1
fi

# -------------------------------------------------------
# Test 3: Verify the full unbroken marker appears on a single line
# -------------------------------------------------------
echo "=== Test 3: Verify full marker on single log line ==="

if grep -q "${MARKER_START}.*${MARKER_END}" "$LOG_FILE"; then
  echo "PASS: full unbroken marker found on single line"
else
  echo "FAIL: marker is split or incomplete"
  exit 1
fi

echo ""
echo "All log_max_msg_size tests passed."
exit 0
