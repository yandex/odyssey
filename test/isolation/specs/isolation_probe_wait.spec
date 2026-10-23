# Exercise Bind/Describe/Execute of isolationtester's virtual lock-wait probe.
setup
{
    CREATE TABLE isolation_probe_wait (id integer PRIMARY KEY, value integer);
    INSERT INTO isolation_probe_wait VALUES (1, 0);
}

teardown
{
    DROP TABLE isolation_probe_wait;
}

session "s1"
step "s1_begin" { BEGIN; }
step "s1_lock" { UPDATE isolation_probe_wait SET value = 1 WHERE id = 1; }
step "s1_commit" { COMMIT; }

session "s2"
step "s2_wait" { UPDATE isolation_probe_wait SET value = 2 WHERE id = 1; }
step "s2_read" { SELECT value FROM isolation_probe_wait WHERE id = 1; }

permutation "s1_begin" "s1_lock" "s2_wait" "s1_commit" "s2_read"
