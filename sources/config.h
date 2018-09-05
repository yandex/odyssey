#ifndef ODYSSEY_CONFIG_H
#define ODYSSEY_CONFIG_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_config_storage od_config_storage_t;
typedef struct od_config_route   od_config_route_t;
typedef struct od_config_listen  od_config_listen_t;
typedef struct od_config_auth    od_config_auth_t;
typedef struct od_config         od_config_t;

typedef enum
{
	OD_AUTH_UNDEF,
	OD_AUTH_NONE,
	OD_AUTH_BLOCK,
	OD_AUTH_CLEAR_TEXT,
	OD_AUTH_MD5,
	OD_AUTH_CERT
} od_auth_t;

typedef enum
{
	OD_TLS_DISABLE,
	OD_TLS_ALLOW,
	OD_TLS_REQUIRE,
	OD_TLS_VERIFY_CA,
	OD_TLS_VERIFY_FULL
} od_tls_t;

typedef enum
{
	OD_POOL_TYPE_SESSION,
	OD_POOL_TYPE_TRANSACTION
} od_pool_type_t;

typedef enum
{
	OD_STORAGE_TYPE_REMOTE,
	OD_STORAGE_TYPE_LOCAL
} od_storage_type_t;

struct od_config_storage
{
	char              *name;
	char              *type;
	od_storage_type_t  storage_type;
	char              *host;
	int                port;
	od_tls_t           tls_mode;
	char              *tls;
	char              *tls_ca_file;
	char              *tls_key_file;
	char              *tls_cert_file;
	char              *tls_protocols;
	od_list_t          link;
};

struct od_config_auth
{
	char      *common_name;
	od_list_t  link;
};

struct od_config_route
{
	/* id */
	char                *db_name;
	int                  db_name_len;
	int                  db_is_default;
	char                *user_name;
	int                  user_name_len;
	int                  user_is_default;
	/* auth */
	char                *auth;
	od_auth_t            auth_mode;
	char                *auth_query;
	char                *auth_query_db;
	char                *auth_query_user;
	int                  auth_common_name_default;
	od_list_t            auth_common_names;
	/* password */
	char                *password;
	int                  password_len;
	/* storage */
	od_config_storage_t *storage;
	char                *storage_name;
	char                *storage_db;
	char                *storage_user;
	int                  storage_user_len;
	char                *storage_password;
	int                  storage_password_len;
	/* pool */
	od_pool_type_t       pool;
	char                *pool_sz;
	int                  pool_size;
	int                  pool_timeout;
	int                  pool_ttl;
	int                  pool_cancel;
	int                  pool_rollback;
	/* misc */
	int                  client_fwd_error;
	int                  client_max_set;
	int                  client_max;
	int                  log_debug;
	od_list_t            link;
};

struct od_config_listen
{
	char      *host;
	int        port;
	int        backlog;
	od_tls_t   tls_mode;
	char      *tls;
	char      *tls_ca_file;
	char      *tls_key_file;
	char      *tls_cert_file;
	char      *tls_protocols;
	od_list_t  link;
};

struct od_config
{
	/* main */
	int        daemonize;
	int        log_to_stdout;
	int        log_debug;
	int        log_config;
	int        log_session;
	int        log_query;
	char      *log_file;
	char      *log_format;
	int        log_stats;
	int        log_syslog;
	char      *log_syslog_ident;
	char      *log_syslog_facility;
	int        stats_interval;
	char      *pid_file;
	char      *unix_socket_dir;
	char      *unix_socket_mode;
	int        readahead;
	int        nodelay;
	int        keepalive;
	int        workers;
	int        resolvers;
	int        client_max_set;
	int        client_max;
	int        cache_coroutine;
	int        cache_msg_gc_size;
	int        coroutine_stack_size;
	/* temprorary storages */
	od_list_t  storages;
	/* routes */
	od_list_t  routes;
	/* listen servers */
	od_list_t  listen;
};

void od_config_init(od_config_t*);
void od_config_free(od_config_t*);
int  od_config_validate(od_config_t*, od_logger_t*);
void od_config_print(od_config_t*, od_logger_t*, int);

od_config_listen_t*
od_config_listen_add(od_config_t*);

od_config_storage_t*
od_config_storage_add(od_config_t*);

od_config_storage_t*
od_config_storage_copy(od_config_storage_t*);

od_config_storage_t*
od_config_storage_match(od_config_t*, char*);

void od_config_storage_free(od_config_storage_t*);

od_config_route_t*
od_config_route_add(od_config_t*);

void od_config_route_free(od_config_route_t*);

od_config_route_t*
od_config_route_forward(od_config_t*, char*, char*);

od_config_route_t*
od_config_route_match(od_config_t*, char*, char*);

od_config_auth_t*
od_config_auth_add(od_config_route_t*);

void od_config_auth_free(od_config_auth_t*);

#endif /* ODYSSEY_CONFIG_H */
