
SELECT 1;

SHOW odyssey.pin_backend;

SET odyssey.pin_backend = "on";
SHOW odyssey.pin_backend;

SET odyssey.pin_backend TO 'off';
SHOW odyssey.pin_backend;

SET odyssey.pin_backend = "true";
SHOW odyssey.pin_backend;

SET odyssey.pin_backend TO 'false';
SHOW odyssey.pin_backend;

SET odyssey.pin_backend = "1";
SHOW odyssey.pin_backend;

SET odyssey.pin_backend TO '0';
SHOW odyssey.pin_backend;

SET odyssey.pin_backend = "invalid";
SHOW odyssey.pin_backend;

-- pg-style bool spellings
SET odyssey.pin_backend = "yes";
SHOW odyssey.pin_backend;

SET odyssey.pin_backend TO 'no';
SHOW odyssey.pin_backend;

SET odyssey.pin_backend = "t";
SHOW odyssey.pin_backend;

SET odyssey.opportunistic_acquire = "yes";
SHOW odyssey.opportunistic_acquire;

SET odyssey.opportunistic_acquire TO '0';
SHOW odyssey.opportunistic_acquire;
