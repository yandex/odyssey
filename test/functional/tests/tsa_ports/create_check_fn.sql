create or replace function check_is_in_recovery(expected boolean)
returns integer
as $$
begin
    if pg_is_in_recovery() is distinct from expected then
        raise exception 'pg_is_in_recovery()=% but expected=%', pg_is_in_recovery(), expected;
    end if;
    return 1;
end;
$$ language plpgsql;
