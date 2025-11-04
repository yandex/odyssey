#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct mm_ring_buffer {
	size_t capacity;
	size_t rpos;
	size_t wpos;
	size_t size;
	uint8_t data[];
} mm_ring_buffer_t;

mm_ring_buffer_t *mm_ring_buffer_create(size_t capacity);
void mm_ring_buffer_free(mm_ring_buffer_t *rbuf);

size_t mm_ring_buffer_read(mm_ring_buffer_t *rbuf, void *out, size_t count);
size_t mm_ring_buffer_drain(mm_ring_buffer_t *rbuf, size_t count);
size_t mm_ring_buffer_write(mm_ring_buffer_t *rbuf, const void *data,
			    size_t count);

size_t mm_ring_buffer_size(const mm_ring_buffer_t *rbuf);
size_t mm_ring_buffer_capacity(const mm_ring_buffer_t *rbuf);
size_t mm_ring_buffer_available(const mm_ring_buffer_t *rbuf);
int mm_ring_buffer_is_full(const mm_ring_buffer_t *rbuf);

void mm_ring_buffer_clear(mm_ring_buffer_t *rbuf);
