# Pooling with Odyssey

Connection pooling is essential for efficient use of PostgreSQL in
multi-user or high-load environments. Poolers manage and reuse database
connections, reducing connection overhead and improving application performance.
Without pooling, each client would establish its own connection, causing
resource exhaustion and degraded responsiveness. Using a connection pooler
ensures scalability, stability, and optimal database resource utilization.

Odyssey is _really good at pooling_ and allows you to create
large production setups.

Pooling is controled by rules in your configuration file.
For full pooling parameters see [rules configuration](../configuration/rules.md)

There are two types of pooling: session pooling and transaction pooling.

----

## Session pooling

Session pooling is appropriate when applications require a persistent
database connection for each client session, maintaining session-specific
settings or temporary tables. The main advantage is that each client keeps
the same backend connection for the duration of their session, ensuring
reliable session state. However, session pooling does not reduce the total
number of connections to PostgreSQL, which may lead to scalability issues
under high load. It is less efficient than transaction pooling in
environments with many short-lived queries.

To create session pooling for db.user write this in your configuration file:
```plaintext
database "db" {
    user "user" {
        ...
        pool "session"
    }
}
```

## Transaction pooling

Transaction pooling is ideal when applications do not rely on session-level
features and can work with any database connection for each transaction.
The main advantage is high scalability: the pooler can multiplex many client
connections over a smaller number of PostgreSQL backends, greatly reducing
resource usage. However, transaction pooling does not preserve session state
between transactions, so features like session variables or temporary tables
are not supported. This mode provides the best efficiency for stateless
workloads with short transactions.

To create transaction pooling for db.user write this in your configuration file:
```plaintext
database "db" {
    user "user" {
        ...
        pool "transaction"
    }
}
```

### Prepared statements support

As said, transaction pooling does not preserve session state between transactions,
so features like session variables or temporary tables are not supported.
But you can use prepared statements in transaction pooling.

To create transaction pooling with prepared statements support for db.user write this in your configuration file:
```plaintext
database "db" {
    user "user" {
        ...
        pool "transaction"
        pool_reserve_prepared_statement yes
    }
}
```