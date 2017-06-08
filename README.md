shapito
--------

PostgreSQL protocol-level client C library.

**API REFERENCE*

**PostgreSQL packet validators**

```C
so_read_startup()
so_read()
```

**Frontend to Backend messages**

```C
so_fewrite_startup_message()
so_fewrite_cancel()
so_fewrite_ssl_request()
so_fewrite_terminate()
so_fewrite_password()
so_fewrite_query()
so_fewrite_parse()
so_fewrite_bind()
so_fewrite_describe();
so_fewrite_execute();
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
so_bewrite_error()
so_bewrite_error_fatal()
so_bewrite_error_panic()
so_bewrite_notice()
so_bewrite_authentication_ok()
so_bewrite_authentication_clear_text()
so_bewrite_authentication_md5()
so_bewrite_backend_key_data()
so_bewrite_parameter_status()
so_bewrite_empty_query()
so_bewrite_complete()
so_bewrite_ready()
so_bewrite_parse_complete()
so_bewrite_bind_complete()
so_bewrite_portal_suspended()
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
