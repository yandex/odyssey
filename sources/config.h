#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#ifdef LDAP_FOUND
#define LDAP_MIN_COROUTINE_STACK_SIZE 16
#endif

struct od_config_listen {
	od_tls_opts_t *tls_opts;

	char *host;
	int port;

	int backlog;

	int client_login_timeout;
	int compression;

	od_list_t link;

	od_target_session_attrs_t target_session_attrs;
};

struct od_config_online_restart_drop_options {
	int drop_enabled;
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

struct od_config {
	int daemonize;
	int priority;
	int sequential_routing;
	/* logging */
	int log_to_stdout;
	int log_debug;
	int log_config;
	int log_session;
	int log_query;
	char *log_file;
	char *log_format;
	int log_stats;
	int log_general_stats_prom;
	int log_route_stats_prom;
	int log_syslog;
	char *log_syslog_ident;
	char *log_syslog_facility;
	/*         */
	int stats_interval;
	/* system related settings */
	char *pid_file;
	char *unix_socket_dir;
	char *unix_socket_mode;
	char *locks_dir;
	char *external_auth_socket_path;
	/* sigusr2 etc */
	int graceful_die_on_errors;
	int graceful_shutdown_timeout_ms;
	int enable_online_restart_feature;
	od_config_online_restart_drop_options_t online_restart_drop_options;
	int bindwith_reuseport;
	/*                         */
	int readahead;
	int nodelay;

	/* TCP KEEPALIVE related settings */
	int keepalive;
	int keepalive_keep_interval;
	int keepalive_probes;
	int keepalive_usr_timeout;
	/*                                */
	int workers;
	int resolvers;
	/*         client                 */
	int client_max_set;
	int client_max;
	int client_max_routing;
	int server_login_retry;
	int reserve_session_server_connection;
	/*  */
	int cache_coroutine;
	int cache_msg_gc_size;
	int coroutine_stack_size;
	char *hba_file;
	// Soft interval between group checks
	int group_checker_interval;
	od_list_t listen;

	int backend_connect_timeout_ms;

	int virtual_processing; /* enables some cases for full-virtual query processing */

	char availability_zone[OD_MAX_AVAILABILITY_ZONE_LENGTH];

	int max_sigterms_to_die;

	od_config_soft_oom_t soft_oom;
};

void od_config_init(od_config_t *);
void od_config_free(od_config_t *);
void od_config_reload(od_config_t *, od_config_t *);
int od_config_validate(od_config_t *, od_logger_t *);
void od_config_print(od_config_t *, od_logger_t *);

od_config_listen_t *od_config_listen_add(od_config_t *);
