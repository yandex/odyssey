#ifndef ODYSSEY_RULES_H
#define ODYSSEY_RULES_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_rule_storage od_rule_storage_t;
typedef struct od_rule_auth    od_rule_auth_t;
typedef struct od_rule         od_rule_t;
typedef struct od_rules        od_rules_t;
typedef struct od_db_state     od_db_state_t;

typedef enum
{
	OD_RULE_TLS_DISABLE,
	OD_RULE_TLS_ALLOW,
	OD_RULE_TLS_REQUIRE,
	OD_RULE_TLS_VERIFY_CA,
	OD_RULE_TLS_VERIFY_FULL
} od_rule_tls_t;

typedef enum
{
	OD_RULE_AUTH_UNDEF,
	OD_RULE_AUTH_NONE,
	OD_RULE_AUTH_BLOCK,
	OD_RULE_AUTH_CLEAR_TEXT,
	OD_RULE_AUTH_MD5,
	OD_RULE_AUTH_SCRAM_SHA_256,
	OD_RULE_AUTH_CERT
} od_rule_auth_type_t;

typedef enum
{
	OD_RULE_POOL_SESSION,
	OD_RULE_POOL_TRANSACTION
} od_rule_pool_type_t;

typedef enum
{
	OD_RULE_STORAGE_REMOTE,
	OD_RULE_STORAGE_LOCAL,
	OD_RULE_STORAGE_REPLICATION,
	OD_RULE_STORAGE_REPLICATION_LOGICAL,
} od_rule_storage_type_t;

struct od_rule_storage
{
	char                   *name;
	char                   *type;
	od_rule_storage_type_t  storage_type;
	char                   *host;
	int                     port;
	od_rule_tls_t           tls_mode;
	char                   *tls;
	char                   *tls_ca_file;
	char                   *tls_key_file;
	char                   *tls_cert_file;
	char                   *tls_protocols;
	od_list_t               link;
};

struct od_rule_auth
{
	char      *common_name;
	od_list_t  link;
};

struct od_rule
{
	/* versioning */
	int                     mark;
	int                     obsolete;
	int                     refs;
	/* id */
	char                   *db_name;
	int                     db_name_len;
	int                     db_is_default;
	char                   *user_name;
	int                     user_name_len;
	int                     user_is_default;
	/* auth */
	char                   *auth;
	od_rule_auth_type_t     auth_mode;
	char                   *auth_query;
	char                   *auth_query_db;
	char                   *auth_pam_service;
	char                   *auth_query_user;
	int                     auth_common_name_default;
	od_list_t               auth_common_names;
	int                     auth_common_names_count;
	/* password */
	char                   *password;
	int                     password_len;
	/* storage */
	od_rule_storage_t      *storage;
	char                   *storage_name;
	char                   *storage_db;
	char                   *storage_user;
	int                     storage_user_len;
	char                   *storage_password;
	int                     storage_password_len;
	/* pool */
	od_rule_pool_type_t     pool;
	char                   *pool_sz;
	int                     pool_size;
	int                     pool_timeout;
	int                     pool_ttl;
	int                     pool_discard;
	int                     pool_cancel;
	int                     pool_rollback;
	/* misc */
	int                     client_fwd_error;
	int                     client_max_set;
	int                     client_max;
	int                     log_debug;
	od_list_t               link;
    /* state is mutable */
    od_db_state_t          *db_state;
};

struct od_db_state {
    /* db_name */
    char* db_name;
    int  db_name_len;
    /* db_state */
    bool is_active;

    /* freeing */
    bool mark;

    od_list_t link;
};



struct od_rules
{
	od_list_t storages;
	od_list_t rules;
	od_list_t db_states;
};


od_db_state_t* od_db_state_allocate(void);
int            od_db_state_init(od_db_state_t* state, od_rule_t* rule);
void           od_db_state_free(od_db_state_t *db_state);

void od_rules_init(od_rules_t*);
void od_rules_free(od_rules_t*);
int  od_rules_validate(od_rules_t*, od_config_t*, od_logger_t*);
int  od_rules_merge(od_rules_t*, od_rules_t*);
void od_rules_print(od_rules_t*, od_logger_t*);

/* rule */
od_rule_t*
od_rules_add(od_rules_t*);
void od_rules_ref(od_rule_t*);
void od_rules_unref(od_rule_t*);
int  od_rules_compare(od_rule_t*, od_rule_t*);

od_rule_t*
od_rules_forward(od_rules_t*, char*, char*);

od_rule_t*
od_rules_match(od_rules_t*, char*, char*);

/* storage */
od_rule_storage_t*
od_rules_storage_add(od_rules_t*);

od_rule_storage_t*
od_rules_storage_match(od_rules_t*, char*);

od_rule_storage_t*
od_rules_storage_copy(od_rule_storage_t*);

void od_rules_storage_free(od_rule_storage_t*);

/* auth */
od_rule_auth_t*
od_rules_auth_add(od_rule_t*);

void od_rules_auth_free(od_rule_auth_t*);

/* db_states */
int od_rules_build_db_states(od_rules_t *rules, od_error_t *error);

#endif /* ODYSSEY_RULES_H */
