#ifndef OD_SCHEME_H_
#define OD_SCHEME_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odscheme_server_t odscheme_server_t;
typedef struct odscheme_route_t odscheme_route_t;
typedef struct odscheme_t odscheme_t;

typedef enum {
	OD_PUNDEF,
	OD_PSESSION,
	OD_PSTATEMENT,
	OD_PTRANSACTION
} odpooling_t;

typedef enum {
	OD_RUNDEF,
	OD_RFORWARD,
	OD_RROUND_ROBIN,
} odrouting_t;

struct odscheme_server_t {
	int       id;
	char     *name;
	char     *host;
	int       port;
	int       is_default;
	odlist_t  link;
};

struct odscheme_route_t {
	odscheme_server_t *server;
	char              *target;
	char              *route;
	char              *database;
	char              *user;
	char              *password;
	int                client_max;
	int                pool_min;
	int                pool_max;
	odlist_t           link;
};

struct odscheme_t {
	char        *config_file;
	int          server_id;
	/* main */
	int          daemonize;
	char        *log_file;
	char        *pid_file;
	char        *pooling;
	odpooling_t  pooling_mode;
	/* listen */
	char        *host;
	int          port;
	int          workers;
	int          client_max;
	/* servers */
	odlist_t     servers;
	/* routing */
	char        *routing;
	odrouting_t  routing_mode;
	odlist_t     routing_table;
};

void od_schemeinit(odscheme_t*);
void od_schemefree(odscheme_t*);
int  od_schemevalidate(odscheme_t*, odlog_t*);
void od_schemeprint(odscheme_t*, odlog_t*);

odscheme_server_t*
od_schemeserver_add(odscheme_t*);

odscheme_server_t*
od_schemeserver_match(odscheme_t*, char*);

odscheme_route_t*
od_schemeroute_add(odscheme_t*);

odscheme_route_t*
od_schemeroute_match(odscheme_t*, char*);

#endif
