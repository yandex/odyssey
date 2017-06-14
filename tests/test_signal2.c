
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

#include <unistd.h>
#include <signal.h>

static int hits = 0;

static void
coroutine_2(void *arg)
{
	int rc;
	rc = machine_signal_wait(UINT32_MAX);
	test(rc == SIGINT);
	hits++;
}

static void
coroutine(void *arg)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	int rc;
	rc = machine_signal_init(&mask);
	test(rc == 0);

	int64_t id;
	id = machine_coroutine_create(coroutine_2, NULL);
	test(id != -1);

	machine_sleep(0);

	rc = kill(getpid(), SIGINT);
	test(rc == 0);

	rc = machine_signal_wait(UINT32_MAX);
	test(rc == SIGINT);
	hits++;

	machine_sleep(0);

	test(hits == 2);
}

void
test_signal2(void)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	machinarium_init();

	int id;
	id = machine_create("test", coroutine, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();

	sigprocmask(SIG_UNBLOCK, &mask, NULL);
}
