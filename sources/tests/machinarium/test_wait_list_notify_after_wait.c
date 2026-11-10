#include <machinarium/machinarium.h>
#include <machinarium/wait_list.h>
#include <tests/odyssey_test.h>

typedef struct {
	mm_wait_list_t *wl;
	machine_wait_group_t *wg;
} test_arg_t;

static inline void producer_coroutine(void *arg)
{
	test_arg_t *t_arg = arg;

	machine_sleep(500);
	mm_wait_list_notify(t_arg->wl);
	machine_wait_group_done(t_arg->wg);
}

static inline void consumer_coroutine(void *arg)
{
	test_arg_t *t_arg = arg;
	mm_wait_list_t *wl = t_arg->wl;

	uint64_t start, end, total_time;
	int rc;

	start = machine_time_ms();
	rc = mm_wait_list_wait(wl, NULL, 1000);
	end = machine_time_ms();
	test(rc == 0);
	total_time = end - start;
	test(total_time > 400 && total_time < 1000);
	machine_wait_group_done(t_arg->wg);
}

static inline void test_notify_after_wait(void *arg)
{
	(void)arg;

	mm_wait_list_t *wl = mm_wait_list_create(NULL);

	machine_wait_group_t *wg = machine_wait_group_create();

	test_arg_t t_arg;
	t_arg.wl = wl;
	t_arg.wg = wg;
	machine_wait_group_add(wg); /* consumer */
	machine_wait_group_add(wg); /* producer */

	int consumer_id;
	consumer_id = machine_coroutine_create(consumer_coroutine, &t_arg);
	test(consumer_id != -1);

	int producer_id;
	producer_id = machine_coroutine_create(producer_coroutine, &t_arg);
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

void machinarium_test_wait_list_notify_after_wait(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_wait_list_notify_after_wait",
			    test_notify_after_wait, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
