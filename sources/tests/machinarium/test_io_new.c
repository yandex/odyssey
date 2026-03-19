
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

static void coroutine(void *arg)
{
	(void)arg;
	mm_io_t *io = mm_io_create();
	test(io != NULL);

	mm_io_free(io);
}

void machinarium_test_io_new(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", coroutine, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
