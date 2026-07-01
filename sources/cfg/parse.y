%start config

%define api.pure full
%define parse.error verbose
%locations

%parse-param { yyscan_t scanner }
%parse-param { od_cfg_parse_ctx_t *ctx }

%lex-param   { yyscan_t scanner }

%code requires {
	#include <odyssey.h>
	#include <od_memory.h>
	#include <cfg/loc.h>

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
	typedef void *yyscan_t;
#endif

	typedef struct od_cfg_parse_ctx od_cfg_parse_ctx_t;

#ifndef YYLTYPE_IS_DECLARED
#define YYLTYPE_IS_DECLARED 1
	typedef od_cfg_location_t YYLTYPE;
#endif
}

%code {
	#include <cfg/ctx.h>
	#include <cfg/model.h>
	#include <sysv.h>

	int yylex(YYSTYPE *yylval, YYLTYPE *yylloc, yyscan_t scanner);

	static void yyerror(YYLTYPE *loc,
						yyscan_t scanner,
						od_cfg_parse_ctx_t *ctx,
						const char *msg)
	{
		(void)scanner;

		od_cfg_diag_error(ctx->diags, *loc, "%s", msg);
	}
}

%union {
	char *str;
	int64_t num;
	int boolean;
}

%token ERROR_TOKEN

%token <str> IDENT
%token <str> STRING
%token <num> NUMBER

%destructor { od_free($$); } <str>

%token YES "yes"
%token NO "no"
%token DAEMONIZE "daemonize"
%token SEQUENTIAL_ROUTING "sequential_routing"
%token ENABLE_ONLINE_RESTART "enable_online_restart"
%token VIRTUAL_PROCESSING "virtual_processing"
%token GRACEFUL_DIE_ON_ERRORS "graceful_die_on_errors"
%token BINDWITH_REUSEPORT "bindwith_reuseport"
%token ENABLE_HOST_WATCHER "enable_host_watcher"
%token LOG_DEBUG "log_debug"
%token LOG_TO_STDOUT "log_to_stdout"
%token LOG_CONFIG "log_config"
%token LOG_SESSION "log_session"
%token LOG_QUERY "log_query"
%token LOG_STATS "log_stats"
%token LOG_ASYNC "log_async"
%token LOG_SYSLOG "log_syslog"
%token SMART_SEARCH_PATH_ENQUOTING "smart_search_path_enquoting"
%token NODELAY "nodelay"
%token DISABLE_NOLINGER "disable_nolinger"
%token LOG_GENERAL_STATS_PROM "log_general_stats_prom"
%token LOG_ROUTE_STATS_PROM "log_route_stats_prom"
%token PID_FILE "pid_file"
%token UNIX_SOCKET_DIR "unix_socket_dir"
%token UNIX_SOCKET_MODE "unix_socket_mode"
%token LOCKS_DIR "locks_dir"
%token EXTERNAL_AUTH_SOCKET_PATH "external_auth_socket_path"
%token AVAILABILITY_ZONE "availability_zone"
%token LOG_FILE "log_file"
%token LOG_FORMAT "log_format"
%token LOG_SYSLOG_IDENT "log_syslog_ident"
%token LOG_SYSLOG_FACILITY "log_syslog_facility"
%token CPU_AFFINITY "cpu_affinity"
%token HBA_FILE "hba_file"
%token PRIORITY "priority"
%token GRACEFUL_SHUTDOWN_TIMEOUT_MS "graceful_shutdown_timeout_ms"
%token LOG_QUEUE_DEPTH "log_queue_depth"
%token STATS_INTERVAL "stats_interval"
%token CLIENT_MAX "client_max"
%token CLIENT_MAX_ROUTING "client_max_routing"
%token SERVER_LOGIN_RETRY "server_login_retry"
%token READAHEAD "readahead"
%token KEEPALIVE "keepalive"
%token KEEPALIVE_KEEP_INTERVAL "keepalive_keep_interval"
%token KEEPALIVE_PROBES "keepalive_probes"
%token KEEPALIVE_USR_TIMEOUT "keepalive_usr_timeout"
%token MAX_SIGTERMS_TO_DIE "max_sigterms_to_die"
%token BACKEND_CONNECT_TIMEOUT_MS "backend_connect_timeout_ms"
%token CANCEL_TIMEOUT_MS "cancel_timeout_ms"
%token CANCEL_QUEUE_TIMEOUT_MS "cancel_queue_timeout_ms"
%token CANCEL_MAX_INFLIGHT "cancel_max_inflight"
%token RESOLVERS "resolvers"
%token DNS_CACHE_TTL "dns_cache_ttl"
%token CACHE_MSG_GC_SIZE "cache_msg_gc_size"
%token CACHE_COROUTINE "cache_coroutine"
%token COROUTINE_STACK_SIZE "coroutine_stack_size"
%token PROMHTTP_SERVER_PORT "promhttp_server_port"
%token GROUP_CHECKER_INTERVAL "group_checker_interval"
%token SHARED_POOL "shared_pool"
%token POOL_SIZE "pool_size"
%token CONN_DROP_OPTIONS "conn_drop_options"
%token ONLINE_RESTART_DROP_OPTIONS "online_restart_drop_options"
%token DROP_ENABLED "drop_enabled"
%token RATE "rate"
%token INTERVAL_MS "interval_ms"
%token LISTEN "listen"
%token HOST "host"
%token PORT "port"
%token TARGET_SESSION_ATTRS "target_session_attrs"
%token CLIENT_LOGIN_TIMEOUT "client_login_timeout"
%token BACKLOG "backlog"
%token TLS "tls"
%token TLS_CA_FILE "tls_ca_file"
%token TLS_KEY_FILE "tls_key_file"
%token TLS_CERT_FILE "tls_cert_file"
%token TLS_PROTOCOLS "tls_protocols"
%token CATCHUP_TIMEOUT "catchup_timeout"
%token CATCHUP_CHECKS "catchup_checks"
%token STORAGE "storage"
%token TYPE "type"
%token SERVER_MAX_ROUTING "server_max_routing"
%token ENDPOINTS_STATUS_POLL_INTERVAL "endpoints_status_poll_interval"
%token BALANCING "balancing"
%token METHOD "method"
%token SHOW_NOTICE_MESSAGES "show_notice_messages"
%token COMPRESSION "compression"
%token WORKERS "workers"
%token PIPELINE "pipeline"
%token CACHE "cache"
%token CACHE_CHUNK "cache_chunk"
%token PACKET_READ_SIZE "packet_read_size"
%token PACKET_WRITE_QUEUE "packet_write_queue"
%token SOFT_OOM "soft_oom"
%token LIMIT "limit"
%token PROCESS "process"
%token CHECK_INTERVAL_MS "check_interval_ms"
%token DROP "drop"
%token SIGNAL "signal"
%token MAX_RATE "max_rate"
%token LDAP_ENDPOINT "ldap_endpoint"
%token LDAP_SERVER "ldapserver"
%token LDAP_PORT "ldapport"
%token LDAP_PREFIX "ldapprefix"
%token LDAP_SUFFIX "ldapsuffix"
%token LDAP_BASEDN "ldapbasedn"
%token LDAP_BINDDN "ldapbinddn"
%token LDAP_BIND_PASSWD "ldapbindpasswd"
%token LDAP_SEARCH_ATTRIBUTE "ldapsearchattribute"
%token LDAP_SCHEME "ldapscheme"
%token LDAP_SCOPE "ldapscope"
%token LDAP_SEARCH_FILTER "ldapsearchfilter"
%token DEFAULT "default"
%token DATABASE "database"
%token USER "user"
%token LOCAL "local"
%token HOSTSSL "hostssl"
%token HOSTNOSSL "hostnossl"
%token LDAP_STORAGE_USERNAME "ldap_storage_username"
%token LDAP_STORAGE_PASSWORD "ldap_storage_password"
%token AUTH_COMMON_NAME "auth_common_name"
%token AUTHENTICATION "authentication"
%token AUTH_MODULE "auth_module"
%token ENABLE_MDB_IAMPROXY_AUTH "enable_mdb_iamproxy_auth"
%token MDB_IAMPROXY_SOCKET_PATH "mdb_iamproxy_socket_path"
%token AUTH_PAM_SERVICE "auth_pam_service"
%token AUTH_QUERY "auth_query"
%token AUTH_QUERY_DB "auth_query_db"
%token AUTH_QUERY_USER "auth_query_user"
%token PASSWORD_PASSTHROUGH "password_passthrough"
%token PASSWORD "password"
%token ROLE "role"
%token CLIENT_FWD_ERROR "client_fwd_error"
%token CLIENT_SHOW_ID "client_show_id"
%token RESERVE_SESSION_SERVER_CONNECTION "reserve_session_server_connection"
%token QUANTILES "quantiles"
%token APPLICATION_NAME_ADD_HOST "application_name_add_host"
%token SERVER_DROP_ON_BACKEND_PLAN_ERROR "server_drop_on_backend_plan_error"
%token SERVER_LIFETIME "server_lifetime"
%token LDAP_POOL_SIZE "ldap_pool_size"
%token LDAP_POOL_TIMEOUT "ldap_pool_timeout"
%token LDAP_POOL_TTL "ldap_pool_ttl"
%token MAINTAIN_PARAMS "maintain_params"
%token POOL "pool"
%token POOL_ROUTING "pool_routing"
%token MIN_POOL_SIZE "min_pool_size"
%token POOL_TIMEOUT "pool_timeout"
%token POOL_TTL "pool_ttl"
%token POOL_DISCARD "pool_discard"
%token POOL_SMART_DISCARD "pool_smart_discard"
%token POOL_DISCARD_QUERY "pool_discard_query"
%token POOL_CANCEL "pool_cancel"
%token POOL_ROLLBACK "pool_rollback"
%token POOL_RESET_TIMEOUT_MS "pool_reset_timeout_ms"
%token POOL_RESERVE_PREPARED_STATEMENT "pool_reserve_prepared_statement"
%token POOL_PIN_ON_LISTEN "pool_pin_on_listen"
%token POOL_ATTACH_CHECK "pool_attach_check"
%token POOL_ACQUIRE_FAIL_FAST "pool_acquire_fail_fast"
%token POOL_NOTICE_AFTER_WAITING_MS "pool_notice_after_waiting_ms"
%token POOL_CLIENT_IDLE_TIMEOUT "pool_client_idle_timeout"
%token POOL_IDLE_IN_TRANSACTION_TIMEOUT "pool_idle_in_transaction_timeout"
%token STORAGE_DATABASE "storage_db"
%token STORAGE_USER "storage_user"
%token STORAGE_PASSWORD "storage_password"
%token LDAP_ENDPOINT_NAME "ldap_endpoint_name"
%token LDAP_STORAGE_CREDENTIALS_ATTR "ldap_storage_credentials_attr"
%token LDAP_STORAGE_CREDENTIALS "ldap_storage_credentials"
%token WATCHDOG_LAG_QUERY "watchdog_lag_query"
%token WATCHDOG_LAG_INTERVAL "watchdog_lag_interval"
%token WATCHDOG "watchdog"
%token OPTIONS "options"
%token BACKEND_STARTUP_OPTIONS "backend_startup_options"
%token GROUP_QUERY "group_query"
%token GROUP_QUERY_USER "group_query_user"
%token GROUP_QUERY_DB "group_query_db"
%token GROUP "group"

%type <boolean> bool_value
%type <str> string_value
%type <num> int_value
%type <num> conn_type
%type <num> optional_conn_type
%type <str> optional_string_value

%%

config:
	{ ctx->model->location = @$; }
	top_items
	;

top_items:
	  %empty
	| top_items top_item
	;

top_item:
	DAEMONIZE bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.daemonize,
							$2,
							@1,
							"daemonize");
		}
	| SEQUENTIAL_ROUTING bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.sequential_routing,
							$2,
							@1,
							"sequential_routing");
		}
	| ENABLE_ONLINE_RESTART bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.enable_online_restart,
							$2,
							@1,
							"enable_online_restart");
		}
	| VIRTUAL_PROCESSING bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.virtual_processing,
							$2,
							@1,
							"virtual_processing");
		}
	| GRACEFUL_DIE_ON_ERRORS bool_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"graceful_die_on_errors");
		}
	| BINDWITH_REUSEPORT bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.bindwith_reuseport,
							$2,
							@1,
							"bindwith_reuseport");
		}
	| ENABLE_HOST_WATCHER bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.enable_host_watcher,
							$2,
							@1,
							"enable_host_watcher");
		}
	| LOG_DEBUG bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_debug,
							$2,
							@1,
							"log_debug");
		}
	| LOG_TO_STDOUT bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_to_stdout,
							$2,
							@1,
							"log_to_stdout");
		}
	| LOG_CONFIG bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_config,
							$2,
							@1,
							"log_config");
		}
	| LOG_SESSION bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_session,
							$2,
							@1,
							"log_session");
		}
	| LOG_QUERY bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_query,
							$2,
							@1,
							"log_query");
		}
	| LOG_STATS bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_stats,
							$2,
							@1,
							"log_stats");
		}
	| LOG_ASYNC bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_async,
							$2,
							@1,
							"log_async");
		}
	| LOG_SYSLOG bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_syslog,
							$2,
							@1,
							"log_syslog");
		}
	| SMART_SEARCH_PATH_ENQUOTING bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.smart_search_path_enquoting,
							$2,
							@1,
							"smart_search_path_enquoting");
		}
	| NODELAY bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.nodelay,
							$2,
							@1,
							"nodelay");
		}
	| DISABLE_NOLINGER bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.disable_nolinger,
							$2,
							@1,
							"disable_nolinger");
		}
	| LOG_GENERAL_STATS_PROM bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_general_stats_prom,
							$2,
							@1,
							"log_general_stats_prom");
		}
	| LOG_ROUTE_STATS_PROM bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->model->global.log_route_stats_prom,
							$2,
							@1,
							"log_route_stats_prom");
		}
	| PID_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.pid_file,
							  $2,
							  @1,
							  "pid_file");
			$2 = NULL;
		}
	| UNIX_SOCKET_DIR string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.unix_socket_dir,
							  $2,
							  @1,
							  "unix_socket_dir");
			$2 = NULL;
		}
	| UNIX_SOCKET_MODE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.unix_socket_mode,
							  $2,
							  @1,
							  "unix_socket_mode");
			$2 = NULL;
		}
	| LOCKS_DIR string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.locks_dir,
							  $2,
							  @1,
							  "locks_dir");
			$2 = NULL;
		}
	| EXTERNAL_AUTH_SOCKET_PATH string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.external_auth_socket_path,
							  $2,
							  @1,
							  "external_auth_socket_path");
			$2 = NULL;
		}
	| AVAILABILITY_ZONE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.availability_zone,
							  $2,
							  @1,
							  "availability_zone");
			$2 = NULL;
		}
	| LOG_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.log_file,
							  $2,
							  @1,
							  "log_file");
			$2 = NULL;
		}
	| LOG_FORMAT string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.log_format,
							  $2,
							  @1,
							  "log_format");
			$2 = NULL;
		}
	| LOG_SYSLOG_IDENT string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.log_syslog_ident,
							  $2,
							  @1,
							  "log_syslog_ident");
			$2 = NULL;
		}
	| LOG_SYSLOG_FACILITY string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.log_syslog_facility,
							  $2,
							  @1,
							  "log_syslog_facility");
			$2 = NULL;
		}
	| CPU_AFFINITY string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.cpu_affinity,
							  $2,
							  @1,
							  "cpu_affinity");
			$2 = NULL;
		}
	| HBA_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->model->global.hba_file,
							  $2,
							  @1,
							  "hba_file");
			$2 = NULL;
		}
	| PRIORITY int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.priority,
							$2,
							@1,
							"priority");
		}
	| GRACEFUL_SHUTDOWN_TIMEOUT_MS int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.graceful_shutdown_timeout_ms,
							$2,
							@1,
							"graceful_shutdown_timeout_ms");
		}
	| LOG_QUEUE_DEPTH int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.log_queue_depth,
							$2,
							@1,
							"log_queue_depth");
		}
	| STATS_INTERVAL int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.stats_interval,
							$2,
							@1,
							"stats_interval");
		}
	| CLIENT_MAX int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.client_max,
							$2,
							@1,
							"client_max");
		}
	| CLIENT_MAX_ROUTING int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.client_max_routing,
							$2,
							@1,
							"client_max_routing");
		}
	| SERVER_LOGIN_RETRY int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.server_login_retry,
							$2,
							@1,
							"server_login_retry");
		}
	| READAHEAD int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.readahead,
							$2,
							@1,
							"readahead");
		}
	| KEEPALIVE int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.keepalive,
							$2,
							@1,
							"keepalive");
		}
	| KEEPALIVE_KEEP_INTERVAL int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.keepalive_keep_interval,
							$2,
							@1,
							"keepalive_keep_interval");
		}
	| KEEPALIVE_PROBES int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.keepalive_probes,
							$2,
							@1,
							"keepalive_probes");
		}
	| KEEPALIVE_USR_TIMEOUT int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.keepalive_usr_timeout,
							$2,
							@1,
							"keepalive_usr_timeout");
		}
	| MAX_SIGTERMS_TO_DIE int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.max_sigterms_to_die,
							$2,
							@1,
							"max_sigterms_to_die");
		}
	| BACKEND_CONNECT_TIMEOUT_MS int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.backend_connect_timeout_ms,
							$2,
							@1,
							"backend_connect_timeout_ms");
		}
	| CANCEL_TIMEOUT_MS int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.cancel_timeout_ms,
							$2,
							@1,
							"cancel_timeout_ms");
		}
	| CANCEL_QUEUE_TIMEOUT_MS int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.cancel_queue_timeout_ms,
							$2,
							@1,
							"cancel_queue_timeout_ms");
		}
	| CANCEL_MAX_INFLIGHT int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.cancel_max_inflight,
							$2,
							@1,
							"cancel_max_inflight");
		}
	| RESOLVERS int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
							&ctx->model->global.resolvers,
							$2,
							1,
							INT_MAX,
							@1,
							"resolvers");
		}
	| DNS_CACHE_TTL int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.dns_cache_ttl,
							$2,
							@1,
							"dns_cache_ttl");
		}
	| CACHE_MSG_GC_SIZE int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.cache_msg_gc_size,
							$2,
							@1,
							"cache_msg_gc_size");
		}
	| CACHE_COROUTINE int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.cache_coroutine,
							$2,
							@1,
							"cache_coroutine");
		}
	| COROUTINE_STACK_SIZE int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.coroutine_stack_size,
							$2,
							@1,
							"coroutine_stack_size");
		}
	| PROMHTTP_SERVER_PORT int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
							&ctx->model->global.promhttp_server_port,
							$2,
							1, 65535,
							@1,
							"promhttp_server_port");
		}
	| GROUP_CHECKER_INTERVAL int_value
		{
			od_cfg_set_int_from_i64(ctx->diags,
							&ctx->model->global.group_checker_interval,
							$2,
							@1,
							"group_checker_interval");
		}
	| WORKERS int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
							&ctx->model->global.workers,
							$2,
							1,
							INT_MAX,
							@1,
							"workers");
		}
	| WORKERS string_value
		{
			if (strcmp($2, "auto") != 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "'%s' is invalid value for workers", $2);
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			od_cfg_set_int_from_i64(ctx->diags,
						&ctx->model->global.workers,
						(1 + od_get_ncpu()) / 2,
						@1,
						"workers");
			od_free($2);
			$2 = NULL;
		}
	| PIPELINE int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
											@1,
											"pipeline");
		}
	| CACHE int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
											@1,
											"cache");
		}
	| CACHE_CHUNK int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
											@1,
											"cache_chunk");
		}
	| PACKET_READ_SIZE int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
											@1,
											"packet_read_size");
		}
	| PACKET_WRITE_QUEUE int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
											@1,
											"packet_write_queue");
		}
	| listen_section
	| storage_section
	| database_section
	| ldap_endpoint_section
	| shared_pool_section
	| conn_drop_options_section
	| soft_oom_section
	;

database_section:
	  database_named
	| database_default
	;

database_named:
	DATABASE string_value
		{
			od_cfg_database_t *db = od_cfg_model_add_database(ctx->model,
															  $2,
															  0 /* is_default */,
															  @2, @1);
			if (db == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate database");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			$2 = NULL;

			ctx->current_database = db;
		}
	'{' database_items '}'
	{
		ctx->current_database = NULL;
	}
	;

database_default:
	DATABASE DEFAULT
		{
			od_cfg_database_t *db = od_cfg_model_add_database(ctx->model,
															  NULL,
															  1 /* is_default */,
															  @2, @1);
			if (db == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate database");
				YYERROR;
			}

			ctx->current_database = db;
		}
	'{' database_items '}'
	{
		ctx->current_database = NULL;
	}
	;

database_items:
	  %empty
	| database_items database_item
	;

database_item:
	user_route_item
	;

user_route_item:
	  user_named
	| user_default
	| group
	;

group:
	GROUP string_value optional_string_value optional_conn_type
		{
			ctx->current_group = od_cfg_database_add_group(ctx->current_database,
														   $2,
														   @1,
														   @1,
														   $3,
														   @1,
														   $4,
														   @1);
			if (ctx->current_group == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate group");
				od_free($2);
				od_free($3);
				$2 = NULL;
				$3 = NULL;
				YYERROR;
			}
			ctx->current_user = ctx->current_group;
			$2 = NULL;
			$3 = NULL;
		}
		'{' route_items '}'
		{
			ctx->current_user = NULL;
			ctx->current_group = NULL;
		}

user_default:
	USER DEFAULT optional_string_value optional_conn_type
		{
			od_cfg_route_t *user = od_cfg_database_add_user(ctx->current_database,
																 NULL,
																 1 /* is_default */,
																 @1,
																 @1,
																 $3,
																 @1,
																 $4,
																 @1);
			if (user == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate user");
				$3 = NULL;
				YYERROR;
			}

			$3 = NULL;

			ctx->current_user = user;
		}
		'{' route_items '}'
		{
			ctx->current_user = NULL;
		}
	;

user_named:
	USER string_value optional_string_value optional_conn_type
		{
			od_cfg_route_t *user = od_cfg_database_add_user(ctx->current_database,
																 $2,
																 0 /* is_default */,
																 @1,
																 @1,
																 $3,
																 @1,
																 $4,
																 @1);
			if (user == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate user");
				od_free($2);
				$2 = NULL;
				$3 = NULL;
				YYERROR;
			}

			$2 = NULL;
			$3 = NULL;
			ctx->current_user = user;
		}
		'{' route_items '}'
		{
			ctx->current_user = NULL;
		}
	;


optional_string_value:
	  string_value { $$ = $1; }
	| %empty       { $$ = NULL; }

optional_conn_type:
	  conn_type { $$ = $1; }
	| %empty    { $$ = OD_CFG_CONN_TYPE_DEFAULT; }
	;

conn_type:
	  DEFAULT   { $$ = OD_CFG_CONN_TYPE_DEFAULT; }
	| HOST      { $$ = OD_CFG_CONN_TYPE_HOST; }
	| HOSTSSL   { $$ = OD_CFG_CONN_TYPE_HOSTSSL; }
	| HOSTNOSSL { $$ = OD_CFG_CONN_TYPE_HOSTNOSSL; }
	| LOCAL     { $$ = OD_CFG_CONN_TYPE_LOCAL; }
	;

route_items:
	  %empty
	| route_items route_item
	;

route_item:
	  AUTH_COMMON_NAME string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_rule_auth_common_name_t *cn = od_cfg_user_route_add_auth_common_name(ur,
																						$2,
																						0 /* is_default */,
																						@1,
																						@1);
			if (cn == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate auth common name");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			$2 = NULL;
		}
	| AUTH_COMMON_NAME DEFAULT
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_rule_auth_common_name_t *cn = od_cfg_user_route_add_auth_common_name(ur,
																						NULL,
																						1 /* is_default */,
																						@1,
																						@1);
			if (cn == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate auth common name");
				YYERROR;
			}
		}
	| AUTHENTICATION string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->authentication,
							  $2,
							  @1,
							  "authentication");
			$2 = NULL;
		}
	| AUTH_MODULE string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->auth_module,
							  $2,
							  @1,
							  "auth_module");
			$2 = NULL;
		}
	| ENABLE_MDB_IAMPROXY_AUTH bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->enable_mdb_iamproxy_auth,
							$2,
							@1,
							"enable_mdb_iamproxy_auth");
		}
	| MDB_IAMPROXY_SOCKET_PATH string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->mdb_iamproxy_socket_path,
							  $2,
							  @1,
							  "mdb_iamproxy_socket_path");
			$2 = NULL;
		}
	| AUTH_PAM_SERVICE string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->auth_pam_service,
							  $2,
							  @1,
							  "auth_pam_service");
			$2 = NULL;
		}
	
	| TARGET_SESSION_ATTRS string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->target_session_attrs,
							  $2,
							  @1,
							  "target_session_attrs");
			$2 = NULL;
		}

	| AUTH_QUERY string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->auth_query,
							  $2,
							  @1,
							  "auth_query");
			$2 = NULL;
		}
	| AUTH_QUERY_DB string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->auth_query_db,
							  $2,
							  @1,
							  "auth_query_db");
			$2 = NULL;
		}
	| AUTH_QUERY_USER string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->auth_query_user,
							  $2,
							  @1,
							  "auth_query_user");
			$2 = NULL;
		}
	| PASSWORD_PASSTHROUGH bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->password_passthrough,
							$2,
							@1,
							"password_passthrough");
		}
	| PASSWORD string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->password,
							  $2,
							  @1,
							  "password");
			$2 = NULL;
		}
	| ROLE string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->role,
							  $2,
							  @1,
							  "role");
			$2 = NULL;
		}
	| CLIENT_MAX int_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_int_range_from_i64(ctx->diags,
										  &ur->client_max,
										  $2,
										  1,
										  INT_MAX,
										  @1,
										  "client_max");
		}
	| CLIENT_FWD_ERROR bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->client_fwd_error,
							$2,
							@1,
							"client_fwd_error");
		}
	| CLIENT_SHOW_ID bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->client_show_id,
							$2,
							@1,
							"client_show_id");
		}
	| RESERVE_SESSION_SERVER_CONNECTION bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->reserve_session_server_connection,
							$2,
							@1,
							"reserve_session_server_connection");
		}
	| QUANTILES string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->quantiles,
							  $2,
							  @1,
							  "quantiles");
			$2 = NULL;
		}
	| APPLICATION_NAME_ADD_HOST bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->application_name_add_host,
							$2,
							@1,
							"application_name_add_host");
		}
	| SERVER_DROP_ON_BACKEND_PLAN_ERROR bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->server_drop_on_backend_plan_error,
							$2,
							@1,
							"server_drop_on_backend_plan_error");
		}
	| SERVER_LIFETIME int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "server_lifitime must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_u64(ctx->diags,
							   &ur->server_lifetime,
							   (uint64_t)$2,
							   @1,
							   "server_lifetime");
			}
		}
	| LDAP_POOL_SIZE int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"ldap_pool_size");
		}
	| LDAP_POOL_TIMEOUT int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"ldap_pool_timeout");
		}
	| LDAP_POOL_TTL int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"ldap_pool_ttl");
		}
	| MAINTAIN_PARAMS bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->maintain_params,
							$2,
							@1,
							"maintain_params");
		}
	| POOL string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->pool,
							  $2,
							  @1,
							  "pool");
			$2 = NULL;
		}
	| POOL_ROUTING string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->pool_routing,
							  $2,
							  @1,
							  "pool_routing");
			$2 = NULL;
		}
	| MIN_POOL_SIZE int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "min_pool_size must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_int(ctx->diags,
							   &ur->min_pool_size,
							   (uint64_t)$2,
							   @1,
							   "min_pool_size");
			}
		}
	| POOL_SIZE int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_size must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_int(ctx->diags,
							   &ur->pool_size,
							   (uint64_t)$2,
							   @1,
							   "pool_size");
			}
		}
	| POOL_TIMEOUT int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_timeout must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_int(ctx->diags,
							   &ur->pool_timeout,
							   (uint64_t)$2,
							   @1,
							   "pool_timeout");
			}
		}
	| POOL_TTL int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_ttl must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_int(ctx->diags,
							   &ur->pool_ttl,
							   (uint64_t)$2,
							   @1,
							   "pool_ttl");
			}
		}
	| POOL_DISCARD bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_discard,
							$2,
							@1,
							"pool_discard");
		}
	| POOL_SMART_DISCARD bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_smart_discard,
							$2,
							@1,
							"pool_smart_discard");
		}
	| POOL_DISCARD_QUERY string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->pool_discard_query,
							  $2,
							  @1,
							  "pool_discard_query");
			$2 = NULL;
		}
	| POOL_CANCEL bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_cancel,
							$2,
							@1,
							"pool_cancel");
		}
	| POOL_ROLLBACK bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_rollback,
							$2,
							@1,
							"pool_rollback");
		}
	| POOL_RESET_TIMEOUT_MS int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_reset_timeout_ms must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_u64(ctx->diags,
							   &ur->pool_reset_timeout_ms,
							   (uint64_t)$2,
							   @1,
							   "pool_reset_timeout_ms");
			}
		}
	| POOL_RESERVE_PREPARED_STATEMENT bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_reserve_prepared_statement,
							$2,
							@1,
							"pool_reserve_prepared_statement");
		}
	| POOL_PIN_ON_LISTEN bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_pin_on_listen,
							$2,
							@1,
							"pool_pin_on_listen");
		}
	| POOL_ATTACH_CHECK bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_attach_check,
							$2,
							@1,
							"pool_attach_check");
		}
	| POOL_ACQUIRE_FAIL_FAST bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->pool_acquire_fail_fast,
							$2,
							@1,
							"pool_acquire_fail_fast");
		}
	| POOL_NOTICE_AFTER_WAITING_MS int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_notice_after_waiting_ms must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_u64(ctx->diags,
							   &ur->pool_notice_after_waiting_ms,
							   (uint64_t)$2,
							   @1,
							   "pool_notice_after_waiting_ms");
			}
		}
	| POOL_CLIENT_IDLE_TIMEOUT int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_client_idle_timeout must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_u64(ctx->diags,
							   &ur->pool_client_idle_timeout,
							   (uint64_t)$2,
							   @1,
							   "pool_client_idle_timeout");
			}
		}
	| POOL_IDLE_IN_TRANSACTION_TIMEOUT int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "pool_idle_in_transaction_timeout must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_u64(ctx->diags,
							   &ur->pool_idle_in_transaction_timeout,
							   (uint64_t)$2,
							   @1,
							   "pool_idle_in_transaction_timeout");
			}
		}
	| SHARED_POOL string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->shared_pool,
							  $2,
							  @1,
							  "shared_pool");
			$2 = NULL;
		}
	| STORAGE string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->storage,
							  $2,
							  @1,
							  "storage");
			$2 = NULL;
		}
	| STORAGE_DATABASE string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->storage_database,
							  $2,
							  @1,
							  "storage_database");
			$2 = NULL;
		}
	| STORAGE_USER string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->storage_user,
							  $2,
							  @1,
							  "storage_user");
			$2 = NULL;
		}
	| STORAGE_PASSWORD string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->storage_password,
							  $2,
							  @1,
							  "storage_password");
			$2 = NULL;
		}
	| LOG_DEBUG bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->log_debug,
							$2,
							@1,
							"log_debug");
		}
	| LOG_QUERY bool_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_bool(ctx->diags,
							&ur->log_query,
							$2,
							@1,
							"log_query");
		}
	| LDAP_ENDPOINT_NAME string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->ldap_endpoint_name,
							  $2,
							  @1,
							  "ldap_endpoint_name");
			$2 = NULL;
		}
	| LDAP_STORAGE_CREDENTIALS_ATTR string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_set_string(ctx->diags,
							  &ur->ldap_storage_credentials_attr,
							  $2,
							  @1,
							  "ldap_storage_credentials_attr");
			$2 = NULL;
		}
	| LDAP_STORAGE_CREDENTIALS string_value
		{
			od_cfg_database_t *db = ctx->current_database;
			od_cfg_route_t *ur = ctx->current_user;

			od_cfg_ldap_storage_credentials_t *creds = od_cfg_user_route_add_ldap_creds(ur, @1, $2);
			if (creds == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "can't allocate ldap storage credentials for %s.%s",
								  db->name, ur->name);
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			$2 = NULL;

			ctx->current_ldap_creds = creds;
		}
		'{' ldap_storage_credentials_items '}'
	| WATCHDOG_LAG_QUERY string_value
		{
			od_cfg_route_t *ur = ctx->current_user;
			od_cfg_database_t *db = ctx->current_database;

			if (ctx->current_watchdog == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "watchdog_lag_query is not allowed outside watchdog, rule %s.%s",
								  db->name, ur->name);
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			od_cfg_set_string(ctx->diags,
							  &ur->watchdog_lag_query,
							  $2,
							  @1,
							  "watchdog_lag_query");
			$2 = NULL;
		}
	| WATCHDOG_LAG_INTERVAL int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "watchdog_lag_interval must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_database_t *db = ctx->current_database;

				if (ctx->current_watchdog == NULL) {
					od_cfg_diag_error(ctx->diags, @1,
									"watchdog_lag_interval is not allowed outside watchdog, rule %s.%s",
									db->name, ur->name);
					YYERROR;
				}

				od_cfg_set_int(ctx->diags,
							   &ur->watchdog_lag_interval,
							   (uint64_t)$2,
							   @1,
							   "watchdog_lag_interval");
			}
		}
	| CATCHUP_TIMEOUT int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "catchup_timeout must be non-negative");
			} else {
				od_cfg_route_t *ur = ctx->current_user;
				od_cfg_set_int(ctx->diags,
							   &ur->catchup_timeout,
							   (uint64_t)$2,
							   @1,
							   "catchup_timeout");
			}
		}
	| CATCHUP_CHECKS int_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"catchup_checks");
		}
	| GROUP_QUERY string_value
		{
			od_cfg_database_t *db = ctx->current_database;
			od_cfg_route_t *ur = ctx->current_user;

			if (ctx->current_group == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "group_query is not allowed outside group, rule %s.%s",
								  db->name, ur->name);
				YYERROR;
			}

			od_cfg_set_string(ctx->diags,
							  &ur->group_query,
							  $2,
							  @1,
							  "group_query");
			$2 = NULL;
		}
	| GROUP_QUERY_USER string_value
		{
			od_cfg_database_t *db = ctx->current_database;
			od_cfg_route_t *ur = ctx->current_user;

			if (ctx->current_group == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "group_query_user is not allowed outside group, rule %s.%s",
								  db->name, ur->name);
				YYERROR;
			}

			od_cfg_set_string(ctx->diags,
							  &ur->group_query_user,
							  $2,
							  @1,
							  "group_query_user");
			$2 = NULL;
		}
	| GROUP_QUERY_DB string_value
		{
			od_cfg_database_t *db = ctx->current_database;
			od_cfg_route_t *ur = ctx->current_user;

			if (ctx->current_group == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "group_query_db is not allowed outside group, rule %s.%s",
								  db->name, ur->name);
				YYERROR;
			}

			od_cfg_set_string(ctx->diags,
							  &ur->group_query_db,
							  $2,
							  @1,
							  "group_query_db");
			$2 = NULL;
		}
	| OPTIONS
		{
			ctx->current_kvp_list = &ctx->current_user->pgoptions;
		}
		'{' kvp_items '}'
		{
			ctx->current_kvp_list = NULL;
		}
	| BACKEND_STARTUP_OPTIONS
		{
			ctx->current_kvp_list = &ctx->current_user->backend_startup_options;
		}
		'{' kvp_items '}'
		{
			ctx->current_kvp_list = NULL;
		}
	;

kvp_items:
	  %empty
	| kvp_items string_value string_value
		{
			od_cfg_string_kvp_t *p = od_cfg_string_kvp_list_add(ctx->current_kvp_list,
																@1,
																$2,
																@1,
																$3,
																@1);
			if (p == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate key value pair");
				od_free($2);
				od_free($3);
				$2 = NULL;
				$3 = NULL;
				YYERROR;
			}

			$2 = NULL;
			$3 = NULL;
		}
	;

ldap_storage_credentials_items:
	  %empty
	| ldap_storage_credentials_items ldap_storage_credentials_item
	;

ldap_storage_credentials_item:
	  LDAP_STORAGE_USERNAME string_value
		{
			od_cfg_ldap_storage_credentials_t *ur = ctx->current_ldap_creds;
			od_cfg_set_string(ctx->diags,
							  &ur->ldap_storage_username,
							  $2,
							  @1,
							  "ldap_storage_username");
			$2 = NULL;
		}
	| LDAP_STORAGE_PASSWORD string_value
		{
			od_cfg_ldap_storage_credentials_t *ur = ctx->current_ldap_creds;
			od_cfg_set_string(ctx->diags,
							  &ur->ldap_storage_password,
							  $2,
							  @1,
							  "ldap_storage_password");
			$2 = NULL;
		}
	;

ldap_endpoint_section:
	  LDAP_ENDPOINT string_value
		{
			ctx->current_ldap_endpoint =
				od_cfg_model_add_ldap_endpoint(ctx->model, $2, @2, @1);

			if (ctx->current_ldap_endpoint == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate ldap_endpoint");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			$2 = NULL;
		}
	  '{' ldap_endpoint_items '}'
		{
			ctx->current_ldap_endpoint = NULL;
		}
	;

ldap_endpoint_items:
	  %empty
	| ldap_endpoint_items ldap_endpoint_item
	;

ldap_endpoint_item:
	  LDAP_SERVER string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapserver,
							  $2, @1, "ldapserver");
			$2 = NULL;
		}
	| LDAP_PORT int_value
		{
			if ($2 < 0) {
				od_cfg_diag_error(ctx->diags, @1,
								  "ldapport must be non-negative");
			} else {
				od_cfg_set_u64(ctx->diags,
							   &ctx->current_ldap_endpoint->ldapport,
							   (uint64_t)$2,
							   @1,
							   "ldapport");
			}
		}
	| LDAP_PREFIX string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapprefix,
							  $2, @1, "ldapprefix");
			$2 = NULL;
		}
	| LDAP_SUFFIX string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapsuffix,
							  $2, @1, "ldapsuffix");
			$2 = NULL;
		}
	| LDAP_SEARCH_ATTRIBUTE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapsearchattribute,
							  $2, @1, "ldapsearchattribute");
			$2 = NULL;
		}
	| LDAP_SCOPE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapscope,
							  $2, @1, "ldapscope");
			$2 = NULL;
		}
	| LDAP_SCHEME string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapscheme,
							  $2, @1, "ldapscheme");
			$2 = NULL;
		}
	| LDAP_BASEDN string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapbasedn,
							  $2, @1, "ldapbasedn");
			$2 = NULL;
		}
	| LDAP_BINDDN string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapbinddn,
							  $2, @1, "ldapbinddn");
			$2 = NULL;
		}
	| LDAP_BIND_PASSWD string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapbindpasswd,
							  $2, @1, "ldapbindpasswd");
			$2 = NULL;
		}
	| LDAP_SEARCH_FILTER string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_ldap_endpoint->ldapsearchfilter,
							  $2, @1, "ldapsearchfilter");
			$2 = NULL;
		}
	;



storage_section:
	  STORAGE string_value
		{
			ctx->current_storage =
				od_cfg_model_add_storage(ctx->model, $2, @2, @1);

			if (ctx->current_storage == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate storage section");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			$2 = NULL;
		}
	  '{' storage_items '}'
		{
			ctx->current_storage = NULL;
		}
	;

storage_items:
	  %empty
	| storage_items storage_item
	;

storage_item:
	  TYPE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->type,
							  $2,
							  @1,
							  "type");
			$2 = NULL;
		}
	| HOST string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->host,
							  $2,
							  @1,
							  "host");
			$2 = NULL;
		}
	| PORT int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_storage->port,
									$2,
									1,
									65535,
									@1,
									"port");
		}
	| TLS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->tls,
							  $2,
							  @1,
							  "tls");
			$2 = NULL;
		}
	| TLS_CA_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->tls_ca_file,
							  $2, @1, "tls_ca_file");
			$2 = NULL;
		}
	| TLS_KEY_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->tls_key_file,
							  $2,
							  @1,
							  "tls_key_file");
			$2 = NULL;
		}
	| TLS_CERT_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->tls_cert_file,
							  $2,
							  @1,
							  "tls_cert_file");
			$2 = NULL;
		}
	| TLS_PROTOCOLS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_storage->tls_protocols,
							  $2,
							  @1,
							  "tls_protocols");
			$2 = NULL;
		}
	| SERVER_MAX_ROUTING int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_storage->server_max_routing,
									$2,
									0, INT_MAX,
									@1,
									"server_max_routing");
		}
	| ENDPOINTS_STATUS_POLL_INTERVAL int_value
		{
			od_cfg_set_int_range_from_i64(
				ctx->diags,
				&ctx->current_storage->endpoints_status_poll_interval_ms,
				$2,
				0, INT_MAX,
				@1,
				"endpoints_status_poll_interval");
		}
	| WATCHDOG
		{
			od_cfg_storage_t *s = ctx->current_storage;

			if (s->watchdog != NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "redefinition of watchdog for storage '%s'", s->name);
				YYERROR;
			}

			od_cfg_route_t *r = od_cfg_storage_add_watchdog(s, @1);
			if (r == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "can't allocate watchdog for storage '%s'", s->name);
				YYERROR;
			}

			ctx->current_watchdog = r;
			ctx->current_user = r;
		}
		'{' route_items '}'
		{
			ctx->current_watchdog = NULL;
			ctx->current_user = NULL;
		}
	| balancing_section
	;

balancing_section:
	  BALANCING
		{
			od_cfg_balancing_t *b = &ctx->current_storage->balancing;

			if (b->seen.is_set) {
				od_cfg_diag_error(ctx->diags, @1,
								  "balancing section is redefined for storage '%s'",
								  ctx->current_storage->name);
				YYERROR;
			}

			b->seen.is_set = 1;
			b->seen.location = @1;
			b->location = @1;

			ctx->current_balancing = b;
		}
	  '{' balancing_items '}'
		{
			ctx->current_balancing = NULL;
		}
	;


balancing_items:
	  %empty
	| balancing_items balancing_item
	;

balancing_item:
	  balancing_method_section
	| SHOW_NOTICE_MESSAGES bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->current_balancing->show_notice_messages,
							$2,
							@1,
							"show_notice_messages");
		}
	;

balancing_method_section:
	  METHOD string_value
		{
			od_cfg_balancing_method_t *m =
				&ctx->current_balancing->method;

			if (m->seen.is_set) {
				od_cfg_diag_error(ctx->diags, @1,
								  "balancing method is redefined");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}

			m->seen.is_set = 1;
			m->seen.location = @1;
			m->location = @1;

			m->name = $2;
			$2 = NULL;

			ctx->current_balancing_method = m;
		}
	  '{' balancing_method_items '}'
		{
			ctx->current_balancing_method = NULL;
		}
	;

balancing_method_items:
	  %empty
	;

listen_section:
	  LISTEN
		{
			ctx->current_listen =
				od_cfg_model_add_listen(ctx->model, @1);

			if (ctx->current_listen == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate listen section");
				YYERROR;
			}
		}
	  '{' listen_items '}'
		{
			ctx->current_listen = NULL;
		}
	;

listen_items:
	  %empty
	| listen_items listen_item
	;

listen_item:
	  HOST string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->host,
							  $2,
							  @1,
							  "host");
			$2 = NULL;
		}
	| PORT int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_listen->port,
									$2,
									1, 65535,
									@1,
									"port");
		}
	| TARGET_SESSION_ATTRS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->target_session_attrs,
							  $2,
							  @1,
							  "target_session_attrs");
			$2 = NULL;
		}
	| CLIENT_LOGIN_TIMEOUT int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_listen->client_login_timeout,
									$2,
									0, INT_MAX,
									@1,
									"client_login_timeout");
		}
	| BACKLOG int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_listen->backlog,
									$2,
									0, INT_MAX,
									@1,
									"backlog");
		}
	| TLS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->tls,
							  $2,
							  @1,
							  "tls");
			$2 = NULL;
		}
	| TLS_CA_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->tls_ca_file,
							  $2,
							  @1,
							  "tls_ca_file");
			$2 = NULL;
		}
	| TLS_KEY_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->tls_key_file,
							  $2,
							  @1,
							  "tls_key_file");
			$2 = NULL;
		}
	| TLS_CERT_FILE string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->tls_cert_file,
							  $2,
							  @1,
							  "tls_cert_file");
			$2 = NULL;
		}
	| TLS_PROTOCOLS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_listen->tls_protocols,
							  $2,
							  @1,
							  "tls_protocols");
			$2 = NULL;
		}
	| CATCHUP_TIMEOUT int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
								&ctx->current_listen->catchup_timeout,
								$2,
								0, INT_MAX,
								@1,
								"catchup_timeout");
		}
	| COMPRESSION bool_value
		{
			od_cfg_diag_deprecated_ignored(ctx->diags,
										@1,
										"compression");
		}
	;

shared_pool_section:
	  SHARED_POOL string_value
		{
			ctx->current_shared_pool =
				od_cfg_model_add_shared_pool(ctx->model, $2, @1, @1);
			if (ctx->current_shared_pool == NULL) {
				od_cfg_diag_error(ctx->diags, @1,
								  "failed to allocate shared_pool");
				od_free($2);
				$2 = NULL;
				YYERROR;
			}
			$2 = NULL;
		}
	  '{' shared_pool_items '}'
		{
			ctx->current_shared_pool = NULL;
		}
	;

conn_drop_options_section:
	  conn_drop_options_keyword
		{
			od_cfg_conn_drop_options_t *opts =
				&ctx->model->conn_drop_options;

			if (opts->seen.is_set) {
				od_cfg_diag_error(ctx->diags, @1,
								  "conn_drop_options is redefined");
				YYERROR;
			}

			opts->seen.is_set = 1;
			opts->seen.location = @1;
			opts->location = @1;

			ctx->current_conn_drop_options = opts;
		}
	  '{' conn_drop_options_items '}'
		{
			ctx->current_conn_drop_options = NULL;
		}
	;

soft_oom_section:
	SOFT_OOM
		{
			od_cfg_soft_oom_t *soom = &ctx->model->soft_oom;

			if (soom->seen.is_set) {
				od_cfg_diag_error(ctx->diags, @1,
								  "soft_oom is redefined");
				YYERROR;
			}

			soom->seen.is_set = 1;
			soom->seen.location = @1;
			soom->location = @1;

			ctx->current_soft_oom = soom;
		}
		'{' soft_oom_items '}'
		{
			ctx->current_soft_oom = NULL;
		}
	;

soft_oom_items:
	 %empty
	| soft_oom_items soft_oom_item
	;

soft_oom_item:
	  LIMIT int_value
		{
			if ($2 < 0) {
				 od_cfg_diag_error(ctx->diags, @1,
						  "limit must be non-negative");
			} else {
				od_cfg_set_u64(ctx->diags,
							&ctx->current_soft_oom->limit,
							(uint64_t) $2,
							@1,
							"limit");
			}
		}
	| PROCESS string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_soft_oom->process,
							  $2,
							  @1,
							  "process");
			$2 = NULL;
		}
	| CHECK_INTERVAL_MS int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_soft_oom->check_interval_ms,
									$2,
									0, INT_MAX,
									@1,
									"check_interval_ms");
		}
	| soft_oom_drop
	;

soft_oom_drop:
	DROP
		{
			od_cfg_soft_oom_drop_t *drop = &ctx->current_soft_oom->drop;

			if (drop->seen.is_set) {
				od_cfg_diag_error(ctx->diags, @1,
								  "soft_oom.drop is redefined");
				YYERROR;
			}

			drop->seen.is_set = 1;
			drop->seen.location = @1;
			drop->location = @1;

			ctx->current_soft_oom_drop = drop;
		}
		'{' soft_oom_drop_items '}'
		{
			ctx->current_soft_oom_drop = NULL;
		}
	;

soft_oom_drop_items:
	  %empty
	| soft_oom_drop_items soft_oom_drop_item
	;

soft_oom_drop_item:
	  SIGNAL string_value
		{
			od_cfg_set_string(ctx->diags,
							  &ctx->current_soft_oom_drop->signal,
							  $2,
							  @1,
							  "signal");
			$2 = NULL;
		}
	| MAX_RATE int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_soft_oom_drop->max_rate,
									$2,
									0, INT_MAX,
									@1,
									"max_rate");
		}
	;


conn_drop_options_keyword:
	  CONN_DROP_OPTIONS
	| ONLINE_RESTART_DROP_OPTIONS
	;

conn_drop_options_items:
	  %empty
	| conn_drop_options_items conn_drop_options_item
	;

conn_drop_options_item:
	  DROP_ENABLED bool_value
		{
			od_cfg_set_bool(ctx->diags,
							&ctx->current_conn_drop_options->drop_enabled,
							$2,
							@1,
							"drop_enabled");
		}
	| RATE int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_conn_drop_options->rate,
									$2,
									1, INT_MAX,
									@1,
									"rate");
		}
	| INTERVAL_MS int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_conn_drop_options->interval_ms,
									$2,
									1, INT_MAX,
									@1,
									"interval_ms");
		}
	;

shared_pool_items:
	  %empty
	| shared_pool_items shared_pool_item
	;

shared_pool_item:
	  POOL_SIZE int_value
		{
			od_cfg_set_int_range_from_i64(ctx->diags,
									&ctx->current_shared_pool->pool_size,
									$2,
									0, INT_MAX,
									@1,
									"pool_size");
		}
	;

bool_value:
	  YES { $$ = 1; }
	| NO  { $$ = 0; }
	;

string_value:
	STRING { $$ = $1; $1 = NULL; }
	;

int_value:
	NUMBER { $$ = $1; }
	;

%%
