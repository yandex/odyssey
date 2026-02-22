# Shared pools support
*since 1.5.1*

Sometimes, you may need to limit the number of connections for several users at once.
A good example is splitting routes for application connections and those from human users, such as DBAs.
The latter never connect simultaneously, so it's appropriate to limit their connections to a low number
to avoid consuming too much of the `max_connections` quota.


You cannot achieve this with [general rules](../configuration/rules.md), as each rule has its own pool.
Instead, you need a shared pool.

----

## Configuration

Suppose you have several users that you want to limit together: `db1.dba1`, `db1.dba2`, `db2.dba1`, and `db2.dba2`.
You will need to define a `shared_pool` in the global section of your configuration file,
then reference it by name in your rules. Here's an example:

```plain
shared_pool "idm" {
    pool_size 3
}

database "db1" {
    user "dba1" {
        shared_pool "idm"
        ...
    }

    user "dba2" {
        shared_pool "idm"
        ...
    }
}

database "db1" {
    user "dba1" {
        shared_pool "idm"
        ...
    }

    user "dba3" {
        shared_pool "idm"
        ...
    }
}
```

With this configuration, only 3 connections are available from those routes, and all others
will be waiting, as per the standard rules.
