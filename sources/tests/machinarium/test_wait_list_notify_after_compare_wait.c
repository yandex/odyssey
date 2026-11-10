#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

typedef struct {
	mm_wait_list_t *wl;
	atomic_uint_fast64_t *wl_word;
	machine_wait_group_t *wg;
} test_arg_t;

static void *opaque = (void *)0xdeadbeefcafebabe;

static void opaque_check(void *op, void *arg)
{
	(void)op;
	(void)arg;
	test(op == opaque);
	test(arg == opaque);
}

static inline void producer(void *arg)
{
	test_arg_t *t_arg = arg;
	mm_wait_list_t *wl = t_arg->wl;
	atomic_uint_fast64_t *wl_word = t_arg->wl_word;

	machine_sleep(500);
	atomic_store(wl_word, 1);
	test(mm_wait_list_notify_cb(wl, opaque_check, opaque) == opaque);
	machine_wait_group_done(t_arg->wg);
}

static inline void consumer(void *arg)
{
	test_arg_t *t_arg = arg;
	mm_wait_list_t *wl = t_arg->wl;
	atomic_uint_fast64_t *wl_word = t_arg->wl_word;

	uint64_t start, end, total_time;
	int rc;

	start = machine_time_ms();
	rc = mm_wait_list_compare_wait(wl, opaque, 0, 1000);
	end = machine_time_ms();
	test(rc == 0);
	total_time = end - start;
	test(total_time > 400 && total_time < 1000);
	test(atomic_load(wl_word) == 1);
	machine_wait_group_done(t_arg->wg);
}

static inline void test_notify_after_compare_wait_coroutines(void *arg)
{
	(void)arg;

	atomic_uint_fast64_t atomic;
	atomic_store(&atomic, 0);

	mm_wait_list_t *wl = mm_wait_list_create(&atomic);

	machine_wait_group_t *wg = machine_wait_group_create();

	test_arg_t t_arg;
	t_arg.wl = wl;
	t_arg.wl_word = &atomic;
	t_arg.wg = wg;
	machine_wait_group_add(wg); /* consumer */
	machine_wait_group_add(wg); /* producer */

	int consumer_id;
	consumer_id = machine_coroutine_create(consumer, &t_arg);
	test(consumer_id != -1);

	int producer_id;
	producer_id = machine_coroutine_create(producer, &t_arg);
	test(producer_id != -1);

	/*
	 * wait for both coroutines to complete; machine_join() is not
	 * used on purpose: a coroutine may complete before the joiner
	 * gets to join it (completed coroutines are not findable), and
	 * the completion order depends on the wakeup dispatch details
	 */
	int rc;
	rc = machine_wait_group_wait(wg, 5000);
	test(rc == 0);

	machine_wait_group_destroy(wg);
	mm_wait_list_free(wl);
}

static inline void test_notify_after_compare_wait_threads(void *arg)
{
	(void)arg;

	atomic_uint_fast64_t atomic;
	atomic_store(&atomic, 0);

	mm_wait_list_t *wl = mm_wait_list_create(&atomic);

	machine_wait_group_t *wg = machine_wait_group_create();

	test_arg_t t_arg;
	t_arg.wl = wl;
	t_arg.wl_word = &atomic;
	t_arg.wg = wg;
	machine_wait_group_add(wg); /* consumer */
	machine_wait_group_add(wg); /* producer */

	int consumer_id;
	consumer_id = machine_create("consumer_thread", consumer, &t_arg);
	test(consumer_id != -1);

	int producer_id;
	producer_id = machine_create("producer_thread", producer, &t_arg);
	test(producer_id != -1);

	int rc;
	rc = machine_wait_group_wait(wg, 5000);
	test(rc == 0);

	machine_wait_group_destroy(wg);

	/* reap the machines */
	rc = machine_wait(producer_id);
	test(rc == 0);
	rc = machine_wait(consumer_id);
	test(rc == 0);

	mm_wait_list_free(wl);
}

void machinarium_test_wait_list_notify_after_compare_wait(void)
{
	machinarium_init();

	int id;
	id = machine_create(
		"test_wait_list_notify_after_compare_wait_coroutines",
		test_notify_after_compare_wait_coroutines, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	id = machine_create("test_wait_list_notify_after_compare_wait_threads",
			    test_notify_after_compare_wait_threads, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
