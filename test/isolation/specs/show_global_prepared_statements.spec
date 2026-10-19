# SHOW GLOBAL_PREPARED_STATEMENTS with two clients holding the same
# parsed statement: both sessions allocate it via the extended protocol
# (\parse), then the console session checks the refcount covers both
# clients (a client and a server reference each, plus the map entry).

setup
{
    SELECT 1;
}

teardown
{
    DEALLOCATE ALL;
}

session "s1"
step "s1_parse"
{
    SELECT 1::int + $1::int
    \parse ptest_s1
}

session "s2"
step "s2_parse"
{
    SELECT 1::int + $1::int
    \parse ptest_s2
}

session "console"
database "console"
step "show"
{
    SHOW GLOBAL_PREPARED_STATEMENTS;
}

permutation
s1_parse s2_parse show
