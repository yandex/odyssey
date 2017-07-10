#ifndef SHAPITO_FE_WRITE_H
#define SHAPITO_FE_WRITE_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_fe_arg shapito_fe_arg_t;

struct shapito_fe_arg
{
	char *name;
	int   len;
};

SHAPITO_API static inline int
shapito_fe_write_startup_message(shapito_stream_t *stream, int argc, shapito_fe_arg_t *argv)
{
	int size = sizeof(uint32_t) + /* len */
	           sizeof(uint32_t) + /* version */
	           sizeof(uint8_t);   /* last '\0' */
	int i = 0;
	for (; i < argc; i++)
		size += argv[i].len;
	int rc = shapito_stream_ensure(stream, size);
	if (shapito_unlikely(rc == -1))
		return -1;
	/* len */
	shapito_stream_write32(stream, size);
	/* version */
	shapito_stream_write32(stream, 196608);
	/* arguments */
	for (i = 0; i < argc; i++)
		shapito_stream_write(stream, argv[i].name, argv[i].len);
	/* eof */
	shapito_stream_write8(stream, 0);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_cancel(shapito_stream_t *stream, uint32_t pid, uint32_t key)
{
	int size = sizeof(uint32_t) + /* len */
	           sizeof(uint32_t) + /* special */
	           sizeof(uint32_t) + /* pid */
	           sizeof(uint32_t);  /* key */
	int rc = shapito_stream_ensure(stream, size);
	if (shapito_unlikely(rc == -1))
		return -1;
	/* len */
	shapito_stream_write32(stream, size);
	/* special */
	shapito_stream_write32(stream, 80877102);
	/* pid */
	shapito_stream_write32(stream, pid);
	/* key */
	shapito_stream_write32(stream, key);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_ssl_request(shapito_stream_t *stream)
{
	int size = sizeof(uint32_t) + /* len */
	           sizeof(uint32_t);  /* special */
	int rc = shapito_stream_ensure(stream, size);
	if (shapito_unlikely(rc == -1))
		return -1;
	/* len */
	shapito_stream_write32(stream, size);
	/* special */
	shapito_stream_write32(stream, 80877103);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_terminate(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'X');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_password(shapito_stream_t *stream, char *password, int len)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + len);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'p');
	shapito_stream_write32(stream, sizeof(uint32_t) + len);
	shapito_stream_write(stream, password, len);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_query(shapito_stream_t *stream, char *query, int len)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + len);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'Q');
	shapito_stream_write32(stream, sizeof(uint32_t) + len);
	shapito_stream_write(stream, query, len);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_parse(shapito_stream_t *stream,
                       char *operator_name, int operator_len,
                       char *query, int query_len,
                       uint16_t typec, int *typev)
{
	uint32_t size = operator_len + query_len + sizeof(uint16_t) +
	                typec * sizeof(uint32_t);
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'P');
	shapito_stream_write32(stream, sizeof(uint32_t) + size);
	shapito_stream_write(stream, operator_name, operator_len);
	shapito_stream_write(stream, query, query_len);
	shapito_stream_write16(stream, typec);
	int i = 0;
	for (; i < typec; i++)
		shapito_stream_write32(stream, typev[i]);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_bind(shapito_stream_t *stream,
                      char *portal_name, int portal_len,
                      char *operator_name, int operator_len,
                      int argc_call_types, int call_types[],
                      int argc_result_types, int result_types[],
                      int argc, int *argv_len, char **argv)
{
	int size = portal_len + operator_len +
	           sizeof(uint16_t) +                     /* argc_call_types */
	           sizeof(uint16_t) * argc_call_types +   /* call_types */
	           sizeof(uint16_t) +                     /* argc_result_types */
	           sizeof(uint16_t) * argc_result_types + /* result_types */
	           sizeof(uint16_t);                      /* argc */
	int i = 0;
	for (; i < argc; i++) {
		size += sizeof(uint32_t);
		if (argv_len[i] == -1)
			continue;
		size += argv_len[i];
	}
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'B');
	shapito_stream_write32(stream, sizeof(uint32_t) + size);
	shapito_stream_write(stream, portal_name, portal_len);
	shapito_stream_write(stream, operator_name, operator_len);

	shapito_stream_write16(stream, argc_call_types);
	for (i = 0; i < argc_call_types; i++)
		shapito_stream_write16(stream, call_types[i]);

	shapito_stream_write16(stream, argc);
	for (i = 0; i < argc; i++) {
		shapito_stream_write32(stream, argv_len[i]);
		if (argv_len[i] == -1)
			continue;
		shapito_stream_write(stream, argv[i], argv_len[i]);
	}

	shapito_stream_write16(stream, argc_result_types);
	for (i = 0; i < argc_result_types; i++)
		shapito_stream_write16(stream, result_types[i]);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_describe(shapito_stream_t *stream, uint8_t type, char *name, int name_len)
{
	int size = sizeof(type) + name_len;
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'D');
	shapito_stream_write32(stream, sizeof(uint32_t) + size);
	shapito_stream_write8(stream, type);
	shapito_stream_write(stream, name, name_len);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_execute(shapito_stream_t *stream, char *portal, int portal_len, uint32_t limit)
{
	int size = portal_len + sizeof(limit);
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t) + size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'E');
	shapito_stream_write32(stream, sizeof(uint32_t) + size);
	shapito_stream_write(stream, portal, portal_len);
	shapito_stream_write32(stream, limit);
	return 0;
}

SHAPITO_API static inline int
shapito_fe_write_sync(shapito_stream_t *stream)
{
	int rc = shapito_stream_ensure(stream, sizeof(shapito_header_t));
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_stream_write8(stream, 'S');
	shapito_stream_write32(stream, sizeof(uint32_t));
	return 0;
}

#endif /* SHAPITO_FE_WRITE_H */
