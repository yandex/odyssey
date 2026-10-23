#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Wait events.
 *
 * Lightweight instrumentation of blocking points: every place where a
 * coroutine is about to sleep reports the event it is waiting on by
 * incrementing a global per-event counter and decrements it back on
 * wakeup.  Counters are process-wide atomics, so any worker can read
 * the current number of waiters without taking any lock.
 */

#include <stdatomic.h>

typedef enum {
	OD_WAIT_NONE = 0,
	OD_WAIT_QUEUE,
	OD_WAIT_CONNECT_TCP,
	OD_WAIT_CONNECT_TLS,
	OD_WAIT_AUTH_LDAP,
	OD_WAIT_AUTH_PAM,
	OD_WAIT_AUTH_EXTERNAL,
	OD_WAIT_CLIENT_READ,
	OD_WAIT_SERVER_READ,
	OD_WAIT_PAUSE,
	OD_WAIT_RATELIMIT_CANCEL,
	OD_WAIT_RATELIMIT_ROUTING,
	OD_WAIT_COUNT
} od_wait_event_t;

/* current number of waiters per event, indexed by od_wait_event_t */
extern atomic_uint_fast64_t od_wait_counters[OD_WAIT_COUNT];

static inline void od_wait_event_start(od_wait_event_t event)
{
	atomic_fetch_add_explicit(&od_wait_counters[event], 1,
				  memory_order_relaxed);
}

static inline void od_wait_event_end(od_wait_event_t event)
{
	atomic_fetch_sub_explicit(&od_wait_counters[event], 1,
				  memory_order_relaxed);
}

const char *od_wait_event_name(od_wait_event_t event);

void od_wait_counters_reset(void);
