
#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>

atomic_int hit = 0;

typedef size_t (*recurse_fn)(size_t);

#ifdef __clang__
size_t __attribute__((noinline, disable_tail_calls))
#else
size_t __attribute__((noinline, optimize("-fno-optimize-sibling-calls")))
#endif
recurse(size_t n)
{
	if (n == 0) {
		return 1;
	}

	if (machinarium_check_stack_depth()) {
		atomic_store(&hit, 1);
		return 1;
	}

	volatile char buf[128];
	(void)buf;

	recurse_fn volatile fn = recurse;
	volatile size_t v = n;
	return v + fn(n - 1);
}

static void test_coroutine(void *arg)
{
	(void)arg;
	size_t unused = recurse(10000000000);
	test(atomic_load(&hit) == 1);
	test(unused > 0);
}

void machinarium_test_check_stack_depth(void)
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
