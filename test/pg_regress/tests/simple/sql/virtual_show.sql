
SELECT 1;

SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs = "read-only";
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'read-write';
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs = "prefer-standby";
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'any';
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'z';
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'read-onl';
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'read-only-foo';
SHOW odyssey.target_session_attrs;

SET odyssey.target_session_attrs TO 'an';
SHOW odyssey.target_session_attrs;