
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

#include <unistd.h>
#include <signal.h>

static void
coroutine(void *arg)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	int rc;
	rc = machine_signal_init(&mask);
	test(rc == 0);

	rc = kill(getpid(), SIGINT);
	test(rc == 0);

	rc = machine_signal_wait(UINT32_MAX);
	test(rc == SIGINT);

	rc = machine_signal_wait(100);
	test(rc == -1);
}

void
test_signal1(void)
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
