#ifndef ODYSSEY_RULE_STORAGE_H
#define ODYSSEY_RULE_STORAGE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_rule_storage od_rule_storage_t;
typedef struct od_storage_watchdog od_storage_watchdog_t;

/* Storage Watchdog */
typedef enum {
	OD_RULE_STORAGE_REMOTE,
	OD_RULE_STORAGE_LOCAL,
} od_rule_storage_type_t;

struct od_storage_watchdog {
	char *route_usr;
	char *route_db;

	char *storage_user;
	char *storage_db;

	char *query;
	int interval;

	/* soft shutdown on reload */
	pthread_mutex_t mu;
	int online;

	od_global_t *global;

	od_rule_storage_t *storage;

	od_atomic_u64_t finished;
};

od_storage_watchdog_t *od_storage_watchdog_allocate(od_global_t *);
int od_storage_watchdog_free(od_storage_watchdog_t *watchdog);

/* */
typedef struct od_storage_endpoint od_storage_endpoint_t;

typedef enum {
	OD_TARGET_SESSION_ATTRS_RW,
	OD_TARGET_SESSION_ATTRS_RO,
	OD_TARGET_SESSION_ATTRS_ANY,
} od_target_session_attrs_t;

static inline char *
od_target_session_attrs_to_pg_mode_str(od_target_session_attrs_t tsa)
{
	switch (tsa) {
	case OD_TARGET_SESSION_ATTRS_RW:
		return "primary";
	case OD_TARGET_SESSION_ATTRS_RO:
		return "standby";
	case OD_TARGET_SESSION_ATTRS_ANY:
		return "any";
	}

	return "<unknown>";
}

typedef enum {
	OD_STORAGE_ENDPOINT_STATUS_DOWN,
	OD_STORAGE_ENDPOINT_STATUS_ALIVE,
} od_storage_endpoint_alive_status_t;

typedef struct {
	uint64_t last_update_time_ms;
	od_target_session_attrs_t tsa;
	od_storage_endpoint_alive_status_t alive;
} od_storage_endpoint_status_t;

struct od_storage_endpoint {
	char *host; /* NULL - terminated */
	int port; /* TODO: support somehow */

	od_storage_endpoint_status_t status;
};

bool od_storage_endpoint_equal(od_storage_endpoint_t *a,
			       od_storage_endpoint_t *b);

bool od_storage_endpoint_status_is_outdated(od_storage_endpoint_t *endpoint);

typedef struct od_auth_cache_value od_auth_cache_value_t;
struct od_auth_cache_value {
	uint64_t timestamp;
	char *passwd;
	uint32_t passwd_len;
};

struct od_rule_storage {
	od_tls_opts_t *tls_opts;

	char *name;
	char *type;
	od_rule_storage_type_t storage_type;
	/* round-robin atomic counter for endpoint selection */
	od_atomic_u32_t rr_counter;

	od_storage_endpoint_t *endpoints;
	size_t endpoints_count;

	char *host; /* host or host,host or [host]:port[,host...] */
	int port; /* default port */

	od_target_session_attrs_t target_session_attrs;

	int server_max_routing;
	od_storage_watchdog_t *watchdog;

	od_hashmap_t *acache;

	od_list_t link;
};

/* storage API */
od_rule_storage_t *od_rules_storage_allocate(void);
od_rule_storage_t *od_rules_storage_copy(od_rule_storage_t *);
bool od_rules_storage_equal_names(od_rule_storage_t *a, od_rule_storage_t *b);

void od_rules_storage_free(od_rule_storage_t *);

/* watchdog */
void od_storage_watchdog_watch(void *arg);

#endif /* ODYSSEY_RULE_STORAGE_H */
