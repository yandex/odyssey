#ifndef OD_CLIENT_H_
#define OD_CLIENT_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odclient_t odclient_t;

struct odclient_t {
	ftio_t       io;
	odserver_t  *server;
	odlist_t     link;
};

static inline void
od_clientinit(odclient_t *c)
{
	c->io = NULL;
	c->server = NULL;
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
	free(c);
}

#endif
