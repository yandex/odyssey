#include <machinarium/machinarium.h>
#include <machinarium/ds/heap.h>
#include <tests/odyssey_test.h>

typedef struct {
	mm_heap_node_t hnode;
	int value;
} el_t;

static inline int cmp(const mm_heap_node_t *a, const mm_heap_node_t *b)
{
	const el_t *aa = mm_container_of(a, el_t, hnode);
	const el_t *bb = mm_container_of(b, el_t, hnode);

	if (aa->value < bb->value) {
		return 1;
	}

	return 0;
}

static void test_min_pop_empty(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	test(mm_heap_validate(&h));

	test(mm_heap_size(&h) == 0);
	test(mm_heap_min(&h) == NULL);
	test(mm_heap_pop(&h) == NULL);
}

static void test_push_pop(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	el_t el;
	el.value = 1337;
	mm_heap_push(&h, &el.hnode);

	test(mm_heap_size(&h) == 1);
	test(mm_heap_min(&h) == &el.hnode);

	test(mm_heap_validate(&h));

	mm_heap_node_t *e = mm_heap_pop(&h);
	test(e == &el.hnode);
	test(mm_container_of(e, el_t, hnode)->value == 1337);

	test(mm_heap_size(&h) == 0);
	test(mm_heap_min(&h) == NULL);
	test(mm_heap_pop(&h) == NULL);
}

static void test_push_min(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	el_t el1;
	el1.value = 1337;
	mm_heap_push(&h, &el1.hnode);

	el_t el2;
	el2.value = -1337;
	mm_heap_push(&h, &el2.hnode);

	test(mm_heap_validate(&h));

	test(mm_heap_size(&h) == 2);
	test(mm_heap_min(&h) == &el2.hnode);

	mm_heap_node_t *e = mm_heap_pop(&h);
	test(e == &el2.hnode);
	test(mm_container_of(e, el_t, hnode)->value == -1337);

	test(mm_heap_size(&h) == 1);

	e = mm_heap_pop(&h);
	test(e == &el1.hnode);
	test(mm_container_of(e, el_t, hnode)->value == 1337);

	test(mm_heap_size(&h) == 0);
	test(mm_heap_min(&h) == NULL);
	test(mm_heap_pop(&h) == NULL);
}

static void test_push_pop_some(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	el_t el[10];
	for (int i = (int)lengthof(el) - 1; i >= 0; --i) {
		el[i].value = i;
		mm_heap_push(&h, &el[i].hnode);
	}

	test(mm_heap_validate(&h));

	for (int i = 0; i < (int)lengthof(el); ++i) {
		el_t *e = mm_container_of(mm_heap_pop(&h), el_t, hnode);
		test(e->value == i);
	}

	test(mm_heap_size(&h) == 0);
	test(mm_heap_min(&h) == NULL);
}

static void test_remove_middle_with_right_child(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	el_t el[7];
	for (int i = 0; i < 7; ++i) {
		el[i].value = i;
		mm_heap_push(&h, &el[i].hnode);
	}

	mm_heap_remove(&h, &el[1].hnode);

	test(mm_heap_size(&h) == 6);
	test(mm_heap_validate(&h));

	int expected[] = { 0, 2, 3, 4, 5, 6 };
	for (int i = 0; i < 6; ++i) {
		el_t *e = mm_container_of(mm_heap_pop(&h), el_t, hnode);
		test(e->value == expected[i]);
	}
}

static void test_random(void)
{
	mm_heap_t h;
	mm_heap_init(&h, cmp);

	enum { N = 500 };
	el_t el[N];
	int alive[N];
	memset(alive, 0, sizeof(alive));

	srand(42);

	for (int iter = 0; iter < 20000; ++iter) {
		int op = rand() % 2;
		int idx = rand() % N;

		if (op == 0 && !alive[idx]) {
			el[idx].value = rand() % 1000;
			mm_heap_push(&h, &el[idx].hnode);
			alive[idx] = 1;
		} else if (alive[idx]) {
			mm_heap_remove(&h, &el[idx].hnode);
			alive[idx] = 0;
		}

		test(mm_heap_validate(&h));
	}
}

void machinarium_test_heap(void)
{
	machinarium_init();

	test_min_pop_empty();
	test_push_pop();
	test_push_min();
	test_push_pop_some();
	test_remove_middle_with_right_child();
	test_random();

	machinarium_free();
}
