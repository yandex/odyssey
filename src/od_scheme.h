#ifndef OD_SCHEME_H_
#define OD_SCHEME_H_

/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

typedef struct odscheme_server_t odscheme_server_t;
typedef struct odscheme_route_t odscheme_route_t;
typedef struct odscheme_t odscheme_t;

struct odscheme_server_t {
	char     *name;
	char     *host;
	int       port;
	int       is_default;
	odlist_t  link;
};

struct odscheme_route_t {
	char     *database;
	char     *user;
	char     *password;
	int       client_max;
	int       pool_min;
	int       pool_max;
	odlist_t  link;
};

struct odscheme_t {
	char     *config_file;
	/* main */
	int       daemonize;
	char     *log_file;
	char     *pid_file;
	char     *pooling;
	/* listen */
	char     *host;
	int       port;
	int       workers;
	int       client_max;
	/* servers */
	odlist_t  servers;
	/* routing */
	char     *routing;
	odlist_t  routing_table;
};

void od_schemeinit(odscheme_t*);
void od_schemefree(odscheme_t*);

odscheme_server_t*
od_scheme_addserver(odscheme_t*);
odscheme_route_t*
od_scheme_addroute(odscheme_t*);

void od_schemeprint(odscheme_t*, odlog_t*);

#endif
