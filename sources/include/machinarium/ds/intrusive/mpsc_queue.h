#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * lock-free intrusive multi-producer single-consumer (!!!) queue.
 */

#include <stdatomic.h>
#include <stdint.h>

typedef struct mm_mpsc_node {
	_Atomic(struct mm_mpsc_node *) next;
} mm_mpsc_node_t;

typedef struct {
	/* consumer-only actually, but let it be atomic anyway */
	_Atomic(mm_mpsc_node_t *) head;
	_Atomic(mm_mpsc_node_t *) tail;
	mm_mpsc_node_t stub;
	/* approximate queue size */
	_Atomic(size_t) size;
} mm_mpsc_queue_t;

static inline void mm_mpsc_queue_init(mm_mpsc_queue_t *q)
{
	atomic_init(&q->stub.next, NULL);
	atomic_init(&q->head, &q->stub);
	atomic_init(&q->tail, &q->stub);
	atomic_init(&q->size, 0);
}

static inline size_t mm_mpsc_queue_size(mm_mpsc_queue_t *q)
{
	return atomic_load_explicit(&q->size, memory_order_relaxed);
}

static inline int mm_mpsc_queue_empty(mm_mpsc_queue_t *q)
{
	mm_mpsc_node_t *head =
		atomic_load_explicit(&q->head, memory_order_relaxed);
	mm_mpsc_node_t *next =
		atomic_load_explicit(&head->next, memory_order_acquire);

	return next == NULL;
}

/*
 * Pop one element. Returns NULL if queue is empty or a producer is
 * mid-push (the window between atomic_exchange and prev->next = n).
 */
static inline mm_mpsc_node_t *mm_mpsc_queue_pop(mm_mpsc_queue_t *q)
{
	mm_mpsc_node_t *head =
		atomic_load_explicit(&q->head, memory_order_relaxed);
	mm_mpsc_node_t *next =
		atomic_load_explicit(&head->next, memory_order_acquire);

	if (next == NULL) {
		/* nb: may be incomplete push, ignore anyway */
		return NULL;
	}

	atomic_store_explicit(&q->head, next, memory_order_relaxed);
	atomic_fetch_sub_explicit(&q->size, 1, memory_order_relaxed);

	return next;
}

static inline size_t mm_mpsc_queue_push(mm_mpsc_queue_t *q, mm_mpsc_node_t *n)
{
	atomic_store_explicit(&n->next, NULL, memory_order_relaxed);
	mm_mpsc_node_t *prev =
		atomic_exchange_explicit(&q->tail, n, memory_order_acq_rel);
	atomic_store_explicit(&prev->next, n, memory_order_release);

	return atomic_fetch_add_explicit(&q->size, 1, memory_order_relaxed);
}
