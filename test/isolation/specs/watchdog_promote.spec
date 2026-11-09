# A result read before promotion may still publish read-only. The next full
# watchdog poll must replace it with read-write on the same cached endpoint.
setup { LOAD watchdog_before_status_publish; }
setup { LOAD watchdog_after_poll; }

session "standby"
step "promote" { SELECT pg_promote(true, 60); }

session "control"
step "await_old_result" { LOAD watchdog_before_status_publish WAIT; }
step "release_old_result" { LOAD watchdog_before_status_publish; }
step "await_old_poll" { LOAD watchdog_after_poll WAIT; }
step "old_cache" { SHOW tsa_cache; }
step "arm_new_result" { LOAD watchdog_before_status_publish; }
step "release_old_poll" { LOAD watchdog_after_poll; }
step "await_new_result" { LOAD watchdog_before_status_publish WAIT; }
step "arm_new_poll" { LOAD watchdog_after_poll; }
step "release_new_result" { LOAD watchdog_before_status_publish; }
step "await_new_poll" { LOAD watchdog_after_poll WAIT; }
step "new_cache" { SHOW tsa_cache; }
step "release_new_poll" { LOAD watchdog_after_poll; }

session "client"
step "require_writer" { SET odyssey.target_session_attrs TO 'read-write'; }
step "writer" { SELECT pg_is_in_recovery(); }

permutation await_old_result promote release_old_result await_old_poll old_cache arm_new_result release_old_poll await_new_result arm_new_poll release_new_result await_new_poll new_cache release_new_poll require_writer writer
