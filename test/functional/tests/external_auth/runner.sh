#!/bin/bash

set -eux

/usr/bin/odyssey /tests/external_auth/external_auth.conf
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -c 'select current_user' 2>&1 && {
    echo "Test authentication successful, but it should have failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    exit 1
}

/tests/external_auth/external-auth-agent 1>/tests/external_auth/agent.log 2>&1 &
agent_pid=$!
sleep 1

echo "Running first authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -c 'select current_user' 2>&1 || {
    echo "First authentication failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    exit 1
}

echo "Running second authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -c 'select 1' 2>&1 || {
    echo "Second authentication failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    echo '-----------'
    cat /var/log/odyssey.log
    exit 1
}

echo "Running third authentication attempt..."
psql 'host=localhost port=6432 user=postgres dbname=postgres password=wrong-token' -c 'select 1' 2>&1 && {
    echo "Third authentication successful, but it should have failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    echo '-----------'
    cat /var/log/odyssey.log
    exit 1
}

echo "All authentications tests completed successfully"

pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 || {
    echo "pgbench failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    echo '-----------'
    cat /var/log/odyssey.log
    exit 1
}

pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=some-token' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 --connect || {
    echo "pgbench failed"
    echo '-----------'
    cat /tests/external_auth/agent.log
    echo '-----------'
    cat /var/log/odyssey.log
    exit 1
}
pgbench_result=$?

ody-stop

# Clean up
kill -9 $agent_pid 2>/dev/null || true

echo "Test completed successfully"