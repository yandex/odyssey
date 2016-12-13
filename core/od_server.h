#ifndef OD_SERVER_H_
#define OD_SERVER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_server_t od_server_t;

typedef enum {
	OD_SUNDEF,
	OD_SIDLE,
	OD_SEXPIRE,
	OD_SCLOSE,
	OD_SCONNECT,
	OD_SRESET,
	OD_SACTIVE
} od_serverstate_t;

struct od_server_t {
	od_serverstate_t  state;
	so_stream_t       stream;
	mm_io_t           io;
	int               is_transaction;
	int64_t           count_request;
	int64_t           count_reply;
	int               idle_time;
	so_key_t          key;
	so_key_t          key_client;
	void             *route;
	void             *pooler;
	od_list_t         link;
};

static inline int
od_server_is_sync(od_server_t *s) {
	return s->count_request == s->count_reply;
}

static inline void
od_serverinit(od_server_t *s)
{
	s->state          = OD_SUNDEF;
	s->route          = NULL;
	s->io             = NULL;
	s->pooler         = NULL;
	s->idle_time      = 0;
	s->is_transaction = 0;
	s->count_request  = 0;
	s->count_reply    = 0;
	so_keyinit(&s->key);
	so_keyinit(&s->key_client);
	so_stream_init(&s->stream);
	od_listinit(&s->link);
}

static inline od_server_t*
od_serveralloc(void)
{
	od_server_t *s = malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	od_serverinit(s);
	return s;
}

static inline void
od_serverfree(od_server_t *s)
{
	so_stream_free(&s->stream);
	free(s);
}

#endif
