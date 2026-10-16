-- LISTEN sets a listen-origin pin, dropped by UNLISTEN *
LISTEN a;
UNLISTEN *;
SHOW odyssey.pin_backend;

-- a manually set pin survives UNLISTEN *, even with an active LISTEN
SET odyssey.pin_backend = on;
LISTEN b;
UNLISTEN *;
SHOW odyssey.pin_backend;

-- ...in the extended protocol as well
UNLISTEN * \parse u
\bind_named u
\g
SHOW odyssey.pin_backend;
