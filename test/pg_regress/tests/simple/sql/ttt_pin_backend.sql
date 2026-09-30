SELECT 1;

SHOW odyssey.pin_backend;

BEGIN;
CREATE TEMP TABLE z_ttt(i int);
SELECT 1;
SHOW odyssey.pin_backend;
COMMIT;

INSERT INTO z_ttt VALUES (42);
SELECT 1;
SHOW odyssey.pin_backend;

DROP TABLE z_ttt;
SELECT 1;
SHOW odyssey.pin_backend;
