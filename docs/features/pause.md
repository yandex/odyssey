# Statements execution pausing

Sometimes you need to pause statements execution, ex: with Postgresql minor update.
This only possible for transactional pooling because the connections
to pg server will be reset anyway.

To stop Odyssey execution you will need [console configured](console.md)
and paused user must have [transactional pooling](pooling.md).

----

## Configuration

Lets assume you have next parts of your configuration:
```conf
...

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

...

database "postgres" {
    user "postgres" {
        authentication "none"
        pool "transactional"
        storage "postgres_server"
    }
}

...
```

## Pausing

Just execute:
```sh
psql 'host=localhost port=6432 user=console dbname=console' -c 'pause'
```

## How does it looks for client connections?

Just like currently transaction finished and new transactions sleeps until
Odyssey resumed. For:

```sh
pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' --progress 1
```

runs it looks like:

```plain
progress: 1.0 s, 13685.2 tps, lat 0.569 ms stddev 3.550, 0 failed
+ psql -h localhost -p 6432 -c pause -U console -d console
progress: 2.0 s, 0.0 tps, lat 0.000 ms stddev 0.000, 0 failed
progress: 3.0 s, 0.0 tps, lat 0.000 ms stddev 0.000, 0 failed
...
progress: 7.0 s, 0.0 tps, lat 0.000 ms stddev 0.000, 0 failed
+ psql -h localhost -p 6432 -c resume -U console -d console
progress: 8.0 s, 4165.3 tps, lat 17.347 ms stddev 344.607, 0 failed
progress: 9.0 s, 22651.8 tps, lat 0.441 ms stddev 0.066, 0 failed
progress: 10.0 s, 23471.7 tps, lat 0.425 ms stddev 0.053, 0 failed
```

## Resuming

Just execute:
```sh
psql 'host=localhost port=6432 user=console dbname=console' -c 'resume'
```

## State checking

Just execute:
```sh
psql 'host=localhost port=6432 user=console dbname=console' -c 'show is_paused'
```