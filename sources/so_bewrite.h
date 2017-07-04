#ifndef SO_BEWRITE_H_
#define SO_BEWRITE_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

static inline int
so_bewrite_error_as(so_stream_t *stream, char *severity, char *code,
                    char *message, int len)
{
	int size = 1 /* S */ + 6 +
	           1 /* C */ + 6 +
	           1 /* M */ + len + 1 +
	           1 /* zero */;
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'E');
	so_stream_write32(stream, sizeof(uint32_t) + size);
	so_stream_write8(stream, 'S');
	so_stream_write(stream, severity, 6);
	so_stream_write8(stream, 'C');
	so_stream_write(stream, code, 6);
	so_stream_write8(stream, 'M');
	so_stream_write(stream, message, len);
	so_stream_write8(stream, 0);
	so_stream_write8(stream, 0);
	return 0;
}

static inline int
so_bewrite_error(so_stream_t *stream, char *code, char *message, int len)
{
	return so_bewrite_error_as(stream, "ERROR", code, message, len);
}

static inline int
so_bewrite_error_fatal(so_stream_t *stream, char *code, char *message, int len)
{
	return so_bewrite_error_as(stream, "FATAL", code, message, len);
}

static inline int
so_bewrite_error_panic(so_stream_t *stream, char *code, char *message, int len)
{
	return so_bewrite_error_as(stream, "PANIC", code, message, len);
}

static inline int
so_bewrite_notice(so_stream_t *stream, char *message, int len)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + len + 1);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'N');
	so_stream_write32(stream, sizeof(uint32_t) + len);
	so_stream_write(stream, message, len);
	so_stream_write8(stream, 0);
	return 0;
}

static inline int
so_bewrite_authentication_ok(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'R');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t));
	so_stream_write32(stream, 0);
	return 0;
}

static inline int
so_bewrite_authentication_clear_text(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'R');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t));
	so_stream_write32(stream, 3);
	return 0;
}

static inline int
so_bewrite_authentication_md5(so_stream_t *stream, char salt[4])
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint32_t) + 4);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'R');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t) + 4);
	so_stream_write32(stream, 5);
	so_stream_write(stream, salt, 4);
	return 0;
}

static inline int
so_bewrite_backend_key_data(so_stream_t *stream, uint32_t pid, uint32_t key)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) +
	                          sizeof(uint32_t) +
	                          sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'K');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t) +
	                  sizeof(uint32_t));
	so_stream_write32(stream, pid);
	so_stream_write32(stream, key);
	return 0;
}

static inline int
so_bewrite_parameter_status(so_stream_t *stream, char *key, int key_len,
                            char *value, int value_len)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + key_len + value_len);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'S');
	so_stream_write32(stream, sizeof(uint32_t) + key_len + value_len);
	so_stream_write(stream, key, key_len);
	so_stream_write(stream, value, value_len);
	return 0;
}

static inline int
so_bewrite_ready(so_stream_t *stream, uint8_t status)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint8_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'Z');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint8_t));
	so_stream_write8(stream, status);
	return 0;
}

static inline int
so_bewrite_row_description(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint16_t));
	if (so_unlikely(rc == -1))
		return -1;
	int position = so_stream_used(stream);
	so_stream_write8(stream, 'T');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint16_t));
	so_stream_write16(stream, 0);
	return position;
}

static inline int
so_bewrite_row_description_add(so_stream_t *stream, int start,
                               char *name, int name_len,
                               int32_t table_id,
                               int16_t attrnum,
                               int32_t type_id,
                               int16_t type_size,
                               int32_t type_modifier,
                               int32_t format_code)
{
	int size = name_len + 1 +
	           sizeof(uint32_t) /* table_id */+
	           sizeof(uint16_t) /* attrnum */ +
	           sizeof(uint32_t) /* type_id */ +
	           sizeof(uint16_t) /* type_size */ +
	           sizeof(uint32_t) /* type_modifier */ +
	           sizeof(uint16_t) /* format_code */;
	int rc = so_stream_ensure(stream, size);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write(stream, name, name_len);
	so_stream_write8(stream, 0);
	so_stream_write32(stream, table_id);
	so_stream_write16(stream, attrnum);
	so_stream_write32(stream, type_id);
	so_stream_write16(stream, type_size);
	so_stream_write32(stream, type_modifier);
	so_stream_write16(stream, format_code);

	so_header_t *header;
	header = (so_header_t*)(stream->start + start);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	char *pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	so_stream_read32(&total_size, &pos, &pos_size);
	so_stream_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	so_stream_write32to((char*)&header->len, total_size);
	so_stream_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

static inline int
so_bewrite_data_row(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + sizeof(uint16_t));
	if (so_unlikely(rc == -1))
		return -1;
	int position = so_stream_used(stream);
	so_stream_write8(stream, 'D');
	so_stream_write32(stream, sizeof(uint32_t) + sizeof(uint16_t));
	so_stream_write16(stream, 0);
	return position;
}

static inline int
so_bewrite_data_row_add(so_stream_t *stream, int start, char *data, int32_t len)
{
	int is_null = len == -1;
	int size = sizeof(uint32_t) + (is_null) ? 0 : len;
	int rc = so_stream_ensure(stream, size);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write32(stream, len);
	if (! is_null)
		so_stream_write(stream, data, len);

	so_header_t *header;
	header = (so_header_t*)(stream->start + start);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	char *pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	so_stream_read32(&total_size, &pos, &pos_size);
	so_stream_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	so_stream_write32to((char*)&header->len, total_size);
	so_stream_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

static inline int
so_bewrite_complete(so_stream_t *stream, char *message, int len)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t) + len);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'C');
	so_stream_write32(stream, sizeof(uint32_t) + len);
	so_stream_write(stream, message, len);
	return 0;
}

static inline int
so_bewrite_empty_query(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'I');
	so_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_parse_complete(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, '1');
	so_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_bind_complete(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, '2');
	so_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_portal_suspended(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 's');
	so_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_no_data(so_stream_t *stream)
{
	int rc = so_stream_ensure(stream, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(stream, 'n');
	so_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

#endif
