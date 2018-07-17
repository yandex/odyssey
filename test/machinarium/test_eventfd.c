
#include <machinarium.h>
#include <odyssey_test.h>

static void
test_condition_coroutine(void *arg)
{
	machine_io_t *io = arg;
	uint64_t wakeup_ = 123;
	int rc;
	rc = machine_write(io, (void*)&wakeup_, sizeof(wakeup_), UINT32_MAX);
	test(rc == 0);
}

static void
test_waiter(void *arg)
{
	(void)arg;
	machine_io_t *event = machine_io_create();
	int rc;
	rc = machine_eventfd(event);
	test(rc == 0);

	rc = machine_io_attach(event);
	test(rc == 0);

	int64_t a;
	a = machine_coroutine_create(test_condition_coroutine, event);
	test(a != -1);

	uint64_t wakeup_;
	rc = machine_read(event, (void*)&wakeup_, sizeof(wakeup_), UINT32_MAX);
	test(rc == 0);
	test(wakeup_ == 123);

	machine_close(event);
	machine_stop();
}

void
machinarium_test_eventfd0(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
