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

The client counters distinguish three different client states, which is easy to get wrong:

| Column | Meaning |
| --- | --- |
| `cl_active` | Clients currently attached to a backend connection |
| `cl_waiting` | Clients holding a route but not attached to a backend, i.e. idle between transactions. This is **not** a sign of pool pressure |
| `cl_queue` | Clients blocked waiting for a backend connection to become free. A non-zero value means the pool is exhausted |
| `maxwait` | Whole seconds the longest-queued client has been waiting so far, `0` when nothing is queued |
| `maxwait_us` | Sub-second remainder of `maxwait`, in microseconds |

A client that starts to queue moves out of `cl_waiting` into `cl_queue`, so `cl_waiting`
can drop exactly when the pool becomes saturated. Use `cl_queue` and `maxwait` to detect
pool exhaustion.

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


## pause

Pause Odyssey execution. This will drop any session connections and
pause statement executions for transaction pools.

Can be used to reduce damage if you need to perform some operation
on Postgres

`pause`

## resume

Resume Odyssey statements execution.

`resume`
