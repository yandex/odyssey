**shapito**

PostgreSQL protocol-level client C library.

Library is designed to provide most of the functionality needed to write or read
[PostgreSQL protocol messages](https://www.postgresql.org/docs/9.6/static/protocol.html).
Both Frontend (client to server) and Backend (server to client) messages are supported, making
it possible to write client or server simulation applications.

No network part is supported. Only buffer management and packet validation.

**PostgreSQL packet readers**

```C
/* Read StartupMessage, CancelRequest or SSLRequest */
so_read_startup()

/* Read any other PostgreSQL packet */
so_read()
```

**FRONTEND**

**Write messages to Backend**

```C
/* StartupMessage */
so_fewrite_startup_message()

/* CancelRequest */
so_fewrite_cancel()

/* SSLRequest */
so_fewrite_ssl_request()

/* Terminate */
so_fewrite_terminate()

/* PasswordMessage */
so_fewrite_password()

/* Query */
so_fewrite_query()

/* Parse */
so_fewrite_parse()

/* Bind */
so_fewrite_bind()

/* Describe */
so_fewrite_describe();

/* Execute */
so_fewrite_execute();

/* Sync */
so_fewrite_sync();
```

**Read messages from Backend**

```C
so_feread_ready();
so_feread_key();
so_feread_auth();
so_feread_parameter();
```

**BACKEND**

**Write messages to Frontend**

```C
/* ErrorResponse */
so_bewrite_error()
so_bewrite_error_fatal()
so_bewrite_error_panic()

/* NoticeResponse */
so_bewrite_notice()

/* AuthenticationOk */
so_bewrite_authentication_ok()

/* AuthenticationCleartextPassword */
so_bewrite_authentication_clear_text()

/* AuthenticationMD5Password */
so_bewrite_authentication_md5()

/* BackendKeyData */
so_bewrite_backend_key_data()

/* ParameterStatus */
so_bewrite_parameter_status()

/* EmptyQueryResponse */
so_bewrite_empty_query()

/* CommandComplete */
so_bewrite_complete()

/* ReadyForQuery */
so_bewrite_ready()

/* ParseComplete */
so_bewrite_parse_complete()

/* BindComplete */
so_bewrite_bind_complete()

/* PortalSuspended */
so_bewrite_portal_suspended()

/* NoData */
so_bewrite_no_data()
```

**Read messages from Frontend**

```C
so_beread_startup();
so_beread_password();
```
