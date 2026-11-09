
#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

/*
 * Served-lag guard of the affinity wakeup: the caller's own shard is
 * preferred only while its service counter runs at most
 * MM_WL_SERVICE_LAG serves ahead of the most lagging non-empty foreign
 * shard; past that the globally oldest waiter is served, so no shard
 * can starve.
 */

#define TEST_TIMEOUT_MS 5000
#define TEST_OWN_WAITERS 6

static mm_wait_list_t test_wl;
static uint64_t test_nonempty[16 / 64 + 1];
static uint64_t test_served[16];
static mm_wl_shard_t test_shards[16];
static atomic_uint_fast64_t test_word;
static atomic_uint_fast64_t test_own_done;

static void on_notify_record(void *private, void *arg)
{
	(void)private;
	(void)arg;
}

typedef struct {
	mm_wait_list_t *wl;
	atomic_uint_fast64_t *word;
	atomic_uint_fast64_t *done;
	void *private;
} test_consumer_arg_t;

static inline void consumer_wait(void *arg)
{
	test_consumer_arg_t *c = arg;

	int rc;
	rc = mm_wait_list_compare_wait(c->wl, c->private, 0, TEST_TIMEOUT_MS);
	test(rc == 0);

	atomic_fetch_add(c->done, 1);
}

/* foreign shard waiter, runs on its own machine */
static inline void foreign_machine_main(void *arg)
{
	consumer_wait(arg);
}

static inline void test_wait_enqueued(int count)
{
	uint64_t deadline = machine_time_ms() + TEST_TIMEOUT_MS;
	while ((int)atomic_load(&test_wl.sleepy_count) < count) {
		test(machine_time_ms() < deadline);
		machine_sleep(5);
	}
}

static inline void guard_test_root(void *arg)
{
	(void)arg;

	atomic_store(&test_word, 0);
	atomic_store(&test_own_done, 0);

	/* foreign machine: its waiter enqueues first (the globally
	 * oldest one), served[foreign] starts at zero */
	test_consumer_arg_t foreign;
	foreign.wl = &test_wl;
	foreign.word = &test_word;
	foreign.done = &test_own_done;
	foreign.private = (void *)0xF00D;

	int mf;
	mf = machine_create("guard_foreign", foreign_machine_main, &foreign);
	test(mf != -1);

	test_wait_enqueued(1);

	/* own shard waiters: coroutines on this (claimer) machine */
	test_consumer_arg_t own[TEST_OWN_WAITERS];
	int i;
	for (i = 0; i < TEST_OWN_WAITERS; i++) {
		own[i].wl = &test_wl;
		own[i].word = &test_word;
		own[i].done = &test_own_done;
		own[i].private = (void *)(long)(0x1000 + i);
		int id = machine_coroutine_create(consumer_wait, &own[i]);
		test(id != -1);
	}

	test_wait_enqueued(1 + TEST_OWN_WAITERS);

	atomic_store(&test_word, 1);

	/* serve the own shard MM_WL_SERVICE_LAG + 1 times: the guard
	 * stays quiet while served[own] <= served[foreign] + LAG */
	void *private;
	for (i = 0; i < MM_WL_SERVICE_LAG + 1; i++) {
		private = mm_wait_list_notify_cb_affinity(
			&test_wl, on_notify_record, NULL);
		test(private == (void *)(long)(0x1000 + i));
	}

	/* served[own] = LAG + 1, served[foreign] = 0: the guard trips
	 * and the globally oldest waiter (the foreign one) must be
	 * served despite the own shard being non-empty */
	private = mm_wait_list_notify_cb_affinity(&test_wl, on_notify_record,
						  NULL);
	test(private == (void *)0xF00D);

	/* the lag is within the bound again - the own shard is served */
	private = mm_wait_list_notify_cb_affinity(&test_wl, on_notify_record,
						  NULL);
	test(private == (void *)(long)0x1005);

	/* wait for the own waiters to complete (they were all served) */
	uint64_t deadline = machine_time_ms() + TEST_TIMEOUT_MS;
	while (atomic_load(&test_own_done) < TEST_OWN_WAITERS) {
		test(machine_time_ms() < deadline);
		machine_sleep(5);
	}

	int rc;
	rc = machine_wait(mf);
	test(rc == 0);
}

void machinarium_test_wait_list_guard(void)
{
	machinarium_init();

	mm_wait_list_init_sharded(&test_wl, &test_word, test_nonempty,
				  test_served, test_shards, 16);

	int id;
	id = machine_create("test_wait_list_guard", guard_test_root, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	mm_wait_list_destroy(&test_wl);

	machinarium_free();
}
