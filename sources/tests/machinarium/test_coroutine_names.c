#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>

static inline void sleepy(void *arg)
{
	(void)arg;
	machine_sleep(50);
}

static inline void test_coro_names(void *arg)
{
	(void)arg;

	int64_t coro1;
	coro1 = machine_coroutine_create_named(sleepy, NULL, NULL);
	test(coro1 != -1);

	int64_t coro2;
	coro2 = machine_coroutine_create_named(sleepy, NULL, "");
	test(coro2 != -1);

	int64_t coro3;
	coro3 = machine_coroutine_create_named(sleepy, NULL, "a");
	test(coro3 != -1);

	int64_t coro4;
	coro4 = machine_coroutine_create_named(sleepy, NULL, "ab");
	test(coro4 != -1);

	int64_t coro5;
	coro5 = machine_coroutine_create_named(sleepy, NULL, "abcdef");
	test(coro5 != -1);

	int64_t coro6;
	coro6 = machine_coroutine_create_named(sleepy, NULL, "abcdefghqwerty");
	test(coro6 != -1);

	int64_t coro7;
	coro7 = machine_coroutine_create_named(sleepy, NULL, "abcdefghqwertyi");
	test(coro7 != -1);

	int64_t coro8;
	coro8 = machine_coroutine_create_named(sleepy, NULL,
					       "abcdefghqwertyi_");
	test(coro8 != -1);

	int64_t coro9;
	coro9 = machine_coroutine_create_named(sleepy, NULL,
					       "abcdefghqwertyi_a");
	test(coro9 != -1);

	int64_t coro10;
	coro10 = machine_coroutine_create_named(sleepy, NULL,
						"abcdefghqwertyi_aa");
	test(coro10 != -1);

	test(strcmp(machine_coroutine_get_name(coro1), "") == 0);
	test(strcmp(machine_coroutine_get_name(coro2), "") == 0);
	test(strcmp(machine_coroutine_get_name(coro3), "a") == 0);
	test(strcmp(machine_coroutine_get_name(coro4), "ab") == 0);
	test(strcmp(machine_coroutine_get_name(coro5), "abcdef") == 0);
	test(strcmp(machine_coroutine_get_name(coro6), "abcdefghqwerty") == 0);
	test(strcmp(machine_coroutine_get_name(coro7), "abcdefghqwertyi") == 0);
	test(strcmp(machine_coroutine_get_name(coro8), "abcdefghqwertyi") == 0);
	test(strcmp(machine_coroutine_get_name(coro9), "abcdefghqwertyi") == 0);
	test(strcmp(machine_coroutine_get_name(coro10), "abcdefghqwertyi") ==
	     0);

	machine_join(coro1);
	machine_join(coro2);
	machine_join(coro3);
	machine_join(coro4);
	machine_join(coro5);
	machine_join(coro6);
	machine_join(coro7);
	machine_join(coro8);
	machine_join(coro9);
	machine_join(coro10);
}

void machinarium_test_coroutine_names(void)
{
	machinarium_init();

	int id;
	id = machine_create("test_coro_names", test_coro_names, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
