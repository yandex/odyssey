#ifndef OD_SCHEME_H
#define OD_SCHEME_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_schemestorage od_schemestorage_t;
typedef struct od_schemeroute   od_schemeroute_t;
typedef struct od_scheme        od_scheme_t;

typedef enum
{
	OD_LOGFORMAT_TEXT,
	OD_LOGFORMAT_TSKV
} od_logformat_t;

typedef enum
{
	OD_SREMOTE,
	OD_SLOCAL
} od_storagetype_t;

typedef enum
{
	OD_PSESSION,
	OD_PTRANSACTION
} od_pooling_t;

typedef enum
{
	OD_AUNDEF,
	OD_ABLOCK,
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
	char             *tls;
	char             *tls_ca_file;
	char             *tls_key_file;
	char             *tls_cert_file;
	char             *tls_protocols;
	od_list_t         link;
};

struct od_schemeroute
{
	/* id */
	char               *db_name;
	int                 db_name_len;
	int                 db_is_default;
	char               *user_name;
	int                 user_name_len;
	int                 user_is_default;
	int                 is_obsolete;
	int                 version;
	int                 refs;
	/* auth */
	char               *auth;
	od_auth_t           auth_mode;
	/* password */
	char               *password;
	int                 password_len;
	/* storage */
	od_schemestorage_t *storage;
	char               *storage_name;
	char               *storage_db;
	char               *storage_user;
	int                 storage_user_len;
	char               *storage_password;
	int                 storage_password_len;
	/* pool */
	od_pooling_t        pool;
	char               *pool_sz;
	int                 pool_size;
	int                 pool_timeout;
	int                 pool_ttl;
	int                 pool_cancel;
	int                 pool_discard;
	int                 pool_rollback;
	/* limits */
	int                 client_max_set;
	int                 client_max;
	od_list_t           link;
};

struct od_scheme
{
	/* main */
	int            daemonize;
	int            log_stdout;
	int            log_debug;
	int            log_config;
	int            log_session;
	char          *log_file;
	char          *log_format_name;
	od_logformat_t log_format;
	int            log_statistics;
	char          *pid_file;
	int            syslog;
	char          *syslog_ident;
	char          *syslog_facility;
	int            readahead;
	int            server_pipelining;
	/* listen */
	char          *host;
	int            port;
	int            backlog;
	int            nodelay;
	int            keepalive;
	int            workers;
	int            client_max_set;
	int            client_max;
	od_tls_t       tls_verify;
	char          *tls;
	char          *tls_ca_file;
	char          *tls_key_file;
	char          *tls_cert_file;
	char          *tls_protocols;
	/* temprorary storages */
	od_list_t      storages;
	/* routes */
	od_list_t      routes;
};

void od_scheme_init(od_scheme_t*);
void od_scheme_free(od_scheme_t*);
int  od_scheme_validate(od_scheme_t*, od_logger_t*);
void od_scheme_print(od_scheme_t*, od_logger_t*, int);
int  od_scheme_merge(od_scheme_t*, od_logger_t*, od_scheme_t*);

od_schemestorage_t*
od_schemestorage_add(od_scheme_t*);

od_schemestorage_t*
od_schemestorage_match(od_scheme_t*, char*);

static inline void
od_schemeroute_ref(od_schemeroute_t *route)
{
	route->refs++;
}

static inline void
od_schemeroute_unref(od_schemeroute_t *route)
{
	assert(route->refs > 0);
	route->refs--;
}

od_schemeroute_t*
od_schemeroute_add(od_scheme_t*, int);

void od_schemeroute_free(od_schemeroute_t*);

od_schemeroute_t*
od_schemeroute_forward(od_scheme_t*, char*, char*);

od_schemeroute_t*
od_schemeroute_match(od_scheme_t*, char*, char*);

od_schemeroute_t*
od_schemeroute_match_latest(od_scheme_t*, char*, char*);

int od_schemeroute_compare(od_schemeroute_t*, od_schemeroute_t*);

#endif /* OD_SCHEME_H */
