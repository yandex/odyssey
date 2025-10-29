# Prometheus metrics

This section describes a ways to export Odyssey metrics in Prometheus format.

----

## Exporter

There is metrics exporter in [prometheus/exporter](https://github.com/yandex/odyssey/blob/master/prometheus/exporter/).
This is an http server, that connects to Odyssey console and expose it metrics
in Prometheus format on `/metrics` endpoint on specified address.

To use it, you will need build and run:
```plain
> go mod download && go build -o odyssey-prom-exporter

> ./odyssey-prom-exporter -h
usage: odyssey-prom-exporter [<flags>]


Flags:
  -h, --[no-]help                Show context-sensitive help (also try --help-long and --help-man).
      --odyssey.connectionString="host=localhost port=6432 user=console dbname=console sslmode=disable"  
                                 Connection string for accessing Odyssey.
      --[no-]web.systemd-socket  Use systemd socket activation listeners instead of port listeners (Linux only).
      --web.listen-address=:9876 ...  
                                 Addresses on which to expose metrics and web interface. Repeatable for multiple addresses. Examples: `:9100` or `[::1]:9100` for http,
                                 `vsock://:9100` for vsock
      --web.config.file=""       Path to configuration file that can enable TLS or authentication. See:
                                 https://github.com/prometheus/exporter-toolkit/blob/master/docs/web-configuration.md
      --[no-]version             Show application version.
```

Currently in developing stage, so if you have any troubles
with this exporter, please, [contact us](../about/contributing.md).

## Route-level gauges

Starting with the Go-based exporter, Odyssey exposes per-route server pool gauges that pair `SHOW POOLS_EXTENDED;` with `SHOW DATABASES;` to keep track of the configured capacity:

- `odyssey_server_pool_active_route{user="<user>",database="<db>"}` — number of active backend connections serving the route.
- `odyssey_server_pool_idle_route{user="<user>",database="<db>"}` — number of idle backend connections kept ready for that route.
- `odyssey_server_pool_capacity_route{user="<user>",database="<db>"}` — configured `pool_size` for the route, falling back to the current pool size when it is unlimited.

You can monitor saturation by comparing these gauges, for example:

```
odyssey_server_pool_active_route / odyssey_server_pool_capacity_route
```

Values close to `1` indicate the route is exhausting the available server connections.

## Legacy built in support

Not supported anymore. See example of usage in [docker/prometheus-legacy/](https://github.com/yandex/odyssey/tree/master/docker/prometheus-legacy/)
