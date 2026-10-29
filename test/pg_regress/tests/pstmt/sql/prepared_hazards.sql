\pset format unaligned
\set VERBOSITY sqlstate
select $1::int as v \parse x
-- Arguments are opaque SQL to the minimal parser.
SET odyssey.parser_mode = 'mini';
EXECUTE x(1+2);
SET odyssey.parser_mode = 'full';
EXECUTE x(1+2);
-- Unicode-escaped names require the full parser.
EXECUTE U&"\0078"(1+2);
SET odyssey.parser_mode = 'mini';
EXECUTE U&"\0078"(1+2);
