#!/bin/bash -x
#
# Integration test: cancel request routing_clients leak
#
# Reproduces a production bug where od_backend_connect_cancel() hangs
# forever on od_read() with UINT32_MAX timeout, leaking:
#   - routing_clients counter (never decremented)
#   - coroutines (stuck forever)
#   - socket file descriptors
#
# When enough routing slots leak, client_max_routing is reached and
# the accept loop blocks ALL new connections — including admin console.
#
# Architecture:
#   PG:5432 <-- evil_proxy:6433 <-- odyssey:6432 <-- send_cancel.py
#
# The evil proxy forwards cancel requests to PG but keeps the client
# socket open forever, simulating a network condition where EOF never
# arrives back to Odyssey.
#
set -ex

TEST_DIR=/tests/cancel-leak
PROXY_PORT=6433
ODYSSEY_PORT=6432
CANCEL_COUNT=20

# --- Start evil proxy ---
python3 "$TEST_DIR/evil_proxy.py" $PROXY_PORT 127.0.0.1 5432 \
    > /tmp/evil_proxy.log 2>&1 &
PROXY_PID=$!
sleep 1

kill -0 $PROXY_PID || {
    echo "FAIL: evil proxy did not start"
    cat /tmp/evil_proxy.log
    exit 1
}

cleanup() {
    kill $PROXY_PID 2>/dev/null || true
    rm -f /tmp/evil_proxy.pid
    ody-stop 2>/dev/null || true
}
trap cleanup EXIT

# --- Start Odyssey ---
/usr/bin/odyssey "$TEST_DIR/cancel-leak.conf"
sleep 1

# --- Verify connectivity ---
psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'SELECT 1'

# --- Baseline: routing_clients should be 0 ---
sleep 2  # let stats print once for baseline coroutine count

get_routing_clients() {
    timeout 3 psql 'host=localhost port=6432 user=console dbname=console' \
        -t -A -c 'show lists' 2>/dev/null | grep routing_clients | cut -d'|' -f2 | tr -d ' '
}

get_coroutine_total() {
    # Sum active coroutines from all worker + system stats lines (last batch)
    local total=0
    while IFS= read -r line; do
        local active
        active=$(echo "$line" | grep -oP 'coroutines \(\K[0-9]+')
        if [ -n "$active" ]; then
            total=$((total + active))
        fi
    done < <(grep 'coroutines (' /var/log/odyssey.log | tail -3)
    echo "$total"
}

initial_rc=$(get_routing_clients)
initial_coro=$(get_coroutine_total)
echo "Initial: routing_clients=$initial_rc, coroutines=$initial_coro"

if [ "$initial_rc" != "0" ]; then
    echo "FAIL: routing_clients not 0 at start ($initial_rc)"
    exit 1
fi

# --- Fire parallel cancel requests ---
echo "Firing $CANCEL_COUNT parallel cancel requests..."
python3 "$TEST_DIR/send_cancel.py" 127.0.0.1 $ODYSSEY_PORT postgres postgres $CANCEL_COUNT

# Wait for cancels to settle + stats to print
sleep 5

# --- Check results ---
after_coro=$(get_coroutine_total)
echo "After cancels: coroutines=$after_coro (was $initial_coro)"

# Try to read routing_clients — if blocked, that's the ultimate proof
set +e
after_rc=$(get_routing_clients)
rc_exit=$?
set -e

if [ -z "$after_rc" ] || [ $rc_exit -ne 0 ]; then
    # Console is blocked — accept loop is saturated
    echo "Console BLOCKED — routing slots fully saturated"
    after_rc="BLOCKED"
fi

echo "After cancels: routing_clients=$after_rc"

# --- Check for production log message ---
cmr_hits=$(grep -c 'client is waiting in routing queue' /var/log/odyssey.log || echo 0)
echo "Log 'client is waiting in routing queue': $cmr_hits hits"

# --- Try a new data connection ---
set +e
timeout 5 psql 'host=localhost port=6432 user=postgres dbname=postgres' \
    -c "SELECT 'canary'" > /tmp/canary.out 2>&1
canary_rc=$?
set -e

echo "Canary connection: exit_code=$canary_rc (124=timeout)"

# --- Verify the bug is present (unfixed) or absent (fixed) ---
coro_delta=$((after_coro - initial_coro))
echo ""
echo "=== Results ==="
echo "  routing_clients: $initial_rc -> $after_rc"
echo "  coroutine delta: $coro_delta"
echo "  console blocked: $([ "$after_rc" = 'BLOCKED' ] && echo YES || echo no)"
echo "  canary blocked:  $([ $canary_rc -eq 124 ] && echo YES || echo no)"
echo "  log cmr hits:    $cmr_hits"

# The test PASSes if the fix is in place: routing_clients returns to 0
# The test FAILs if the bug exists: routing_clients leaks and blocks everything
if [ "$after_rc" = "0" ]; then
    echo ""
    echo "PASS: routing_clients returned to 0 — cancel leak fix is working"
    exit 0
fi

# Bug detected
echo ""
echo "FAIL: cancel leak detected"
if [ "$after_rc" = "BLOCKED" ]; then
    echo "  Admin console is unreachable (accept loop blocked)"
fi
if [ $canary_rc -eq 124 ]; then
    echo "  New data connections are blocked"
fi
if [ $cmr_hits -gt 0 ]; then
    echo "  Log shows '(client_max_routing) client is waiting in routing queue'"
fi
echo "  Coroutines leaked: $coro_delta"

# Show diagnostic info
echo ""
echo "=== Proxy log ==="
tail -20 /tmp/evil_proxy.log
echo ""
echo "=== Odyssey log (last 30 lines) ==="
tail -30 /var/log/odyssey.log

exit 1
