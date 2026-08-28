

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <unistd.h>

#include <machinarium/machinarium.h>

#include <status.h>
#include <types.h>
#include <router.h>
#include <config.h>
#include <od_memory.h>
#include <util.h>

void od_config_init(od_config_t *config)
{
	config->daemonize = 0;
	config->priority = 0;
	config->sequential_routing = 0;
	config->log_debug = 0;
	config->log_to_stdout = 1;
	config->log_config = 0;
	config->log_session = 1;
	config->log_query = 0;
	config->log_file = NULL;
	config->log_stats = 1;
	config->log_async = 1;
	config->log_queue_depth = 30000;
	config->log_general_stats_prom = 0;
	config->log_route_stats_prom = 0;
	config->stats_interval = 3;
	config->log_format = NULL;
	config->pid_file = NULL;
	config->unix_socket_dir = NULL;
	config->locks_dir = NULL;
	config->external_auth_socket_path = NULL;
	config->enable_online_restart_feature = 1;
	config->conn_drop_options.drop_enabled = 1;
	config->conn_drop_options.rate = 1;
	config->conn_drop_options.interval_ms = 1000; /* 1 sec */
	config->bindwith_reuseport = 1;
	config->graceful_die_on_errors = 0;
	config->unix_socket_mode = NULL;

	config->virtual_transaction = 1;

	config->log_syslog = 0;
	config->log_syslog_ident = NULL;
	config->log_syslog_facility = NULL;

	config->readahead = sysconf(_SC_PAGESIZE);
	config->nodelay = 1;
	config->disable_nolinger = 0;

	config->keepalive = 15;
	config->keepalive_keep_interval = 5;
	config->keepalive_probes = 3;
	config->keepalive_usr_timeout = 0; /* use sys default */

	config->cpu_affinity = NULL;
	// od_affinity_config_init(&config->cpu_affinity);

	config->workers = 1;
	config->resolvers = 1;
	config->client_max_set = 0;
	config->client_max = 0;
	config->client_max_routing = 0;
	config->accept_rate_limit = 0;
	config->server_login_retry = 1;
	config->cache_coroutine = 1024;
	config->cache_msg_gc_size = 0;
	config->coroutine_stack_size = 16;
	config->system_coroutine_stack_size = 32;
	config->hba_file = NULL;
	config->max_sigterms_to_die = 3;
	config->group_checker_interval = 7000; /* 7 seconds */
	od_list_init(&config->listen);

	config->backend_connect_timeout_ms = 30U * 1000U; /* 30 seconds */
	config->cancel_timeout_ms = 1U * 1000U; /* 1 seconds */
	config->cancel_queue_timeout_ms = -1;
	config->cancel_max_inflight = -1;
	config->virtual_processing = 0;

	config->graceful_shutdown_timeout_ms = 30 * 1000; /* 30 seconds */

	memset(config->availability_zone, 0, sizeof(config->availability_zone));

	memset(&config->soft_oom, 0, sizeof(config->soft_oom));
	config->soft_oom.check_interval_ms = 1000;
	config->soft_oom.drop.enabled = 0;
	config->soft_oom.drop.max_rate = 3;
	config->soft_oom.drop.signal = SIGTERM;

	memset(&config->query_parsing, 0, sizeof(config->query_parsing));
	config->query_parsing.mem_limit_bytes = 1024 * 1024;
	config->query_parsing.mode = OD_CONFIG_QUERY_PARSING_MODE_MINIMAL;

	config->host_watcher_enabled = 0;

	config->smart_search_path_enquoting = 0;

	config->dns_ttl_ms = 30 * 1000;
}

void od_config_reload(od_config_t *current_config, od_config_t *new_config)
{
	current_config->log_debug = new_config->log_debug;
	current_config->log_config = new_config->log_config;
	current_config->log_session = new_config->log_session;
	current_config->log_query = new_config->log_query;
	current_config->log_stats = new_config->log_stats;
	current_config->stats_interval = new_config->stats_interval;
	current_config->client_max_set = new_config->client_max_set;
	current_config->client_max = new_config->client_max;
	current_config->max_sigterms_to_die = new_config->max_sigterms_to_die;
	current_config->client_max_routing = new_config->client_max_routing;
	current_config->accept_rate_limit = new_config->accept_rate_limit;
	current_config->server_login_retry = new_config->server_login_retry;
	current_config->backend_connect_timeout_ms =
		new_config->backend_connect_timeout_ms;
	current_config->cancel_timeout_ms = new_config->cancel_timeout_ms;
	current_config->smart_search_path_enquoting =
		new_config->smart_search_path_enquoting;
	current_config->disable_nolinger = new_config->disable_nolinger;
	current_config->keepalive = new_config->keepalive;
	current_config->keepalive_keep_interval =
		new_config->keepalive_keep_interval;
	current_config->keepalive_probes = new_config->keepalive_probes;
	current_config->keepalive_usr_timeout =
		new_config->keepalive_usr_timeout;
	current_config->graceful_shutdown_timeout_ms =
		new_config->graceful_shutdown_timeout_ms;
}

static void od_config_listen_free(od_config_listen_t *);

void od_config_free(od_config_t *config)
{
	od_list_t *i, *n;
	od_list_foreach_safe (&config->listen, i, n) {
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		od_config_listen_free(listen);
	}
	if (config->unix_socket_mode) {
		od_free(config->unix_socket_mode);
	}
	if (config->log_file) {
		od_free(config->log_file);
	}
	if (config->log_format) {
		od_free(config->log_format);
	}
	if (config->pid_file) {
		od_free(config->pid_file);
	}
	if (config->unix_socket_dir) {
		od_free(config->unix_socket_dir);
	}
	if (config->log_syslog_ident) {
		od_free(config->log_syslog_ident);
	}
	if (config->log_syslog_facility) {
		od_free(config->log_syslog_facility);
	}
	if (config->locks_dir) {
		od_free(config->locks_dir);
		if (config->hba_file) {
			od_free(config->hba_file);
		}
	}
	if (config->external_auth_socket_path) {
		od_free(config->external_auth_socket_path);
	}
	if (config->cpu_affinity) {
		od_free(config->cpu_affinity);
	}
}

od_config_listen_t *od_config_listen_add(od_config_t *config)
{
	od_config_listen_t *listen =
		(od_config_listen_t *)od_malloc(sizeof(od_config_listen_t));
	if (listen == NULL) {
		return NULL;
	}

	memset(listen, 0, sizeof(*listen));

	listen->tls_opts = od_tls_opts_alloc();
	if (listen->tls_opts == NULL) {
		od_free(listen);
		return NULL;
	}

	listen->port = 6432;
	listen->backlog = 128;
	listen->client_login_timeout = 15000;
	listen->reserved_clients = -1; /* disabled */
	listen->target_session_attrs = OD_TARGET_SESSION_ATTRS_UNDEF;
	listen->balancing_override_set = 0;
	od_storage_balancing_init(&listen->balancing_override);

	od_list_init(&listen->link);
	od_list_append(&config->listen, &listen->link);

	return listen;
}

static void od_config_listen_free(od_config_listen_t *config)
{
	if (config->host) {
		od_free(config->host);
	}

	if (config->tls_opts) {
		od_tls_opts_free(config->tls_opts);
	}

	od_storage_balancing_destroy(&config->balancing_override);

	for (size_t i = 0; i < config->storage_count; ++i) {
		od_free(config->storage_names[i]);
	}
	od_free(config->storage_names);

	od_free(config);
}

int od_config_validate(od_config_t *config, od_logger_t *logger)
{
	if (config->workers <= 0) {
		od_error(logger, "config", NULL, NULL, "bad workers number");
		return -1;
	}

	if (config->resolvers <= 0) {
		od_error(logger, "config", NULL, NULL, "bad resolvers number");
		return -1;
	}

	if (config->coroutine_stack_size < 4) {
		od_error(logger, "config", NULL, NULL,
			 "bad coroutine_stack_size number");
		return -1;
	}

	if (config->system_coroutine_stack_size < 4) {
		od_error(logger, "config", NULL, NULL,
			 "bad system_coroutine_stack_size number");
		return -1;
	}

	if (config->log_format == NULL) {
		od_error(logger, "config", NULL, NULL,
			 "log_format is not defined");
		return -1;
	}

	/* unix_socket_mode */
	if (config->unix_socket_dir) {
		if (config->unix_socket_mode == NULL) {
			od_error(logger, "config", NULL, NULL,
				 "unix_socket_mode is not set");
			return -1;
		}
	}

	/* listen */
	if (od_list_empty(&config->listen)) {
		od_error(logger, "config", NULL, NULL,
			 "no listen servers defined");
		return -1;
	}

	od_list_t *i;
	od_list_foreach (&config->listen, i) {
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		if (listen->host == NULL) {
			if (config->unix_socket_dir == NULL) {
				od_error(
					logger, "config", NULL, NULL,
					"listen host is not set and no unix_socket_dir is specified");
				return -1;
			}
		}

		/* tls options */
		if (listen->tls_opts->tls) {
			if (strcmp(listen->tls_opts->tls, "disable") == 0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_DISABLE;
			} else if (strcmp(listen->tls_opts->tls, "allow") ==
				   0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_ALLOW;
			} else if (strcmp(listen->tls_opts->tls, "prefer") ==
				   0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_PREFER;
			} else if (strcmp(listen->tls_opts->tls, "require") ==
				   0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_REQUIRE;
			} else if (strcmp(listen->tls_opts->tls, "verify_ca") ==
				   0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_VERIFY_CA;
			} else if (strcmp(listen->tls_opts->tls,
					  "verify_full") == 0) {
				listen->tls_opts->tls_mode =
					OD_CONFIG_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "config", NULL, NULL,
					 "unknown tls_opts->tls mode");
				return -1;
			}
		}
	}

	if (config->enable_online_restart_feature &&
	    !config->bindwith_reuseport) {
		od_error(
			logger, "config", NULL, NULL,
			"online restart feature works only with SO_REUSEPORT. Disable "
			"online restart or/and enable bindwith_reuseport");
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

static inline char *od_config_yes_no(int value)
{
	return value ? "yes" : "no";
}

/*
 * Description of the global configuration for SHOW CONFIG.
 *
 * The default column is not stored here on purpose: od_config_field_default()
 * initializes a throwaway od_config_t through od_config_init() and reads the
 * same offset, so the reported defaults cannot drift away from the real ones.
 *
 * The reloadable flag has no such source and must be kept in sync with
 * od_config_reload() by hand.
 */

#define OD_CONFIG_FIELD_NOT_SET "(not set)"

static const od_config_field_t od_config_fields[] = {
	{ "daemonize", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, daemonize),
	  0 },
	{ "priority", OD_CONFIG_FIELD_INT, offsetof(od_config_t, priority), 0 },
	{ "sequential_routing", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, sequential_routing), 0 },
	{ "log_to_stdout", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, log_to_stdout), 0 },
	{ "log_debug", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_debug),
	  1 },
	{ "log_config", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_config),
	  1 },
	{ "log_session", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, log_session), 1 },
	{ "log_query", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_query),
	  1 },
	{ "log_queue_depth", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, log_queue_depth), 0 },
	{ "log_async", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_async),
	  0 },
	{ "log_file", OD_CONFIG_FIELD_STRING, offsetof(od_config_t, log_file),
	  0 },
	{ "log_format", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, log_format), 0 },
	{ "log_stats", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_stats),
	  1 },
	{ "log_general_stats_prom", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, log_general_stats_prom), 0 },
	{ "log_route_stats_prom", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, log_route_stats_prom), 0 },
	{ "log_syslog", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, log_syslog),
	  0 },
	{ "log_syslog_ident", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, log_syslog_ident), 0 },
	{ "log_syslog_facility", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, log_syslog_facility), 0 },
	{ "stats_interval", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, stats_interval), 1 },
	{ "pid_file", OD_CONFIG_FIELD_STRING, offsetof(od_config_t, pid_file),
	  0 },
	{ "unix_socket_dir", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, unix_socket_dir), 0 },
	{ "unix_socket_mode", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, unix_socket_mode), 0 },
	{ "locks_dir", OD_CONFIG_FIELD_STRING, offsetof(od_config_t, locks_dir),
	  0 },
	{ "external_auth_socket_path", OD_CONFIG_FIELD_STRING,
	  offsetof(od_config_t, external_auth_socket_path), 0 },
	{ "graceful_die_on_errors", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, graceful_die_on_errors), 0 },
	{ "graceful_shutdown_timeout_ms", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, graceful_shutdown_timeout_ms), 1 },
	{ "enable_online_restart", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, enable_online_restart_feature), 0 },
	{ "bindwith_reuseport", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, bindwith_reuseport), 0 },
	{ "readahead", OD_CONFIG_FIELD_INT, offsetof(od_config_t, readahead),
	  0 },
	{ "nodelay", OD_CONFIG_FIELD_BOOL, offsetof(od_config_t, nodelay), 0 },
	{ "disable_nolinger", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, disable_nolinger), 1 },
	{ "keepalive", OD_CONFIG_FIELD_INT, offsetof(od_config_t, keepalive),
	  1 },
	{ "keepalive_keep_interval", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, keepalive_keep_interval), 1 },
	{ "keepalive_probes", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, keepalive_probes), 1 },
	{ "keepalive_usr_timeout", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, keepalive_usr_timeout), 1 },
	{ "workers", OD_CONFIG_FIELD_INT, offsetof(od_config_t, workers), 0 },
	{ "resolvers", OD_CONFIG_FIELD_INT, offsetof(od_config_t, resolvers),
	  0 },
	{ "client_max", OD_CONFIG_FIELD_INT, offsetof(od_config_t, client_max),
	  1 },
	{ "client_max_routing", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, client_max_routing), 1 },
	{ "accept_rate_limit", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, accept_rate_limit), 1 },
	{ "server_login_retry", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, server_login_retry), 1 },
	{ "cache_coroutine", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, cache_coroutine), 0 },
	{ "cache_msg_gc_size", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, cache_msg_gc_size), 0 },
	{ "coroutine_stack_size", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, coroutine_stack_size), 0 },
	{ "system_coroutine_stack_size", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, system_coroutine_stack_size), 0 },
	{ "dns_cache_ttl", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, dns_ttl_ms), 0 },
	{ "hba_file", OD_CONFIG_FIELD_STRING, offsetof(od_config_t, hba_file),
	  0 },
	{ "group_checker_interval", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, group_checker_interval), 0 },
	{ "backend_connect_timeout_ms", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, backend_connect_timeout_ms), 1 },
	{ "cancel_timeout_ms", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, cancel_timeout_ms), 1 },
	{ "cancel_queue_timeout_ms", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, cancel_queue_timeout_ms), 0 },
	{ "cancel_max_inflight", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, cancel_max_inflight), 0 },
	{ "virtual_processing", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, virtual_processing), 0 },
	{ "virtual_transaction", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, virtual_transaction), 0 },
	{ "availability_zone", OD_CONFIG_FIELD_STRING_INLINE,
	  offsetof(od_config_t, availability_zone), 0 },
	{ "max_sigterms_to_die", OD_CONFIG_FIELD_INT,
	  offsetof(od_config_t, max_sigterms_to_die), 1 },
	{ "enable_host_watcher", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, host_watcher_enabled), 0 },
	{ "smart_search_path_enquoting", OD_CONFIG_FIELD_BOOL,
	  offsetof(od_config_t, smart_search_path_enquoting), 1 },
};

size_t od_config_fields_count(void)
{
	return sizeof(od_config_fields) / sizeof(od_config_fields[0]);
}

const od_config_field_t *od_config_field_at(size_t index)
{
	if (index >= od_config_fields_count()) {
		return NULL;
	}
	return &od_config_fields[index];
}

void od_config_field_value(od_config_t *config, const od_config_field_t *field,
			   char *buf, size_t size)
{
	char *at = (char *)config + field->offset;

	switch (field->type) {
	case OD_CONFIG_FIELD_INT:
		od_snprintf(buf, size, "%d", *(int *)at);
		break;
	case OD_CONFIG_FIELD_BOOL:
		od_snprintf(buf, size, "%s", *(int *)at ? "yes" : "no");
		break;
	case OD_CONFIG_FIELD_STRING: {
		char *value = *(char **)at;
		od_snprintf(buf, size, "%s",
			    value ? value : OD_CONFIG_FIELD_NOT_SET);
		break;
	}
	case OD_CONFIG_FIELD_STRING_INLINE:
		od_snprintf(buf, size, "%s",
			    at[0] ? at : OD_CONFIG_FIELD_NOT_SET);
		break;
	default:
		od_snprintf(buf, size, "%s", OD_CONFIG_FIELD_NOT_SET);
		break;
	}
}

/*
 * Defaults are shared by every caller and never change, so they are built
 * once per process. od_config_t is too large to keep on a coroutine stack,
 * and od_config_init() allocates nothing, so this instance needs no
 * matching od_config_free().
 */
static od_config_t od_config_defaults;
static pthread_once_t od_config_defaults_ctrl = PTHREAD_ONCE_INIT;

static void od_config_defaults_build(void)
{
	od_config_init(&od_config_defaults);
}

void od_config_field_default(const od_config_field_t *field, char *buf,
			     size_t size)
{
	pthread_once(&od_config_defaults_ctrl, od_config_defaults_build);
	od_config_field_value(&od_config_defaults, field, buf, size);
}

void od_config_print(od_config_t *config, od_logger_t *logger)
{
	od_log(logger, "config", NULL, NULL, "daemonize               %s",
	       od_config_yes_no(config->daemonize));
	od_log(logger, "config", NULL, NULL, "priority                %d",
	       config->priority);
	od_log(logger, "config", NULL, NULL, "sequential_routing      %s",
	       od_config_yes_no(config->sequential_routing));
	if (config->pid_file) {
		od_log(logger, "config", NULL, NULL,
		       "pid_file                %s", config->pid_file);
	}
	if (config->unix_socket_dir) {
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_dir         %s", config->unix_socket_dir);
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_mode        %s", config->unix_socket_mode);
	}
	if (config->log_format) {
		od_log(logger, "config", NULL, NULL,
		       "log_format              %s", config->log_format);
	}
	if (config->log_file) {
		od_log(logger, "config", NULL, NULL,
		       "log_file                %s", config->log_file);
	}
	od_log(logger, "config", NULL, NULL, "log_to_stdout           %s",
	       od_config_yes_no(config->log_to_stdout));
	od_log(logger, "config", NULL, NULL, "log_syslog              %s",
	       od_config_yes_no(config->log_syslog));
	if (config->log_syslog_ident) {
		od_log(logger, "config", NULL, NULL,
		       "log_syslog_ident        %s", config->log_syslog_ident);
	}
	if (config->log_syslog_facility) {
		od_log(logger, "config", NULL, NULL,
		       "log_syslog_facility     %s",
		       config->log_syslog_facility);
	}
	od_log(logger, "config", NULL, NULL, "log_debug               %s",
	       od_config_yes_no(config->log_debug));
	od_log(logger, "config", NULL, NULL, "log_config              %s",
	       od_config_yes_no(config->log_config));
	od_log(logger, "config", NULL, NULL, "log_session             %s",
	       od_config_yes_no(config->log_session));
	od_log(logger, "config", NULL, NULL, "log_query               %s",
	       od_config_yes_no(config->log_query));
	od_log(logger, "config", NULL, NULL, "log_stats               %s",
	       od_config_yes_no(config->log_stats));
	od_log(logger, "config", NULL, NULL, "log_general_stats_prom  %s",
	       od_config_yes_no(config->log_general_stats_prom));
	od_log(logger, "config", NULL, NULL, "log_route_stats_prom    %s",
	       od_config_yes_no(config->log_route_stats_prom));
	od_log(logger, "config", NULL, NULL, "log_queue_depth         %d",
	       config->log_queue_depth);
	od_log(logger, "config", NULL, NULL, "stats_interval          %d",
	       config->stats_interval);
	od_log(logger, "config", NULL, NULL, "readahead               %d",
	       config->readahead);
	od_log(logger, "config", NULL, NULL, "nodelay                 %s",
	       od_config_yes_no(config->nodelay));
	od_log(logger, "config", NULL, NULL, "disable_nolinger        %s",
	       od_config_yes_no(config->disable_nolinger));
	od_log(logger, "config", NULL, NULL, "keepalive               %d",
	       config->keepalive);
	od_log(logger, "config", NULL, NULL, "keepalive_keep_interval %d",
	       config->keepalive_keep_interval);
	od_log(logger, "config", NULL, NULL, "keepalive_probes        %d",
	       config->keepalive_probes);
	od_log(logger, "config", NULL, NULL, "keepalive_usr_timeout   %d",
	       config->keepalive_usr_timeout);
	if (config->client_max_set) {
		od_log(logger, "config", NULL, NULL,
		       "client_max              %d", config->client_max);
	}
	od_log(logger, "config", NULL, NULL, "client_max_routing      %d",
	       config->client_max_routing);
	od_log(logger, "config", NULL, NULL, "server_login_retry      %d",
	       config->server_login_retry);
	od_log(logger, "config", NULL, NULL, "cache_msg_gc_size       %d",
	       config->cache_msg_gc_size);
	od_log(logger, "config", NULL, NULL, "cache_coroutine         %d",
	       config->cache_coroutine);
	od_log(logger, "config", NULL, NULL, "coroutine_stack_size    %d",
	       config->coroutine_stack_size);
	od_log(logger, "config", NULL, NULL, "system_coroutine_stack_size %d",
	       config->system_coroutine_stack_size);
	od_log(logger, "config", NULL, NULL, "workers                 %d",
	       config->workers);
	od_log(logger, "config", NULL, NULL, "resolvers               %d",
	       config->resolvers);
	od_log(logger, "config", NULL, NULL, "backend_connect_timeout_ms %u",
	       config->backend_connect_timeout_ms);
	od_log(logger, "config", NULL, NULL, "cancel_timeout_ms         %u",
	       config->cancel_timeout_ms);
	od_log(logger, "config", NULL, NULL, "cancel_queue_timeout_ms   %u",
	       config->cancel_queue_timeout_ms);
	od_log(logger, "config", NULL, NULL, "cancel_max_inflight       %d",
	       config->cancel_max_inflight);
	od_log(logger, "config", NULL, NULL, "dns_ttl_ms              %d",
	       config->dns_ttl_ms);
	od_log(logger, "config", NULL, NULL, "group_checker_interval  %d",
	       config->group_checker_interval);
	od_log(logger, "config", NULL, NULL, "max_sigterms_to_die     %d",
	       config->max_sigterms_to_die);
	od_log(logger, "config", NULL, NULL, "virtual_processing      %s",
	       od_config_yes_no(config->virtual_processing));
	od_log(logger, "config", NULL, NULL, "virtual_transaction     %s",
	       od_config_yes_no(config->virtual_transaction));
	od_log(logger, "config", NULL, NULL, "smart_search_path_enquoting %s",
	       od_config_yes_no(config->smart_search_path_enquoting));
	if (config->availability_zone[0]) {
		od_log(logger, "config", NULL, NULL,
		       "availability_zone       %s", config->availability_zone);
	}
	od_log(logger, "config", NULL, NULL, "enable_host_watcher:    %d",
	       config->host_watcher_enabled);

	od_log(logger, "config", NULL, NULL, "online restart enabled: %d",
	       config->enable_online_restart_feature);

	od_log(logger, "config", NULL, NULL, "graceful die enabled:   %d",
	       config->graceful_die_on_errors);

	od_log(logger, "config", NULL, NULL, "bindwith_reuseport:     %d",
	       config->bindwith_reuseport);

	if (config->hba_file) {
		od_log(logger, "config", NULL, NULL,
		       "hba_file                %s", config->hba_file);
	}

	if (config->conn_drop_options.drop_enabled) {
		od_log(logger, "config", NULL, NULL,
		       "conn_drop_options: enabled, rate %d, interval_ms %d",
		       config->conn_drop_options.rate,
		       config->conn_drop_options.interval_ms);
	}

	if (config->soft_oom.enabled) {
		od_log(logger, "config", NULL, NULL,
		       "soft_oom: check '%s' every %dms with limit %" PRIu64
		       " bytes",
		       config->soft_oom.process,
		       config->soft_oom.check_interval_ms,
		       config->soft_oom.limit_bytes);
		if (config->soft_oom.drop.enabled) {
			od_log(logger, "config", NULL, NULL,
			       "soft_oom drop: signal %d, max_rate %d",
			       config->soft_oom.drop.signal,
			       config->soft_oom.drop.max_rate);
		}
	} else {
		od_log(logger, "config", NULL, NULL,
		       "soft_oom:         DISABLED");
	}

	od_list_t *i;
	od_list_foreach (&config->listen, i) {
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		od_log(logger, "config", NULL, NULL, "listen");
		od_log(logger, "config", NULL, NULL, "  host          %s",
		       listen->host ? listen->host : "<unix socket>");
		od_log(logger, "config", NULL, NULL, "  port          %d",
		       listen->port);
		od_log(logger, "config", NULL, NULL, "  backlog       %d",
		       listen->backlog);
		od_log(logger, "config", NULL, NULL,
		       "  client_login_timeout %d",
		       listen->client_login_timeout);
		od_log(logger, "config", NULL, NULL, "  reserved_clients %d",
		       listen->reserved_clients);
		od_log(logger, "config", NULL, NULL,
		       "  target_session_attrs %s",
		       od_target_session_attrs_to_str(
			       listen->target_session_attrs));
		if (listen->balancing_override_set) {
			od_log(logger, "config", NULL, NULL,
			       "  balancing override: method %s, az_aware %d",
			       od_balancing_method_to_str(
				       listen->balancing_override.method.type),
			       listen->balancing_override.method.az_aware);
		}
		if (listen->catchup_timeout) {
			od_log(logger, "config", NULL, NULL,
			       "  catchup_timeout %d", listen->catchup_timeout);
		}
		if (listen->tls_opts->tls) {
			od_log(logger, "config", NULL, NULL,
			       "  tls           %s", listen->tls_opts->tls);
		}
		if (listen->tls_opts->tls_ca_file) {
			od_log(logger, "config", NULL, NULL,
			       "  tls_ca_file   %s",
			       listen->tls_opts->tls_ca_file);
		}
		if (listen->tls_opts->tls_key_file) {
			od_log(logger, "config", NULL, NULL,
			       "  tls_key_file  %s",
			       listen->tls_opts->tls_key_file);
		}
		if (listen->tls_opts->tls_cert_file) {
			od_log(logger, "config", NULL, NULL,
			       "  tls_cert_file %s",
			       listen->tls_opts->tls_cert_file);
		}
		if (listen->tls_opts->tls_protocols) {
			od_log(logger, "config", NULL, NULL,
			       "  tls_protocols %s",
			       listen->tls_opts->tls_protocols);
		}

		for (size_t i = 0; i < listen->storage_count; ++i) {
			od_log(logger, "config", NULL, NULL,
			       "  storage       %s", listen->storage_names[i]);
		}
	}
}
