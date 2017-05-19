
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static machine_channel_t channel;

static void
test_fiber2(void *arg)
{
	machine_msg_t msg;
	msg = machine_channel_read(channel, UINT_MAX);
	test(msg == NULL);
	test(machine_cancelled());
}

static void
test_fiber(void *arg)
{
	channel = machine_channel_create();
	test(channel != NULL);

	int id;
	id = machine_fiber_create(test_fiber2, NULL);
	machine_sleep(0);
	machine_cancel(id);
	machine_join(id);

	machine_channel_free(channel);
}

void
test_channel_cancel(void)
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
