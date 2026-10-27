# Each session waits for the other session's table lock to be released.
setup { CREATE TABLE isolation_lock (i integer); }
teardown { DROP TABLE isolation_lock; }

session "s1"
step "s1_begin" { BEGIN; }
step "s1_lock" { LOCK TABLE isolation_lock IN ACCESS EXCLUSIVE MODE; }
step "s1_commit" { COMMIT; }

session "s2"
step "s2_begin" { BEGIN; }
step "s2_lock" { LOCK TABLE isolation_lock IN ACCESS EXCLUSIVE MODE; }
step "s2_commit" { COMMIT; }

permutation s1_begin s2_begin s1_lock s2_lock s1_commit s2_commit
permutation s1_begin s2_begin s2_lock s1_lock s2_commit s1_commit
