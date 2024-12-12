
#include <machinarium.h>
#include <odyssey_test.h>

static void test_coroutine(void *arg)
{
	(void)arg;
	machine_channel_t *channel;
	channel = machine_channel_create(0);
	test(channel != NULL);

	machine_msg_t *msg;
	msg = machine_msg_create(0);
	test(msg != NULL);

	test(machine_channel_ready_count(channel) == 0);

	machine_msg_set_type(msg, 1);
	machine_channel_write(channel, msg);
	test(machine_channel_ready_count(channel) == 1);

	machine_channel_write(channel, msg);
	test(machine_channel_ready_count(channel) == 2);

	machine_channel_write(channel, msg);
	test(machine_channel_ready_count(channel) == 3);

	(void)machine_channel_read_back(channel, UINT32_MAX);
	test(machine_channel_ready_count(channel) == 2);

	(void)machine_channel_read_back(channel, UINT32_MAX);
	test(machine_channel_ready_count(channel) == 1);

	(void)machine_channel_read_back(channel, UINT32_MAX);
	test(machine_channel_ready_count(channel) == 0);

	machine_msg_free(msg);

	machine_channel_free(channel);
}

void machinarium_test_channel_ready_count(void)
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
