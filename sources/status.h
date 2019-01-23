#ifndef ODYSSEY_STATUS_H
#define ODYSSEY_STATUS_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef enum
{
	OD_UNDEF,
	OD_OK,
	OD_SKIP,
	OD_DETACH,
	OD_STOP,
	OD_EOOM,
	OD_EATTACH,
	OD_ESERVER_CONNECT,
	OD_ESERVER_READ,
	OD_ESERVER_WRITE,
	OD_ECLIENT_READ,
	OD_ECLIENT_WRITE
} od_status_t;

#endif /* ODYSSEY_STATUS_H */
