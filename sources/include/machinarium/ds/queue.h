#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/*
 * yet another concurrent queue implementation
 */

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

typedef void (*mm_queue_dtor_fn)(void *val);

typedef struct {
	pthread_spinlock_t lock;

	size_t cap;
	size_t cap_bytes;
	size_t mask;
	size_t elsize;

	size_t size;
	size_t head; /* in bytes */
	size_t tail; /* in bytes */

	mm_queue_dtor_fn dtor;

	uint8_t *buf;
} mm_queue_t;

int mm_queue_init(mm_queue_t *queue, size_t capacity, size_t elsize,
		  mm_queue_dtor_fn dtor);
void mm_queue_destroy(mm_queue_t *queue);

int mm_queue_push(mm_queue_t *queue, const void *val);
int mm_queue_pop(mm_queue_t *queue, void *val);

size_t mm_queue_pop_batch(mm_queue_t *queue, void *dst, size_t max);

size_t mm_queue_size(mm_queue_t *queue);

#define mm_queue_init_types(queue, cap, elsize, type, dtor) \
	mm_queue_init((queue), (cap), (elsize), _Alignof(type), (dtor))
