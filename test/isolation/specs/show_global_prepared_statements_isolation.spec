# Statements created by SQL PREPARE are tracked globally: two clients
# each create the same statement under their own name, and SHOW checks
# the refcount covers both.

setup
{
    DEALLOCATE isolationtester_waiting;
}

teardown
{
    DEALLOCATE ALL;
}

session "s1"
step "s1_prep"
{
    PREPARE pa AS SELECT 1::int + $1::int
}

session "s2"
step "s2_prep"
{
    PREPARE pb AS SELECT 1::int + $1::int
}

session "s3"
step "show"
{
    SHOW GLOBAL_PREPARED_STATEMENTS
}

permutation
s1_prep s2_prep show
