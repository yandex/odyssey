#ifndef OD_LINK_H_
#define OD_LINK_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_link_t od_link_t;

struct od_link_t {
	od_client_t       *client;
	int                server_is_active;
	od_server_t       *server;
	od_routerstatus_t  rc;
};

static inline void
od_linkinit(od_link_t *link, od_client_t *client, od_server_t *server)
{
	link->client           = client;
	link->server_is_active = 1;
	link->server           = server;
	link->rc               = OD_RS_UNDEF;
}

static inline void
od_link_break(od_link_t *link, od_routerstatus_t status)
{
	link->rc = status;
}

static inline int
od_link_isbroken(od_link_t *link)
{
	return link->rc != OD_RS_UNDEF;
}

#endif
