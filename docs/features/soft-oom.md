# Soft OOM support
*since 1.4.1*

Sometimes, even when using a connection pooler, you may encounter
Out Of Memory (OOM) situations on the PostgreSQL server. In such
cases, it is advisable to temporarily suspend accepting new connections
until PostgreSQL’s memory consumption returns to a normal level.
To address this, we have implemented a "soft OOM" mechanism, which is
described in detail below. This mechanism helps prevent further
overloading the database by gracefully managing new connection attempts
during memory pressure events.

----

## Configuration

First of all you will need describe memory limitations in `soft-oom`
section at the root of [configuration](../configuration/overview.md):

```plaintext
soft_oom {
    process 'postgres'
    limit   750MB
}
```

After configured `soft_oom` section, you need to enable soft oom killer
in rules, there you want that feature to be enabled:

```plaintext

database "db" {
    user "user" {
        ...
        enable_soft_oom yes
    }
}
```

After that, all new connections to `db.user` will be answered by Odyssey
itself process described in `soft_oom` reached limit by total memory consumption:

```bash
psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42'
psql: error: connection to server at "localhost" (::1), port 6432 failed: Connection refused
	Is the server running on that host and accepting TCP/IP connections?
connection to server at "localhost" (127.0.0.1), port 6432 failed: FATAL:  odyssey: cbde8d0203caf: soft out of memory ('postgres' uses 157771776 bytes)
```