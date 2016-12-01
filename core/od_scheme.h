#ifndef OD_SCHEME_H_
#define OD_SCHEME_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_schemeserver_t od_schemeserver_t;
typedef struct od_schemeroute_t od_schemeroute_t;
typedef struct od_scheme_t od_scheme_t;

typedef enum {
	OD_PUNDEF,
	OD_PSESSION,
	OD_PTRANSACTION
} od_pooling_t;

typedef enum {
	OD_RUNDEF,
	OD_RFORWARD
} od_routing_t;

struct od_schemeserver_t {
	int        id;
	char      *name;
	char      *host;
	int        port;
	int        is_default;
	od_list_t  link;
};

struct od_schemeroute_t {
	od_schemeserver_t *server;
	int                is_default;
	char              *target;
	char              *route;
	char              *database;
	char              *user;
	char              *password;
	int                ttl;
	int                client_max;
	int                pool_min;
	int                pool_max;
	od_list_t          link;
};

struct od_scheme_t {
	char             *config_file;
	int               server_id;
	/* main */
	int               daemonize;
	char             *log_file;
	char             *pid_file;
	int               syslog;
	char             *syslog_ident;
	char             *syslog_facility;
	char             *pooling;
	od_pooling_t      pooling_mode;
	/* listen */
	char             *host;
	int               port;
	int               backlog;
	int               nodelay;
	int               keepalive;
	int               workers;
	int               client_max;
	/* servers */
	od_list_t         servers;
	/* routing */
	char             *routing;
	od_routing_t      routing_mode;
	od_schemeroute_t *routing_default;
	od_list_t         routing_table;
};

void od_schemeinit(od_scheme_t*);
void od_schemefree(od_scheme_t*);
int  od_schemevalidate(od_scheme_t*, od_log_t*);
void od_schemeprint(od_scheme_t*, od_log_t*);

od_schemeserver_t*
od_schemeserver_add(od_scheme_t*);

od_schemeserver_t*
od_schemeserver_match(od_scheme_t*, char*);

od_schemeroute_t*
od_schemeroute_add(od_scheme_t*);

od_schemeroute_t*
od_schemeroute_match(od_scheme_t*, char*);

#endif
