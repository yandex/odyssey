**shapito*

PostgreSQL protocol-level client C library.

**API REFERENCE**

**PostgreSQL packet validators**

```C
/* Validate StartupMessage, CancelRequest or SSLRequest */
so_read_startup()

/* Validate any other PostgreSQL packet */
so_read()
```

**Frontend to Backend messages**

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

```C
so_feread_ready();
so_feread_key();
so_feread_auth();
so_feread_parameter();
```

**Backend to Frontend messages**

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

```C
so_beread_startup();
so_beread_password();
```

**Resizable memory buffer**

```C
so_stream_init()
so_stream_free()
so_stream_size()
so_stream_used()
so_stream_left()
so_stream_reset()
so_stream_ensure()
so_stream_advance()
so_stream_write8()
so_stream_write16()
so_stream_write32()
so_stream_write()
so_stream_read8()
so_stream_read16()
so_stream_read32()
so_stream_readsz()
so_stream_read()
```
