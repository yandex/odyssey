#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/ds/intrusive/lf-stack.h>
#include <tests/odyssey_test.h>

typedef struct {
	mm_lf_stack_entry_t link;
	int value;
} el_t;

static void test_init_destroy(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	test(mm_lf_stack_pop(&st) == NULL);

	mm_lf_stack_destroy(&st);
}

static void test_push_pop(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	el_t el;
	el.value = 1337;
	mm_lf_stack_push(&st, &el.link);

	mm_lf_stack_entry_t *e = mm_lf_stack_pop(&st);
	test(e == &el.link);
	test(mm_container_of(e, el_t, link)->value == 1337);

	test(mm_lf_stack_pop(&st) == NULL);

	mm_lf_stack_destroy(&st);
}

static void test_lifo_order(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	enum { N = 100 };
	el_t el[N];

	for (int i = 0; i < (int)lengthof(el); i++) {
		el[i].value = i;
		mm_lf_stack_push(&st, &el[i].link);
	}

	for (int i = (int)lengthof(el) - 1; i >= 0; i--) {
		mm_lf_stack_entry_t *e = mm_lf_stack_pop(&st);
		test(e == &el[i].link);
		test(mm_container_of(e, el_t, link)->value == i);
	}

	test(mm_lf_stack_pop(&st) == NULL);

	mm_lf_stack_destroy(&st);
}

static void test_reuse(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	el_t el;
	el.value = 1;

	for (int i = 0; i < 1000; i++) {
		mm_lf_stack_push(&st, &el.link);
		mm_lf_stack_entry_t *e = mm_lf_stack_pop(&st);
		test(e == &el.link);
	}

	test(mm_lf_stack_pop(&st) == NULL);

	mm_lf_stack_destroy(&st);
}

/* concurrent push stress test.
 *
 * each thread has a private pool of elements and pushes them all
 * concurrently. after all threads join, the main thread drains the
 * stack and verifies the count.
 */
#define CONC_NTHREADS 8
#define CONC_NPOOL 1000

typedef struct {
	mm_lf_stack_t *st;
	el_t pool[CONC_NPOOL];
	int thread_id;
} conc_arg_t;

static void *conc_worker(void *arg)
{
	conc_arg_t *a = arg;

	for (int i = 0; i < CONC_NPOOL; i++) {
		a->pool[i].value = a->thread_id * CONC_NPOOL + i;
		mm_lf_stack_push(a->st, &a->pool[i].link);
	}

	return NULL;
}

static void test_concurrent(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	conc_arg_t args[CONC_NTHREADS];
	memset(args, 0, sizeof(args));
	for (int i = 0; i < CONC_NTHREADS; i++) {
		args[i].st = &st;
	}

	pthread_t threads[CONC_NTHREADS];
	for (int i = 0; i < CONC_NTHREADS; i++) {
		args[i].thread_id = i;
		pthread_create(&threads[i], NULL, conc_worker, &args[i]);
	}

	for (int i = 0; i < CONC_NTHREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	/* drain remaining and count */
	int remaining = 0;
	while (mm_lf_stack_pop(&st) != NULL) {
		remaining++;
	}

	test(remaining == CONC_NTHREADS * CONC_NPOOL);

	mm_lf_stack_destroy(&st);
}

/* concurrent push+pop: threads pop and push back, simulating a real
 * object pool (take object, use, return). with tagged pointers this
 * is safe from ABA.
 */
#define MIX_NTHREADS 4
#define MIX_NPOOL 256
#define MIX_ITERS 100000

typedef struct {
	mm_lf_stack_t *st;
	el_t pool[MIX_NPOOL];
	int thread_id;
} mix_arg_t;

static void *mix_worker(void *arg)
{
	mix_arg_t *a = arg;
	mm_lf_stack_entry_t *held = NULL;

	for (int i = 0; i < MIX_NPOOL; i++) {
		a->pool[i].value = a->thread_id * MIX_NPOOL + i;
		mm_lf_stack_push(a->st, &a->pool[i].link);
	}

	for (int i = 0; i < MIX_ITERS; i++) {
		mm_lf_stack_entry_t *e = mm_lf_stack_pop(a->st);
		if (e) {
			if (held) {
				mm_lf_stack_push(a->st, held);
			}
			held = e;
		}
	}

	if (held) {
		mm_lf_stack_push(a->st, held);
	}

	return NULL;
}

static void test_concurrent_mix(void)
{
	mm_lf_stack_t st;
	mm_lf_stack_init(&st);

	mix_arg_t args[MIX_NTHREADS];
	memset(args, 0, sizeof(args));
	for (int i = 0; i < MIX_NTHREADS; i++) {
		args[i].st = &st;
		args[i].thread_id = i;
	}

	pthread_t threads[MIX_NTHREADS];
	for (int i = 0; i < MIX_NTHREADS; i++) {
		pthread_create(&threads[i], NULL, mix_worker, &args[i]);
	}

	for (int i = 0; i < MIX_NTHREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	int remaining = 0;
	while (mm_lf_stack_pop(&st) != NULL) {
		remaining++;
	}

	test(remaining == MIX_NTHREADS * MIX_NPOOL);

	mm_lf_stack_destroy(&st);
}

void machinarium_test_lf_stack(void)
{
	machinarium_init();

	test_init_destroy();
	test_push_pop();
	test_lifo_order();
	test_reuse();
	test_concurrent();
	test_concurrent_mix();

	machinarium_free();
}
