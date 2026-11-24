#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/* Check kernel capabilities on a temporary ring and private poll target.
 * Return 0 on support, otherwise -1 with errno. ENOTSUP means missing build or
 * kernel support; permission and resource errors preserve their errno.
 */
int mm_uring_probe(void);

#include <machinarium/build.h>
#ifdef HAVE_IO_URING
#include <sys/socket.h>
#include <machinarium/poll.h>

extern mm_pollif_t mm_uring_if;
typedef struct mm_uring_op mm_uring_op_t;
struct mm_uring_op {
	/* Called on the loop thread with byte count, EOF, or negative errno. */
	void (*complete)(mm_uring_op_t *, int result, unsigned flags);
};

/* Queue only on an io_uring poller; on success the op, fd and storage
 * must stay valid through completion. On SQ exhaustion return -1/EAGAIN. */
int mm_uring_recv(mm_poll_t *, mm_uring_op_t *, int fd, void *, unsigned len);
int mm_uring_sendmsg(mm_poll_t *, mm_uring_op_t *, int fd,
		     const struct msghdr *);
#endif
