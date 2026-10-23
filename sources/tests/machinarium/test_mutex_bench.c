
/*
 * machinarium mutex benchmark (playground).
 */

#include <stdio.h>
#include <time.h>

#include <machinarium/machinarium.h>
#include <machinarium/mutex.h>
#include <tests/odyssey_test.h>

typedef struct {
	mm_mutex_t *mutex;
	machine_wait_group_t *wg;
	int iters;
	int yield_every;
	uint64_t counter;
} bench_arg_t;

static uint64_t bench_now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void bench_worker(void *arg)
{
	bench_arg_t *a = arg;

	for (int i = 0; i < a->iters; ++i) {
		test(mm_mutex_lock(a->mutex, UINT32_MAX) == 1);
		a->counter++;
		if (a->yield_every > 0 && (i % a->yield_every) == 0) {
			machine_sleep(0);
		}
		mm_mutex_unlock(a->mutex);
	}

	machine_wait_group_done(a->wg);
}

static void bench_report(const char *name, int coroutines, bench_arg_t *arg,
			 uint64_t start_ns)
{
	machine_wait_group_t *wg = arg->wg;
	test(machine_wait_group_wait(wg, UINT32_MAX) == 0);

	uint64_t ns = bench_now_ns() - start_ns;
	uint64_t total = (uint64_t)coroutines * (uint64_t)arg->iters;

	printf("  %-30s %d coros %12llu iters %8.1f ns/op\n", name, coroutines,
	       (unsigned long long)total, (double)ns / (double)total);

	test(arg->counter == total);
}

static void bench_coroutines(const char *name, int coroutines, int iters,
			     int yield_every)
{
	bench_arg_t arg = { .mutex = mm_mutex_create(),
			    .wg = machine_wait_group_create(),
			    .iters = iters,
			    .yield_every = yield_every,
			    .counter = 0 };
	test(arg.mutex != NULL);
	test(arg.wg != NULL);

	uint64_t start = bench_now_ns();

	for (int i = 0; i < coroutines; ++i) {
		machine_wait_group_add(arg.wg);
		test(machine_coroutine_create(bench_worker, &arg) != -1);
	}

	bench_report(name, coroutines, &arg, start);

	machine_wait_group_destroy(arg.wg);
	mm_mutex_free(arg.mutex);
}

static void bench_machines(const char *name, int machines, int iters)
{
	bench_arg_t arg = { .mutex = mm_mutex_create(),
			    .wg = machine_wait_group_create(),
			    .iters = iters,
			    .yield_every = 0,
			    .counter = 0 };
	test(arg.mutex != NULL);
	test(arg.wg != NULL);

	uint64_t start = bench_now_ns();

	for (int i = 0; i < machines; ++i) {
		machine_wait_group_add(arg.wg);
		test(machine_create("mutex_bench_worker", bench_worker, &arg) !=
		     -1);
	}

	bench_report(name, machines, &arg, start);

	machine_wait_group_destroy(arg.wg);
	mm_mutex_free(arg.mutex);
}

static void mutex_bench_run(void *arg)
{
	(void)arg;

	printf("machinarium mutex benchmark\n");

	bench_coroutines("uncontended:", 1, 10 * 1000 * 1000, 0);
	bench_coroutines("contended (1 machine):", 4, 1000 * 1000, 256);
	bench_machines("contended (4 machines):", 4, 1000 * 1000);
}

void machinarium_mutex_benchmark(void)
{
	machinarium_init();

	int id = machine_create("mutex_bench", mutex_bench_run, NULL);
	test(id != -1);
	test(machine_wait(id) != -1);

	machinarium_free();
}
