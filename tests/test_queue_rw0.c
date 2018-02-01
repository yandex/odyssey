
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
	machine_queue_t *queue;
	queue = machine_queue_create();
	test(queue != NULL);

	machine_msg_t *msg;
	msg = machine_msg_create(123, 0);
	test(msg != NULL);

	machine_queue_write(queue, msg);

	machine_msg_t *msg_in;
	msg_in = machine_queue_read(queue, 0);
	test(msg_in != NULL);
	test(msg_in == msg);
	machine_msg_free(msg_in);

	machine_queue_free(queue);
}

void
test_queue_rw0(void)
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
