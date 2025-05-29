
SELECT 1;
SET a.b TO 'z';
SHOW a.b;

SET odyssey.target_session_attrs = "read-only";
-- should fail
SHOW odyssey.target_session_attrs;

/* XXX : support this syntax too */
SET odyssey.target_session_attrs TO 'read-write';

-- should succeed
SHOW odyssey.target_session_attrs;