-- a DISCARD ALL rejected by the server must not drop
-- the statements odyssey tracks for the client
select 42 as v \parse failed_discard_pstmt

begin;
select 1;
discard all;
rollback;

\bind_named failed_discard_pstmt
\g
