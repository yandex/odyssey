#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <cmocka.h>
#include <pthread.h>
#include <malloc.h>

#include "sleep_lock.h"
#include "memcontext.h"
#include "memutils.h"

static volatile int free_called = 0;

void __real_free(void *ptr);
void __wrap_free(void *ptr)
{
	free_called += (int) mock();
	__real_free(ptr);
}

static void test_consistency(void **state) {
	mcxt_context_t	top = mcxt_new(NULL),
					old,
					child1 = mcxt_new(top),
					child2 = mcxt_new(top);

	assert_ptr_equal(current_mcxt, NULL);
	old = mcxt_switch_to(top);
	assert_ptr_equal(old, NULL);
	assert_ptr_equal(current_mcxt, top);

	/* top */
	assert_ptr_equal(top->firstchild, child1);
	assert_ptr_equal(top->nextchild, NULL);
	assert_ptr_equal(top->prevchild, NULL);

	/* child */
	assert_ptr_equal(child1->parent, top);
	assert_ptr_equal(child1->firstchild, NULL);
	assert_ptr_equal(child1->nextchild, child2);
	assert_ptr_equal(child1->prevchild, NULL);
	assert_ptr_equal(child1->lastchunk, NULL);

	/* child2 */
	assert_ptr_equal(child2->parent, top);
	assert_ptr_equal(child2->firstchild, NULL);
	assert_ptr_equal(child2->nextchild, NULL);
	assert_ptr_equal(child2->prevchild, child1);
	assert_ptr_equal(child2->lastchunk, NULL);

	old = mcxt_switch_to(old);
	assert_ptr_equal(old, top);
	assert_ptr_equal(current_mcxt, NULL);

	(void) state;	/* keep compiler quiet */
}

static void test_allocation(void **state) {
	mcxt_chunk_t		chunk,
					chunk2,
					chunk3;
	mcxt_context_t	top = mcxt_new(NULL);
	char		   *block3;

	chunk = GetMemoryChunk(mcxt_alloc_mem(top, 100, false));
#ifdef MCXT_PROTECTION_CHECK
	assert_true(chunk->chunk_type == mct_alloc);
#endif
	assert_true(chunk->context == top);
	assert_true(malloc_usable_size(chunk) >= MEMORY_CHUNK_SIZE + 100);
	assert_ptr_equal(top->lastchunk, chunk);

	chunk2 = GetMemoryChunk(mcxt_alloc_mem(top, 130, false));
	assert_ptr_equal(top->lastchunk, chunk2);
	assert_ptr_equal(chunk2->prev, chunk);
	assert_ptr_equal(chunk2->next, NULL);
	assert_true(malloc_usable_size(chunk2) >= MEMORY_CHUNK_SIZE + 130);

	assert_ptr_equal(chunk->next, chunk2);
	assert_ptr_equal(chunk->prev, NULL);

	block3 = mcxt_alloc_mem(top, 10, true);
	chunk3 = GetMemoryChunk(block3);
	assert_ptr_equal(top->lastchunk, chunk3);
	assert_ptr_equal(chunk3->prev, chunk2);
	assert_ptr_equal(chunk3->next, NULL);
	assert_ptr_equal(chunk2->next, chunk3);
	assert_ptr_equal(chunk2->prev, chunk);

	for (int i = 0; i < 10; i++)
		assert_true(block3[i] == '\0');

	will_return_always(__wrap_free, 1);
	free_called = 0;

	mcxt_reset(top, false);
	assert_ptr_equal(top->lastchunk, NULL);
	assert_int_equal(free_called, 3);

	mcxt_delete(top);
	assert_int_equal(free_called, 4);

	(void) state;	/* keep compiler quiet */
}

static void test_tree_deletion(void **state) {
	mcxt_context_t	top = mcxt_new(NULL),
					child1 = mcxt_new(top),
					child2 = mcxt_new(top);

	mcxt_new(child1);
	mcxt_new(child1);
	mcxt_new(child2);

	will_return_always(__wrap_free, 1);
	free_called = 0;
	mcxt_delete(top);
	assert_int_equal(free_called, 6);

	(void) state;	/* keep compiler quiet */
}

static void test_part_tree_deletion(void **state) {
	mcxt_context_t	top = mcxt_new(NULL),
					child1,
					child1_1,
					child2,
					child2_1,
					child2_2,
					child2_3,
					child2_1_1,
					child2_1_2,
					child3;

	will_return_always(__wrap_free, 1);

	assert_ptr_equal(top->firstchild, NULL);
	assert_ptr_equal(top->nextchild, NULL);
	assert_ptr_equal(top->prevchild, NULL);
	assert_ptr_equal(top->parent, NULL);

	child1 = mcxt_new(top);
	assert_ptr_equal(top->firstchild, child1);
	assert_ptr_equal(child1->firstchild, NULL);
	assert_ptr_equal(child1->nextchild, NULL);
	assert_ptr_equal(child1->prevchild, NULL);
	assert_ptr_equal(child1->parent, top);

	child1_1 = mcxt_new(child1);
	assert_ptr_equal(child1->firstchild, child1_1);
	assert_ptr_equal(child1->nextchild, NULL);
	assert_ptr_equal(child1->prevchild, NULL);
	assert_ptr_equal(child1->parent, top);

	assert_ptr_equal(child1_1->firstchild, NULL);
	assert_ptr_equal(child1_1->nextchild, NULL);
	assert_ptr_equal(child1_1->prevchild, NULL);
	assert_ptr_equal(child1_1->parent, child1);

	child2 = mcxt_new(top);
	assert_ptr_equal(top->firstchild, child1);
	assert_ptr_equal(child2->firstchild, NULL);
	assert_ptr_equal(child2->nextchild, NULL);
	assert_ptr_equal(child2->prevchild, child1);
	assert_ptr_equal(child2->parent, top);

	assert_ptr_equal(child1->nextchild, child2);
	assert_ptr_equal(child1->prevchild, NULL);
	assert_ptr_equal(child1->parent, top);

	free_called = 0;
	mcxt_delete(child1_1);
	assert_int_equal(free_called, 1);
	assert_ptr_equal(child1->firstchild, NULL);

	child2_1 = mcxt_new(child2);
	child2_2 = mcxt_new(child2);
	child2_3 = mcxt_new(child2);
	child2_1_1 = mcxt_new(child2_1);
	child2_1_2 = mcxt_new(child2_1);

	child3 = mcxt_new(top);

	assert_ptr_equal(child2_1->prevchild, NULL);
	assert_ptr_equal(child2_1->nextchild, child2_2);
	assert_ptr_equal(child2_2->prevchild, child2_1);
	assert_ptr_equal(child2_2->nextchild, child2_3);
	assert_ptr_equal(child2_3->prevchild, child2_2);
	assert_ptr_equal(child2_3->nextchild, NULL);
	assert_ptr_equal(child2_1->firstchild, child2_1_1);
	assert_ptr_equal(child2_1_1->nextchild, child2_1_2);
	assert_ptr_equal(child2_1_2->prevchild, child2_1_1);

	free_called = 0;
	mcxt_delete(child2);

	assert_ptr_equal(top->firstchild, child1);
	assert_ptr_equal(child1->nextchild, child3);
	assert_ptr_equal(child1->prevchild, NULL);

	assert_ptr_equal(child3->prevchild, child1);
	assert_ptr_equal(child3->nextchild, NULL);

	assert_int_equal(free_called, 6);

	(void) state;	/* keep compiler quiet */
}

static void test_reset(void **state) {
	mcxt_context_t	top = mcxt_new(NULL),
					child1 = mcxt_new(top),
					child2 = mcxt_new(top),
					old;

	free_called = 0;
	will_return_always(__wrap_free, 1);

	old = mcxt_switch_to(top);
	mcxt_alloc(10);
	mcxt_alloc(10);
	mcxt_alloc(10);
	mcxt_switch_to(old);

	old = mcxt_switch_to(child1);
	mcxt_alloc(10);
	mcxt_alloc(10);
	mcxt_switch_to(old);

	old = mcxt_switch_to(child2);
	mcxt_alloc(10);
	mcxt_switch_to(old);

	assert_int_equal(mcxt_chunks_count(top), 3);
	assert_int_equal(mcxt_chunks_count(child1), 2);
	assert_int_equal(mcxt_chunks_count(child2), 1);

	mcxt_reset(top, false);
	assert_int_equal(mcxt_chunks_count(top), 0);
	assert_int_equal(mcxt_chunks_count(child1), 2);
	assert_int_equal(mcxt_chunks_count(child2), 1);

	mcxt_reset(top, true);
	assert_int_equal(mcxt_chunks_count(top), 0);
	assert_int_equal(mcxt_chunks_count(child1), 0);
	assert_int_equal(mcxt_chunks_count(child2), 0);
	assert_int_equal(free_called, 6);

	(void) state;	/* keep compiler quiet */
}

static void test_helpers(void **state) {
	mcxt_context_t	top = mcxt_new(NULL),
					child1 = mcxt_new(top),
					old;
	mcxt_chunk_t		chunk1,
					chunk2;

	will_return_always(__wrap_free, 1);

	mcxt_switch_to(top);
	chunk1 = GetMemoryChunk(mcxt_alloc(10));
	assert_ptr_equal(chunk1->context, top);

	old = mcxt_switch_to(child1);
	assert_ptr_equal(old, top);
	chunk2 = GetMemoryChunk(mcxt_alloc0(10));
	assert_ptr_equal(chunk2->context, child1);
	old = mcxt_switch_to(old);
	assert_ptr_equal(old, child1);

	assert_int_equal(mcxt_chunks_count(top), 1);
	assert_int_equal(mcxt_chunks_count(child1), 1);

	free_called = 0;
	mcxt_free(ChunkDataOffset(chunk1));
	mcxt_free(ChunkDataOffset(chunk2));

	assert_int_equal(free_called, 2);
	assert_int_equal(mcxt_chunks_count(top), 0);
	assert_int_equal(mcxt_chunks_count(child1), 0);

	(void) state;	/* keep compiler quiet */
}

void callback1(void *arg)
{
	int	*counter = arg;
	(*counter)++;
}

void callback2(void *arg)
{
	int	*counter = arg;
	(*counter)++;
}

static void test_callbacks(void **state) {
	int counter1 = 0,
		counter2 = 0;

	will_return_always(__wrap_free, 1);
	free_called = 0;

	mcxt_context_t	top = mcxt_new(NULL);
	mcxt_add_reset_callback(top, callback1, (void *) &counter1);
	mcxt_add_reset_callback(top, callback2, (void *) &counter2);
	mcxt_reset(top, true);
	assert_int_equal(counter1, 1);
	assert_int_equal(counter2, 1);
	mcxt_clean_reset_callbacks(top);
	assert_int_equal(free_called, 2);

	mcxt_reset(top, true);
	assert_int_equal(counter1, 1);
	assert_int_equal(counter2, 1);

	(void) state;	/* keep compiler quiet */
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_consistency),
        cmocka_unit_test(test_helpers),
        cmocka_unit_test(test_allocation),
        cmocka_unit_test(test_tree_deletion),
		cmocka_unit_test(test_part_tree_deletion),
		cmocka_unit_test(test_reset),
		cmocka_unit_test(test_helpers),
		cmocka_unit_test(test_callbacks)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
