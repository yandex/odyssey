# Each fixture client holds a transaction-level advisory lock through
# Odyssey. A forced wait lets the controller stop that client's reads.
# Only Odyssey's write timeout can then release the transaction lock.

session "set_waiter"
setup { BEGIN; }
step "wait_set" { SELECT pg_advisory_xact_lock(759103); }
step "release_set" { COMMIT; }

session "error_waiter"
setup { BEGIN; }
step "wait_error" { SELECT pg_advisory_xact_lock(759104); }
step "release_error" { COMMIT; }

session "controller"
step "stall_set" { COPY (SELECT 'go') TO '@SIGNAL_DIR@/set.go'; }
step "stall_error" { COPY (SELECT 'go') TO '@SIGNAL_DIR@/error.go'; }

permutation wait_set(*) stall_set release_set wait_error(*) stall_error release_error
