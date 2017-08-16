#ifndef SHAPITO_BE_WRITE_H
#define SHAPITO_BE_WRITE_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

SHAPITO_API static inline int
shapito_be_write_error_as(shapito_stream_t *stream, char *severity, char *code,
                          char *message, int len)
{
	int size = 1 /* S */ + 6 +
	           1 /* C */ + 6 +
	           1 /* M */ + len + 1 +
	           1 /* zero */;
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'E');
	shapito_stream_write32(stream, sizeof(uint32_t) + size);
	shapito_stream_write8(stream, 'S');
	shapito_stream_write(stream, severity, 6);
	shapito_stream_write8(stream, 'C');
	shapito_stream_write(stream, code, 6);
	shapito_stream_write8(stream, 'M');
	shapito_stream_write(stream, message, len);
	shapito_stream_write8(stream, 0);
	shapito_stream_write8(stream, 0);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_error(shapito_stream_t *stream, char *code, char *message, int len)
{
	return shapito_be_write_error_as(stream, "ERROR", code, message, len);
}

SHAPITO_API static inline int
shapito_be_write_error_fatal(shapito_stream_t *stream, char *code, char *message, int len)
{
	return shapito_be_write_error_as(stream, "FATAL", code, message, len);
}

SHAPITO_API static inline int
shapito_be_write_error_panic(shapito_stream_t *stream, char *code, char *message, int len)
{
	return shapito_be_write_error_as(stream, "PANIC", code, message, len);
}

SHAPITO_API static inline int
shapito_be_write_notice(shapito_stream_t *stream, char *message, int len)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + len + 1);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'N');
	shapito_stream_write32(stream, sizeof(uint32_t) + len);
	shapito_stream_write(stream, message, len);
	shapito_stream_write8(stream, 0);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_authentication_ok(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint32_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'R');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t));
	shapito_stream_write32(stream, 0);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_authentication_clear_text(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint32_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'R');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t));
	shapito_stream_write32(stream, 3);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_authentication_md5(shapito_stream_t *stream, char salt[4])
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint32_t) + 4);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'R');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t) + 4);
	shapito_stream_write32(stream, 5);
	shapito_stream_write(stream, salt, 4);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_backend_key_data(shapito_stream_t *stream, uint32_t pid, uint32_t key)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) +
	                               sizeof(uint32_t) +
	                               sizeof(uint32_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'K');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint32_t) +
	                       sizeof(uint32_t));
	shapito_stream_write32(stream, pid);
	shapito_stream_write32(stream, key);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_parameter_status(shapito_stream_t *stream, char *key, int key_len,
                                  char *value, int value_len)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + key_len + value_len);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'S');
	shapito_stream_write32(stream, sizeof(uint32_t) + key_len + value_len);
	shapito_stream_write(stream, key, key_len);
	shapito_stream_write(stream, value, value_len);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_ready(shapito_stream_t *stream, uint8_t status)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint8_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'Z');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint8_t));
	shapito_stream_write8(stream, status);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_row_description(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint16_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	int position = shapito_stream_used(stream);
	shapito_stream_write8(stream, 'T');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint16_t));
	shapito_stream_write16(stream, 0);
	return position;
}

SHAPITO_API static inline int
shapito_be_write_row_description_add(shapito_stream_t *stream, int start,
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
	int rc = shapito_stream_ensure(stream, size);
	if (shapito_unlikely(rc == -1))
		return -1;

	shapito_stream_write(stream, name, name_len);
	shapito_stream_write8(stream, 0);
	shapito_stream_write32(stream, table_id);
	shapito_stream_write16(stream, attrnum);
	shapito_stream_write32(stream, type_id);
	shapito_stream_write16(stream, type_size);
	shapito_stream_write32(stream, type_modifier);
	shapito_stream_write16(stream, format_code);

	shapito_header_t *header;
	header = (shapito_header_t*)(stream->start + start);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	char *pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	shapito_stream_read32(&total_size, &pos, &pos_size);
	shapito_stream_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	shapito_stream_write32to((char*)&header->len, total_size);
	shapito_stream_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_row_descriptionf(shapito_stream_t *stream, char *fmt, ...)
{
	int offset;
	offset = shapito_be_write_row_description(stream);
	if (shapito_unlikely(offset == -1))
		return -1;
	va_list args;
	va_start(args, fmt);
	while (*fmt) {
		char *name = va_arg(args, char*);
		int   name_len = strlen(name);
		int rc;
		switch (*fmt) {
		case 's':
			name = va_arg(args, char*);
			rc = shapito_be_write_row_description_add(stream, offset, name, name_len,
			                                          0, 0, 20, -1, 0, 0);
			break;
		case 'd':
			rc = shapito_be_write_row_description_add(stream, offset, name, name_len,
			                                          0, 0, 23, 4, 0, 0);
			break;
		case 'l':
			rc = shapito_be_write_row_description_add(stream, offset, name, name_len,
			                                          0, 0, 25, 8, 0, 0);
			break;
		default:
			va_end(args);
			return -1;
		}
		if (rc == -1)
			return -1;
		fmt++;
	}
	va_end(args);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_data_row(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + sizeof(uint16_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	int position = shapito_stream_used(stream);
	shapito_stream_write8(stream, 'D');
	shapito_stream_write32(stream, sizeof(uint32_t) + sizeof(uint16_t));
	shapito_stream_write16(stream, 0);
	return position;
}

SHAPITO_API static inline int
shapito_be_write_data_row_add(shapito_stream_t *stream, int start, char *data, int32_t len)
{
	int is_null = len == -1;
	int size = sizeof(uint32_t) + (is_null ? 0 : len);
	int rc = shapito_stream_ensure(stream, size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write32(stream, len);
	if (! is_null)
		shapito_stream_write(stream, data, len);

	shapito_header_t *header;
	header = (shapito_header_t*)(stream->start + start);
	uint32_t pos_size = sizeof(uint32_t) + sizeof(uint16_t);
	char *pos = (char*)&header->len;
	uint32_t total_size;
	uint16_t count;
	shapito_stream_read32(&total_size, &pos, &pos_size);
	shapito_stream_read16(&count, &pos, &pos_size);
	total_size += size;
	count++;
	shapito_stream_write32to((char*)&header->len, total_size);
	shapito_stream_write16to((char*)&header->len + sizeof(uint32_t), count);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_complete(shapito_stream_t *stream, char *message, int len)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + len);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'C');
	shapito_stream_write32(stream, sizeof(uint32_t) + len);
	shapito_stream_write(stream, message, len);
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_empty_query(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'I');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_parse_complete(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, '1');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_bind_complete(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, '2');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_portal_suspended(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 's');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

SHAPITO_API static inline int
shapito_be_write_no_data(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'n');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

#endif /* SHAPITO_BE_WRITE_H */
