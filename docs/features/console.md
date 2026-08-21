# Odyssey console

Odyssey supports administration console, that are configuraed like
'local' [storage](../configuration/storage.md).

This console allows to connect to Odyssey with `psql` like usual
and performs some 'admin' operations or retrieve some metrics/statistics.

----

## Configuration

You will need to create a local storage and rule for accessing the console.
Odyssey configuration to create an ability for console connections can look like:
```conf
storage "local" {
    type "local"
}

database "console" {
    user "console" {
        authentication "none"
        role "admin"
        pool "session"
        storage "local"
    }
}
```

After that you can connect and execute commands in console with any postgresql client:
```sh
psql -h localhost -p 6432 -U console -d console -c "show clients"

psql -h localhost -p 6432 -U console -d console -c "show servers"

psql -h localhost -p 6432 -U console -d console -c "pause"
```

## kill_client

Drop connection with specified client.

`kill_client c123dfsdfg2`

## reload

Reload Odyssey configuration. Can be used to set some Odyssey parameters
without restarting Odyssey.

`reload`

## help

Writes available commands to execute in console.

`help`

## show ...

### show clients

Writes list of currently connected clients.

`show clients`

### show servers

Writes list of currently connected servers.

`show servers`

### show server_prep_stmts

Writes list of currently allocated prepared statements.

`show server_prep_stmts`

### show pools

Write information about currently allocated pools for every database.user

`show pools`

### show pools_extended

Write even more information about currently allocated pools for every database.user

`show pools_extended`

### show storages

Write information about current storages that are used to connect to PostgreSQL

`show storages`

### show version

Write Odyssey version

`show version`

### show listen

Show list of currently listened addresses

`show listen`

### show is_paused

Show if Odyssey is paused or not

`show is_paused`

### show errors

Show statistics about currently appeared errors

`show errors`

### show databases

Show info about databases

`show databases`


### show host utilization

Show info about host resources utilization, in percentages

```plain
console=> show host_utilization;
  cpu  |  mem  
-------+-------
 13.37 | 15.77
```


### show config

Reports the global configuration of the running process.

`show config`

```plain
console=> show config;
             key             | value | default | changeable
-----------------------------+-------+---------+------------
 workers                     | 4     | 1       | no
 log_query                   | yes   | no      | yes
 ...
```

| Column | Meaning |
| --- | --- |
| `key` | parameter name, as accepted by the configuration parser |
| `value` | value the running process is using right now |
| `default` | built-in value used when the parameter is absent from the configuration file |
| `changeable` | whether `RELOAD` applies a new value without a restart |

Reading `odyssey.conf` tells you what was asked for, not what is in
effect. `RELOAD` applies only part of the global configuration; the rest
keeps the value the process started with, and no error is reported. The
`changeable` column tells the two apart, so comparing `value` against the
file is enough to spot a parameter that needs a restart.

Notes:

- `default` is the built-in initial value. A few parameters are derived
  from others when left unset - `client_max_routing`, for instance,
  becomes `workers * 64` - so `value` can differ from `default` even
  though the configuration file does not mention the parameter.
- Values longer than 255 characters are truncated in the output.
- Per-route settings are not reported here, see `show rules`. Listen
  sockets are reported by `show listen`, storages by `show storages`.

## pause

Pause Odyssey execution. This will drop any session connections and
pause statement executions for transaction pools.

Can be used to reduce damage if you need to perform some operation
on Postgres

`pause`

## resume

Resume Odyssey statements execution.

`resume`
