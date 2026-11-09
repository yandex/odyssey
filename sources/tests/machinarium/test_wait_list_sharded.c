
#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

/*
 * Sharded wait list with affinity-based wakeup:
 * a waiter is enqueued into the shard of its machine, a waker running
 * on the same machine prefers its own shard, otherwise the globally
 * oldest waiter is woken.
 */

#define TEST_TIMEOUT_MS 3000

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

typedef struct {
	mm_wait_list_t *wl;
	atomic_uint_fast64_t *word;
	atomic_uint_fast64_t *done;
} test_claimer_arg_t;

static inline void consumer_wait(void *arg)
{
	test_consumer_arg_t *c = arg;

	int rc;
	rc = mm_wait_list_compare_wait(c->wl, c->private, 0, TEST_TIMEOUT_MS);
	test(rc == 0);

	atomic_fetch_add(c->done, 1);
}

/* same, but the waiter may legitimately time out (not notified) */
static inline void consumer_wait_nofail(void *arg)
{
	test_consumer_arg_t *c = arg;

	mm_wait_list_compare_wait(c->wl, c->private, 0, TEST_TIMEOUT_MS);

	atomic_fetch_add(c->done, 1);
}

/* machine main: enqueue a waiter and park until notified */
static inline void consumer_machine_main(void *arg)
{
	consumer_wait(arg);
}

static inline void consumer_machine_main_nofail(void *arg)
{
	consumer_wait_nofail(arg);
}

static inline void test_wait_done(atomic_uint_fast64_t *done, int expected)
{
	uint64_t deadline = machine_time_ms() + TEST_TIMEOUT_MS;
	while (atomic_load(done) < (uint64_t)expected) {
		test(machine_time_ms() < deadline);
		machine_sleep(5);
	}
}

/*
 * two waiters on different machines; a plain (non-affinity) notify from
 * the third machine must wake the globally oldest one
 */
static atomic_uint_fast64_t test1_word;
static atomic_uint_fast64_t test1_done;
static mm_wait_list_t *test1_wl;

static inline void test1_notify_machine(void *arg)
{
	(void)arg;

	/* both waiters are enqueued by now */
	machine_sleep(200);

	atomic_store(&test1_word, 1);
	void *private;
	private = mm_wait_list_notify_cb(test1_wl, on_notify_record, NULL);
	/* the waiter of the first (older) machine must be woken */
	test(private == (void *)0x1000);
}

static inline void test_notify_picks_oldest(void *arg)
{
	(void)arg;

	atomic_store(&test1_word, 0);
	atomic_store(&test1_done, 0);

	static mm_wl_shard_t test1_shards[16];
	static uint64_t test1_nonempty[16 / 64 + 1];
	static uint64_t test1_served[16];
	static mm_wait_list_t test1_wl_storage;
	mm_wait_list_init_sharded(&test1_wl_storage, &test1_word,
				  test1_nonempty, test1_served, test1_shards,
				  16);
	test1_wl = &test1_wl_storage;

	test_consumer_arg_t c1;
	c1.wl = test1_wl;
	c1.word = &test1_word;
	c1.done = &test1_done;
	c1.private = (void *)0x1000;

	test_consumer_arg_t c2;
	c2.wl = test1_wl;
	c2.word = &test1_word;
	c2.done = &test1_done;
	c2.private = (void *)0x2000;

	int m1;
	m1 = machine_create("t1_consumer1", consumer_machine_main, &c1);
	test(m1 != -1);

	int m2;
	m2 = machine_create("t1_consumer2", consumer_machine_main_nofail, &c2);
	test(m2 != -1);

	int mn;
	mn = machine_create("t1_notifier", test1_notify_machine, NULL);
	test(mn != -1);

	test_wait_done(&test1_done, 1);

	int rc;
	rc = machine_wait(m1);
	test(rc == 0);
	rc = machine_wait(m2);
	test(rc == 0);
	rc = machine_wait(mn);
	test(rc == 0);

	mm_wait_list_destroy(test1_wl);
}

/*
 * the older waiter sits on a foreign machine, the newer one on the
 * claimer machine: affinity must prefer the own-machine waiter,
 * a plain notify must prefer the globally oldest one
 */
static mm_wait_list_t *test2_wl;
static atomic_uint_fast64_t test2_word;

static atomic_uint_fast64_t test2_done;
static void *test2_affinity_result;
static void *test2_plain_result;

static inline void test2_own_machine_main(void *arg)
{
	(void)arg;

	test_consumer_arg_t own;
	own.wl = test2_wl;
	own.word = &test2_word;
	own.done = &test2_done;
	own.private = (void *)0x0BAD0;

	/* the foreign waiter (0xF00D) is enqueued first by the other
	 * machine, wait a bit to make sure */
	machine_sleep(200);

	int own_coro;
	own_coro = machine_coroutine_create(consumer_wait, &own);
	test(own_coro != -1);

	/* let the own-machine waiter enqueue */
	machine_sleep(200);

	atomic_store(&test2_word, 1);

	/* affinity: own machine shard has a waiter - it must be picked
	 * despite the foreign waiter being older */
	test2_affinity_result = mm_wait_list_notify_cb_affinity(
		test2_wl, on_notify_record, NULL);
	test(test2_affinity_result == (void *)0x0BAD0);

	/* plain notify: the globally oldest (foreign) waiter must be
	 * picked */
	test2_plain_result =
		mm_wait_list_notify_cb(test2_wl, on_notify_record, NULL);
	test(test2_plain_result == (void *)0xF00D);

	/* wait for the own-machine waiter to finish before leaving this
	 * machine: its arg lives on this coroutine stack */
	int rc;
	rc = machine_join(own_coro);
	test(rc == 0);

	atomic_fetch_add(&test2_done, 1);
}

static inline void test2_foreign_machine_main(void *arg)
{
	consumer_wait(arg);
}

static inline void test_affinity_prefers_own_machine(void *arg)
{
	(void)arg;

	atomic_store(&test2_word, 0);
	atomic_store(&test2_done, 0);
	test2_affinity_result = NULL;
	test2_plain_result = NULL;

	static mm_wl_shard_t test2_shards[16];
	static uint64_t test2_nonempty[16 / 64 + 1];
	static uint64_t test2_served[16];
	static mm_wait_list_t test2_wl_storage;
	mm_wait_list_init_sharded(&test2_wl_storage, &test2_word,
				  test2_nonempty, test2_served, test2_shards,
				  16);
	test2_wl = &test2_wl_storage;

	test_consumer_arg_t foreign;
	foreign.wl = test2_wl;
	foreign.word = &test2_word;
	foreign.done = &test2_done;
	foreign.private = (void *)0xF00D;

	int mf;
	mf = machine_create("t2_foreign", test2_foreign_machine_main, &foreign);
	test(mf != -1);

	int mo;
	mo = machine_create("t2_own", test2_own_machine_main, NULL);
	test(mo != -1);

	test_wait_done(&test2_done, 3);

	int rc;
	rc = machine_wait(mf);
	test(rc == 0);
	rc = machine_wait(mo);
	test(rc == 0);

	mm_wait_list_destroy(test2_wl);
}

/*
 * affinity with an empty own shard falls back to the globally oldest
 * waiter
 */
static mm_wait_list_t *test3_wl;
static atomic_uint_fast64_t test3_word;
static atomic_uint_fast64_t test3_done;

static inline void test3_claimer_machine(void *arg)
{
	(void)arg;

	machine_sleep(200);

	atomic_store(&test3_word, 1);
	void *private;
	private = mm_wait_list_notify_cb_affinity(test3_wl, on_notify_record,
						  NULL);
	/* own shard is empty - the foreign waiter must be woken */
	test(private == (void *)0xBEEF);

	atomic_fetch_add(&test3_done, 1);
}

static inline void test_affinity_fallback(void *arg)
{
	(void)arg;

	atomic_store(&test3_word, 0);
	atomic_store(&test3_done, 0);

	static mm_wl_shard_t test3_shards[16];
	static uint64_t test3_nonempty[16 / 64 + 1];
	static uint64_t test3_served[16];
	static mm_wait_list_t test3_wl_storage;
	mm_wait_list_init_sharded(&test3_wl_storage, &test3_word,
				  test3_nonempty, test3_served, test3_shards,
				  16);
	test3_wl = &test3_wl_storage;

	test_consumer_arg_t foreign;
	foreign.wl = test3_wl;
	foreign.word = &test3_word;
	foreign.done = &test3_done;
	foreign.private = (void *)0xBEEF;

	int mf;
	mf = machine_create("t3_foreign", consumer_machine_main, &foreign);
	test(mf != -1);

	int mc;
	mc = machine_create("t3_claimer", test3_claimer_machine, NULL);
	test(mc != -1);

	test_wait_done(&test3_done, 2);

	int rc;
	rc = machine_wait(mf);
	test(rc == 0);
	rc = machine_wait(mc);
	test(rc == 0);

	mm_wait_list_destroy(test3_wl);
}

void machinarium_test_wait_list_sharded(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_notify_picks_oldest",
			    test_notify_picks_oldest, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	id = machine_create("test_affinity_prefers_own_machine",
			    test_affinity_prefers_own_machine, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	id = machine_create("test_affinity_fallback", test_affinity_fallback,
			    NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
