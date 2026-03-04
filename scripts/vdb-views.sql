CREATE EXTENSION dblink;
CREATE SERVER odyssey FOREIGN DATA WRAPPER dblink_fdw OPTIONS (host 'localhost',
                                                               port '6432',
                                                               dbname 'console');

CREATE USER MAPPING FOR PUBLIC SERVER odyssey
    OPTIONS (user 'console' /*, password 'foobar' */);

ALTER USER MAPPING FOR PUBLIC SERVER odyssey
    OPTIONS (set user 'console' /*, set password 'foobarbaz' */);

DROP SCHEMA IF EXISTS odyssey CASCADE;
CREATE SCHEMA IF NOT EXISTS odyssey;

CREATE OR REPLACE VIEW odyssey.version AS
    SELECT * FROM dblink('odyssey', 'show version') AS
    _(
        version text
    );

CREATE OR REPLACE VIEW odyssey.clients AS
    SELECT * FROM dblink('odyssey', 'show clients') AS
    _(
        type text,
        "user" text,
        database text,
        state text,
        storage_user text,
        addr text,
        port int,
        local_addr text,
        local_port int,
        connect_time timestamp with time zone,
        request_time timestamp with time zone,
        wait int,
        wait_us int,
        id text,
        ptr text,
        coro int,
        remote_pid int,
        tls text
    );

CREATE OR REPLACE VIEW odyssey.databases AS
    SELECT * FROM dblink('odyssey', 'show databases') AS
    _(
        name text,
        host text,
        port int,
        database text,
        force_user text,
        pool_size int,
        reserve_pool int,
        pool_mode text,
        max_connections int,
        current_connections int,
        paused int,
        "disabled" int
    );

CREATE OR REPLACE VIEW odyssey.lists AS
    SELECT * FROM dblink('odyssey', 'show lists') AS
    _(
        list text,
        items int
    );

CREATE OR REPLACE VIEW odyssey.pools AS
    SELECT * FROM dblink('odyssey', 'show pools') AS
    _(
        database text,
        "user" text,
        cl_active int,
        cl_waiting int,
        sv_active int,
        sv_idle int,
        sv_used int,
        sv_tested int,
        sv_login int,
        maxwait int,
        maxwait_us int,
        pool_mode text
    );

CREATE OR REPLACE VIEW odyssey.servers AS
    SELECT * FROM dblink('odyssey', 'show servers') AS
    _(
        type text,
        "user" text,
        database text,
        state text,
        addr text,
        port int,
        local_addr text,
        local_port int,
        wait int,
        wait_us int,
        connect_time timestamp with time zone,
        request_time timestamp with time zone,
        ptr text,
        link text,
        remote_pid int,
        tls text,
        "offline" int
    );

CREATE OR REPLACE VIEW odyssey.pools_extended AS
    SELECT * FROM dblink('odyssey', 'show pools_extended') AS
    _(
        database text,
        "user" text,
        cl_active int,
        cl_waiting int,
        sv_active int,
        sv_idle int,
        sv_used int,
        sv_tested int,
        sv_login int,
        maxwait int,
        maxwait_us int,
        pool_mode text,
        bytes_received int,
        bytes_sent int,
        tcp_conn_count int
    );

CREATE OR REPLACE VIEW odyssey.listen AS
    SELECT * FROM dblink('odyssey', 'show listen') AS
    _(
        host text,
        port int,
        tls text,
        tls_cert_file text,
        tls_key_file text,
        tls_ca_file text,
        tls_protocols text
    );

CREATE OR REPLACE VIEW odyssey.errors AS
    SELECT * FROM dblink('odyssey', 'show errors') AS
    _(
        error_type text,
        count int
    );

CREATE OR REPLACE VIEW odyssey.errors_per_route AS
    SELECT * FROM dblink('odyssey', 'show errors_per_route') AS
    _(
        error_type text,
        "user" text,
        "database" text,
        count int
    );

CREATE OR REPLACE VIEW odyssey.storages AS
    SELECT * FROM dblink('odyssey', 'show storages') AS
    _(
        type text,
        host text,
        port int,
        tls text,
        tls_cert_file text,
        tls_key_file text,
        tls_ca_file text,
        tls_protocols text
    );

CREATE OR REPLACE VIEW odyssey.rules AS
    SELECT * FROM dblink('odyssey', 'show rules') AS
    _(
        "database" text,
        "user" text,
        address text,
        connection_type text,
        obsolete text
    );

CREATE OR REPLACE VIEW odyssey.host_utilization AS
    SELECT * FROM dblink('odyssey', 'show host_utilization') AS
    _(
        cpu float,
        mem float
    );

GRANT USAGE ON SCHEMA odyssey TO rkhapov;
GRANT USAGE ON FOREIGN SERVER odyssey TO rkhapov;
GRANT SELECT ON ALL TABLES IN SCHEMA odyssey TO rkhapov;