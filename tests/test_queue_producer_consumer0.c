
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static machine_queue_t *queue;

static int producer;
static int consumer;

static void
test_consumer(void *arg)
{
	int i = 0;
	for (; i < 100; i++) {
		machine_msg_t *msg;
		msg = machine_queue_read(queue, UINT32_MAX);
		machine_msg_free(msg);
	}
}

static void
test_producer(void *arg)
{
	int i = 0;
	for (; i < 100; i++) {
		machine_msg_t *msg;
		msg = machine_msg_create(i, 0);
		test(msg != NULL);
		machine_queue_write(queue, msg);
	}
}

void
test_queue_producer_consumer0(void)
{
	machinarium_init();

	queue = machine_queue_create();
	test(queue != NULL);

	producer = machine_create("producer", test_producer, NULL);
	test(producer != -1);

	consumer = machine_create("consumer", test_consumer, NULL);
	test(consumer != -1);

	int rc;
	rc = machine_wait(consumer);
	test(rc != -1);

	rc = machine_wait(producer);
	test(rc != -1);

	machine_queue_free(queue);

	machinarium_free();
}
