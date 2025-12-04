#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>

#include <sys/uio.h>

/*
 * yet another virtual ring buffer - ring buffer that can be used
 * for every cases, that works with straight bytes array (like read syscall)
 */

typedef struct {
	uint8_t *data;
	size_t capacity;
	size_t rpos;
	size_t wpos;
} mm_virtual_rbuf_t;

mm_virtual_rbuf_t *mm_virtual_rbuf_create(size_t capacity);
void mm_virtual_rbuf_free(mm_virtual_rbuf_t *vrb);

size_t mm_virtual_rbuf_capacity(const mm_virtual_rbuf_t *vrb);
size_t mm_virtual_rbuf_size(const mm_virtual_rbuf_t *vrb);
size_t mm_virtual_rbuf_free_size(const mm_virtual_rbuf_t *vrb);

/* zero copy interfaces for syscalls */
struct iovec mm_virtual_rbuf_write_begin(const mm_virtual_rbuf_t *vrb);
void mm_virtual_rbuf_write_commit(mm_virtual_rbuf_t *vrb, size_t len);

struct iovec mm_virtual_rbuf_read_begin(const mm_virtual_rbuf_t *vrb);
void mm_virtual_rbuf_read_commit(mm_virtual_rbuf_t *vrb, size_t len);

/* classic rbuf interface */
size_t mm_virtual_rbuf_read(mm_virtual_rbuf_t *vrb, void *out, size_t count);
size_t mm_virtual_rbuf_drain(mm_virtual_rbuf_t *vrb, size_t count);
size_t mm_virtual_rbuf_write(mm_virtual_rbuf_t *vrb, const void *data,
			     size_t count);
