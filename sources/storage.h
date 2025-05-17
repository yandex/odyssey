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

static inline const char *od_storage_endpoint_alive_satus_to_str(
	od_storage_endpoint_alive_status_t status)
{
	switch (status) {
	case OD_STORAGE_ENDPOINT_STATUS_DOWN:
		return "down";
	case OD_STORAGE_ENDPOINT_STATUS_ALIVE:
		return "alive";
	default:
		return "unknown";
	}
}

typedef struct {
	uint64_t last_update_time_ms;
	od_target_session_attrs_t tsa;
	od_storage_endpoint_alive_status_t alive;
	pthread_spinlock_t lock;
} od_storage_endpoint_status_t;

static inline void
od_storage_endpoint_status_init(od_storage_endpoint_status_t *status)
{
	status->last_update_time_ms = 0ULL;
	status->tsa = OD_TARGET_SESSION_ATTRS_ANY;
	status->alive = OD_STORAGE_ENDPOINT_STATUS_ALIVE;
	pthread_spin_init(&status->lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void
od_storage_endpoint_status_destroy(od_storage_endpoint_status_t *status)
{
	pthread_spin_destroy(&status->lock);
}

static inline bool
od_storage_endpoint_status_is_outdated(od_storage_endpoint_status_t *status,
				       uint64_t interval)
{
	pthread_spin_lock(&status->lock);

	uint64_t last_update_time_ms = status->last_update_time_ms;

	pthread_spin_unlock(&status->lock);

	return (machine_time_ms() - last_update_time_ms) > interval;
}

static inline void
od_storage_endpoint_status_get(od_storage_endpoint_status_t *status,
			       od_storage_endpoint_status_t *out)
{
	pthread_spin_lock(&status->lock);

	out->alive = status->alive;
	out->last_update_time_ms = status->last_update_time_ms;
	out->tsa = status->tsa;

	pthread_spin_unlock(&status->lock);
}

static inline void
od_storage_endpoint_status_set(od_storage_endpoint_status_t *status,
			       od_storage_endpoint_status_t *value)
{
	pthread_spin_lock(&status->lock);

	status->alive = value->alive;
	status->last_update_time_ms = value->last_update_time_ms;
	status->tsa = value->tsa;

	pthread_spin_unlock(&status->lock);
}

struct od_storage_endpoint {
	char *host; /* NULL - terminated */
	int port; /* TODO: support somehow */

	od_storage_endpoint_status_t status;
};

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

	int endpoints_status_poll_interval_ms;
};

/* storage API */
od_rule_storage_t *od_rules_storage_allocate(void);
od_rule_storage_t *od_rules_storage_copy(od_rule_storage_t *);

void od_rules_storage_free(od_rule_storage_t *);

/* watchdog */
void od_storage_watchdog_watch(void *arg);

#endif /* ODYSSEY_RULE_STORAGE_H */
