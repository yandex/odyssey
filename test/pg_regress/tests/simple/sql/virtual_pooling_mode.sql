
\c postgres suser
SHOW odyssey.pooling_mode;

SET odyssey.pooling_mode = 'transaction';
SHOW odyssey.pooling_mode;

\c postgres tuser
SHOW odyssey.pooling_mode;

SET odyssey.pooling_mode = 'session';
SHOW odyssey.pooling_mode;

\c postgres stuser
SHOW odyssey.pooling_mode;

\c postgres suser
SHOW odyssey.pooling_mode;
