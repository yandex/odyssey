#include <machinarium/machinarium.h>
#include <machinarium/ds/queue.h>
#include <od_memory.h>
#include <tests/odyssey_test.h>

static void test_init_destroy(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 10, sizeof(int), NULL);

	mm_queue_destroy(&q);
}

static void test_push_pop(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 10, sizeof(int), NULL);

	int val = 5;
	test(mm_queue_push(&q, &val) == 1);
	val = 42;
	test(mm_queue_pop(&q, &val) == 1);
	test(val == 5);

	mm_queue_destroy(&q);
}

static void test_push_full(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 4, sizeof(int), NULL);

	for (int i = 0; i < 4; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	int val = 99;
	test(mm_queue_push(&q, &val) == 0);
	test(mm_queue_size(&q) == 4);

	mm_queue_destroy(&q);
}

static void test_pop_empty(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 4, sizeof(int), NULL);

	int val = 99;
	test(mm_queue_pop(&q, &val) == 0);
	test(val == 99);

	mm_queue_destroy(&q);
}

static void test_fifo_order(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 8, sizeof(int), NULL);

	for (int i = 0; i < 8; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	for (int i = 0; i < 8; i++) {
		int val;
		test(mm_queue_pop(&q, &val) == 1);
		test(val == i);
	}

	mm_queue_destroy(&q);
}

static void test_wraparound(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 4, sizeof(int), NULL);

	for (int i = 0; i < 4; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	for (int i = 0; i < 2; i++) {
		int val;
		test(mm_queue_pop(&q, &val) == 1);
		test(val == i);
	}

	for (int i = 4; i < 6; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	int expected[] = { 2, 3, 4, 5 };
	for (int i = 0; i < 4; i++) {
		int val;
		test(mm_queue_pop(&q, &val) == 1);
		test(val == expected[i]);
	}

	test(mm_queue_size(&q) == 0);

	mm_queue_destroy(&q);
}

static void test_pop_batch_simple(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 8, sizeof(int), NULL);

	for (int i = 0; i < 5; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	int dst[8];
	size_t n = mm_queue_pop_batch(&q, dst, 8);
	test(n == 5);
	for (int i = 0; i < 5; i++) {
		test(dst[i] == i);
	}
	size_t s = mm_queue_size(&q);
	test(s == 0);

	mm_queue_destroy(&q);
}

static void test_pop_batch_wraparound(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 4, sizeof(int), NULL);

	for (int i = 0; i < 4; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}
	for (int i = 0; i < 2; i++) {
		int val;
		mm_queue_pop(&q, &val);
	}

	for (int i = 4; i < 6; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	int dst[4];
	size_t n = mm_queue_pop_batch(&q, dst, 4);
	test(n == 4);
	int expected[] = { 2, 3, 4, 5 };
	for (int i = 0; i < 4; i++) {
		test(dst[i] == expected[i]);
	}

	mm_queue_destroy(&q);
}

static void test_pop_batch_limit(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 8, sizeof(int), NULL);

	for (int i = 0; i < 8; i++) {
		test(mm_queue_push(&q, &i) == 1);
	}

	int dst[4];
	size_t n = mm_queue_pop_batch(&q, dst, 4);
	test(n == 4);
	test(mm_queue_size(&q) == 4);

	for (int i = 0; i < 4; i++) {
		test(dst[i] == i);
	}

	mm_queue_destroy(&q);
}

int dtor_count = 0;
static void my_dtor(void *l)
{
	++dtor_count;
	od_free(*(void **)l);
}

static void test_dtor(void)
{
	mm_queue_t q;

	mm_queue_init(&q, 4, sizeof(int *), my_dtor);

	for (int i = 0; i < 3; i++) {
		int *val = od_malloc(sizeof(int));
		*val = i;
		test(mm_queue_push(&q, &val) == 1);
	}

	mm_queue_destroy(&q);

	test(dtor_count == 3);
}

typedef struct {
	mm_queue_t *q;
	int count;
} thread_arg_t;

static void producer_coroutine(void *arg)
{
	thread_arg_t *a = arg;
	for (int i = 0; i < a->count; i++) {
		while (!mm_queue_push(a->q, &i)) {
			machine_sleep(0);
		}
	}
}

static atomic_uint_fast64_t consumed;
static void consumer_coroutine(void *arg)
{
	atomic_init(&consumed, 0);
	thread_arg_t *a = arg;
	int prev = -1;
	for (int i = 0; i < a->count; i++) {
		int val;
		while (!mm_queue_pop(a->q, &val)) {
			machine_sleep(0);
		}
		test(val == prev + 1);
		prev = val;

		atomic_fetch_add(&consumed, 1);
	}
}

static void test_concurrent(void)
{
	mm_queue_t q;
	mm_queue_init(&q, 16, sizeof(int), NULL);

	thread_arg_t arg = { .q = &q, .count = 1000000 };

	int64_t m1 = machine_create("producer", producer_coroutine, &arg);
	test(m1 != -1);
	int64_t m2 = machine_create("consumer", consumer_coroutine, &arg);
	test(m2 != -1);

	machine_wait(m1);
	machine_wait(m2);

	uint64_t c = atomic_load(&consumed);
	test(c == 1000000);

	mm_queue_destroy(&q);
}

void machinarium_test_queue(void)
{
	machinarium_init();

	test_init_destroy();
	test_push_pop();
	test_push_full();
	test_pop_empty();
	test_fifo_order();
	test_wraparound();
	test_pop_batch_simple();
	test_pop_batch_wraparound();
	test_pop_batch_limit();
	test_dtor();
	test_concurrent();

	machinarium_free();
}
