#ifndef SO_BEWRITE_H_
#define SO_BEWRITE_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

static inline int
so_bewrite_error_as(so_stream_t *buf, char *severity, char *code,
                    char *message, int len)
{
	int size = 1 /* S */ + 6 +
	           1 /* C */ + 6 +
	           1 /* M */ + len + 1 +
	           1 /* zero */;
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'E');
	so_stream_write32(buf, sizeof(uint32_t) + size);
	so_stream_write8(buf, 'S');
	so_stream_write(buf, (uint8_t*)severity, 6);
	so_stream_write8(buf, 'C');
	so_stream_write(buf, (uint8_t*)code, 6);
	so_stream_write8(buf, 'M');
	so_stream_write(buf, (uint8_t*)message, len);
	so_stream_write8(buf, 0);
	so_stream_write8(buf, 0);
	return 0;
}

static inline int
so_bewrite_error(so_stream_t *buf, char *code, char *message, int len)
{
	return so_bewrite_error_as(buf, "ERROR", code, message, len);
}

static inline int
so_bewrite_error_fatal(so_stream_t *buf, char *code, char *message, int len)
{
	return so_bewrite_error_as(buf, "FATAL", code, message, len);
}

static inline int
so_bewrite_error_panic(so_stream_t *buf, char *code, char *message, int len)
{
	return so_bewrite_error_as(buf, "PANIC", code, message, len);
}

static inline int
so_bewrite_notice(so_stream_t *buf, char *message, int len)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + len + 1);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'N');
	so_stream_write32(buf, sizeof(uint32_t) + len);
	so_stream_write(buf, (uint8_t*)message, len);
	so_stream_write8(buf, 0);
	return 0;
}

static inline int
so_bewrite_authentication_ok(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'R');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint32_t));
	so_stream_write32(buf, 0);
	return 0;
}

static inline int
so_bewrite_authentication_clear_text(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'R');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint32_t));
	so_stream_write32(buf, 3);
	return 0;
}

static inline int
so_bewrite_authentication_md5(so_stream_t *buf, uint8_t salt[4])
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + sizeof(uint32_t) + 4);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'R');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint32_t) + 4);
	so_stream_write32(buf, 5);
	so_stream_write(buf, salt, 4);
	return 0;
}

static inline int
so_bewrite_backend_key_data(so_stream_t *buf, uint32_t pid, uint32_t key)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) +
	                      sizeof(uint32_t) +
	                      sizeof(uint32_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'K');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint32_t) +
	              sizeof(uint32_t));
	so_stream_write32(buf, pid);
	so_stream_write32(buf, key);
	return 0;
}

static inline int
so_bewrite_parameter_status(so_stream_t *buf, char *key, int key_len,
                            char *value, int value_len)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + key_len + value_len);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'S');
	so_stream_write32(buf, sizeof(uint32_t) + key_len + value_len);
	so_stream_write(buf, (uint8_t*)key, key_len);
	so_stream_write(buf, (uint8_t*)value, value_len);
	return 0;
}

static inline int
so_bewrite_ready(so_stream_t *buf, uint8_t status)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + sizeof(uint8_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'Z');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint8_t));
	so_stream_write8(buf, status);
	return 0;
}

static inline int
so_bewrite_row_description(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + sizeof(uint16_t));
	if (so_unlikely(rc == -1))
		return -1;
	int position = so_stream_used(buf);
	so_stream_write8(buf, 'T');
	so_stream_write32(buf, sizeof(uint32_t) + sizeof(uint16_t));
	so_stream_write16(buf, 0);
	return position;
}

static inline int
so_bewrite_row_description_add(so_stream_t *buf, int start,
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
	int rc = so_stream_ensure(buf, size);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write(buf, (uint8_t*)name, name_len);
	so_stream_write8(buf, 0);
	so_stream_write32(buf, table_id);
	so_stream_write16(buf, attrnum);
	so_stream_write32(buf, type_id);
	so_stream_write16(buf, type_size);
	so_stream_write32(buf, type_modifier);
	so_stream_write16(buf, format_code);

	so_header_t *header;
	header = (so_header_t*)(buf->s + start);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	uint8_t *pos = (uint8_t*)&header->len;
	uint32_t total_size;
	uint16_t count;
	so_stream_read32(&total_size, &pos, &pos_size);
	so_stream_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	so_stream_write32to((uint8_t*)&header->len, total_size);
	so_stream_write16to((uint8_t*)&header->len + sizeof(uint32_t), count);
	return 0;
}

static inline int
so_bewrite_complete(so_stream_t *buf, char *message, int len)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + len);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'C');
	so_stream_write32(buf, sizeof(uint32_t) + len);
	so_stream_write(buf, (uint8_t*)message, len);
	return 0;
}

static inline int
so_bewrite_empty_query(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'I');
	so_stream_write32(buf, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_parse_complete(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, '1');
	so_stream_write32(buf, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_bind_complete(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, '2');
	so_stream_write32(buf, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_portal_suspended(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 's');
	so_stream_write32(buf, sizeof(uint32_t));
	return 0;
}

static inline int
so_bewrite_no_data(so_stream_t *buf)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'n');
	so_stream_write32(buf, sizeof(uint32_t));
	return 0;
}

#endif
