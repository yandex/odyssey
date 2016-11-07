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
	char *host;
	int   port;
};

struct odscheme_route_t {
	char *database;
	char *user;
	int   client_max;
	int   pool_size_min;
	int   pool_size_max;
};

struct odscheme_t {
	/* main */
	int       daemonize;
	char     *log_file;
	char     *pid_file;
	char     *pooling;
	/* listen */
	char     *host;
	int       port;
	int       workers;
	int       connection_max;
	/* servers */
	odlist_t  servers;
	/* routing */
	char     *routing;
	odlist_t  routing_table;
};

void od_schemeinit(odscheme_t*);
void od_schemefree(odscheme_t*);

#endif
