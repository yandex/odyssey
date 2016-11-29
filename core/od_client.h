#ifndef OD_CLIENT_H_
#define OD_CLIENT_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odclient_t odclient_t;

struct odclient_t {
	mm_io_t         io;
	so_bestartup_t  startup;
	so_key_t        key;
	so_stream_t     stream;
	odserver_t     *server;
	void           *pooler;
	uint64_t        id;
	odlist_t        link;
};

static inline void
od_clientinit(odclient_t *c)
{
	c->id = 0;
	c->io = NULL;
	c->server = NULL;
	c->pooler = NULL;
	so_bestartup_init(&c->startup);
	so_keyinit(&c->key);
	so_stream_init(&c->stream);
	od_listinit(&c->link);
}

static inline odclient_t*
od_clientalloc(void)
{
	odclient_t *c = malloc(sizeof(*c));
	if (c == NULL)
		return NULL;
	od_clientinit(c);
	return c;
}

static inline void
od_clientfree(odclient_t *c)
{
	so_bestartup_free(&c->startup);
	so_stream_free(&c->stream);
	free(c);
}

#endif
