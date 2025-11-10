# Prometheus metrics

This section describes ways to export Odyssey metrics in Prometheus format.

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
      --odyssey.scrape-timeout=5s
                                 Maximum duration for a single `/metrics` scrape (inherits the HTTP request context; use 0s to disable).
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

Each `/metrics` request now inherits the HTTP context and uses `--odyssey.scrape-timeout` (default `5s`) to bound how long the exporter waits for `SHOW ...` commands. If the timeout or client cancellation fires, outstanding queries are aborted and the scrape returns `odyssey_exporter_up 0` along with any metrics collected before the interruption.

## Route-level metrics

The Go-based exporter scrapes `SHOW POOLS_EXTENDED;` and `SHOW DATABASES;` and now emits **only label-based metrics** (legacy `odyssey_pool_<db>_<user>_*` names have been removed). Each route sample is keyed by the `user` and `database` labels:

| Metric | Labels | Type | Description |
| --- | --- | --- | --- |
| `odyssey_client_pool_active_route` | `user`, `database` | Gauge | Clients currently using the route. |
| `odyssey_client_pool_waiting_route` | `user`, `database` | Gauge | Clients blocked waiting for a server connection. |
| `odyssey_client_pool_maxwait_seconds_route` | `user`, `database` | Gauge | Maximum observed wait in seconds. |
| `odyssey_server_pool_capacity_configured_route` | `user`, `database` | Gauge | Configured `pool_size` from `SHOW DATABASES` (`0` means unlimited). When Odyssey doesn’t expose the mapping for a route, the exporter falls back to the observed `active+idle` at scrape time. |
| `odyssey_route_pool_mode_info` | `user`, `database`, `mode` | Gauge | `1` for the active pool mode (`session`, `transaction`, `statement`). |
| `odyssey_route_bytes_received_total` | `user`, `database` | Counter | Bytes received from clients on the route. |
| `odyssey_route_bytes_sent_total` | `user`, `database` | Counter | Bytes sent to PostgreSQL backends. |
| `odyssey_route_tcp_connections_total` | `user`, `database` | Counter | TCP connections opened toward the backend. |
| `odyssey_route_query_duration_seconds` | `user`, `database`, `quantile` | Gauge | Query latency quantiles (available when the `quantiles` rule option is set). |
| `odyssey_route_transaction_duration_seconds` | `user`, `database`, `quantile` | Gauge | Transaction latency quantiles. |

Saturation examples:

```
odyssey_server_pool_state_route{state="active"} / odyssey_server_pool_capacity_configured_route
```

If `capacity_configured_route == 0` (unlimited), prefer absolute values or derive current load directly from the unified family, for example:

```
sum by (user, database) (odyssey_server_pool_state_route{state=~"active|idle"})
```

Quantiles (`*_duration_seconds`) are instantaneous TDigest estimates; treat thresholds like gauges (for example, `odyssey_route_query_duration_seconds{quantile="0.95"} > 0.5`).

## Database-level averages

Odyssey’s cron thread still computes the historical averages that fed the legacy Prometheus endpoint. The Go exporter re-exposes the most in-demand gauges:

- `odyssey_database_avg_tx_per_second{database="<db>"}` — average transactions per second observed over the most recent `stats_interval` window.
- `odyssey_database_avg_query_per_second{database="<db>"}` — same concept for queries (per-second throughput).
- `odyssey_database_avg_recv_bytes_per_second{database="<db>"}` — average bytes/sec received from clients (SHOW STATS `avg_recv`).
- `odyssey_database_avg_sent_bytes_per_second{database="<db>"}` — average bytes/sec sent to servers (SHOW STATS `avg_sent`).
- `odyssey_database_avg_query_time_seconds{database="<db>"}` — average query latency (seconds) over the stats window (SHOW STATS `avg_query_time`).
- `odyssey_database_avg_xact_time_seconds{database="<db>"}` — average transaction latency (seconds) (SHOW STATS `avg_xact_time`).
- `odyssey_database_avg_wait_time_seconds{database="<db>"}` — average wait time for server (seconds) (SHOW STATS `avg_wait_time`).

## Error counters

`SHOW ERRORS;` is exported as a single counter family: `odyssey_errors_total{type="OD_ECLIENT_READ"}`. Every error type reported by Odyssey becomes a label value, so new error codes do not require exporter changes.

## Legacy built in support

Not supported anymore. See example of usage in [docker/prometheus-legacy/](https://github.com/yandex/odyssey/tree/master/docker/prometheus-legacy/)
