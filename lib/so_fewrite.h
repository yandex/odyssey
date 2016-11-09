#ifndef SO_FEWRITE_H_
#define SO_FEWRITE_H_

/*
 * sonata.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct sofearg_t sofearg_t;

struct sofearg_t {
	char *name;
	int len;
};

static inline int
so_fewrite_startup_message(sobuf_t *buf, int argc, sofearg_t *argv)
{
	int size = sizeof(uint32_t) + /* len */
	           sizeof(uint32_t) + /* version */
	           sizeof(uint8_t);   /* last '\0' */
	int i = 0;
	for (; i < argc; i++)
		size += argv[i].len;
	int rc = so_bufensure(buf, size);
	if (so_unlikely(rc == -1))
		return -1;
	/* len */
	so_bufwrite32(buf, size);
	/* version */
	so_bufwrite32(buf, 196608);
	/* arguments */
	for (i = 0; i < argc; i++)
		so_bufwrite(buf, (uint8_t*)argv[i].name, argv[i].len);
	/* eof */
	so_bufwrite8(buf, 0);
	return 0;
}

static inline int
so_fewrite_cancel(sobuf_t *buf, uint32_t pid, uint32_t key)
{
	int size = sizeof(uint32_t) + /* len */
	           sizeof(uint32_t) + /* special */
	           sizeof(uint32_t) + /* pid */
	           sizeof(uint32_t);  /* key */
	int rc = so_bufensure(buf, size);
	if (so_unlikely(rc == -1))
		return -1;
	/* len */
	so_bufwrite32(buf, size);
	/* special */
	so_bufwrite32(buf, 80877102);
	/* pid */
	so_bufwrite32(buf, pid);
	/* key */
	so_bufwrite32(buf, key);
	return 0;
}

static inline int
so_fewrite_terminate(sobuf_t *buf)
{
	int rc = so_bufensure(buf, sizeof(soheader_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'X');
	so_bufwrite32(buf, sizeof(uint32_t));
	return 0;
}

static inline int
so_fewrite_password(sobuf_t *buf, char *password, int len)
{
	int rc = so_bufensure(buf, sizeof(soheader_t) + len);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'p');
	so_bufwrite32(buf, sizeof(uint32_t) + len);
	so_bufwrite(buf, (uint8_t*)password, len);
	return 0;
}

static inline int
so_fewrite_query(sobuf_t *buf, char *query, int len)
{
	int rc = so_bufensure(buf, sizeof(soheader_t) + len);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'Q');
	so_bufwrite32(buf, sizeof(uint32_t) + len);
	so_bufwrite(buf, (uint8_t*)query, len);
	return 0;
}

static inline int
so_fewrite_parse(sobuf_t *buf,
                 char *operator_name, int operator_len,
                 char *query, int query_len,
                 uint16_t typec, int *typev)
{
	uint32_t size = operator_len + query_len + sizeof(uint16_t) +
	                typec * sizeof(uint32_t);
	int rc = so_bufensure(buf, sizeof(soheader_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'P');
	so_bufwrite32(buf, sizeof(uint32_t) + size);
	so_bufwrite(buf, (uint8_t*)operator_name, operator_len);
	so_bufwrite(buf, (uint8_t*)query, query_len);
	so_bufwrite16(buf, typec);
	int i = 0;
	for (; i < typec; i++)
		so_bufwrite32(buf, typev[i]);
	return 0;
}

static inline int
so_fewrite_bind(sobuf_t *buf,
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
	int rc = so_bufensure(buf, sizeof(soheader_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'B');
	so_bufwrite32(buf, sizeof(uint32_t) + size);
	so_bufwrite(buf, (uint8_t*)portal_name, portal_len);
	so_bufwrite(buf, (uint8_t*)operator_name, operator_len);

	so_bufwrite16(buf, argc_call_types);
	for (i = 0; i < argc_call_types; i++)
		so_bufwrite16(buf, call_types[i]);

	so_bufwrite16(buf, argc);
	for (i = 0; i < argc; i++) {
		so_bufwrite32(buf, argv_len[i]);
		if (argv_len[i] == -1)
			continue;
		so_bufwrite(buf, (uint8_t*)argv[i], argv_len[i]);
	}

	so_bufwrite16(buf, argc_result_types);
	for (i = 0; i < argc_result_types; i++)
		so_bufwrite16(buf, result_types[i]);
	return 0;
}

static inline int
so_fewrite_describe(sobuf_t *buf, uint8_t type, char *name, int name_len)
{
	int size = sizeof(type) + name_len;
	int rc = so_bufensure(buf, sizeof(soheader_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'D');
	so_bufwrite32(buf, sizeof(uint32_t) + size);
	so_bufwrite8(buf, type);
	so_bufwrite(buf, (uint8_t*)name, name_len);
	return 0;
}

static inline int
so_fewrite_execute(sobuf_t *buf, char *portal, int portal_len, uint32_t limit)
{
	int size = portal_len + sizeof(limit);
	int rc = so_bufensure(buf, sizeof(soheader_t) + size);
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'E');
	so_bufwrite32(buf, sizeof(uint32_t) + size);
	so_bufwrite(buf, (uint8_t*)portal, portal_len);
	so_bufwrite32(buf, limit);
	return 0;
}

static inline int
so_fewrite_sync(sobuf_t *buf)
{
	int rc = so_bufensure(buf, sizeof(soheader_t));
	if (so_unlikely(rc == -1))
		return -1;
	so_bufwrite8(buf, 'S');
	so_bufwrite32(buf, sizeof(uint32_t));
	return 0;
}

#endif
