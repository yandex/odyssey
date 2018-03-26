**shapito**

PostgreSQL protocol-level client C library.

Library is designed to provide most of the functionality needed to write or read
[PostgreSQL protocol messages](https://www.postgresql.org/docs/9.6/static/protocol.html).
Both Frontend (client to server) and Backend (server to client) messages are supported, making
it possible to write client or server simulation applications.

No network part is supported. Only buffer management and packet validation.

**PostgreSQL packet readers**

```C
/* Read initial message (StartupMessage, CancelRequest, SSLRequest) */
shapito_read_startup()

/* Read any other PostgreSQL packet */
shapito_read()
```

**FRONTEND**

**Write messages to Backend**

```C
/* StartupMessage */
shapito_fe_write_startup_message()

/* CancelRequest */
shapito_fe_write_cancel()

/* SSLRequest */
shapito_fe_write_ssl_request()

/* Terminate */
shapito_fe_write_terminate()

/* PasswordMessage */
shapito_fe_write_password()

/* Query */
shapito_fe_write_query()

/* Parse */
shapito_fe_write_parse()

/* Bind */
shapito_fe_write_bind()

/* Describe */
shapito_fe_write_describe();

/* Execute */
shapito_fe_write_execute();

/* Sync */
shapito_fe_write_sync();
```

**Read messages from Backend**

```C
/* ReadyForQuery */
shapito_fe_read_ready();

/* BackendKeyData */
shapito_fe_read_key();

/* Authentication messages */
shapito_fe_read_auth();

/* ParameterStatus */
shapito_fe_read_parameter();

/* ErrorResponse */
shapito_fe_read_error();
```

**BACKEND**

**Write messages to Frontend**

```C
/* ErrorResponse */
shapito_be_write_error()
shapito_be_write_error_fatal()
shapito_be_write_error_panic()

/* NoticeResponse */
shapito_be_write_notice()

/* AuthenticationOk */
shapito_be_write_authentication_ok()

/* AuthenticationCleartextPassword */
shapito_be_write_authentication_clear_text()

/* AuthenticationMD5Password */
shapito_be_write_authentication_md5()

/* BackendKeyData */
shapito_be_write_backend_key_data()

/* ParameterStatus */
shapito_be_write_parameter_status()

/* EmptyQueryResponse */
shapito_be_write_empty_query()

/* CommandComplete */
shapito_be_write_complete()

/* ReadyForQuery */
shapito_be_write_ready()

/* ParseComplete */
shapito_be_write_parse_complete()

/* BindComplete */
shapito_be_write_bind_complete()

/* PortalSuspended */
shapito_be_write_portal_suspended()

/* NoData */
shapito_be_write_no_data()

/* RowDescription */
shapito_be_write_row_description()
shapito_be_write_row_description_add()

/* DataRow */
shapito_be_write_data_row()
shapito_be_write_data_row_add()
```

**Read messages from Frontend**

```C
/* Read StartupMessage, CancelRequest or SSLRequest */
shapito_be_read_startup();

/* PasswordMessage */
shapito_be_read_password();
```
