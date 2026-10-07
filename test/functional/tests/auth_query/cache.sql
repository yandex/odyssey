CREATE SCHEMA auth_query_cache_test;

CREATE TABLE auth_query_cache_test.credentials (
    username text PRIMARY KEY,
    password text
);
INSERT INTO auth_query_cache_test.credentials VALUES
    ('cache_alice', 'alpha'),
    ('cache_bob', 'beta'),
    ('cache_error', 'delta'),
    ('cache_retry', 'epsilon'),
    ('cache_parallel', 'parallel'),
    ('cache_reload', 'before');

CREATE TABLE auth_query_cache_test.failures (username text PRIMARY KEY);
CREATE TABLE auth_query_cache_test.requests (username text);
CREATE TABLE auth_query_cache_test.gate (enabled boolean);

CREATE FUNCTION auth_query_cache_test.lookup(requested_user text)
RETURNS TABLE(username text, password text) LANGUAGE plpgsql AS $$
BEGIN
    IF requested_user LIKE 'cache_busy_%' OR requested_user = 'cache_reload' THEN
        WHILE EXISTS (SELECT 1 FROM auth_query_cache_test.gate WHERE enabled) LOOP
            -- Odyssey applies the auth query timeout to each read, so
            -- notices keep a gated query from timing out.
            RAISE NOTICE 'auth query cache gate';
            PERFORM pg_sleep(0.05);
        END LOOP;
    END IF;
    IF requested_user = 'cache_parallel' THEN
        PERFORM pg_sleep(0.2);
    END IF;
    IF EXISTS (SELECT 1 FROM auth_query_cache_test.failures f
               WHERE f.username = requested_user) THEN
        RAISE EXCEPTION 'auth query cache test failure for %', requested_user;
    END IF;
    INSERT INTO auth_query_cache_test.requests VALUES (requested_user);
    RETURN QUERY SELECT c.username, c.password
        FROM auth_query_cache_test.credentials c
        WHERE c.username = requested_user;
END;
$$;

CREATE DATABASE auth_query_source_new;
\connect auth_query_source_new
CREATE SCHEMA auth_query_cache_test;
CREATE FUNCTION auth_query_cache_test.lookup(requested_user text)
RETURNS TABLE(username text, password text) LANGUAGE sql AS $$
    SELECT requested_user, 'after'::text;
$$;
