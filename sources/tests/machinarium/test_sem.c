#include <machinarium/machinarium.h>
#include <machinarium/sem.h>
#include <tests/odyssey_test.h>

#define NUM_THREADS 10

static atomic_uint_fast64_t concurrency;
static atomic_uint_fast64_t result;

static void worker(void *a)
{
	mm_sem_t *sem = a;

	for (int i = 0; i < 1000000; ++i) {
		mm_sem_wait(sem);
		uint64_t r = atomic_fetch_add(&concurrency, 1);

		if (r > 1) {
			atomic_store(&result, 1);
		}

		test(r <= 3);

		atomic_fetch_sub(&concurrency, 1);
		mm_sem_post(sem);
	}
}

static void do_test(void *arg)
{
	(void)arg;

	mm_sem_t sem;
	mm_sem_init(&sem, 4);

	atomic_init(&concurrency, 0);
	atomic_init(&result, 0);

	int64_t workers[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; ++i) {
		char name[10];
		snprintf(name, sizeof(name), "worker %d", i);
		workers[i] = machine_create(name, worker, &sem);
		test(workers[i] != -1);
	}

	for (int i = 0; i < NUM_THREADS; ++i) {
		machine_wait(workers[i]);
	}

	test(atomic_load(&result) == 1);

	mm_sem_destroy(&sem);
}

void machinarium_test_sem(void)
{
	machinarium_init();

	int id;
	id = machine_create("machinarium_test_sem", do_test, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
