#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/log_max_msg_size/config.conf
sleep 1

LOG_FILE="/var/log/odyssey.log"

# Generate a marker that is exactly 2048 characters long (well above default 1024)
MARKER_START="LOGMAXTEST_BEGIN_"
MARKER_END="_LOGMAXTEST_END"
FILL=$(printf '%0.sA' $(seq 1 2000))
LONG_VALUE="${MARKER_START}${FILL}${MARKER_END}"
LONG_VALUE_LEN=${#LONG_VALUE}

echo "Generated test value length: $LONG_VALUE_LEN"

# Execute a query containing the long string through odyssey
# log_query is enabled, so odyssey will log this SQL
psql 'host=localhost port=6432 user=postgres dbname=postgres' \
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

ody-stop
