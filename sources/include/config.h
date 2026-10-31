#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>

#include <tls_config.h>
#include <list.h>
#include <tsa.h>
#include <balancing.h>
#include <types.h>
#include <affinity.h>
#include <logger.h>
#include <common_const.h>

#ifdef LDAP_FOUND
#define LDAP_MIN_COROUTINE_STACK_SIZE 16
#endif

struct od_config_listen {
	od_tls_opts_t *tls_opts;

	char *host;
	int port;

	int backlog;

	int client_login_timeout_ms;
	int compression;

	od_list_t link;

	od_target_session_attrs_t target_session_attrs;
	int auto_route_ro_on_standby;
	od_storage_balancing_t balancing_override;
	int balancing_override_set;
	int catchup_timeout;

	char **storage_names;
	size_t storage_count;

	int reserved_clients;
};

struct od_config_conn_drop_options {
	int drop_enabled;
	int rate;
	int interval_ms;
};

struct od_config_soft_oom_drop {
	int enabled;
	int signal;
	int max_rate;
};

struct od_config_soft_oom {
	int enabled;
	uint64_t limit_bytes;
	char process[256];
	int check_interval_ms;
	od_config_soft_oom_drop_t drop;
};

typedef enum {
	OD_CONFIG_QUERY_PARSING_MODE_DISABLED,
	OD_CONFIG_QUERY_PARSING_MODE_MINIMAL,
	OD_CONFIG_QUERY_PARSING_MODE_FULL,
} od_config_query_parsing_mode_t;

typedef struct {
	od_config_query_parsing_mode_t mode;
	size_t mem_limit_bytes;
	size_t max_query_len;
	/*
	 * additional standby-friendly function names from
	 * query_parsing.standby_function_list (extends the built-in list)
	 */
	char **standby_function_names;
	size_t standby_function_names_count;
} od_config_query_parsing_t;

struct od_config {
	int daemonize;
	int priority;
	int sequential_routing;
	/* logging */
	int log_to_stdout;
	_Atomic int log_debug;
	_Atomic int log_debug_sampling;
	_Atomic int maintain_params;
	_Atomic int log_config;
	_Atomic int log_session;
	_Atomic int log_query;
	_Atomic int log_query_sampling;
	int log_queue_depth;
	int log_async;
	char *log_file;
	char *log_format;
	_Atomic int log_stats;
	int log_general_stats_prom;
	int log_route_stats_prom;
	int log_syslog;
	char *log_syslog_ident;
	char *log_syslog_facility;
	/*         */
	/* statistics interval in seconds */
	_Atomic int stats_interval;
	/* system related settings */
	char *pid_file;
	char *unix_socket_dir;
	char *unix_socket_mode;
	char *locks_dir;
	char *external_auth_socket_path;
	/* sigusr2 etc */
	/* TODO: delete this option - online restart is always enabled now */
	int graceful_die_on_errors;
	 _Atomic uint64_t graceful_shutdown_timeout_ms;
	/* TODO: delete this option - online restart is always enabled now */
	int enable_online_restart_feature;
	od_config_conn_drop_options_t conn_drop_options;
	int bindwith_reuseport;
	/*                         */
	int readahead;
	int nodelay;
	_Atomic int disable_nolinger;

	/* TCP KEEPALIVE related settings (keepalive and interval in seconds,
	 * usr timeout = TCP_USER_TIMEOUT in milliseconds) */
	_Atomic int keepalive;
	_Atomic int keepalive_keep_interval;
	_Atomic int keepalive_probes;
	 _Atomic int64_t keepalive_usr_timeout_ms;
	/*                                */
	int workers;
	int resolvers;
	/*         client                 */
	_Atomic int client_max_set;
	_Atomic int client_max;
	_Atomic int client_max_routing;
	_Atomic int accept_rate_limit;
	 _Atomic uint64_t server_login_retry_ms;
	int reserve_session_server_connection;
	/*  */
	int cache_coroutine;
	int cache_msg_gc_size;
	int cache_msg_gc_count;
	int coroutine_stack_size;
	int system_coroutine_stack_size;
	int dns_cache_ttl_ms;
	char *hba_file;
	/* Soft interval between group checks (milliseconds) */
	uint64_t group_checker_interval_ms;

	/*
	 * auth_query cached password lifetime (milliseconds) and the
	 * maximum random jitter added on top of it (0 disables jitter)
	 */
	 _Atomic uint64_t auth_query_cache_ttl_ms;
	 _Atomic uint64_t auth_query_cache_ttl_jitter_ms;
	/*
	 * Delay before the next auth_query password refresh attempt
	 * after a failed one (milliseconds)
	 */
	 _Atomic uint64_t auth_query_cache_error_retry_ms;
	/*
	 * Continue to serve the stale cached password when an auth_query
	 * password refresh fails; when disabled the cached value is
	 * dropped and clients can not authenticate until a refresh
	 * succeeds
	 */
	_Atomic int auth_query_cache_stale_on_error;
	/*
	 * auth_query execution timeout (milliseconds), also used as the
	 * initial wait for the first password fetch of a route
	 */
	 _Atomic uint64_t auth_query_timeout_ms;
	od_list_t listen;

	 _Atomic uint64_t backend_connect_timeout_ms;
	 _Atomic uint64_t cancel_timeout_ms;
	int64_t cancel_queue_timeout_ms;
	int cancel_max_inflight;
	int cancel_rate_limit;

	/*
	 * Ignore incoming client cancel requests entirely:
	 * they are dropped without forwarding to the backend
	 */
	_Atomic int ignore_cancel;

	int virtual_processing; /* enables some cases for full-virtual query processing */
	int virtual_transaction;

	/*
	 * forward replication connections to the server as is,
	 * bypassing all of the odyssey logic (authentication, pooling etc.)
	 * separately for physical and logical replication
	 */
	_Atomic int replication_proxy_phys;
	_Atomic int replication_proxy_logic;

	char availability_zone[OD_MAX_AVAILABILITY_ZONE_LENGTH];

	_Atomic int max_sigterms_to_die;

	int host_watcher_enabled;

	od_config_soft_oom_t soft_oom;

	od_config_query_parsing_t query_parsing;

	_Atomic int smart_search_path_enquoting;

	od_affinity_config_t *cpu_affinity;
};

/* longest configuration value the console is willing to print */
#define OD_CONFIG_FIELD_VALUE_MAX 256

typedef enum {
	OD_CONFIG_FIELD_INT,
	OD_CONFIG_FIELD_I64,
	OD_CONFIG_FIELD_U64,
	OD_CONFIG_FIELD_BOOL,
	OD_CONFIG_FIELD_STRING,
	OD_CONFIG_FIELD_STRING_INLINE,
} od_config_field_type_t;

typedef struct {
	/* name accepted by the configuration parser */
	char *key;
	od_config_field_type_t type;
	/* offset of the field inside od_config_t */
	size_t offset;
	/* whether RELOAD applies a new value, see od_config_reload() */
	int reloadable;
} od_config_field_t;

size_t od_config_fields_count(void);
const od_config_field_t *od_config_field_at(size_t index);
const od_config_field_t *od_config_field_by_key(const char *key);
int od_config_field_deprecated(const od_config_field_t *field);
int od_config_field_format(const od_config_field_t *field, const char *value,
			   char *buf, size_t size);
void od_config_field_value(od_config_t *config, const od_config_field_t *field,
			   char *buf, size_t size);
void od_config_field_default(const od_config_field_t *field, char *buf,
			     size_t size);

void od_config_init(od_config_t *);
void od_config_free(od_config_t *);
void od_config_reload(od_config_t *, od_config_t *);
int od_config_validate(od_config_t *, od_logger_t *);
void od_config_print(od_config_t *, od_logger_t *);

od_config_listen_t *od_config_listen_add(od_config_t *);

static inline char *od_config_listen_host_name(od_config_listen_t *listen)
{
	return listen->host == NULL ? "(NULL)" : listen->host;
}
