#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>

#define NUM_THREADS 10

typedef struct {
	machine_wait_list_t *wait_list;
	int num;
} waiter_arg_t;

static inline void test_waiter_coroutine(void *arg)
{
	waiter_arg_t *warg = arg;

	machine_wait_list_t *wait_list = warg->wait_list;

	int rc = machine_wait_list_wait(wait_list, 10000);
	test(rc == 0);

	/*
	 * sleep here to parent coroutine's machine_join
	 * catch this coro before destroying
	 */
	machine_sleep(warg->num * 50);
}

static inline void test_waiter_thread(void *arg)
{
	(void)arg;

	waiter_arg_t *warg = arg;

	waiter_arg_t arg1 = { .wait_list = warg->wait_list, .num = warg->num };
	int waiter1 = machine_coroutine_create(test_waiter_coroutine, &arg1);
	test(waiter1 != -1);

	waiter_arg_t arg2 = { .wait_list = warg->wait_list,
			      .num = warg->num + 1 };
	int waiter2 = machine_coroutine_create(test_waiter_coroutine, &arg2);
	test(waiter2 != -1);

	int rc;
	rc = machine_join(waiter1);
	test(rc == 0);

	rc = machine_join(waiter2);
	test(rc == 0);
}

static inline void test_notify_all(void *arg)
{
	(void)arg;

	machine_wait_list_t *wl = machine_wait_list_create(NULL);

	int waiters[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; ++i) {
		waiter_arg_t *arg = malloc(sizeof(waiter_arg_t));
		arg->wait_list = wl;
		arg->num = 1 + 2 * i;
		char buf[256];
		snprintf(buf, sizeof(buf), "%s%d", "test_waiter_thread_",
			 i + 1);
		int waiter = machine_create(buf, test_waiter_thread, arg);
		test(waiter != -1);

		waiters[i] = waiter;
	}

	machine_sleep(1000);

	machine_wait_list_notify_all(wl);

	int rc;
	for (int i = 0; i < NUM_THREADS; ++i) {
		rc = machine_wait(waiters[i]);
		test(rc == 0);
	}

	/* check that notify_all doesn't break when the wait list is empty */
	machine_wait_list_notify_all(wl);

	machine_wait_list_destroy(wl);
}

void machinarium_test_wait_list_notify_all(void)
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_wait_list_notify_all",
			    test_notify_all, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
