drop table if exists zsb;

create table zsb (id int);

select * from zsb \parse pstmt
\bind_named pstmt
\g

alter table zsb alter column id type float;

\bind_named pstmt
\g

select 42;
