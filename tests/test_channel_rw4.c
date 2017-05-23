
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static machine_channel_t channel;

static void
test_fiber_a(void *arg)
{
	machine_msg_t msg;
	msg = machine_channel_read(channel, UINT_MAX);
	test(msg != NULL);
	test(machine_msg_get_type(msg) == 1);
	machine_msg_free(msg);
}

static void
test_fiber_b(void *arg)
{
	machine_msg_t msg;
	msg = machine_channel_read(channel, UINT_MAX);
	test(msg != NULL);
	test(machine_msg_get_type(msg) == 2);
	machine_msg_free(msg);
}

static void
test_fiber_c(void *arg)
{
	machine_msg_t msg;
	msg = machine_channel_read(channel, UINT_MAX);
	test(msg != NULL);
	test(machine_msg_get_type(msg) == 3);
	machine_msg_free(msg);
}

static void
test_fiber(void *arg)
{
	channel = machine_channel_create();
	test(channel != NULL);

	int a;
	a = machine_fiber_create(test_fiber_a, NULL);
	test(a != -1);
	machine_sleep(0);

	int b;
	b = machine_fiber_create(test_fiber_b, NULL);
	test(b != -1);
	machine_sleep(0);

	int c;
	c = machine_fiber_create(test_fiber_c, NULL);
	test(c != -1);
	machine_sleep(0);

	machine_msg_t msg;
	msg = machine_msg_create(1, 0);
	test(msg != NULL);
	machine_channel_write(channel, msg);
	machine_sleep(0);

	msg = machine_msg_create(2, 0);
	test(msg != NULL);
	machine_channel_write(channel, msg);
	machine_sleep(0);

	msg = machine_msg_create(3, 0);
	test(msg != NULL);
	machine_channel_write(channel, msg);
	machine_sleep(0);

	machine_join(a);
	machine_join(b);
	machine_join(c);

	machine_channel_free(channel);
}

void
test_channel_rw4(void)
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
