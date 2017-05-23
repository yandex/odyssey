
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

static machine_queue_t queue;
static int consumers_count = 5;
static int consumers_stat[5] = {0};

static void
test_consumer(void *arg)
{
	uintptr_t consumer_id = (uintptr_t)arg;
	for (;;) {
		machine_msg_t msg;
		msg = machine_queue_get(queue, UINT32_MAX);
		consumers_stat[consumer_id]++;
		int is_exit = machine_msg_get_type(msg) == UINT32_MAX;
		machine_msg_free(msg);
		if (is_exit)
			break;
	}
}

static void
test_producer(void *arg)
{
	int i = 0;
	for (; i < 100000; i++) {
		machine_msg_t msg;
		msg = machine_msg_create(i, 0);
		test(msg != NULL);
		machine_queue_put(queue, msg);
	}
	/* exit */
	for (i = 0; i < consumers_count; i++ ){
		machine_msg_t msg;
		msg = machine_msg_create(UINT32_MAX, 0);
		test(msg != NULL);
		machine_queue_put(queue, msg);
	}
}

void
test_queue_producer_consumer1(void)
{
	machinarium_init();

	queue = machine_queue_create();
	test(queue != NULL);

	int producer;
	producer = machine_create("producer", test_producer, NULL);
	test(producer != -1);

	int consumers[consumers_count];
	uintptr_t i = 0;
	for (; i < consumers_count; i++ ){
		consumers[i] = machine_create("consumer", test_consumer, (void*)i);
		test(consumers[i] != -1);
	}

	int rc;
	rc = machine_wait(producer);
	test(rc != -1);

	printf("[");
	for (i = 0; i < consumers_count; i++ ){
		rc = machine_wait(consumers[i]);
		test(rc != -1);
		if (i > 0)
			printf(", ");
		printf("%d", consumers_stat[i]);
		fflush(NULL);
	}
	printf("] ");

	machine_queue_free(queue);

	machinarium_free();
}
