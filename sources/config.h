#ifndef ODYSSEY_CONFIG_H
#define ODYSSEY_CONFIG_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_config_listen od_config_listen_t;
typedef struct od_config        od_config_t;

typedef enum
{
	OD_CONFIG_TLS_DISABLE,
	OD_CONFIG_TLS_ALLOW,
	OD_CONFIG_TLS_REQUIRE,
	OD_CONFIG_TLS_VERIFY_CA,
	OD_CONFIG_TLS_VERIFY_FULL
} od_config_tls_t;

struct od_config_listen
{
	char             *host;
	int               port;
	int               backlog;
	od_config_tls_t   tls_mode;
	char             *tls;
	char             *tls_ca_file;
	char             *tls_key_file;
	char             *tls_cert_file;
	char             *tls_protocols;
	od_list_t         link;
};

struct od_config
{
	mcxt_context_t	mcxt;

	int        daemonize;
	int        priority;
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
	od_list_t  listen;
};

static inline int
od_config_is_multi_workers(od_config_t *config)
{
	return config->workers > 1;
}

od_config_t *od_config_allocate(mcxt_context_t mcxt);
void od_config_free(od_config_t*);
int  od_config_validate(od_config_t*, od_logger_t*);
void od_config_print(od_config_t*, od_logger_t*);

od_config_listen_t*
od_config_listen_add(od_config_t*);

#endif /* ODYSSEY_CONFIG_H */
