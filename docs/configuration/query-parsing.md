# Query parsing section

Query parsing is used to classify incoming queries, for example by
[auto_route_ro_on_standby](rules.md#auto_route_ro_on_standby) to decide
whether a query can be safely sent to a standby replica.

```yml
query_parsing {
    mode full
    max_query_len 10000
    mem_limit 5MB
    standby_function_list {
        "get_user_settings"
        "get_user_profile"
    }
}
```

## **mode**

*disabled|minimal|full*

Selects the SQL parsing mode:

* `disabled` — queries are not parsed at all.
* `minimal` — a lightweight parser recognizes a small subset of
  statements (`SHOW`, `SET`, `BEGIN`, `DEALLOCATE`, `DISCARD`,
  `UNLISTEN`) and treats plain `SELECT` statements as read-only.
* `full` — the complete PostgreSQL grammar is used. Queries which can
  not be parsed or can not be classified as read-only require the
  primary.

Default: `minimal`

`mode full`

## **max_query_len**

*integer*

Queries longer than this number of bytes are not parsed.

Default: `10000`

`max_query_len 10000`

## **mem_limit**

*size*

Memory limit for the full grammar parser arena. Supports B/KB/MB/GB
prefixes.

Default: `5MB`

`mem_limit 5MB`

## **standby_function_list**

*list of strings*

User-defined function names which do not modify the database and
therefore can be safely executed on a standby replica.

```yml
standby_function_list {
    "get_user_settings"
    "get_user_profile"
}
```

The list extends the built-in list of standby-friendly built-in
functions (`now()`, `abs()`, `count()` and others). Both variants of the
name are recognized: unqualified and `pg_catalog`-qualified. A
schema-qualified name (`myschema.my_func()`) is never treated as
standby-friendly.

Used in the `full` mode only: in the `minimal` mode function calls are
not parsed, and a `SELECT` containing a function call is considered
read-only. Note that with the `minimal` mode a `SELECT` calling a
function with side effects (for example `pg_sleep()`) can be routed to
a standby replica.

Default: built-in list of standby-friendly functions.
