-- drop a possible leftover override to survive repeated test runs
\! rm -f "$PGTEST_CONF.autoconf"

SHOW odyssey.pooling_mode;

-- override exactly one option via autoconf and reload odyssey
\! echo 'pool "transaction"' > "$PGTEST_CONF.autoconf"
\! pgrep -x odyssey | xargs -r kill -HUP
\! sleep 2

-- the existing connection must survive the reload and keep its rule
SELECT 42;
SHOW odyssey.pooling_mode;

-- a new connection must pick up the new default
\connect
SHOW odyssey.pooling_mode;
