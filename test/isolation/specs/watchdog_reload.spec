# Replace a primary with a standby while its watchdog holds an old result.
setup { LOAD watchdog_before_status_publish; }
setup { LOAD watchdog_after_poll; }
setup { LOAD watchdog_before_exit; }
setup { LOAD watchdog_before_join; }

session "reload"
step "reload" { RELOAD; }
step "reload_done" { SET isolation = done; }

session "primary"
# The native spec scanner treats braces inside SQL strings as delimiters too.
step "replace_storage"
{
  COPY (SELECT 'storage "watched" ' || chr(123) || ' type "remote" host "localhost:5435" endpoints_status_poll_interval 100 watchdog ' || chr(123) || ' authentication "none" storage "watched" storage_db "postgres" storage_user "postgres" pool "transaction" pool_routing "internal" ' || chr(125) || chr(125))
  TO '/home/postgres/regress/conf/isolation_watchdog_storage';
}

session "control"
step "await_old_result" { LOAD watchdog_before_status_publish WAIT; }
step "await_join" { LOAD watchdog_before_join WAIT; }
step "release_old_result" { LOAD watchdog_before_status_publish; }
step "await_old_poll" { LOAD watchdog_after_poll WAIT; }
step "release_old_poll" { LOAD watchdog_after_poll; }
step "await_old_exit" { LOAD watchdog_before_exit WAIT; }
step "release_old_exit" { LOAD watchdog_before_exit; }
step "arm_new_result" { LOAD watchdog_before_status_publish; }
step "arm_new_poll" { LOAD watchdog_after_poll; }
step "release_join" { LOAD watchdog_before_join; }
step "await_new_result" { LOAD watchdog_before_status_publish WAIT; }
step "release_new_result" { LOAD watchdog_before_status_publish; }
step "await_new_poll" { LOAD watchdog_after_poll WAIT; }
step "new_cache" { SHOW tsa_cache; }
step "release_new_poll" { LOAD watchdog_after_poll; }

# Joining the old watchdog precedes reconfiguration and the first new poll.
# The cache must contain only the replacement endpoint, marked read-only.
permutation await_old_result replace_storage reload(*) await_join release_old_result await_old_poll release_old_poll await_old_exit arm_new_result arm_new_poll release_join release_old_exit reload_done await_new_result release_new_result await_new_poll new_cache release_new_poll
