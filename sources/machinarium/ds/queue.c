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
	pthread_spin_lock(&q->lock);
}

static inline void queue_unlock(mm_queue_t *q)
{
	pthread_spin_unlock(&q->lock);
}

int mm_queue_init(mm_queue_t *q, size_t capacity, size_t elsize,
		  mm_queue_dtor_fn dtor)
{
	if (mm_unlikely(capacity == 0)) {
		abort();
	}

	memset(q, 0, sizeof(mm_queue_t));

	pthread_spin_init(&q->lock, PTHREAD_PROCESS_PRIVATE);

	capacity = next_pow2(capacity);

	q->cap = capacity;
	q->cap_bytes = capacity * elsize;
	q->dtor = dtor;
	q->elsize = elsize;
	q->mask = q->cap_bytes - 1;

	q->buf = mm_malloc(q->cap_bytes);
	if (q->buf == NULL) {
		return -1;
	}
	memset(q->buf, 0, q->cap_bytes);

	return 0;
}

void mm_queue_destroy(mm_queue_t *q)
{
	if (q->dtor != NULL) {
		while (q->size > 0) {
			void *val = &q->buf[q->head];
			q->dtor(val);

			q->head += q->elsize;
			q->head &= q->mask;
			--q->size;
		}
	}

	pthread_spin_destroy(&q->lock);
	mm_free(q->buf);

	q->buf = NULL;
}

int mm_queue_push(mm_queue_t *q, const void *val)
{
	int rc = 0;

	queue_lock(q);

	if (q->size < q->cap) {
		void *dst = &q->buf[q->tail];
		memcpy(dst, val, q->elsize);
		q->tail += q->elsize;
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
		void *dst = &q->buf[q->tail];
		memcpy(dst, val, q->elsize);
		q->tail += q->elsize;
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
		const void *src = &q->buf[q->head];
		memcpy(val, src, q->elsize);
		q->head += q->elsize;
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

	total = q->size;
	if (total > max) {
		total = max;
	}

	size_t total_bytes = total * q->elsize;
	size_t to_end = q->cap_bytes - q->head;

	const void *src = &q->buf[q->head];

	if (total_bytes <= to_end) {
		/* [ . . . head . . . . last . . . ] */
		memcpy(dst, src, total_bytes);
	} else {
		/* [ . . last . . . . head . . . ] */
		memcpy(dst, src, to_end);
		memcpy((uint8_t *)dst + to_end, q->buf, total_bytes - to_end);
	}

	q->head += total_bytes;
	q->head &= q->mask;
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
