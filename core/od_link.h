#ifndef OD_LINK_H_
#define OD_LINK_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odlink_t odlink_t;

struct odlink_t {
	odclient_t        *client;
	int                server_is_active;
	odserver_t        *server;
	int64_t            nrequest;
	int64_t            nreply;
	odrouter_status_t  rc;
};

static inline void
od_linkinit(odlink_t *link, odclient_t *client, odserver_t *server)
{
	link->client           = client;
	link->server_is_active = 1;
	link->server           = server;
	link->nrequest         = 0;
	link->nreply           = 0;
	link->rc               = OD_RS_UNDEF;
}

static inline void
od_link_break(odlink_t *link, odrouter_status_t status)
{
	link->rc = status;
}

static inline int
od_link_isbroken(odlink_t *link)
{
	return link->rc != OD_RS_UNDEF;
}

#endif
