#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_rule_auth od_rule_auth_t;
typedef struct od_rule od_rule_t;
typedef struct od_rules od_rules_t;

typedef enum {
	OD_RULE_AUTH_UNDEF,
	OD_RULE_AUTH_NONE,
	OD_RULE_AUTH_BLOCK,
	OD_RULE_AUTH_CLEAR_TEXT,
	OD_RULE_AUTH_EXTERNAL,
	OD_RULE_AUTH_MD5,
	OD_RULE_AUTH_SCRAM_SHA_256,
	OD_RULE_AUTH_CERT
} od_rule_auth_type_t;

typedef struct {
	od_rule_t *rule;
	od_rules_t *rules;
} od_group_checker_run_args;

struct od_rule_auth {
	char *common_name;
	od_list_t link;
};

typedef enum {
	OD_RULE_ROLE_ADMIN,
	OD_RULE_ROLE_STAT,
	OD_RULE_ROLE_NOTALLOW,
	OD_RULE_ROLE_UNDEF,
} od_rule_role_type_t;

typedef struct od_rule_key od_rule_key_t;

struct od_rule_key {
	char *usr_name;
	char *db_name;
	od_address_range_t address_range;

	od_list_t link;
};

static inline void od_rule_key_init(od_rule_key_t *rk)
{
	od_list_init(&rk->link);
}

static inline void od_rule_key_free(od_rule_key_t *rk)
{
	od_list_unlink(&rk->link);

	od_free(rk);
}

struct od_rule {
	/* versioning */
	int mark;
	int obsolete;
	int refs;
	int order;

	/* id */
	char *db_name;
	int db_name_len;
	int db_is_default;
	char *user_name;
	int user_name_len;
	int user_is_default;
	od_address_range_t address_range;
	od_rule_role_type_t user_role;

	/* auth */
	char *auth;
	od_rule_auth_type_t auth_mode;
	char *auth_query;
	char *auth_query_db;
	char *auth_query_user;
	int auth_common_name_default;
	od_list_t auth_common_names;
	int auth_common_names_count;

	int enable_mdb_iamproxy_auth;
	char *mdb_iamproxy_socket_path;

#ifdef PAM_FOUND
	/*  PAM parameters */
	char *auth_pam_service;
	od_pam_auth_data_t *auth_pam_data;
#endif

#ifdef LDAP_FOUND
	char *ldap_endpoint_name;
	od_ldap_endpoint_t *ldap_endpoint;
	int ldap_pool_timeout;
	int ldap_pool_size;
	int ldap_pool_ttl;
	od_list_t ldap_storage_creds_list;
	char *ldap_storage_credentials_attr;
#endif

	char *auth_module;

	/* password */
	char *password;
	int password_len;
	/* storage */
	od_rule_storage_t *storage;
	char *storage_name;
	char *storage_db;

	char *storage_user;
	int storage_user_len;

	char *storage_password;
	int storage_password_len;

	/* pool */
	od_rule_pool_t *pool;
	int catchup_timeout;
	int catchup_checks;

	/* Should we deploy user GUCS when attaching? */
	int maintain_params;

	/* group */
	od_group_t *group; // set if rule is group
	od_rule_t *group_rule;
	char **user_names;
	int users_in_group;

	/* PostgreSQL options */
	kiwi_vars_t vars;

	/* misc */
	int client_fwd_error;
	int reserve_session_server_connection;
	int application_name_add_host;
	int client_max_set;
	int client_max;
	int log_debug;
	int log_query;
	int enable_password_passthrough;
	double *quantiles;
	int quantiles_count;
	uint64_t server_lifetime_us;

	od_target_session_attrs_t target_session_attrs;

	/* some settings that we are going to force-on
	while backend connection acquiring */
	untyped_kiwi_var_t *backend_startup_vars;
	size_t backend_startup_vars_sz;

	od_list_t link;
};

struct od_rules {
	pthread_mutex_t mu;
	od_list_t storages;
#ifdef LDAP_FOUND
	od_list_t ldap_endpoints;
#endif
	od_list_t rules;
};

/* rules */

void od_rules_init(od_rules_t *);
void od_rules_free(od_rules_t *);
int od_rules_validate(od_rules_t *, od_config_t *, od_logger_t *);
/* auto-generate default rules (for auth query etc) if needed */
int od_rules_autogenerate_defaults(od_rules_t *rules, od_logger_t *logger);
int od_rules_merge(od_rules_t *, od_rules_t *, od_list_t *added,
		   od_list_t *deleted, od_list_t *drop);
void od_rules_print(od_rules_t *, od_logger_t *);

int od_rules_cleanup(od_rules_t *rules);

/* rule */
od_rule_t *od_rules_add(od_rules_t *);
void od_rules_ref(od_rule_t *);
void od_rules_unref(od_rule_t *);
int od_rules_compare(od_rule_t *, od_rule_t *);

od_rule_t *od_rules_forward(od_rules_t *, char *, char *,
			    struct sockaddr_storage *, int, int);

/* search rule with desored characteristik */
od_rule_t *od_rules_match(od_rules_t *rules, char *db_name, char *user_name,
			  od_address_range_t *address_range, int db_is_default,
			  int user_is_default, int pool_internal);

/* group */
od_group_t *od_rules_group_allocate(od_global_t *global);

void od_rules_rule_free(od_rule_t *rule);

/* storage API */
od_rule_storage_t *od_rules_storage_match(od_rules_t *, char *);
od_rule_storage_t *od_rules_storage_add(od_rules_t *rules,
					od_rule_storage_t *storage);

od_retcode_t od_rules_storages_watchdogs_run(od_logger_t *logger,
					     od_rules_t *rules);

#ifdef LDAP_FOUND
/* ldap endpoint */
od_ldap_endpoint_t *od_rules_ldap_endpoint_add(od_rules_t *rules,
					       od_ldap_endpoint_t *ldap);
od_ldap_storage_credentials_t *
od_rule_ldap_storage_credentials_add(od_rule_t *rule,
				     od_ldap_storage_credentials_t *lsc);
#endif

/* auth */
od_rule_auth_t *od_rules_auth_add(od_rule_t *);

void od_rules_auth_free(od_rule_auth_t *);

od_retcode_t od_rules_groups_checkers_run(od_logger_t *logger,
					  od_rules_t *rules);

bool od_name_in_rule(od_rule_t *rule, char *name);
