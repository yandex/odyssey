ALTER SYSTEM SET hot_standby_feedback = on;
SELECT pg_reload_conf();
