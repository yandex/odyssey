
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <wait_event.h>
#include <tests/odyssey_test.h>

#include <string.h>

static void test_counters_start_end(void)
{
	od_wait_counters_reset();

	for (int event = OD_WAIT_NONE + 1; event < OD_WAIT_COUNT; event++) {
		uint64_t count = atomic_load_explicit(
			&od_wait_counters[event].counter, memory_order_relaxed);
		test(count == 0);
	}

	/* single start/end pair */
	od_wait_event_start(OD_WAIT_QUEUE);
	uint64_t count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUEUE].counter, memory_order_relaxed);
	test(count == 1);

	/* other counters must be untouched */
	count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUERY_CLIENT_READ].counter,
		memory_order_relaxed);
	test(count == 0);

	od_wait_event_end(OD_WAIT_QUEUE);
	count = atomic_load_explicit(&od_wait_counters[OD_WAIT_QUEUE].counter,
				     memory_order_relaxed);
	test(count == 0);

	/* nested/coexisting waits sum up */
	od_wait_event_start(OD_WAIT_QUERY_CLIENT_READ);
	od_wait_event_start(OD_WAIT_QUERY_CLIENT_READ);
	od_wait_event_start(OD_WAIT_QUERY_SERVER_READ);
	count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUERY_CLIENT_READ].counter,
		memory_order_relaxed);
	test(count == 2);
	count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUERY_SERVER_READ].counter,
		memory_order_relaxed);
	test(count == 1);

	od_wait_event_end(OD_WAIT_QUERY_CLIENT_READ);
	od_wait_event_end(OD_WAIT_QUERY_CLIENT_READ);
	od_wait_event_end(OD_WAIT_QUERY_SERVER_READ);
	count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUERY_CLIENT_READ].counter,
		memory_order_relaxed);
	test(count == 0);
	count = atomic_load_explicit(
		&od_wait_counters[OD_WAIT_QUERY_SERVER_READ].counter,
		memory_order_relaxed);
	test(count == 0);
}

static void test_names(void)
{
	/* every event except NONE must have a non-empty unique name */
	for (int event = OD_WAIT_NONE + 1; event < OD_WAIT_COUNT; event++) {
		const char *name = od_wait_event_name(event);
		test(name != NULL);
		test(strlen(name) > 0);
		test(strcmp(name, "UNKNOWN") != 0);

		/* names must be unique */
		for (int other = OD_WAIT_NONE + 1; other < OD_WAIT_COUNT;
		     other++) {
			if (other == event) {
				continue;
			}
			test(strcmp(name, od_wait_event_name(other)) != 0);
		}
	}

	test(strcmp(od_wait_event_name(OD_WAIT_NONE), "NONE") == 0);
	test(strcmp(od_wait_event_name(OD_WAIT_QUEUE), "QUEUE") == 0);

	/* out of range */
	test(strcmp(od_wait_event_name((od_wait_event_t)OD_WAIT_COUNT),
		    "UNKNOWN") == 0);
}

static void test_classes(void)
{
	/* every event except NONE must map to a real class with a name */
	for (int event = OD_WAIT_NONE + 1; event < OD_WAIT_COUNT; event++) {
		od_wait_class_t class = od_wait_event_class(event);
		test(class > OD_WAIT_CLASS_NONE);
		test(class < OD_WAIT_CLASS_COUNT);

		const char *name = od_wait_class_name(class);
		test(name != NULL);
		test(strlen(name) > 0);
		test(strcmp(name, "Unknown") != 0);
	}

	test(od_wait_event_class(OD_WAIT_NONE) == OD_WAIT_CLASS_NONE);
	test(od_wait_event_class(OD_WAIT_QUEUE) == OD_WAIT_CLASS_POOL);
	test(od_wait_event_class(OD_WAIT_CONNECT_TCP) == OD_WAIT_CLASS_POOL);
	test(od_wait_event_class(OD_WAIT_CONNECT_TLS) == OD_WAIT_CLASS_POOL);
	test(od_wait_event_class(OD_WAIT_RATELIMIT_CANCEL) ==
	     OD_WAIT_CLASS_RATELIMIT);
	test(od_wait_event_class(OD_WAIT_CONNECT_CLIENT_TLS) ==
	     OD_WAIT_CLASS_CONNECT);
	test(od_wait_event_class(OD_WAIT_CONNECT_CLIENT_STARTUP) ==
	     OD_WAIT_CLASS_CONNECT);
	test(od_wait_event_class(OD_WAIT_QUERY_CLIENT_WRITE) ==
	     OD_WAIT_CLASS_QUERY_CYCLE);
	test(od_wait_event_class(OD_WAIT_DEPLOY_CLIENT_WRITE) ==
	     OD_WAIT_CLASS_DEPLOY);
	test(od_wait_event_class(OD_WAIT_AUTH_CLIENT_READ) ==
	     OD_WAIT_CLASS_AUTH);
	test(od_wait_event_class(OD_WAIT_AUTH_LDAP) == OD_WAIT_CLASS_AUTH);
	test(od_wait_event_class(OD_WAIT_QUERY_CLIENT_READ) ==
	     OD_WAIT_CLASS_QUERY_CYCLE);
	test(od_wait_event_class(OD_WAIT_QUERY_SERVER_READ) ==
	     OD_WAIT_CLASS_QUERY_CYCLE);
	test(od_wait_event_class(OD_WAIT_QUERY_PLANNING) ==
	     OD_WAIT_CLASS_QUERY_CYCLE);
	test(od_wait_event_class(OD_WAIT_STREAM_CLIENT_WRITE) ==
	     OD_WAIT_CLASS_STREAM);
	test(od_wait_event_class(OD_WAIT_DEPLOY_SERVER_READ) ==
	     OD_WAIT_CLASS_DEPLOY);
	test(od_wait_event_class(OD_WAIT_RESET_SERVER_WRITE) ==
	     OD_WAIT_CLASS_RESET);
	test(od_wait_event_class(OD_WAIT_RESET_SERVER_READ) ==
	     OD_WAIT_CLASS_RESET);
	test(od_wait_event_class(OD_WAIT_AUTH_PAM) == OD_WAIT_CLASS_AUTH);
	test(od_wait_event_class(OD_WAIT_AUTH_EXTERNAL) == OD_WAIT_CLASS_AUTH);
	test(od_wait_event_class(OD_WAIT_AUTH_SERVER_READ) ==
	     OD_WAIT_CLASS_AUTH);
	test(od_wait_event_class(OD_WAIT_PAUSE) == OD_WAIT_CLASS_SYSTEM);
	test(od_wait_event_class(OD_WAIT_RESET_SERVER_READ) ==
	     OD_WAIT_CLASS_RESET);
	test(od_wait_event_class(OD_WAIT_PAUSE) == OD_WAIT_CLASS_SYSTEM);

	/* out of range */
	test(od_wait_event_class((od_wait_event_t)OD_WAIT_COUNT) ==
	     OD_WAIT_CLASS_NONE);
	test(strcmp(od_wait_class_name((od_wait_class_t)OD_WAIT_CLASS_COUNT),
		    "Unknown") == 0);
}

void odyssey_test_wait_event(void)
{
	test_counters_start_end();
	test_names();
	test_classes();
}
