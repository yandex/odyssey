#pragma once

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

typedef struct {
	uint64_t last_update_time_ms;
	pthread_spinlock_t values_lock;
	bool is_read_write;
} od_storage_endpoint_status_t;

void od_storage_endpoint_status_init(od_storage_endpoint_status_t *status);
void od_storage_endpoint_status_destroy(od_storage_endpoint_status_t *status);
bool od_storage_endpoint_status_is_outdated(
	od_storage_endpoint_status_t *status, uint64_t recheck_interval);
void od_storage_endpoint_status_get(od_storage_endpoint_status_t *status,
				    od_storage_endpoint_status_t *out);
void od_storage_endpoint_status_set(od_storage_endpoint_status_t *status,
				    od_storage_endpoint_status_t *value);
struct od_storage_endpoint {
	od_address_t address;

	od_storage_endpoint_status_t status;
};

int od_storage_parse_endpoints(const char *host_str,
			       od_storage_endpoint_t **out, size_t *count);

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
	atomic_size_t rr_counter;

	od_storage_endpoint_t *endpoints;
	size_t endpoints_count;

	char *host; /* host or host,host or [host]:port[,host...] */
	int port; /* default port */

	int server_max_routing;
	od_storage_watchdog_t *watchdog;

	od_hashmap_t *acache;

	od_list_t link;

	int endpoints_status_poll_interval_ms;
};

/* storage API */
od_rule_storage_t *od_rules_storage_allocate(void);
od_rule_storage_t *od_rules_storage_copy(od_rule_storage_t *);

od_storage_endpoint_t *od_rules_storage_next_endpoint(od_rule_storage_t *);
od_storage_endpoint_t *
od_rules_storage_localhost_or_next_endpoint(od_rule_storage_t *);

void od_rules_storage_free(od_rule_storage_t *);

/* watchdog */
void od_storage_watchdog_watch(void *arg);
