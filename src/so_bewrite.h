#ifndef SO_BEWRITE_H_
#define SO_BEWRITE_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

static inline int
so_bewrite_error(so_stream_t *buf, char *message, int len)
{
	int rc = so_stream_ensure(buf, sizeof(so_header_t) + len + 1);
	if (so_unlikely(rc == -1))
		return -1;
	so_stream_write8(buf, 'E');
	so_stream_write32(buf, sizeof(uint32_t) + len);
	so_stream_write(buf, (uint8_t*)message, len);
	so_stream_write8(buf, 0);
	return 0;
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

/* row description */
/* datarow */

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
