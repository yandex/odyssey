#ifndef OD_SCHEME_H
#define OD_SCHEME_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_schemestorage od_schemestorage_t;
typedef struct od_schemeroute   od_schemeroute_t;
typedef struct od_schemeuser    od_schemeuser_t;
typedef struct od_scheme        od_scheme_t;

typedef enum
{
	OD_SREMOTE,
	OD_SLOCAL
} od_storagetype_t;

typedef enum
{
	OD_PUNDEF,
	OD_PSESSION,
	OD_PTRANSACTION
} od_pooling_t;

typedef enum
{
	OD_AUNDEF,
	OD_ANONE,
	OD_ACLEAR_TEXT,
	OD_AMD5
} od_auth_t;

typedef enum
{
	OD_TDISABLE,
	OD_TALLOW,
	OD_TREQUIRE,
	OD_TVERIFY_CA,
	OD_TVERIFY_FULL
} od_tls_t;

struct od_schemestorage
{
	char             *name;
	char             *type;
	od_storagetype_t  storage_type;
	char             *host;
	int               port;
	od_tls_t          tls_verify;
	char             *tls_mode;
	char             *tls_ca_file;
	char             *tls_key_file;
	char             *tls_cert_file;
	char             *tls_protocols;
	int               is_default;
	od_list_t         link;
};

struct od_schemeroute
{
	int                 is_default;
	char               *target;
	char               *route;
	char               *database;
	char               *user;
	int                 user_len;
	char               *password;
	int                 password_len;
	int                 cancel;
	int                 discard;
	int                 rollback;
	int                 client_max_set;
	int                 client_max;
	int                 pool_size;
	int                 pool_timeout;
	int                 pool_ttl;
	od_schemestorage_t *storage;
	od_list_t           link;
};

struct od_schemeuser
{
	char      *auth;
	od_auth_t  auth_mode;
	char      *user;
	char      *password;
	int        password_len;
	int        is_default;
	int        is_deny;
	od_list_t  link;
};

struct od_scheme
{
	char             *config_file;
	/* main */
	int               daemonize;
	int               log_debug;
	int               log_config;
	char             *log_file;
	char             *pid_file;
	int               syslog;
	char             *syslog_ident;
	char             *syslog_facility;
	int               stats_period;
	int               readahead;
	int               server_pipelining;
	char             *pooling;
	od_pooling_t      pooling_mode;
	/* listen */
	char             *host;
	int               port;
	int               backlog;
	int               nodelay;
	int               keepalive;
	int               workers;
	int               client_max_set;
	int               client_max;
	od_tls_t          tls_verify;
	char             *tls_mode;
	char             *tls_ca_file;
	char             *tls_key_file;
	char             *tls_cert_file;
	char             *tls_protocols;
	/* storages */
	od_list_t         storages;
	/* routing */
	od_schemeroute_t *routing_default;
	od_list_t         routing_table;
	/* users */
	od_list_t         users;
	od_schemeuser_t  *users_default;
};

void od_scheme_init(od_scheme_t*);
void od_scheme_free(od_scheme_t*);
int  od_scheme_validate(od_scheme_t*, od_log_t*);
void od_scheme_print(od_scheme_t*, od_log_t*);

od_schemestorage_t*
od_schemestorage_add(od_scheme_t*);

od_schemestorage_t*
od_schemestorage_match(od_scheme_t*, char*);

od_schemeroute_t*
od_schemeroute_add(od_scheme_t*);

od_schemeroute_t*
od_schemeroute_match(od_scheme_t*, char*);

od_schemeuser_t*
od_schemeuser_add(od_scheme_t*);

od_schemeuser_t*
od_schemeuser_match(od_scheme_t*, char*);

#endif /* OD_SCHEME_H */
