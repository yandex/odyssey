#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdint.h>
#include <stdio.h>

#include <cfg/diag.h>

struct od_cfg_parse_ctx;

typedef struct {
	int is_set;
	od_cfg_location_t location;
} od_cfg_seen_t;

typedef struct {
	int value;
	od_cfg_seen_t seen;
} od_cfg_bool_field_t;

typedef struct {
	int value;
	od_cfg_seen_t seen;
} od_cfg_int_field_t;

typedef struct {
	uint64_t value;
	od_cfg_seen_t seen;
} od_cfg_u64_field_t;

typedef struct {
	char *value;
	od_cfg_seen_t seen;
} od_cfg_string_field_t;

typedef struct od_cfg_global {
	od_cfg_bool_field_t daemonize;
	od_cfg_bool_field_t sequential_routing;
	od_cfg_bool_field_t enable_online_restart;
	od_cfg_bool_field_t virtual_processing;
	od_cfg_bool_field_t bindwith_reuseport;
	od_cfg_bool_field_t enable_host_watcher;
	od_cfg_bool_field_t log_debug;
	od_cfg_bool_field_t log_to_stdout;
	od_cfg_bool_field_t log_config;
	od_cfg_bool_field_t log_session;
	od_cfg_bool_field_t log_query;
	od_cfg_bool_field_t log_stats;
	od_cfg_bool_field_t log_async;
	od_cfg_bool_field_t log_syslog;
	od_cfg_bool_field_t smart_search_path_enquoting;
	od_cfg_bool_field_t nodelay;
	od_cfg_bool_field_t disable_nolinger;
	od_cfg_bool_field_t log_general_stats_prom;
	od_cfg_bool_field_t log_route_stats_prom;

	od_cfg_string_field_t pid_file;
	od_cfg_string_field_t unix_socket_dir;
	od_cfg_string_field_t unix_socket_mode;
	od_cfg_string_field_t locks_dir;
	od_cfg_string_field_t external_auth_socket_path;
	od_cfg_string_field_t availability_zone;
	od_cfg_string_field_t log_file;
	od_cfg_string_field_t log_format;
	od_cfg_string_field_t log_syslog_ident;
	od_cfg_string_field_t log_syslog_facility;
	od_cfg_string_field_t cpu_affinity;
	od_cfg_string_field_t hba_file;

	od_cfg_int_field_t priority;
	od_cfg_int_field_t graceful_shutdown_timeout_ms;
	od_cfg_int_field_t log_queue_depth;
	od_cfg_int_field_t stats_interval;
	od_cfg_int_field_t client_max;
	od_cfg_int_field_t client_max_routing;
	od_cfg_int_field_t server_login_retry;
	od_cfg_int_field_t readahead;
	od_cfg_int_field_t keepalive;
	od_cfg_int_field_t keepalive_keep_interval;
	od_cfg_int_field_t keepalive_probes;
	od_cfg_int_field_t keepalive_usr_timeout;
	od_cfg_int_field_t max_sigterms_to_die;
	od_cfg_int_field_t backend_connect_timeout_ms;
	od_cfg_int_field_t cancel_timeout_ms;
	od_cfg_int_field_t cancel_queue_timeout_ms;
	od_cfg_int_field_t cancel_max_inflight;
	od_cfg_int_field_t resolvers;
	od_cfg_int_field_t dns_cache_ttl;
	od_cfg_int_field_t cache_msg_gc_size;
	od_cfg_int_field_t cache_coroutine;
	od_cfg_int_field_t coroutine_stack_size;
	od_cfg_int_field_t promhttp_server_port;
	od_cfg_int_field_t group_checker_interval;
	od_cfg_int_field_t workers;

} od_cfg_global_t;

typedef struct od_cfg_listen {
	od_cfg_location_t location;

	od_cfg_string_field_t host;
	od_cfg_int_field_t port;
	od_cfg_string_field_t target_session_attrs;

	od_cfg_int_field_t client_login_timeout;
	od_cfg_int_field_t backlog;

	od_cfg_string_field_t tls;
	od_cfg_string_field_t tls_ca_file;
	od_cfg_string_field_t tls_key_file;
	od_cfg_string_field_t tls_cert_file;
	od_cfg_string_field_t tls_protocols;

	od_cfg_int_field_t catchup_timeout;

} od_cfg_listen_t;

typedef struct od_cfg_balancing_method {
	od_cfg_seen_t seen;
	od_cfg_location_t location;

	char *name;
} od_cfg_balancing_method_t;

typedef struct od_cfg_balancing {
	od_cfg_seen_t seen;
	od_cfg_location_t location;

	od_cfg_balancing_method_t method;
	od_cfg_bool_field_t show_notice_messages;
} od_cfg_balancing_t;

typedef struct od_cfg_route od_cfg_route_t;

typedef struct od_cfg_storage {
	od_cfg_location_t location;

	char *name;
	od_cfg_location_t name_location;

	od_cfg_string_field_t type;
	od_cfg_string_field_t host;
	od_cfg_int_field_t port;

	od_cfg_string_field_t tls;
	od_cfg_string_field_t tls_ca_file;
	od_cfg_string_field_t tls_key_file;
	od_cfg_string_field_t tls_cert_file;
	od_cfg_string_field_t tls_protocols;

	od_cfg_int_field_t server_max_routing;
	od_cfg_int_field_t endpoints_status_poll_interval_ms;

	od_cfg_balancing_t balancing;

	od_cfg_route_t *watchdog;
} od_cfg_storage_t;

od_cfg_route_t *od_cfg_storage_add_watchdog(od_cfg_storage_t *s,
					    od_cfg_location_t loc);

typedef struct od_cfg_conn_drop_options {
	od_cfg_seen_t seen;
	od_cfg_location_t location;

	od_cfg_bool_field_t drop_enabled;
	od_cfg_int_field_t rate;
	od_cfg_int_field_t interval_ms;
} od_cfg_conn_drop_options_t;

typedef struct od_cfg_shared_pool {
	od_cfg_location_t location;

	char *name;
	od_cfg_location_t name_location;

	od_cfg_int_field_t pool_size;
} od_cfg_shared_pool_t;

typedef struct od_cfg_soft_oom_drop {
	od_cfg_seen_t seen;
	od_cfg_location_t location;

	od_cfg_string_field_t signal;
	od_cfg_int_field_t max_rate;
} od_cfg_soft_oom_drop_t;

typedef struct od_cfg_soft_oom {
	od_cfg_seen_t seen;
	od_cfg_location_t location;

	od_cfg_u64_field_t limit;
	od_cfg_string_field_t process;
	od_cfg_int_field_t check_interval_ms;

	od_cfg_soft_oom_drop_t drop;
} od_cfg_soft_oom_t;

typedef struct od_cfg_ldap_endpoint {
	od_cfg_location_t location;

	char *name;
	od_cfg_location_t name_location;

	od_cfg_string_field_t ldapserver;
	od_cfg_u64_field_t ldapport;

	od_cfg_string_field_t ldapprefix;
	od_cfg_string_field_t ldapsuffix;
	od_cfg_string_field_t ldapsearchattribute;
	od_cfg_string_field_t ldapscope;
	od_cfg_string_field_t ldapscheme;
	od_cfg_string_field_t ldapbasedn;
	od_cfg_string_field_t ldapbinddn;
	od_cfg_string_field_t ldapbindpasswd;
	od_cfg_string_field_t ldapsearchfilter;
} od_cfg_ldap_endpoint_t;

typedef enum od_cfg_conn_type {
	OD_CFG_CONN_TYPE_DEFAULT,
	OD_CFG_CONN_TYPE_LOCAL,
	OD_CFG_CONN_TYPE_HOST,
	OD_CFG_CONN_TYPE_HOSTSSL,
	OD_CFG_CONN_TYPE_HOSTNOSSL
} od_cfg_conn_type_t;

typedef struct od_cfg_rule_auth_common_name {
	od_cfg_location_t location;

	char *name;
	od_cfg_location_t name_location;

	int is_default;
} od_cfg_rule_auth_common_name_t;

typedef struct od_cfg_ldap_storage_credentials {
	od_cfg_location_t location;

	char *name;
	od_cfg_location_t name_location;

	od_cfg_string_field_t ldap_storage_username;
	od_cfg_string_field_t ldap_storage_password;
} od_cfg_ldap_storage_credentials_t;

typedef struct {
	od_cfg_location_t location;

	char *key;
	od_cfg_location_t key_location;

	char *value;
	od_cfg_location_t value_location;
} od_cfg_string_kvp_t;

typedef struct {
	od_cfg_location_t location;
	od_cfg_seen_t seen;

	od_cfg_string_kvp_t **pairs;
	size_t pairs_count;
	size_t pairs_capacity;
} od_cfg_string_kvp_list_t;

od_cfg_string_kvp_t *
od_cfg_string_kvp_list_add(od_cfg_string_kvp_list_t *list,
			   od_cfg_location_t location, char *key,
			   od_cfg_location_t key_location, char *value,
			   od_cfg_location_t value_location);

struct od_cfg_route {
	od_cfg_location_t location;
	int is_default;
	char *name;
	od_cfg_location_t name_location;

	char *address_range;
	od_cfg_location_t address_range_location;

	od_cfg_conn_type_t conn_type;
	od_cfg_location_t conn_type_location;

	od_cfg_rule_auth_common_name_t **auth_common_names;
	size_t auth_common_names_count;
	size_t auth_common_names_capacity;

	od_cfg_string_field_t authentication;
	od_cfg_string_field_t auth_module;
	od_cfg_bool_field_t enable_mdb_iamproxy_auth;
	od_cfg_string_field_t mdb_iamproxy_socket_path;
	od_cfg_string_field_t auth_pam_service;
	od_cfg_string_field_t target_session_attrs;
	od_cfg_string_field_t auth_query;
	od_cfg_string_field_t auth_query_db;
	od_cfg_string_field_t auth_query_user;
	od_cfg_bool_field_t password_passthrough;
	od_cfg_string_field_t password;
	od_cfg_string_field_t role;
	od_cfg_int_field_t client_max;
	od_cfg_bool_field_t client_fwd_error;
	od_cfg_bool_field_t client_show_id;
	od_cfg_bool_field_t reserve_session_server_connection;
	od_cfg_string_field_t quantiles;
	od_cfg_bool_field_t application_name_add_host;
	od_cfg_bool_field_t server_drop_on_backend_plan_error;
	od_cfg_u64_field_t server_lifetime;
	od_cfg_int_field_t ldap_pool_size;
	od_cfg_int_field_t ldap_pool_timeout;
	od_cfg_int_field_t ldap_pool_ttl;
	od_cfg_bool_field_t maintain_params;
	od_cfg_string_field_t pool;
	od_cfg_string_field_t pool_routing;
	od_cfg_int_field_t min_pool_size;
	od_cfg_int_field_t pool_size;
	od_cfg_int_field_t pool_timeout;
	od_cfg_int_field_t pool_ttl;
	od_cfg_bool_field_t pool_discard;
	od_cfg_bool_field_t pool_smart_discard;
	od_cfg_string_field_t pool_discard_query;
	od_cfg_bool_field_t pool_cancel;
	od_cfg_bool_field_t pool_rollback;
	od_cfg_u64_field_t pool_reset_timeout_ms;
	od_cfg_bool_field_t pool_reserve_prepared_statement;
	od_cfg_bool_field_t pool_pin_on_listen;
	od_cfg_bool_field_t pool_attach_check;
	od_cfg_bool_field_t pool_acquire_fail_fast;
	od_cfg_u64_field_t pool_notice_after_waiting_ms;
	od_cfg_u64_field_t pool_client_idle_timeout;
	od_cfg_u64_field_t pool_idle_in_transaction_timeout;
	od_cfg_string_field_t shared_pool;
	od_cfg_string_field_t storage;
	od_cfg_string_field_t storage_database;
	od_cfg_string_field_t storage_user;
	od_cfg_string_field_t storage_password;
	od_cfg_bool_field_t log_debug;
	od_cfg_bool_field_t log_query;
	od_cfg_string_field_t ldap_endpoint_name;
	od_cfg_string_field_t ldap_storage_credentials_attr;

	od_cfg_ldap_storage_credentials_t **ldap_storage_credentials;
	size_t ldap_storage_credentials_count;
	size_t ldap_storage_credentials_capacity;

	od_cfg_string_field_t watchdog_lag_query;
	od_cfg_int_field_t watchdog_lag_interval;
	od_cfg_int_field_t catchup_timeout;
	od_cfg_string_kvp_list_t pgoptions;
	od_cfg_string_kvp_list_t backend_startup_options;
	od_cfg_string_field_t group_query;
	od_cfg_string_field_t group_query_user;
	od_cfg_string_field_t group_query_db;
};

od_cfg_ldap_storage_credentials_t *
od_cfg_user_route_add_ldap_creds(od_cfg_route_t *r, od_cfg_location_t location,
				 char *name);

od_cfg_rule_auth_common_name_t *od_cfg_user_route_add_auth_common_name(
	od_cfg_route_t *r, char *name, int is_default,
	od_cfg_location_t location, od_cfg_location_t name_location);

typedef struct od_cfg_database {
	od_cfg_location_t location;
	int is_default;
	char *name;
	od_cfg_location_t name_location;

	od_cfg_route_t **users;
	size_t users_count;
	size_t users_capacity;

	od_cfg_route_t **groups;
	size_t groups_count;
	size_t groups_capacity;
} od_cfg_database_t;

od_cfg_route_t *od_cfg_database_add_user(
	od_cfg_database_t *db, char *name, int is_default,
	od_cfg_location_t name_location, od_cfg_location_t location,
	char *addr_range, od_cfg_location_t addr_range_location,
	od_cfg_conn_type_t conn_type, od_cfg_location_t conn_type_location);

od_cfg_route_t *od_cfg_database_add_group(od_cfg_database_t *db, char *name,
					  od_cfg_location_t name_location,
					  od_cfg_location_t location,
					  char *addr_range,
					  od_cfg_location_t addr_range_location,
					  od_cfg_conn_type_t conn_type,
					  od_cfg_location_t conn_type_location);

typedef struct {
	od_cfg_location_t location;

	od_cfg_global_t global;

	od_cfg_conn_drop_options_t conn_drop_options;

	od_cfg_soft_oom_t soft_oom;

	od_cfg_listen_t **listens;
	size_t listens_count;
	size_t listens_capacity;

	od_cfg_shared_pool_t **shared_pools;
	size_t shared_pools_count;
	size_t shared_pools_capacity;

	od_cfg_storage_t **storages;
	size_t storages_count;
	size_t storages_capacity;

	od_cfg_ldap_endpoint_t **ldap_endpoints;
	size_t ldap_endpoints_count;
	size_t ldap_endpoints_capacity;

	od_cfg_database_t **databases;
	size_t databases_count;
	size_t databases_capacity;
} od_cfg_model_t;

void od_cfg_model_init(od_cfg_model_t *model);
void od_cfg_model_free(od_cfg_model_t *model);

void od_cfg_model_dumpf(FILE *file, const od_cfg_model_t *model);

int od_cfg_validate_model(od_cfg_model_t *model, od_cfg_diag_list_t *diags);

od_cfg_listen_t *od_cfg_model_add_listen(od_cfg_model_t *model,
					 od_cfg_location_t location);

od_cfg_storage_t *od_cfg_model_add_storage(od_cfg_model_t *model, char *name,
					   od_cfg_location_t name_location,
					   od_cfg_location_t location);

od_cfg_ldap_endpoint_t *
od_cfg_model_add_ldap_endpoint(od_cfg_model_t *model, char *name,
			       od_cfg_location_t name_location,
			       od_cfg_location_t location);

od_cfg_shared_pool_t *
od_cfg_model_add_shared_pool(od_cfg_model_t *model, char *name,
			     od_cfg_location_t name_location,
			     od_cfg_location_t location);

od_cfg_database_t *od_cfg_model_add_database(od_cfg_model_t *model, char *name,
					     int is_default,
					     od_cfg_location_t name_location,
					     od_cfg_location_t location);

int od_cfg_set_bool(od_cfg_diag_list_t *diags, od_cfg_bool_field_t *field,
		    int value, od_cfg_location_t location,
		    const char *field_name);

int od_cfg_set_int(od_cfg_diag_list_t *diags, od_cfg_int_field_t *field,
		   int value, od_cfg_location_t location,
		   const char *field_name);

int od_cfg_set_int_from_i64(od_cfg_diag_list_t *diags,
			    od_cfg_int_field_t *field, int64_t value,
			    od_cfg_location_t location, const char *field_name);

int od_cfg_set_int_range_from_i64(od_cfg_diag_list_t *diags,
				  od_cfg_int_field_t *field, int64_t value,
				  int min, int max, od_cfg_location_t location,
				  const char *field_name);

int od_cfg_set_u64(od_cfg_diag_list_t *diags, od_cfg_u64_field_t *field,
		   uint64_t value, od_cfg_location_t location,
		   const char *field_name);

int od_cfg_set_string(od_cfg_diag_list_t *diags, od_cfg_string_field_t *field,
		      char *value, od_cfg_location_t location,
		      const char *field_name);
