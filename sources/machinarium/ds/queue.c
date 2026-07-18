#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

#include <machinarium/macro.h>
#include <machinarium/ds/queue.h>
#include <machinarium/memory.h>

static uint64_t next_pow2(uint64_t n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;

	return n;
}

static inline void queue_lock(mm_queue_t *q)
{
	mm_spinlock_lock(&q->lock);
}

static inline void queue_unlock(mm_queue_t *q)
{
	mm_spinlock_unlock(&q->lock);
}

int mm_queue_init(mm_queue_t *q, size_t capacity, size_t elsize,
		  mm_queue_dtor_fn dtor)
{
	if (mm_unlikely(capacity == 0)) {
		abort();
	}

	memset(q, 0, sizeof(mm_queue_t));

	mm_spinlock_init(&q->lock);

	capacity = next_pow2(capacity);

	q->cap = capacity;
	q->dtor = dtor;
	q->elsize = elsize;
	q->mask = capacity - 1;

	q->buf = mm_malloc(capacity * elsize);
	if (q->buf == NULL) {
		return -1;
	}
	memset(q->buf, 0, capacity * elsize);

	return 0;
}

void mm_queue_destroy(mm_queue_t *q)
{
	if (q->dtor != NULL) {
		while (q->size > 0) {
			void *val = q->buf + (q->head * q->elsize);
			q->dtor(val);

			++q->head;
			q->head &= q->mask;
			--q->size;
		}
	}

	mm_spinlock_destroy(&q->lock);
	mm_free(q->buf);

	q->buf = NULL;
}

int mm_queue_push(mm_queue_t *q, const void *val)
{
	int rc = 0;

	queue_lock(q);

	if (q->size < q->cap) {
		void *dst = q->buf + (q->tail * q->elsize);
		memcpy(dst, val, q->elsize);
		++q->tail;
		q->tail &= q->mask;
		++q->size;

		rc = 1;
	}

	queue_unlock(q);

	return rc;
}

int mm_queue_push_extended(mm_queue_t *q, const void *val)
{
	int rc = -1;

	queue_lock(q);

	if (q->size < q->cap) {
		void *dst = q->buf + (q->tail * q->elsize);
		memcpy(dst, val, q->elsize);
		++q->tail;
		q->tail &= q->mask;

		rc = (int)(++q->size);
	}

	queue_unlock(q);

	return rc;
}

int mm_queue_pop(mm_queue_t *q, void *val)
{
	int rc = 0;

	queue_lock(q);

	if (q->size > 0) {
		const void *src = q->buf + (q->head * q->elsize);
		memcpy(val, src, q->elsize);
		++q->head;
		q->head &= q->mask;
		--q->size;

		rc = 1;
	}

	queue_unlock(q);

	return rc;
}

size_t mm_queue_pop_batch(mm_queue_t *q, void *dst, size_t max)
{
	size_t total = 0;

	queue_lock(q);

	if (q->size == 0 || max == 0) {
		queue_unlock(q);
		return 0;
	}

	total = q->size;
	if (total > max) {
		total = max;
	}

	size_t to_end = q->cap - q->head;

	uint8_t *ptr_dst = (uint8_t *)dst;
	uint8_t *ptr_src = q->buf + (q->head * q->elsize);

	if (total <= to_end) {
		/* [ . . . head . . . . last . . . ] */
		memcpy(ptr_dst, ptr_src, total * q->elsize);
	} else {
		/* [ . . last . . . . head . . . ] */
		size_t first_part_bytes = to_end * q->elsize;
		size_t elements_left = total - to_end;
		memcpy(ptr_dst, ptr_src, first_part_bytes);
		memcpy(ptr_dst + first_part_bytes, q->buf,
		       elements_left * q->elsize);
	}

	q->head = (q->head + total) & q->mask;
	q->size -= total;

	queue_unlock(q);

	return total;
}

size_t mm_queue_size(mm_queue_t *q)
{
	queue_lock(q);
	size_t r = q->size;
	queue_unlock(q);

	return r;
}
