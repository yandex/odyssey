# Balancing load with Odyssey
*since 1.4.1*

Load balancing enables Odyssey to efficiently distribute incoming client
queries across multiple PostgreSQL servers. This improves resource utilization,
enhances throughput, and ensures high availability in large-scale deployments.
With load balancing, Odyssey manages the assignment of connections or requests,
helping to prevent any single server from becoming a bottleneck.

![Odyssey balancing](../img/balancing.svg "Odyssey balancing")

Odyssey transparently routes queries to available PostgreSQL backends
based on the selected balancing mode, helping to maximize performance
and reliability without requiring application-level logic changes.

----

## Configuration

You will simply need to specify several hosts in your storage section:
```plaintext
storage "postgres_server" {
	type "remote"
	host "pg-host-1:5432,pg-host-2:5432,pg-host-3:5432"
}
```

See [storage configuration guide](../configuration/storage.md)
for more about storage section.

## Host selection

Host is selected randomly, but with next priorities:

0. **Localhost**. Localhost endpoints will be tried first.
1. **Availability zone**. If `availability_zone` is set in [global section](../configuration/global.md), the endpoints with equals az will be tried second.
   This behaviour is enabled by default and can be turned off per balancing method with [`az_aware no`](../configuration/storage.md#az_aware).
2. **Target session attributes**. Works if [TSA](tsa.md) is specified in some way.
Endpoints with equals target session attributes will be tried third.

## Pool settings

The pools in unique per each endpoint in host, so if you set `pool_size` to 10,
you will get 10 pools per each host, and if there is 3 hosts, you will get 30
total connections.

## Per-listen override

The entire `balancing` section can be overridden per listen endpoint using
[`balancing`](../configuration/listen.md#balancing) in the `listen` section.
When set, it takes precedence over the storage-level `balancing` configuration,
while the round-robin counter and other shared state remain in the storage,
ensuring fair distribution across all listeners that reference the same
storage.

```
listen {
    host "*"
    port 6432
    balancing {
        method "leastconn" {
            az_aware no
        }
        show_notice_messages yes
    }
}
```