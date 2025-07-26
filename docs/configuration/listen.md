# Listen section

Listen section defines listening servers used for accepting
incoming client connections.

It is possible to define several Listen sections. Odyssey will listen on
every specified address.

Odyssey will fail in case it could not bind on any resolved address.

---

## **host**
*string*

If host is not set, Odyssey will try to listen using UNIX socket if
`unix_socket_dir` is set.

Parse strings that looks like `(type://)?[address](:port)?(:availability_zone)?`

`host "*"`

Examples:
* `klg-hostname.com`
* `[klg-hostname.com]:1337`
* `[klg-hostname.com]:klg`
* `[klg-hostname.com]:1337:klg`
* `klg-hostname.com:1337:klg`
* `[klg-hostname.com]:klg:1337`
* `klg-hostname.com,vla-hostname.com`
* `klg-hostname.com,[vla-hostname.com]:31337`
* `[klg-hostname.com]:1337:klg,[vla-hostname.com]:31337:vla`
* 
* `tcp://localhost:1337`
* `unix:///var/lib/postgresql/.s.PGSQL.5432`
* `tcp://localhost:1337,unix:///var/lib/postgresql/.s.PGSQL.5432`

## **port**
*integer*

Default port value if that one is not specified in host string.

`port 6432`

## **backlog**
*integer*

`backlog 128`

## **tls**
*string*

Supported TLS modes:

```
"disable"     - disable TLS protocol
"allow"       - switch to TLS protocol on request
"require"     - TLS clients only
"verify_ca"   - require valid client certificate
"verify_full" - require valid client ceritifcate
```

## **compression**
*yes|no*

Support of PostgreSQL protocol compression (experimental). Set to 'yes' to enable, disabled by default.

`compression no`

## **target_session_attrs**
*string*

Target session attrs feature. Odyssey will lookup for primary/standby
for connection on this listen, depending on value set.
Possible values are:

- read-write - always select host, available for write
- read-only - never select host, available for write
- any (the default one) - select host randomly

Odyssey will traverse hosts of storage and execute pg_is_in_recovery against them, to check if
host is primary or not.

`target_session_attrs "read-write"`

## **client_login_timeout**
*integer*

Prevent client stall during routing for more that client_login_timeout milliseconds.
Defaults to 15000.

## example

```
listen {
    host "*"
    port 6432
    backlog 16
    tls "allow"
    tls_cert_file "/etc/odyssey/ssl/server.crt"
    tls_key_file "/etc/odyssey/ssl/server.key"
    tls_ca_file "/etc/odyssey/ssl/allCAs.pem"
    tls_protocols "tlsv1.2"
}
```
