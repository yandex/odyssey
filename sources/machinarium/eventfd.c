/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <sys/eventfd.h>

#include <machinarium/machinarium.h>

#include <machinarium/eventfd.h>

static void on_read_cb(mm_fd_t *handle)
{
	mm_eventfd_t *efd = handle->on_read_arg;
	mm_cond_signal(&efd->cond);
}

int mm_eventfd_init(mm_eventfd_t *efd)
{
	memset(efd, 0, sizeof(mm_eventfd_t));

	efd->handle.fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (efd->handle.fd == -1) {
		mm_errno_set(errno);
		return -1;
	}

	mm_cond_init(&efd->cond);

	return 0;
}

void mm_eventfd_destroy(mm_eventfd_t *efd)
{
	if (efd->attached) {
		mm_eventfd_detach(efd);
	}

	close(efd->handle.fd);
}

int mm_eventfd_attach(mm_eventfd_t *efd)
{
	mm_errno_set(0);

	if (efd->attached) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}

	int rc;
	rc = mm_loop_add(&mm_self->loop, &efd->handle, on_read_cb, efd, NULL,
			 NULL, NULL, NULL, NULL, NULL);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	efd->attached = 1;

	return 0;
}

int mm_eventfd_detach(mm_eventfd_t *efd)
{
	mm_errno_set(0);
	if (!efd->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	int rc;
	rc = mm_loop_delete(&mm_self->loop, &efd->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	efd->attached = 0;
	return 0;
}

int mm_eventfd_read(mm_eventfd_t *efd, uint32_t timeout_ms)
{
	uint64_t unused = 0;
	while (1) {
		int rc = read(efd->handle.fd, &unused, sizeof(uint64_t));
		if (rc > 0) {
			break;
		}

		int err = errno;
		if (machine_errno_retryable(err)) {
			rc = mm_cond_wait(&efd->cond, timeout_ms);
			if (rc == MM_COND_WAIT_FAIL) {
				mm_errno_set(ETIMEDOUT);
				return -1;
			}

			continue;
		}

		mm_errno_set(err);
		return -1;
	}

	return 0;
}

int mm_eventfd_write(mm_eventfd_t *efd, uint32_t timeout_ms)
{
	(void)timeout_ms;

	uint64_t value = 1;
	int rc = write(efd->handle.fd, &value, sizeof(uint64_t));
	if (rc > 0) {
		return 0;
	}

	int err = errno;
	mm_errno_set(err);
	return -1;
}

void mm_eventfd_peer_to(mm_eventfd_t *efd, mm_io_t *io)
{
	if (efd->cond.propagate != NULL) {
		abort();
	}

	mm_cond_propagate(&efd->cond, &io->cond);
}

void mm_eventfd_remove_peer_to(mm_eventfd_t *efd, mm_io_t *io)
{
	if (efd->cond.propagate != &io->cond) {
		abort();
	}

	mm_cond_propagate(&efd->cond, NULL);
}
