#ifndef OD_ROUTER_H_
#define OD_ROUTER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef enum {
	OD_RS_UNDEF,
	OD_RS_SERVER_EROUTE,
	OD_RS_SERVER_EPOP,
	OD_RS_SERVER_EREAD,
	OD_RS_SERVER_EWRITE,
	OD_RS_CLIENT_EXIT,
	OD_RS_CLIENT_EREAD,
	OD_RS_CLIENT_EWRITE
} odrouter_status_t;

void od_router(void*);

#endif
