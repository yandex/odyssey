#ifndef OD_SCHEME_H
#define OD_SCHEME_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_schemestorage od_schemestorage_t;
typedef struct od_schemedb      od_schemedb_t;
typedef struct od_schemeuser    od_schemeuser_t;
typedef struct od_scheme        od_scheme_t;

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

struct od_schemedb
{
	char            *name;
	od_list_t        users;
	od_schemeuser_t *user_default;
	int              is_default;
	int              is_obsolete;
	int              refs;
	int              version;
	od_list_t        link;
};

struct od_schemeuser
{
	od_schemedb_t      *db;
	/* user */
	char               *user;
	int                 user_len;
	char               *user_password;
	int                 user_password_len;
	/* auth */
	char               *auth;
	od_auth_t           auth_mode;
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
	int                 is_default;
	od_list_t           link;
};

struct od_scheme
{
	/* main */
	int            daemonize;
	int            log_debug;
	int            log_config;
	int            log_session;
	char          *log_file;
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
	/* storages */
	od_list_t      storages;
	/* db */
	od_list_t      dbs;
	od_schemedb_t *db_default;
};

static inline void
od_schemedb_ref(od_schemedb_t *db) {
	db->refs++;
}

static inline void
od_schemedb_unref(od_schemedb_t *db) {
	assert(db->refs > 0);
	db->refs--;
}

void od_scheme_init(od_scheme_t*);
void od_scheme_free(od_scheme_t*);
int  od_scheme_validate(od_scheme_t*, od_log_t*);
void od_scheme_print(od_scheme_t*, od_log_t*);
void od_scheme_merge(od_scheme_t*, od_log_t*, od_scheme_t*);

od_schemestorage_t*
od_schemestorage_add(od_scheme_t*);

od_schemestorage_t*
od_schemestorage_match(od_scheme_t*, char*);

od_schemedb_t*
od_schemedb_add(od_scheme_t*, int);

void od_schemedb_free(od_schemedb_t*);

od_schemedb_t*
od_schemedb_match(od_scheme_t*, char*, int);

od_schemeuser_t*
od_schemeuser_add(od_schemedb_t*);

od_schemeuser_t*
od_schemeuser_match(od_schemedb_t*, char*);

#endif /* OD_SCHEME_H */
