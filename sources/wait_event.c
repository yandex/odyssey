
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <wait_event.h>

atomic_uint_fast64_t od_wait_counters[OD_WAIT_COUNT];

static const char *od_wait_event_names[OD_WAIT_COUNT] = {
	"NONE",	       "QUEUE",	   "CONNECT_TCP",      "CONNECT_TLS",
	"AUTH_LDAP",   "AUTH_PAM", "AUTH_EXTERNAL",    "CLIENT_READ",
	"SERVER_READ", "PAUSE",	   "RATELIMIT_CANCEL", "RATELIMIT_ROUTING",
};

const char *od_wait_event_name(od_wait_event_t event)
{
	if (event >= OD_WAIT_COUNT) {
		return "UNKNOWN";
	}
	return od_wait_event_names[event];
}

void od_wait_counters_reset(void)
{
	for (int i = 0; i < OD_WAIT_COUNT; i++) {
		atomic_store_explicit(&od_wait_counters[i], 0,
				      memory_order_relaxed);
	}
}
