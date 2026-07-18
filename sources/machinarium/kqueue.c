/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#include <machinarium/machinarium.h>
#include <machinarium/kqueue.h>
#include <machinarium/memory.h>

typedef struct mm_kqueue_t mm_kqueue_t;

struct mm_kqueue_t {
	mm_poll_t poll;
	int fd;
	struct kevent *list;
	int size;
	int count;
};

static mm_poll_t *mm_kqueue_create(void)
{
	mm_kqueue_t *kq;
	kq = mm_malloc(sizeof(mm_kqueue_t));
	if (kq == NULL) {
		return NULL;
	}
	kq->poll.iface = &mm_kqueue_if;
	kq->count = 0;
	kq->size = 1024;
	int size = sizeof(struct kevent) * kq->size;
	kq->list = mm_malloc(size);
	if (kq->list == NULL) {
		mm_free(kq);
		return NULL;
	}
	memset(kq->list, 0, size);
	kq->fd = kqueue();
	if (kq->fd == -1) {
		mm_free(kq->list);
		mm_free(kq);
		return NULL;
	}
	fcntl(kq->fd, F_SETFD, FD_CLOEXEC);
	return &kq->poll;
}

static void mm_kqueue_free(mm_poll_t *poll)
{
	mm_kqueue_t *kq = (mm_kqueue_t *)poll;
	if (kq->list) {
		mm_free(kq->list);
	}
	mm_free(poll);
}

static int mm_kqueue_shutdown(mm_poll_t *poll)
{
	mm_kqueue_t *kq = (mm_kqueue_t *)poll;
	if (kq->fd != -1) {
		close(kq->fd);
		kq->fd = -1;
	}
	return 0;
}

static int mm_kqueue_step(mm_poll_t *poll, int timeout)
{
	mm_kqueue_t *kq = (mm_kqueue_t *)poll;
	if (kq->count == 0) {
		return 0;
	}

	struct timespec ts;
	struct timespec *tsp = NULL;
	if (timeout >= 0) {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000L;
		tsp = &ts;
	}

	int count;
	count = kevent(kq->fd, NULL, 0, kq->list, kq->size, tsp);
	if (count <= 0) {
		return 0;
	}

	int i = 0;
	while (i < count) {
		struct kevent *ev = &kq->list[i];
		mm_fd_t *fd = ev->udata;

		fd->last_event = 0;

		int is_eof = (ev->flags & EV_EOF) != 0;
		/* real error condition: EOF + non-zero fflags (errno) */
		int is_err = is_eof && ev->fflags != 0;

		if (is_err && (fd->mask & MM_ERR)) {
			fd->last_event |= MM_ERR;
			assert(fd->on_err);
			fd->on_err(fd);
		}

		if (is_eof && (fd->mask & MM_CLOSE)) {
			fd->last_event |= MM_CLOSE;
			assert(fd->on_close);
			fd->on_close(fd);
		}

		if (ev->filter == EVFILT_READ && (fd->mask & MM_R)) {
			fd->last_event |= MM_R;
			assert(fd->on_read);
			fd->on_read(fd);
		}

		if (ev->filter == EVFILT_WRITE && (fd->mask & MM_W)) {
			fd->last_event |= MM_W;
			assert(fd->on_write);
			fd->on_write(fd);
		}

		i++;
	}
	return count;
}

static int mm_kqueue_resize(mm_kqueue_t *kq)
{
	if (kq->count + 1 <= kq->size) {
		return 0;
	}
	int size = kq->size * 2;
	void *ptr = mm_realloc(kq->list, sizeof(struct kevent) * size);
	if (ptr == NULL) {
		return -1;
	}
	kq->list = ptr;
	kq->size = size;
	return 0;
}

static inline int mm_kqueue_change(mm_kqueue_t *kq, int fd, int16_t filter,
				   u_short flags, void *udata)
{
	struct kevent ev;
	EV_SET(&ev, fd, filter, flags, 0, 0, udata);
	return kevent(kq->fd, &ev, 1, NULL, 0, NULL);
}

static int mm_kqueue_modify(mm_poll_t *poll, mm_fd_t *fd, int mask)
{
	if (fd->mask == mask) {
		return 0;
	}
	mm_kqueue_t *kq = (mm_kqueue_t *)poll;
	int rc;

	int old_r = fd->mask & MM_R;
	int new_r = mask & MM_R;
	int old_w = fd->mask & MM_W;
	int new_w = mask & MM_W;

	if (new_r && !old_r) {
		if (mm_kqueue_resize(kq) == -1) {
			return -1;
		}
		rc = mm_kqueue_change(kq, fd->fd, EVFILT_READ,
				      EV_ADD | EV_ENABLE | EV_CLEAR, fd);
		if (rc == -1) {
			return -1;
		}
		kq->count++;
	} else if (!new_r && old_r) {
		rc = mm_kqueue_change(kq, fd->fd, EVFILT_READ, EV_DELETE, fd);
		if (rc == -1) {
			return -1;
		}
		kq->count--;
	}

	if (new_w && !old_w) {
		if (mm_kqueue_resize(kq) == -1) {
			return -1;
		}
		rc = mm_kqueue_change(kq, fd->fd, EVFILT_WRITE,
				      EV_ADD | EV_ENABLE | EV_CLEAR, fd);
		if (rc == -1) {
			return -1;
		}
		kq->count++;
	} else if (!new_w && old_w) {
		rc = mm_kqueue_change(kq, fd->fd, EVFILT_WRITE, EV_DELETE, fd);
		if (rc == -1) {
			return -1;
		}
		kq->count--;
	}

	fd->mask = mask;
	return 0;
}

static int mm_kqueue_add(mm_poll_t *poll, mm_fd_t *fd, mm_fd_callback_t on_read,
			 void *on_read_arg, mm_fd_callback_t on_write,
			 void *on_write_arg, mm_fd_callback_t on_err,
			 void *on_err_arg, mm_fd_callback_t on_close,
			 void *on_close_arg)
{
	fd->on_read = on_read;
	fd->on_read_arg = on_read_arg;
	fd->on_write = on_write;
	fd->on_write_arg = on_write_arg;
	fd->on_err = on_err;
	fd->on_err_arg = on_err_arg;
	fd->on_close = on_close;
	fd->on_close_arg = on_close_arg;

	int mask = 0;
	if (fd->on_read != NULL) {
		mask |= MM_R;
	}
	if (fd->on_write != NULL) {
		mask |= MM_W;
	}
	if (fd->on_err != NULL) {
		mask |= MM_ERR;
	}
	if (fd->on_close != NULL) {
		mask |= MM_CLOSE;
	}

	return mm_kqueue_modify(poll, fd, mask);
}

static int mm_kqueue_del(mm_poll_t *poll, mm_fd_t *fd)
{
	fd->on_read = NULL;
	fd->on_read_arg = NULL;
	fd->on_write = NULL;
	fd->on_write_arg = NULL;

	return mm_kqueue_modify(poll, fd, 0);
}

mm_pollif_t mm_kqueue_if = { .name = "kqueue",
			     .create = mm_kqueue_create,
			     .free = mm_kqueue_free,
			     .shutdown = mm_kqueue_shutdown,
			     .step = mm_kqueue_step,
			     .add = mm_kqueue_add,
			     .del = mm_kqueue_del };
