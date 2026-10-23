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

#include <stdalign.h>
#include <stdatomic.h>

#define OD_WAIT_CLASS_LIST(X)        \
	X(NONE, "None")              \
	X(RATELIMIT, "RateLimit")    \
	X(CONNECT, "Connect")        \
	X(AUTH, "Auth")              \
	X(QUERY_CYCLE, "QueryCycle") \
	X(POOL, "Pool")              \
	X(STREAM, "Stream")          \
	X(DEPLOY, "Deploy")          \
	X(RESET, "Reset")            \
	X(SYSTEM, "System")

typedef enum {
#define OD_WAIT_CLASS(id, name) OD_WAIT_CLASS_##id,
	OD_WAIT_CLASS_LIST(OD_WAIT_CLASS)
#undef OD_WAIT_CLASS
		OD_WAIT_CLASS_COUNT
} od_wait_class_t;

#define OD_WAIT_EVENT_LIST(X)                                                \
	X(NONE, NONE, "NONE")                                                \
	/* ratelimit: connection rate limiting */                            \
	X(RATELIMIT_ACCEPT, RATELIMIT, "RATELIMIT_ACCEPT")                   \
	X(RATELIMIT_CANCEL, RATELIMIT, "RATELIMIT_CANCEL")                   \
	X(RATELIMIT_ROUTING, RATELIMIT, "RATELIMIT_ROUTING")                 \
	/* connect: establishing the client connection */                    \
	X(CONNECT_CLIENT_STARTUP, CONNECT, "CONNECT_CLIENT_STARTUP")         \
	X(CONNECT_CLIENT_TLS, CONNECT, "CONNECT_CLIENT_TLS")                 \
	/* auth: authentication phase */                                     \
	X(AUTH_CLIENT_READ, AUTH, "AUTH_CLIENT_READ")                        \
	X(AUTH_SERVER_READ, AUTH, "AUTH_SERVER_READ")                        \
	X(AUTH_LDAP, AUTH, "AUTH_LDAP")                                      \
	X(AUTH_PAM, AUTH, "AUTH_PAM")                                        \
	X(AUTH_EXTERNAL, AUTH, "AUTH_EXTERNAL")                              \
	/* query cycle: read command, forward, wait response, send result */ \
	X(QUERY_CLIENT_READ, QUERY_CYCLE, "QUERY_CLIENT_READ")               \
	X(QUERY_SERVER_WRITE, QUERY_CYCLE, "QUERY_SERVER_WRITE")             \
	X(QUERY_SERVER_READ, QUERY_CYCLE, "QUERY_SERVER_READ")               \
	X(QUERY_CLIENT_WRITE, QUERY_CYCLE, "QUERY_CLIENT_WRITE")             \
	/* pool: acquiring a server connection */                            \
	X(QUEUE, POOL, "QUEUE")                                              \
	X(CONNECT_TCP, POOL, "CONNECT_TCP")                                  \
	X(CONNECT_TLS, POOL, "CONNECT_TLS")                                  \
	/* stream: streaming protocols (COPY, replication proxy) */          \
	X(STREAM_CLIENT_READ, STREAM, "STREAM_CLIENT_READ")                  \
	X(STREAM_SERVER_READ, STREAM, "STREAM_SERVER_READ")                  \
	X(STREAM_CLIENT_WRITE, STREAM, "STREAM_CLIENT_WRITE")                \
	X(STREAM_SERVER_WRITE, STREAM, "STREAM_SERVER_WRITE")                \
	/* deploy: configuring a server connection for a client */           \
	X(DEPLOY_CLIENT_WRITE, DEPLOY, "DEPLOY_CLIENT_WRITE")                \
	X(DEPLOY_SERVER_WRITE, DEPLOY, "DEPLOY_SERVER_WRITE")                \
	X(DEPLOY_SERVER_READ, DEPLOY, "DEPLOY_SERVER_READ")                  \
	/* reset: cleaning up a server connection after a client */          \
	X(RESET_SERVER_WRITE, RESET, "RESET_SERVER_WRITE")                   \
	X(RESET_SERVER_READ, RESET, "RESET_SERVER_READ")                     \
	/* system: instance-wide operational state */                        \
	X(PAUSE, SYSTEM, "PAUSE")

typedef enum {
#define OD_WAIT_EVENT(id, class, name) OD_WAIT_##id,
	OD_WAIT_EVENT_LIST(OD_WAIT_EVENT)
#undef OD_WAIT_EVENT
		OD_WAIT_COUNT
} od_wait_event_t;

typedef struct {
	alignas(64) atomic_uint_fast64_t counter;
} od_wait_counter_t;

static_assert(alignof(od_wait_counter_t) >= 64,
	      "wait event counter must occupy a whole cache line");

extern od_wait_counter_t od_wait_counters[OD_WAIT_COUNT];

static inline void od_wait_event_start(od_wait_event_t event)
{
	atomic_fetch_add_explicit(&od_wait_counters[event].counter, 1,
				  memory_order_relaxed);
}

static inline void od_wait_event_end(od_wait_event_t event)
{
	atomic_fetch_sub_explicit(&od_wait_counters[event].counter, 1,
				  memory_order_relaxed);
}

const char *od_wait_event_name(od_wait_event_t event);

od_wait_class_t od_wait_event_class(od_wait_event_t event);

const char *od_wait_class_name(od_wait_class_t class);

void od_wait_counters_reset(void);
