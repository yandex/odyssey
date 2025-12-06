/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machinarium.h>
#include <machinarium/ring_buffer.h>
#include <machinarium/memory.h>
#include <machinarium/machine.h>

/*
 * yet another ring buffer impl
 */

mm_ring_buffer_t *mm_ring_buffer_create(size_t capacity)
{
	size_t full_size =
		sizeof(mm_ring_buffer_t) + capacity * sizeof(uint8_t);

	mm_ring_buffer_t *rbuf = mm_malloc(full_size);
	if (rbuf == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	memset(rbuf, 0, full_size);

	rbuf->capacity = capacity;

	return rbuf;
}

void mm_ring_buffer_free(mm_ring_buffer_t *rbuf)
{
	mm_free(rbuf);
}

size_t mm_ring_buffer_read(mm_ring_buffer_t *rbuf, void *out, size_t count)
{
	if (count == 0 || rbuf->size == 0) {
		return 0;
	}

	if (count > rbuf->size) {
		count = rbuf->size;
	}

	size_t tail_len = rbuf->capacity - rbuf->rpos;

	if (count <= tail_len) {
		/*
		 * enough to read from rpos to right, up to capacity
		 * situations like:
		 *
		 *       |          tail_len         |
		 * [ . . a a a a a a a a a . . . . . . ]
		 *       {  count  }
		 *       ^                 ^
		 *       rpos              wpos
		 *
		 *              |          tail_len         |
		 * [ q q . . .  a a a a a a a a a a a a a a a ]
		 *              {           count           }
		 *       ^      ^
		 *       wpos   rpos
		 *
		 */
		memcpy(out, rbuf->data + rbuf->rpos, count);
		rbuf->rpos = (rbuf->rpos + count) % rbuf->capacity;
	} else {
		/*
		 * need to read tail from rpos and up to end of data
		 * and some head at the begining
		 * situations like:
		 *
		 *                |          tail_len         |
		 * [ q q q q . .  a a a a a a a a a a a a a a a ]
		 *   --->  }      {           count     --->
		 *         ^      ^
		 *         wpos   rpos
		 *
		 *                |          tail_len         |
		 * [ q q q q q q  a a a a a a a a a a a a a a a ]
		 *    --->     }  {           count     --->
		 *                ^
		 *                rpos/wpos
		 */
		memcpy(out, rbuf->data + rbuf->rpos, tail_len);
		memcpy(((uint8_t *)out) + tail_len, rbuf->data,
		       count - tail_len);
		rbuf->rpos = count - tail_len;
	}

	rbuf->size -= count;

	return count;
}

size_t mm_ring_buffer_drain(mm_ring_buffer_t *rbuf, size_t count)
{
	/* same as read() but doesn't copy any bytes */

	if (count == 0 || rbuf->size == 0) {
		return 0;
	}

	if (count > rbuf->size) {
		count = rbuf->size;
	}

	size_t tail_len = rbuf->capacity - rbuf->rpos;

	if (count <= tail_len) {
		rbuf->rpos = (rbuf->rpos + count) % rbuf->capacity;
	} else {
		rbuf->rpos = count - tail_len;
	}

	rbuf->size -= count;

	return count;
}

size_t mm_ring_buffer_write(mm_ring_buffer_t *rbuf, const void *data,
			    size_t count)
{
	size_t free = mm_ring_buffer_available(rbuf);
	if (count == 0 || free == 0) {
		return 0;
	}

	if (count > free) {
		count = free;
	}

	size_t tail_len = rbuf->capacity - rbuf->wpos;

	if (count <= tail_len) {
		/*
		 * need to write at the tail only
		 *
		 *                          | tail_len  |
		 * [ . . .  a a a a a a a a . . . . . . . ]
		 *          ^               ^
		 *          rpos            wpos
		 */
		memcpy(rbuf->data + rbuf->wpos, data, count);
		rbuf->wpos = (rbuf->wpos + count) % rbuf->capacity;
	} else {
		/*
		 * need to write at the tail and then at some head
		 *
		 *   | count - tail_len  |                   | tail_len  |
		 * [ . . . . . . . . . . . . a a a a a a a a . . . . . . . ]
		 *                           ^               ^
		 *                           rpos            wpos
		 */
		memcpy(rbuf->data + rbuf->wpos, data, tail_len);
		memcpy(rbuf->data, ((uint8_t *)data) + tail_len,
		       count - tail_len);
		rbuf->wpos = count - tail_len;
	}

	rbuf->size += count;

	return count;
}

size_t mm_ring_buffer_size(const mm_ring_buffer_t *rbuf)
{
	return rbuf->size;
}

size_t mm_ring_buffer_capacity(const mm_ring_buffer_t *rbuf)
{
	return rbuf->capacity;
}

size_t mm_ring_buffer_available(const mm_ring_buffer_t *rbuf)
{
	return rbuf->capacity - rbuf->size;
}

void mm_ring_buffer_clear(mm_ring_buffer_t *rbuf)
{
	rbuf->size = 0;
	rbuf->wpos = 0;
	rbuf->rpos = 0;
}

int mm_ring_buffer_is_full(const mm_ring_buffer_t *rbuf)
{
	return rbuf->capacity == rbuf->size;
}

MACHINE_API machine_ring_buffer_t *machine_ring_buffer_create(size_t capacity)
{
	mm_ring_buffer_t *rbuf = mm_ring_buffer_create(capacity);
	if (rbuf != NULL) {
		return mm_cast(machine_ring_buffer_t *, rbuf);
	}

	return NULL;
}

MACHINE_API void machine_ring_buffer_free(machine_ring_buffer_t *rbuf)
{
	mm_ring_buffer_t *rb = mm_cast(mm_ring_buffer_t *, rbuf);
	mm_ring_buffer_free(rb);
}

MACHINE_API size_t machine_ring_buffer_read(machine_ring_buffer_t *rbuf,
					    void *out, size_t count)
{
	mm_ring_buffer_t *rb = mm_cast(mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_read(rb, out, count);
}

MACHINE_API size_t machine_ring_buffer_drain(machine_ring_buffer_t *rbuf,
					     size_t count)
{
	mm_ring_buffer_t *rb = mm_cast(mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_drain(rb, count);
}

MACHINE_API size_t machine_ring_buffer_write(machine_ring_buffer_t *rbuf,
					     const void *data, size_t count)
{
	mm_ring_buffer_t *rb = mm_cast(mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_write(rb, data, count);
}

MACHINE_API size_t machine_ring_buffer_size(const machine_ring_buffer_t *rbuf)
{
	const mm_ring_buffer_t *rb = mm_cast(const mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_size(rb);
}

MACHINE_API size_t
machine_ring_buffer_capacity(const machine_ring_buffer_t *rbuf)
{
	const mm_ring_buffer_t *rb = mm_cast(const mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_capacity(rb);
}

MACHINE_API size_t
machine_ring_buffer_available(const machine_ring_buffer_t *rbuf)
{
	const mm_ring_buffer_t *rb = mm_cast(const mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_available(rb);
}

MACHINE_API int machine_ring_buffer_is_full(const machine_ring_buffer_t *rbuf)
{
	const mm_ring_buffer_t *rb = mm_cast(const mm_ring_buffer_t *, rbuf);
	return mm_ring_buffer_is_full(rb);
}
