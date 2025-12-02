#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

/* 
There is a wait group that is created and managed by the main thread.
There are a few threads that wait for this wait group, and there is also a separate thread that destroys it.
To synchronize the wait() calls with the destroy() call, a second wait group (waiters) is used. 
Every thread that waits for the first wait group also calls done() on the second one.  
The destroyer first waits for the second wait group (importantly, without timeout), and only then destroys the first one. 
Note that not all done() calls on the first wait group are completed when destroy() is called. 
*/

typedef struct {
	machine_wait_group_t *group;
	machine_wait_group_t *waiters;
} destroyer_arg_t;

static inline void destroyer(void *arg)
{
	destroyer_arg_t *darg = arg;

	machine_wait_group_wait(darg->waiters, UINT32_MAX);
	machine_wait_group_destroy(darg->group);
}

typedef struct {
	machine_wait_group_t *group;
	machine_wait_group_t *waiters;
	uint32_t timeout;
	uint32_t sleep_before_wait;
} waiter_arg_t;

static inline void waiter(void *arg)
{
	waiter_arg_t *warg = arg;

	machine_wait_group_wait(warg->group, 50);
	machine_wait_group_done(warg->waiters);
}

typedef struct {
	machine_wait_group_t *group;
	uint32_t sleep_before_done;
} doner_arg_t;

static inline void doner(void *arg)
{
	doner_arg_t *darg = arg;

	machine_sleep(darg->sleep_before_done);
	machine_wait_group_done(darg->group);
}

static inline void test_wait_group_lifetime(void *arg)
{
	(void)arg;

	machine_wait_group_t *group = machine_wait_group_create();
	machine_wait_group_t *waiters = machine_wait_group_create();

	/* start doners */
	doner_arg_t doner_arg1 = { .group = group, .sleep_before_done = 0 };
	doner_arg_t doner_arg2 = { .group = group, .sleep_before_done = 0 };
	doner_arg_t doner_arg3 = { .group = group, .sleep_before_done = 100 };
	doner_arg_t doner_arg4 = { .group = group, .sleep_before_done = 100 };

	machine_wait_group_add(group);
	int doner_id1 =
		machine_coroutine_create_named(doner, &doner_arg1, "doner_1");
	test(doner_id1 != -1);

	machine_wait_group_add(group);
	int doner_id2 = machine_create("doner_2", doner, &doner_arg2);
	test(doner_id2 != -1);

	machine_wait_group_add(group);
	int doner_id3 =
		machine_coroutine_create_named(doner, &doner_arg3, "doner_3");
	test(doner_id3 != -1);

	machine_wait_group_add(group);
	int doner_id4 = machine_create("doner_4", doner, &doner_arg4);
	test(doner_id4 != -1);

	/* start waiters */
	waiter_arg_t waiter_arg1 = { .group = group, .waiters = waiters };
	waiter_arg_t waiter_arg2 = { .group = group, .waiters = waiters };
	waiter_arg_t waiter_arg3 = { .group = group, .waiters = waiters };
	waiter_arg_t waiter_arg4 = { .group = group, .waiters = waiters };

	machine_wait_group_add(waiters);
	int waiter_id1 = machine_coroutine_create_named(waiter, &waiter_arg1,
							"waiter_1");
	test(waiter_id1 != -1);

	machine_wait_group_add(waiters);
	int waiter_id2 = machine_create("waiter_2", waiter, &waiter_arg2);
	test(waiter_id2 != -1);

	machine_wait_group_add(waiters);
	int waiter_id3 = machine_coroutine_create_named(waiter, &waiter_arg3,
							"waiter_3");
	test(waiter_id3 != -1);

	machine_wait_group_add(waiters);
	int waiter_id4 = machine_create("waiter_4", waiter, &waiter_arg4);
	test(waiter_id4 != -1);

	/* start destroyer */
	destroyer_arg_t destroyer_arg = { .group = group, .waiters = waiters };
	int destroyer_id =
		machine_create("destroyer", destroyer, &destroyer_arg);
	test(destroyer_id != -1);

	/* free resources */

	/* sleep is used here for simplicity, but is is wrong */
	machine_sleep(150);

	machine_wait(destroyer_id);
	machine_join(waiter_id1);
	machine_wait(waiter_id2);
	machine_join(waiter_id3);
	machine_wait(waiter_id4);
	machine_join(doner_id1);
	machine_wait(doner_id2);
	machine_join(doner_id3);
	machine_wait(doner_id4);

	machine_wait_group_destroy(waiters);
}

void machinarium_test_wait_group_lifetime()
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_wait_group_simple_coroutines",
			    test_wait_group_lifetime, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
