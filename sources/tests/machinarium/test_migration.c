#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/mm.h>
#include <tests/odyssey_test.h>

typedef struct {
	uint64_t destination;
	machine_wait_flag_t *stop;
	machine_wait_flag_t *started;
	atomic_int finished;
	int queued;
	atomic_int blocked;
	atomic_int release;
	int stop_machines;
} migration_test_t;

typedef struct {
	migration_test_t *test;
	mm_coroutine_t *coroutine;
	uint64_t home;
	int value;
} visit_t;

static void migration_destination(void *arg)
{
	migration_test_t *t = arg;
	*machine_thread_private() = t;
	if (t->queued) {
		atomic_store(&t->blocked, 1);
		while (!atomic_load(&t->release)) {
			usleep(1000);
		}
	}
	test(machine_wait_flag_wait(t->stop, UINT32_MAX) == 0);
}

static void migration_visit(void *arg)
{
	visit_t *v = arg;
	test(machine_self() == v->test->destination);
	test(mm_self->scheduler.current == v->coroutine);
	test(*machine_thread_private() == v->test);
	int local = ++v->value;
	machine_sleep(1);
	test(local == v->value);
	test(machine_self() == v->test->destination);
	test(mm_self->scheduler.current == v->coroutine);
	mm_errno_set(EIO);
}

static void migration_child(void *arg)
{
	migration_test_t *t = arg;
	visit_t v = { .test = t,
		      .coroutine = mm_self->scheduler.current,
		      .home = machine_self(),
		      .value = 0 };
	void **private = machine_thread_private();
	for (int i = 0; i < 100; i++) {
		test(machine_coroutine_call_on(t->destination, migration_visit,
					       &v) == 0);
		test(machine_errno() == EIO);
		test(machine_self() == v.home);
		test(machine_thread_private() == private);
		test(mm_self->scheduler.current == v.coroutine);
	}
	test(v.value == 100);
	atomic_fetch_add(&t->finished, 1);
}

static void migration_source(void *arg)
{
	migration_test_t *t = arg;
	int64_t ids[16];
	for (int i = 0; i < 16; i++) {
		ids[i] = machine_coroutine_create(migration_child, t);
		test(ids[i] != -1);
	}
	for (int i = 0; i < 16; i++) {
		/* Some children may have already finished. */
		int rc = machine_join(ids[i]);
		test(rc == 0 || machine_errno() == ENOENT);
	}
}

static void migration_noop(void *arg)
{
	int *called = arg;
	(*called)++;
}

static void migration_nested(void *arg)
{
	test(machine_coroutine_call_on(machine_self(), migration_noop, arg) ==
	     -1);
	test(machine_errno() == EINVAL);
}

static void migration_invalid(void *arg)
{
	(void)arg;
	int called = 0;
	test(machine_coroutine_call_on(UINT64_MAX, migration_noop, &called) ==
	     -1);
	test(machine_errno() == ENOENT);
	test(called == 0);
	test(machine_coroutine_call_on(machine_self(), migration_noop,
				       &called) == 0);
	test(called == 1);
	test(machine_coroutine_call_on(machine_self(), migration_nested,
				       &called) == 0);
	test(called == 1);
	test(machine_coroutine_call_on(machine_self(), NULL, NULL) == -1);
	test(machine_errno() == EINVAL);
	test(machine_cancel(mm_self->scheduler.current->id) == 0);
	test(machine_coroutine_call_on(machine_self(), migration_noop,
				       &called) == -1);
	test(machine_errno() == ECANCELED);
	test(called == 1);
}

void machinarium_test_migration(void)
{
	test(machinarium_init() == 0);
	migration_test_t t = { 0 };
	t.stop = machine_wait_flag_create();
	test(t.stop != NULL);
	int64_t destination =
		machine_create("destination", migration_destination, &t);
	test(destination != -1);
	t.destination = destination;
	int64_t sources[4];
	for (int i = 0; i < 4; i++) {
		sources[i] = machine_create("source", migration_source, &t);
		test(sources[i] != -1);
	}
	for (int i = 0; i < 4; i++) {
		test(machine_wait(sources[i]) == 0);
	}
	test(atomic_load(&t.finished) == 64);
	machine_wait_flag_set(t.stop);
	test(machine_wait(destination) == 0);
	machine_wait_flag_destroy(t.stop);
	int64_t invalid = machine_create("invalid", migration_invalid, NULL);
	test(invalid != -1);
	test(machine_wait(invalid) == 0);
	machinarium_free();
}

static void migration_cancel_visit(void *arg)
{
	migration_test_t *t = arg;
	test(!t->queued);
	machine_wait_flag_set(t->started);
	machine_sleep(60000);
	test(machine_cancelled());
	test(machine_errno() == ECANCELED);
}

static void migration_cancel_child(void *arg)
{
	migration_test_t *t = arg;
	uint64_t home = machine_self();
	int rc = machine_coroutine_call_on(t->destination,
					   migration_cancel_visit, t);
	test(rc == (t->queued ? -1 : 0));
	test(machine_self() == home);
	test(machine_cancelled());
	test(machine_errno() == ECANCELED);
	atomic_fetch_add(&t->finished, 1);
}

static void migration_cancel_source(void *arg)
{
	migration_test_t *t = arg;
	while (t->queued && !atomic_load(&t->blocked)) {
		machine_sleep(1);
	}
	int64_t child = machine_coroutine_create_named(migration_cancel_child,
						       t, "visitor");
	test(child != -1);
	if (t->queued) {
		machine_sleep(10);
	} else {
		test(machine_wait_flag_wait(t->started, 1000) == 0);
	}
	test(strcmp(machine_coroutine_get_name(child), "visitor") == 0);
	test(machine_cancel(child) == 0);
	if (t->queued) {
		/* Repeated cancellation must not fill the finite wakeup pipe used
		 * on BSD/macOS while the target cannot drain notifications. */
		for (int i = 0; i < 10000; i++) {
			test(machine_cancel(child) == 0);
		}
#if defined(__linux__)
		/* eventfd has a larger counter; check the same bound explicitly. */
		mm_machine_t *destination = mm_machinemgr_find_by_id(
			&machinarium.machine_mgr, t->destination);
		test(destination != NULL);
		uint64_t notifications;
		int fd = mm_eventmgr_wakeup_fd(&destination->event_mgr);
		test(read(fd, &notifications, sizeof(notifications)) ==
		     sizeof(notifications));
		test(notifications <= 2); /* one arrival and one cancellation */
		mm_eventmgr_wakeup(fd);
#endif
	}
	atomic_store(&t->release, 1);
	test(machine_join(child) == 0);
	test(atomic_load(&t->finished) == 1);
	machine_wait_flag_set(t->stop);
}

void machinarium_test_migration_cancel(void)
{
	for (int queued = 0; queued <= 1; queued++) {
		test(machinarium_init() == 0);
		migration_test_t t = { .queued = queued };
		t.stop = machine_wait_flag_create();
		t.started = machine_wait_flag_create();
		test(t.stop && t.started);
		int64_t destination = machine_create("destination",
						     migration_destination, &t);
		test(destination != -1);
		t.destination = destination;
		int64_t source =
			machine_create("source", migration_cancel_source, &t);
		test(source != -1);
		/* Joining must not make the destination unavailable to transfers. */
		test(machine_wait(destination) == 0);
		test(machine_wait(source) == 0);
		machine_wait_flag_destroy(t.stop);
		machine_wait_flag_destroy(t.started);
		machinarium_free();
	}
}

static void migration_lifetime_visit(void *arg)
{
	migration_test_t *t = arg;
	int called = 0;
	test(machine_coroutine_call_on(t->destination, migration_noop,
				       &called) == -1);
	test(machine_errno() == EINVAL);
	test(called == 0);
	machine_wait_flag_set(t->stop);
	if (t->stop_machines) {
		/* Let the destination's main coroutine release its wait flag. */
		machine_sleep(1);
		test(machine_stop(machine_self()) == 0);
	}
	/* Both the source and destination main coroutines may now be gone. */
	machine_sleep(20);
}

static void migration_lifetime_child(void *arg)
{
	migration_test_t *t = arg;
	uint64_t home = machine_self();
	test(machine_coroutine_call_on(t->destination, migration_lifetime_visit,
				       t) == 0);
	test(machine_self() == home);
	atomic_store(&t->finished, 1);
}

static void migration_lifetime_source(void *arg)
{
	test(machine_coroutine_create(migration_lifetime_child, arg) != -1);
}

static void migration_lifetime_test(int stop_machines)
{
	test(machinarium_init() == 0);
	migration_test_t t = { .stop_machines = stop_machines };
	t.stop = machine_wait_flag_create();
	test(t.stop != NULL);
	int64_t destination =
		machine_create("destination", migration_destination, &t);
	test(destination != -1);
	t.destination = destination;
	int64_t source =
		machine_create("source", migration_lifetime_source, &t);
	test(source != -1);
	test(machine_wait(destination) == 0);
	test(machine_wait(source) == 0);
	test(atomic_load(&t.finished) == 1);
	machine_wait_flag_destroy(t.stop);
	machinarium_free();
}

void machinarium_test_migration_lifetime(void)
{
	migration_lifetime_test(0);
	migration_lifetime_test(1);
}
