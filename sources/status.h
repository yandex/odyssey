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
	OD_ATTACH,
	OD_DETACH,
	OD_STOP,
	OD_EOOM,
	OD_EATTACH,
	OD_EATTACH_TOO_MANY_CONNECTIONS,
	OD_ESERVER_CONNECT,
	OD_ESERVER_READ,
	OD_ESERVER_WRITE,
	OD_ECLIENT_READ,
	OD_ECLIENT_WRITE
} od_status_t;

static inline char *
od_status_to_str(od_status_t status){
	switch (status)
	{
		case OD_UNDEF:
			return "OD_UNDEF";
		case OD_OK:
			return "OD_OK";
		case OD_SKIP:
			return "OD_SKIP";
		case OD_ATTACH:
			return "OD_UNDEF";
		case OD_DETACH:
			return "OD_DETACH";
		case OD_STOP:
			return "OD_STOP";
		case OD_EOOM:
			return "OD_EOOM";
		case OD_EATTACH:
			return "OD_EATTACH";
		case OD_EATTACH_TOO_MANY_CONNECTIONS:
			return "OD_EATTACH_TOO_MANY_CONNECTIONS";
		case OD_ESERVER_CONNECT:
			return "OD_ESERVER_CONNECT";
		case OD_ESERVER_READ:
			return "OD_ESERVER_READ";
		case OD_ESERVER_WRITE:
			return "OD_ESERVER_WRITE";
		case OD_ECLIENT_READ:
			return "OD_ECLIENT_READ";
		case OD_ECLIENT_WRITE:
			return "OD_ECLIENT_WRITE";
	}
	return "unkonown";
}

#endif /* ODYSSEY_STATUS_H */
