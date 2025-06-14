#include <machinarium.h>
#include <odyssey_test.h>

typedef struct {
	machine_wait_list_t *wait_list;
	int num;
} waiter_arg_t;

static inline void test_waiter(void *arg)
{
	waiter_arg_t *warg = arg;

	machine_wait_list_t *wait_list = warg->wait_list;

	int rc = machine_wait_list_wait(wait_list, 1000);
	test(rc == 0);

	/*
	 * sleep here to parent coroutine's machine_join
	 * catch this coro before destroying
	 */
	machine_sleep(warg->num * 100);
}

static inline void test_notify_all(void *arg)
{
	(void)arg;

	machine_wait_list_t *wl = machine_wait_list_create(NULL);

	waiter_arg_t arg1 = { .wait_list = wl, .num = 1 };
	int waiter1 = machine_coroutine_create(test_waiter, &arg1);
	test(waiter1 != -1);

	waiter_arg_t arg2 = { .wait_list = wl, .num = 2 };
	int waiter2 = machine_coroutine_create(test_waiter, &arg2);
	test(waiter2 != -1);

	waiter_arg_t arg3 = { .wait_list = wl, .num = 3 };
	int waiter3 = machine_coroutine_create(test_waiter, &arg3);
	test(waiter3 != -1);

	machine_sleep(100);

	machine_wait_list_notify_all(wl);

	int rc;
	rc = machine_join(waiter1);
	test(rc == 0);

	rc = machine_join(waiter2);
	test(rc == 0);

	rc = machine_join(waiter3);
	test(rc == 0);

	machine_wait_list_destroy(wl);
}

void machinarium_test_wait_list_notify_all()
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