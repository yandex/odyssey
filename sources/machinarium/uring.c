/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/build.h>
#include <machinarium/uring.h>

#ifdef HAVE_IO_URING
#include <liburing.h>
#include <stdlib.h>
#include <poll.h>

#include <sys/eventfd.h>
#include <unistd.h>
#include <machinarium/machinarium.h>
#include <machinarium/list.h>
#include <machinarium/memory.h>

typedef struct {
	mm_poll_t poll;
	struct io_uring ring;
	mm_list_t watches; /* includes detached watches with outstanding CQEs */
	mm_list_t pending; /* adds/removes not yet copied into the SQ */
	mm_list_t ready; /* one entry per watch with accumulated events */
	int error;
} mm_uring_t;

typedef struct {
	mm_uring_op_t poll_op, cancel_op;
	mm_list_t link, pending, ready;
	int events;
	mm_uring_t *owner;
	mm_fd_t *fd;
	unsigned refs;
} mm_uring_watch_t;

static int mm_uring_probe_error(int error)
{
	errno = error == ENOSYS || error == EINVAL || error == EOPNOTSUPP ?
			ENOTSUP :
			error;
	return -1;
}

/* REGISTER_PROBE advertises POLL_ADD, not its multishot flag. Check the
 * flag on a private idle fd: cancellation must retire both requests. The
 * ring is empty on success, and is destroyed by the caller on any error. */
static int mm_uring_probe_poll(struct io_uring *ring)
{
	int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (fd < 0) {
		return errno;
	}
	int token;
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_poll_multishot(sqe, fd, POLLIN);
	io_uring_sqe_set_data(sqe, &token);
	sqe = io_uring_get_sqe(ring);
	io_uring_prep_cancel(sqe, &token, 0);
	io_uring_sqe_set_data(sqe, NULL);

	int error = 0;
	for (int n = 0; n < 2;) {
		struct __kernel_timespec ts = { .tv_sec = 1 };
		struct io_uring_cqe *cqe;
		int rc = io_uring_submit_and_wait_timeout(ring, &cqe, 1, &ts,
							  NULL);
		if (rc == -EINTR) {
			continue;
		}
		if (rc < 0) {
			error = -rc;
			break;
		}
		int res = cqe->res;
		if (io_uring_cqe_get_data(cqe) == &token) {
			if (res != -ECANCELED) {
				error = res < 0 ? -res : ENOTSUP;
			}
		} else if (res != 0 && res != -ENOENT) {
			error = -res;
		}
		io_uring_cqe_seen(ring, cqe);
		n++;
	}
	close(fd);
	return error;
}

static int mm_uring_init(struct io_uring *ring, unsigned entries)
{
	struct io_uring_params params = { 0 };
	int rc = io_uring_queue_init_params(entries, ring, &params);
	if (rc < 0) {
		return mm_uring_probe_error(-rc);
	}

	int error = ENOTSUP;
	struct io_uring_probe *probe = NULL;
	unsigned features = IORING_FEAT_EXT_ARG | IORING_FEAT_NODROP;
	if ((params.features & features) != features) {
		goto done;
	}

	/* Register explicitly: liburing 2.2's get_probe helper loses errno. */
	probe = calloc(1, sizeof(*probe) + 256 * sizeof(probe->ops[0]));
	if (probe == NULL) {
		error = ENOMEM;
		goto done;
	}
	rc = io_uring_register_probe(ring, probe, 256);
	if (rc < 0) {
		error = -rc;
		goto done;
	}

	const int operations[] = { IORING_OP_POLL_ADD, IORING_OP_ASYNC_CANCEL,
				   IORING_OP_RECV, IORING_OP_SENDMSG };
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]);
	     i++) {
		if (!io_uring_opcode_supported(probe, operations[i])) {
			goto done;
		}
	}
	error = mm_uring_probe_poll(ring);
done:
	free(probe);
	if (error) {
		io_uring_queue_exit(ring);
		return mm_uring_probe_error(error);
	}
	return 0;
}

int mm_uring_probe(void)
{
	struct io_uring ring;
	if (mm_uring_init(&ring, 8) == -1) {
		return -1;
	}
	io_uring_queue_exit(&ring);
	return 0;
}

static void mm_uring_watch_put(mm_uring_watch_t *w)
{
	if (--w->refs == 0) {
		mm_list_unlink(&w->link);
		mm_free(w);
	}
}

static int mm_uring_del(mm_poll_t *, mm_fd_t *);

static void mm_uring_dispatch(mm_uring_t *uring)
{
	while (!mm_list_empty(&uring->ready)) {
		mm_list_t *link = mm_list_pop(&uring->ready);
		mm_uring_watch_t *w =
			mm_container_of(link, mm_uring_watch_t, ready);
		int events = w->events;
		/* This or an earlier callback may detach and free the fd. */
		mm_fd_t *fd = w->fd;
		if (events < 0 && fd) {
			/* Detach before notifying: the callback can reuse the handle. */
			mm_uring_del(&uring->poll, fd);
			fd->last_event = MM_ERR;
			fd->poll_error = -events;
			if (fd->on_err) {
				fd->on_err(fd);
			} else {
				/* No error handler: don't silently lose a wakeup fd. */
				uring->error = -events;
			}
			mm_uring_watch_put(w);
			continue;
		}
		w->events = 0;
		if (fd) {
			fd->last_event = 0;
		}
		if (fd && (events & POLLERR) && fd->on_err) {
			fd->last_event |= MM_ERR;
			fd->on_err(fd);
		}
		if (w->fd && (events & (POLLHUP | POLLRDHUP)) && fd->on_close) {
			fd->last_event |= MM_CLOSE;
			fd->on_close(fd);
		}
		if (w->fd && (events & POLLIN) && fd->on_read) {
			fd->last_event |= MM_R;
			fd->on_read(fd);
		}
		if (w->fd && (events & POLLOUT) && fd->on_write) {
			fd->last_event |= MM_W;
			fd->on_write(fd);
		}
		mm_uring_watch_put(w);
	}
}

static void mm_uring_ready(mm_uring_op_t *op, int events, unsigned flags)
{
	mm_uring_watch_t *w = mm_container_of(op, mm_uring_watch_t, poll_op);
	int final = !(flags & IORING_CQE_F_MORE);
	if (w->fd && (events > 0 || (final && events < 0))) {
		if (!w->events) {
			/* Keep the watch alive through dispatch, even after del. */
			w->refs++;
			mm_list_append(&w->owner->ready, &w->ready);
		}
		/* A terminal error supersedes earlier readiness in this batch. */
		w->events = events < 0 ? events : w->events | events;
	}
	if (!final) {
		return;
	}
	if (!w->fd) {
		mm_uring_watch_put(w);
		return;
	}
	/* A final CQE retires the kernel subscription. Rearm on success;
	 * do not retry a permanently rejected POLL_ADD in a busy loop. */
	if (events >= 0) {
		mm_list_append(&w->owner->pending, &w->pending);
	}
}

static void mm_uring_cancelled(mm_uring_op_t *op, int res, unsigned flags)
{
	(void)flags;
	mm_uring_watch_t *w = mm_container_of(op, mm_uring_watch_t, cancel_op);
	/* ENOENT/EALREADY: the target has completed or is completing. */
	if (res < 0 && res != -ENOENT && res != -EALREADY) {
		w->owner->error = -res;
	}
	mm_uring_watch_put(w);
}

static void mm_uring_prepare_watches(mm_uring_t *uring)
{
	while (!mm_list_empty(&uring->pending)) {
		struct io_uring_sqe *sqe = io_uring_get_sqe(&uring->ring);
		if (sqe == NULL) {
			break;
		}
		mm_uring_watch_t *w =
			mm_container_of(mm_list_pop(&uring->pending),
					mm_uring_watch_t, pending);
		mm_list_init(&w->pending);
		if (!w->fd) {
			io_uring_prep_cancel(sqe, &w->poll_op, 0);
			io_uring_sqe_set_data(sqe, &w->cancel_op);
			continue;
		}
		int mask = w->fd->mask;
		unsigned events = ((mask & MM_R) ? POLLIN : 0) |
				  ((mask & MM_W) ? POLLOUT : 0) |
				  ((mask & MM_CLOSE) ? POLLRDHUP : 0);
		/* Multishot is edge-triggered: don't re-poll a writable socket
		 * on every loop iteration. POLLERR and POLLHUP are implicit. */
		io_uring_prep_poll_multishot(sqe, w->fd->fd, events);
		io_uring_sqe_set_data(sqe, &w->poll_op);
	}
}

static mm_poll_t *mm_uring_create(void)
{
	mm_uring_t *uring = mm_malloc(sizeof(*uring));
	if (uring == NULL) {
		return NULL;
	}
	if (mm_uring_init(&uring->ring, 64) == -1) {
		int error = errno;
		mm_free(uring);
		errno = error;
		return NULL;
	}
	mm_list_init(&uring->watches);
	mm_list_init(&uring->pending);
	mm_list_init(&uring->ready);
	uring->error = 0;
	uring->poll.iface = &mm_uring_if;
	return &uring->poll;
}

static int mm_uring_shutdown(mm_poll_t *poll)
{
	mm_uring_t *uring = (mm_uring_t *)poll;
	io_uring_queue_exit(&uring->ring);
	while (!mm_list_empty(&uring->watches)) {
		mm_list_t *link = mm_list_pop(&uring->watches);
		mm_uring_watch_t *w =
			mm_container_of(link, mm_uring_watch_t, link);
		if (w->fd) {
			w->fd->mask = 0;
			w->fd->poll_data = NULL;
		}
		mm_free(w);
	}
	return 0;
}

static void mm_uring_free(mm_poll_t *poll)
{
	mm_free(poll);
}

static int mm_uring_del(mm_poll_t *poll, mm_fd_t *fd)
{
	mm_uring_t *uring = (mm_uring_t *)poll;
	if (!fd->mask) {
		return 0;
	}
	mm_uring_watch_t *w = fd->poll_data;
	fd->mask = 0;
	fd->poll_data = NULL;
	w->fd = NULL;
	if (w->events < 0) {
		/* Already retired by the kernel; only dispatch still holds a ref. */
		mm_uring_watch_put(w);
	} else if (!mm_list_empty(&w->pending)) {
		/* Never submitted: the original fd may now be closed or reused. */
		mm_list_unlink(&w->pending);
		mm_uring_watch_put(w);
	} else {
		/* Both the cancel CQE and terminal poll CQE reference this watch.
		 * Keep its address unique until both have arrived, in either order. */
		w->refs++;
		mm_list_append(&uring->pending, &w->pending);
	}
	return 0;
}

static int mm_uring_add(mm_poll_t *poll, mm_fd_t *fd, mm_fd_callback_t on_read,
			void *on_read_arg, mm_fd_callback_t on_write,
			void *on_write_arg, mm_fd_callback_t on_err,
			void *on_err_arg, mm_fd_callback_t on_close,
			void *on_close_arg)
{
	mm_uring_t *uring = (mm_uring_t *)poll;
	int mask = (on_read ? MM_R : 0) | (on_write ? MM_W : 0) |
		   (on_err ? MM_ERR : 0) | (on_close ? MM_CLOSE : 0);
	if (!mask) {
		return mm_uring_del(poll, fd);
	}
	if (fd->mask != mask) {
		mm_uring_watch_t *w = mm_malloc(sizeof(*w));
		if (!w) {
			return -1;
		}
		mm_uring_del(poll, fd);
		w->owner = uring;
		w->fd = fd;
		w->refs = 1;
		w->events = 0;
		w->poll_op.complete = mm_uring_ready;
		w->cancel_op.complete = mm_uring_cancelled;
		mm_list_append(&uring->watches, &w->link);
		mm_list_append(&uring->pending, &w->pending);
		fd->poll_data = w;
		fd->mask = mask;
	}
	fd->on_read = on_read;
	fd->on_read_arg = on_read_arg;
	fd->on_write = on_write;
	fd->on_write_arg = on_write_arg;
	fd->on_err = on_err;
	fd->on_err_arg = on_err_arg;
	fd->on_close = on_close;
	fd->on_close_arg = on_close_arg;
	return 0;
}

static struct io_uring_sqe *mm_uring_get_sqe(mm_poll_t *poll)
{
	if (poll->iface != &mm_uring_if) {
		errno = ENOTSUP;
		return NULL;
	}
	struct io_uring_sqe *sqe =
		io_uring_get_sqe(&((mm_uring_t *)poll)->ring);
	if (sqe == NULL) {
		errno = EAGAIN;
	}
	return sqe;
}

int mm_uring_recv(mm_poll_t *poll, mm_uring_op_t *op, int fd, void *buf,
		  unsigned len)
{
	struct io_uring_sqe *sqe = mm_uring_get_sqe(poll);
	if (!sqe) {
		return -1;
	}
	io_uring_prep_recv(sqe, fd, buf, len, 0);
	io_uring_sqe_set_data(sqe, op);
	return 0;
}

int mm_uring_sendmsg(mm_poll_t *poll, mm_uring_op_t *op, int fd,
		     const struct msghdr *msg)
{
	struct io_uring_sqe *sqe = mm_uring_get_sqe(poll);
	if (!sqe) {
		return -1;
	}
	io_uring_prep_sendmsg(sqe, fd, msg, MSG_NOSIGNAL);
	io_uring_sqe_set_data(sqe, op);
	return 0;
}

static void mm_uring_drain(struct io_uring *ring)
{
	unsigned head, count = 0;
	struct io_uring_cqe *cqe;
	io_uring_for_each_cqe(ring, head, cqe)
	{
		mm_uring_op_t *op = io_uring_cqe_get_data(cqe);
		op->complete(op, cqe->res, cqe->flags);
		count++;
	}
	io_uring_cq_advance(ring, count);
}

static int mm_uring_step(mm_poll_t *poll, int timeout_ms)
{
	mm_uring_t *uring = (mm_uring_t *)poll;
	struct io_uring *ring = &uring->ring;
	mm_uring_prepare_watches(uring);
	/* If more subscriptions need SQ space, flush without waiting: the
	 * first event could belong to a watch not submitted yet. */
	if (!mm_list_empty(&uring->pending)) {
		timeout_ms = 0;
	}
	struct __kernel_timespec ts = { .tv_sec = timeout_ms / 1000,
					.tv_nsec = (timeout_ms % 1000) *
						   1000000L };
	struct io_uring_cqe *first;
	/* A nonblocking pass still lets liburing flush CQ overflow when
	 * there is nothing to submit. An empty peek returns -EAGAIN. */
	int rc = io_uring_submit_and_wait_timeout(ring, &first, timeout_ms != 0,
						  timeout_ms > 0 ? &ts : NULL,
						  NULL);
	/* NODROP may report EBUSY while the CQ needs draining. The
	 * unconsumed SQEs stay queued for the next submit. */
	if (rc < 0 && rc != -ETIME && rc != -EINTR && rc != -EAGAIN &&
	    !(rc == -EBUSY && io_uring_cq_ready(ring))) {
		errno = -rc;
		return -1;
	}

	mm_uring_drain(ring);
	mm_uring_dispatch(uring);
	if (uring->error) {
		errno = uring->error;
		return -1;
	}
	return 0;
}

mm_pollif_t mm_uring_if = {
	.name = "io_uring",
	.create = mm_uring_create,
	.free = mm_uring_free,
	.shutdown = mm_uring_shutdown,
	.step = mm_uring_step,
	.add = mm_uring_add,
	.del = mm_uring_del,
};
#else
int mm_uring_probe(void)
{
	errno = ENOTSUP;
	return -1;
}
#endif
