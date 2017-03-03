#ifndef SO_PARAM_H_
#define SO_PARAM_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_param_t     so_param_t;
typedef struct so_paramlist_t so_paramlist_t;

struct so_param_t {
	uint32_t name_len;
	uint32_t value_len;
	char data[];
};

struct so_paramlist_t {
	so_stream_t buf;
};

static inline char*
so_param_name(so_param_t *param) {
	return param->data;
}

static inline char*
so_param_value(so_param_t *param) {
	return param->data + param->name_len;
}

static inline void
so_paramlist_init(so_paramlist_t *list)
{
	so_stream_init(&list->buf);
}

static inline void
so_paramlist_free(so_paramlist_t *list)
{
	so_stream_free(&list->buf);
}

int so_paramlist_add(so_paramlist_t*, uint8_t*, uint32_t,
                     uint8_t*, uint32_t);

#endif
