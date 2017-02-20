#ifndef OD_CLIENT_H_
#define OD_CLIENT_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_client_t od_client_t;

typedef enum {
	OD_CUNDEF,
	OD_CACTIVE,
	OD_CQUEUE
} od_clientstate_t;

struct od_client_t {
	od_clientstate_t state;
	mm_io_t          io;
	od_schemeuser_t *scheme;
	so_bestartup_t   startup;
	so_key_t         key;
	so_stream_t      stream;
	od_server_t     *server;
	void            *route;
	void            *pooler;
	uint64_t         id;
	uint64_t         id_fiber;
	od_list_t        link_pool;
	od_list_t        link;
};

static inline void
od_clientinit(od_client_t *c)
{
	c->state = OD_CUNDEF;
	c->io = NULL;
	c->scheme = NULL;
	c->id = 0;
	c->id_fiber = 0;
	c->server = NULL;
	c->route = NULL;
	c->pooler = NULL;
	so_bestartup_init(&c->startup);
	so_keyinit(&c->key);
	so_stream_init(&c->stream);
	od_listinit(&c->link_pool);
	od_listinit(&c->link);
}

static inline od_client_t*
od_clientalloc(void)
{
	od_client_t *c = malloc(sizeof(*c));
	if (c == NULL)
		return NULL;
	od_clientinit(c);
	return c;
}

static inline void
od_clientfree(od_client_t *c)
{
	so_bestartup_free(&c->startup);
	so_stream_free(&c->stream);
	free(c);
}

#endif
