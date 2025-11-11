# Target session attrs
*since 1.4.1*

The target_session_attrs setting enables a pooler to direct client connections
to PostgreSQL servers that meet specific role or session requirements, such as
preferring a primary or accepting any available node. This option is useful in
clustered and high-availability environments, where routing clients to the correct
type of server—without modifying application code—can be important for both safety
and performance. In scenarios where changing application logic is impractical, target_session_attrs offers an effective, transparent solution.

In every places, where you specify target session attrs, you can specify one of the following values:

- `read-write` - always select host, available for write
- `read-only` - never select host, available for write
- `any` (the default one) - select host randomly

----

## You will need several hosts in your storage

To allow Odyssey select host by TSA, you will need several hosts in your storage.
This can be achieved by config that looks like:
```plaintext
storage "postgres" {
    host "primary:5432,standby:5432"
}
```
See [storage configuration guide](../configuration/storage.md)
for more about storage section.

## TSA at listen

You can specify **tsa** in listen section, to create listen ports
that redirect client connections to PostgreSQL servers that meet specific
role or session requirements.

It looks like this in your configuration file:
```plaintest
listen {
    host "127.0.0.1"
    port 6432
    target_session_attrs "read-only"
}

listen {
    host "127.0.0.1"
    port 6433
    target_session_attrs "read-write"
}
```

After that, you can connect to `127.0.0.1:6432` and `127.0.0.1:6433` and will
get different results for `pg_is_in_recovery()` function:

```sh
$ psql -h 127.0.0.1 -p 6432 -c "select pg_is_in_recovery()"
 pg_is_in_recovery
-------------------
 f
(1 row)

$ psql -h 127.0.0.1 -p 6433 -c "select pg_is_in_recovery()"
 pg_is_in_recovery
-------------------
 t
(1 row)
```

See [listen configuration guide](../configuration/listen.md)
for more about listen section.

## TSA at rule

You can specify **tsa** in rule section, to create users that redirect
client connections to PostgreSQL servers that meet specific role or session
requirements.

It looks like this in your configuration file:
```plaintest
database "db" {
    user "user-ro" {
        ...
        storage "postgres"
        target_session_attrs "read-only"
    }

    user "user-rw" {
        ...
        storage "postgres"
        target_session_attrs "read-write"
    }
}
```

After that, you can connect to `db` database with `user-ro` and `user-rw` users
and will get different results for `pg_is_in_recovery()` function:

```sh
$ psql -h localhost -p 6432 -c "select pg_is_in_recovery()" -U user-ro -d db
 pg_is_in_recovery
-------------------
 f
(1 row)

$ psql -h localhost -p 6432 -c "select pg_is_in_recovery()" -U user-rw -d db
 pg_is_in_recovery
-------------------
 t
(1 row)
```

See [rule configuration guide](../configuration/rules.md) for
more about rules section.
