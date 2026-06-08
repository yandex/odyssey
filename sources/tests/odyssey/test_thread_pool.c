#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <odyssey.h>
#include <thread_pool.h>
#include <od_memory.h>
#include <tests/odyssey_test.h>

static void *calc(void *a)
{
	atomic_size_t *arg = a;

	for (size_t i = 0; i < 1000; ++i) {
		atomic_fetch_add(arg, 1);
		if (i % 50 == 0) {
			machine_sleep(1);
		}
	}

	return NULL;
}

static void test_integral(void *arg)
{
	(void)arg;

	/*
	 * 100 tasks which computes 1000 atomic incs
	 */

	atomic_size_t value;
	atomic_init(&value, 0);

	size_t n = 100;

	od_future_t **futures = od_malloc(n * sizeof(od_future_t *));
	test(futures != NULL);
	memset(futures, 0, n * sizeof(od_future_t *));

	od_thread_pool_t pool;
	test(od_thread_pool_init(&pool, "integrator", 5, 100) == 0);

	for (size_t i = 0; i < n; ++i) {
		futures[i] = od_thread_pool_submit(&pool, calc, &value, 0);
		test(futures[i] != NULL);
	}

	for (size_t i = 0; i < n; ++i) {
		test(od_thread_pool_wait(futures[i], 60 * 1000) == 0);
		od_future_unref(futures[i]);
	}

	test(atomic_load(&value) == (n * 1000));

	od_free(futures);

	od_thread_pool_destroy(&pool);
}

static void *sleepy(void *a)
{
	(void)a;

	machine_sleep(1000);

	return NULL;
}

static void test_queue_full(void *arg)
{
	(void)arg;

	size_t n = 2;

	od_future_t **futures = od_malloc(n * sizeof(od_future_t *));
	test(futures != NULL);
	memset(futures, 0, n * sizeof(od_future_t *));

	od_thread_pool_t pool;
	test(od_thread_pool_init(&pool, "sleeper", 2, 2) == 0);

	futures[0] = od_thread_pool_submit(&pool, sleepy, NULL, 0);
	test(futures[0] != NULL);

	futures[1] = od_thread_pool_submit(&pool, sleepy, NULL, 0);
	test(futures[1] != NULL);

	od_future_t *f = od_thread_pool_submit(&pool, sleepy, NULL, 0);
	test(f == NULL);
	test(mm_errno_get() == EAGAIN);

	for (size_t i = 0; i < n; ++i) {
		test(od_thread_pool_wait(futures[i], 60 * 1000) == 0);
		od_future_unref(futures[i]);
	}

	od_free(futures);

	od_thread_pool_destroy(&pool);
}

void odyssey_test_thread_pool(void)
{
	machinarium_init();

	int64_t rc;
	rc = machine_create("test_integral", test_integral, NULL);
	test(rc > 0);

	test(machine_wait(rc) == 0);

	rc = machine_create("test_queue_full", test_queue_full, NULL);
	test(rc > 0);

	test(machine_wait(rc) == 0);

	machinarium_free();
}
