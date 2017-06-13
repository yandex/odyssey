
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
	machine_queue_free(queue);
}

void
test_queue_create(void)
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
