#!/usr/bin/env bash

set -eux

test_no_restart_on_fail() {
    /usr/bin/odyssey /tests/online-restart/odyssey.conf
    sleep 2

    # broken config
    echo "some trash" > /tests/online-restart/odyssey.conf

    before_restart_pid=$(pidof odyssey)

    kill -sUSR2 $(pidof odyssey)

    sleep 5

    after_restart_pid=$(pidof odyssey)

    ps aux | grep odyssey

    ody_pids_count=$(ps aux | grep '[o]dyssey' | wc -l)

    if [ "$ody_pids_count" != 1 ]; then
        echo "too many odyssey instances (" $ody_pids_count ")"
        cat /var/log/odyssey.log
        exit 1
    fi

    if [ "$before_restart_pid" != "$after_restart_pid" ]; then
        echo "odyssey restarted: much fail!"
        cat /var/log/odyssey.log
        exit 1
    fi

    ody-stop
}

test_several_signals() {
    /usr/bin/odyssey /tests/online-restart/odyssey.conf
    sleep 2

    psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(5)' 2>&1 > /tests/online-restart/result &
    sleep 0.5

    before_restart_pid=$(pidof odyssey)

    kill -sUSR2 "$before_restart_pid" || true
    kill -sUSR2 "$before_restart_pid" || true
    kill -sUSR2 "$before_restart_pid" || true
    kill -sUSR2 "$before_restart_pid" || true
    kill -sUSR2 "$before_restart_pid" || true

    wait -n || {
        echo "psql failed"
        cat /tests/online-restart/result
        exit 1
    }

    sleep 1

    ps aux | grep odyssey

    ody_pids_count=$(ps aux | grep '[o]dyssey' | wc -l)

    if [ "$ody_pids_count" != 1 ]; then
        echo "too many odyssey instances (" $ody_pids_count ")"
        cat /var/log/odyssey.log
        exit 1
    fi

    after_restart_pid=$(pidof odyssey)

    if [ "$before_restart_pid" == "$after_restart_pid" ]; then
        echo "odyssey didn't restarted"
        cat /var/log/odyssey.log
        exit 1
    fi

    ody-stop
}

test_signal_new_instance_before_parent_die() {
    /usr/bin/odyssey /tests/online-restart/odyssey.conf
    sleep 2

    psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(5)' 2>&1 > /tests/online-restart/result &
    sleep 0.5

    before_restart_pid=$(pidof odyssey)

    kill -sUSR2 $before_restart_pid
    sleep 1

    ps aux | grep '[o]dyssey'

    after_restart_pid=$(ps aux | grep '[o]dyssey' | grep -v $before_restart_pid | awk '{print $2}')

    kill -sUSR2 $after_restart_pid || {
        echo 'seems like new instance failed'
        cat /var/log/odyssey.log
        exit 1
    }

    wait -n || {
        echo "psql failed"
        cat /tests/online-restart/result
        exit 1
    }
    sleep 1

    ody_pids_count=$(ps aux | grep '[o]dyssey' | wc -l)

    if [ "$ody_pids_count" != 1 ]; then
        echo "too many odyssey instances (" $ody_pids_count ")"
        cat /var/log/odyssey.log
        exit 1
    fi

    after_restart_pid=$(pidof odyssey)

    if [ "$before_restart_pid" == "$after_restart_pid" ]; then
        echo "odyssey didn't restarted"
        cat /var/log/odyssey.log
        exit 1
    fi

    ody-stop
}

# test_several_signals

# echo "" > /var/log/odyssey.log
# sleep 1

test_signal_new_instance_before_parent_die

# echo "" > /var/log/odyssey.log
# sleep 1

# test_no_restart_on_fail
