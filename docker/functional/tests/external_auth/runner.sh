#!/bin/bash -x

# Start the external auth agent in the background
/tests/external_auth/external-auth-agent 1>/dev/null &
agent_pid=$!

# Give the agent time to start and create the socket
sleep 2

# Start Odyssey
/usr/bin/odyssey /tests/external_auth/external_auth.conf &
odyssey_pid=$!

# Give Odyssey time to start
sleep 3

# Run two authentication attempts
echo "Running first authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -c 'select current_user' > /tests/external_auth/log1 2>&1
auth1_result=$?

echo "Running second authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -c 'select 1' > /tests/external_auth/log2 2>&1
auth2_result=$?

echo "Running third authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=wrong-token' -c 'select 1' > /tests/external_auth/log3 2>&1
auth3_result=$?

# Give some time for the agent to finish processing
sleep 1

# Check if both authentications succeeded
if [ $auth1_result -eq 0 ]; then
    echo "First authentication successful"
    cat /tests/external_auth/log1
else
    echo "First authentication failed"
    cat /tests/external_auth/log1
    exit 1
fi

if [ $auth2_result -eq 0 ]; then
    echo "Second authentication successful"
    cat /tests/external_auth/log2
else
    echo "Second authentication failed"
    cat /tests/external_auth/log2
    exit 1
fi

if [ $auth3_result -eq 0 ]; then
    echo "Third authentication successful, but it should have failed"
    cat /tests/external_auth/log3
    exit 1
else
    echo "Third authentication failed as expected"
    cat /tests/external_auth/log3
fi

# Check that we got the expected results
if grep -q "postgres" /tests/external_auth/log1; then
    echo "First query returned expected user"
else
    echo "First query did not return expected user"
    exit 1
fi

if grep -q "1" /tests/external_auth/log2; then
    echo "Second query returned expected result"
else
    echo "Second query did not return expected result"
    exit 1
fi

if grep -q "incorrect password" /tests/external_auth/log3; then
    echo "Third query returned expected result"
else
    echo "Third query did not return expected result"
    exit 1
fi

echo "All authentications tests completed successfully"

# Check some read-only load
pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10
pgbench_result=$?

if [ $pgbench_result -ne 0 ]; then
    echo "pgbench failed"
    exit 1
fi

pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 --connect
pgbench_result=$?

if [ $pgbench_result -ne 0 ]; then
    echo "pgbench failed"
    exit 1
fi

# Clean up
kill $odyssey_pid 2>/dev/null || true
kill $agent_pid 2>/dev/null || true

# Wait for processes to terminate
wait $odyssey_pid 2>/dev/null || true
wait $agent_pid 2>/dev/null || true

echo "Test completed successfully"
