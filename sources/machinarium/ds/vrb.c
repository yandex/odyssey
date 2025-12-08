#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <machinarium/ds/vrb.h>
#include <machinarium/macro.h>
#include <machinarium/memory.h>
#include <machinarium/machine.h>

static size_t align_by_pagesize(size_t capacity)
{
	size_t page_size = sysconf(_SC_PAGESIZE);

	if (capacity == 0) {
		return page_size;
	}

	return (capacity + page_size - 1) & ~(page_size - 1);
}

mm_virtual_rbuf_t *mm_virtual_rbuf_create(size_t capacity)
{
	mm_virtual_rbuf_t *vrb = mm_malloc(sizeof(mm_virtual_rbuf_t));
	if (vrb == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	memset(vrb, 0, sizeof(mm_virtual_rbuf_t));

	vrb->capacity = align_by_pagesize(capacity);

	int fd = memfd_create("virtual-ring-buffer", MFD_CLOEXEC);
	if (fd < 0) {
		mm_errno_set(errno);
		mm_free(vrb);
		return NULL;
	}

	if (ftruncate(fd, vrb->capacity) < 0) {
		mm_errno_set(errno);
		close(fd);
		mm_free(vrb);
		return NULL;
	}

	vrb->data = mmap(NULL, 2 * vrb->capacity, PROT_NONE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (vrb->data == MAP_FAILED) {
		mm_free(vrb);
		close(fd);
		return NULL;
	}

	void *original = mmap(vrb->data, vrb->capacity, PROT_READ | PROT_WRITE,
			      MAP_SHARED | MAP_FIXED, fd, 0);
	if (original == MAP_FAILED) {
		munmap(vrb->data, 2 * vrb->capacity);
		close(fd);
		mm_free(vrb);
		return NULL;
	}

	void *mirror =
		mmap(vrb->data + vrb->capacity, vrb->capacity,
		     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	if (mirror == MAP_FAILED) {
		munmap(vrb->data, 2 * vrb->capacity);
		close(fd);
		mm_free(vrb);
		return NULL;
	}

	memset(original, 0, vrb->capacity);

	close(fd);

	return vrb;
}

void mm_virtual_rbuf_free(mm_virtual_rbuf_t *vrb)
{
	if (vrb->data != NULL) {
		munmap(vrb->data, 2 * vrb->capacity);
	}

	mm_free(vrb);
}

size_t mm_virtual_rbuf_capacity(const mm_virtual_rbuf_t *vrb)
{
	return vrb->capacity;
}

size_t mm_virtual_rbuf_size(const mm_virtual_rbuf_t *vrb)
{
	return vrb->wpos - vrb->rpos;
}

size_t mm_virtual_rbuf_free_size(const mm_virtual_rbuf_t *vrb)
{
	return vrb->capacity - mm_virtual_rbuf_size(vrb);
}

struct iovec mm_virtual_rbuf_write_begin(const mm_virtual_rbuf_t *vrb)
{
	size_t max = mm_virtual_rbuf_free_size(vrb);
	size_t offset = vrb->wpos % vrb->capacity;
	void *ptr = vrb->data + offset;

	return (struct iovec){
		.iov_base = ptr,
		.iov_len = max,
	};
}

void mm_virtual_rbuf_write_commit(mm_virtual_rbuf_t *vrb, size_t len)
{
	size_t avail = mm_virtual_rbuf_free_size(vrb);
	if (mm_unlikely(len > avail)) {
		/* len must be taken from corresponding write_begin call */
		abort();
	}

	vrb->wpos += len;
}

struct iovec mm_virtual_rbuf_read_begin(const mm_virtual_rbuf_t *vrb)
{
	size_t max = mm_virtual_rbuf_size(vrb);
	size_t offset = vrb->rpos % vrb->capacity;
	void *ptr = vrb->data + offset;

	return (struct iovec){
		.iov_base = ptr,
		.iov_len = max,
	};
}

void mm_virtual_rbuf_read_commit(mm_virtual_rbuf_t *vrb, size_t len)
{
	size_t avail = mm_virtual_rbuf_size(vrb);
	if (mm_unlikely(len > avail)) {
		/* len must be taken from corresponding read_begin call */
		abort();
	}

	vrb->rpos += len;

	if (vrb->rpos == vrb->wpos) {
		vrb->rpos = 0;
		vrb->wpos = 0;
	}
}

size_t mm_virtual_rbuf_read(mm_virtual_rbuf_t *vrb, void *out, size_t count)
{
	struct iovec vec = mm_virtual_rbuf_read_begin(vrb);
	if (count > vec.iov_len) {
		count = vec.iov_len;
	}

	memcpy(out, vec.iov_base, count);

	mm_virtual_rbuf_read_commit(vrb, count);

	return count;
}

size_t mm_virtual_rbuf_drain(mm_virtual_rbuf_t *vrb, size_t count)
{
	struct iovec vec = mm_virtual_rbuf_read_begin(vrb);
	if (count > vec.iov_len) {
		count = vec.iov_len;
	}

	/* reading to nowhere */

	mm_virtual_rbuf_read_commit(vrb, count);

	return count;
}

size_t mm_virtual_rbuf_write(mm_virtual_rbuf_t *vrb, const void *data,
			     size_t count)
{
	struct iovec vec = mm_virtual_rbuf_write_begin(vrb);
	if (count > vec.iov_len) {
		count = vec.iov_len;
	}

	memcpy(vec.iov_base, data, count);

	mm_virtual_rbuf_write_commit(vrb, count);

	return count;
}
