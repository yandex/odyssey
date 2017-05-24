
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static void
test_coroutine(void *arg)
{
	machine_channel_t channel;
	channel = machine_channel_create();
	test(channel != NULL);

	machine_msg_t msg;
	msg = machine_msg_create(123, 0);
	test(msg != NULL);

	machine_channel_write(channel, msg);

	machine_msg_t msg_in;
	msg_in = machine_channel_read(channel, 0);
	test(msg_in != NULL);
	test(msg_in == msg);
	machine_msg_free(msg);

	machine_channel_free(channel);
}

void
test_channel_rw0(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_coroutine, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
