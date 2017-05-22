
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static machine_queue_t queue;

static void
test_fiber2(void *arg)
{
	machine_msg_t msg;
	msg = machine_queue_get(queue, UINT_MAX);
	test(msg != NULL);
	test(machine_msg_get_type(msg) == 123);
	machine_msg_free(msg);

	msg = machine_msg_create(321);
	machine_queue_put(queue, msg);
}

static void
test_fiber(void *arg)
{
	queue = machine_queue_create();
	test(queue != NULL);

	int id;
	id = machine_fiber_create(test_fiber2, NULL);
	machine_sleep(0);

	machine_msg_t msg;
	msg = machine_msg_create(123);
	test(msg != NULL);
	machine_queue_put(queue, msg);

	machine_sleep(0);
	machine_sleep(0);

	msg = machine_queue_get(queue, UINT_MAX);
	test(msg != NULL);
	test(machine_msg_get_type(msg) == 321);
	machine_msg_free(msg);

	machine_join(id);

	machine_queue_free(queue);
}

void
test_queue_rw2(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_fiber, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
