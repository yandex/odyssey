-- Save the server-side reset value before application_name is rewritten.
SELECT reset_val AS application_name FROM pg_settings WHERE name = 'application_name' \gset reset_
SET application_name TO 'changed';
SET application_name TO DEFAULT;
SELECT current_setting('application_name') = :'reset_application_name' AS restored;
SET application_name TO '';
SELECT current_setting('application_name') LIKE ' - %' AS empty_is_rewritten;
SET application_name = DEFAULT;
SELECT current_setting('application_name') = :'reset_application_name' AS restored;

-- Ordinary GUCs still use the backend's default.
SHOW statement_timeout \gset reset_
SET statement_timeout TO '100s';
SET statement_timeout TO DEFAULT;
SELECT current_setting('statement_timeout') = :'reset_statement_timeout' AS restored;

-- Only execute_on_host accepts both DEFAULT and an empty string as a reset.
SET odyssey.execute_on_host TO 'localhost:5432';
SHOW odyssey.execute_on_host;
SET odyssey.execute_on_host TO DEFAULT;
SHOW odyssey.execute_on_host;
SET odyssey.execute_on_host TO 'localhost:5432';
SET odyssey.execute_on_host TO '';
SHOW odyssey.execute_on_host;

-- Rejected values must leave the previous virtual GUC value intact.
SET odyssey.pin_backend TO 'on';
SET odyssey.pin_backend TO DEFAULT;
SET odyssey.pin_backend TO '';
SHOW odyssey.pin_backend;
SET odyssey.opportunistic_acquire TO 'off';
SET odyssey.opportunistic_acquire TO DEFAULT;
SET odyssey.opportunistic_acquire TO '';
SHOW odyssey.opportunistic_acquire;
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.target_session_attrs TO DEFAULT;
SET odyssey.target_session_attrs TO '';
SHOW odyssey.target_session_attrs;
