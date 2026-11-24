#include <sys/eventfd.h>
#include <machinarium/machinarium.h>
#include <machinarium/cond.h>
#include <machinarium/io.h>
#include <machinarium/uring.h>
#include <tests/odyssey_test.h>

struct pipe_end {
	int fd[2];
	int dst;
	int queued;
	char payload;
};

struct waiter {
	mm_uring_op_t op;
	mm_cond_t cond;
	int result;
};

static void complete(mm_uring_op_t *op, int result, unsigned flags)
{
	(void)flags;
	struct waiter *w = mm_container_of(op, struct waiter, op);
	w->result = result;
	mm_cond_signal(&w->cond);
}

static void forward_byte(void *arg)
{
	struct pipe_end *p = arg;
	struct waiter w = { .op.complete = complete };
	mm_cond_init(&w.cond);
	char byte;
	test(mm_uring_recv(mm_self->loop.poll, &w.op, p->fd[0], &byte, 1) == 0);
	p->queued = 1;
	test(mm_cond_wait(&w.cond, 2000) == 0);
	test(w.result == 1 && byte == p->payload);

	struct iovec iov = { .iov_base = &byte, .iov_len = 1 };
	struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
	test(mm_uring_sendmsg(mm_self->loop.poll, &w.op, p->dst, &msg) == 0);
	test(mm_cond_wait(&w.cond, 2000) == 0);
	test(w.result == 1);
}

/* Scheduled after both readers in the first ready pass: yield must not
 * submit either RECV, even though both sockets already contain data. */
static void check_batch(void *arg)
{
	struct pipe_end *ends = arg;
	for (int i = 0; i < 2; i++) {
		char byte;
		test(ends[i].queued);
		test(recv(ends[i].fd[0], &byte, 1, MSG_PEEK | MSG_DONTWAIT) ==
		     1);
		test(byte == ends[i].payload);
	}
}

static void start(void *arg)
{
	struct pipe_end *ends = arg;
	test(mm_self->loop.poll->iface == &mm_uring_if);
	for (int i = 0; i < 2; i++) {
		test(machine_coroutine_create(forward_byte, &ends[i]) >= 0);
	}
	test(machine_coroutine_create(check_batch, ends) >= 0);
}

static void read_ready(mm_fd_t *fd)
{
	mm_cond_signal(fd->on_read_arg);
}

static void check_poll(void *arg)
{
	(void)arg;
	mm_loop_t *loop = &mm_self->loop;
	int pair[2];
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	mm_cond_t cond;
	mm_cond_init(&cond);
	mm_fd_t fd = { .fd = pair[0] };
	test(mm_loop_add_ro(loop, &fd, read_ready, &cond) == 0);
	for (int i = 0; i < 2; i++) {
		char byte;
		test(send(pair[1], "x", 1, 0) == 1);
		test(mm_cond_wait(&cond, 2000) == 0);
		test(recv(pair[0], &byte, 1, MSG_DONTWAIT) == 1);
	}
	/* No socket events: the next loop deadline must still fire. */
	test(mm_cond_wait(&cond, 10) == -1 && machine_errno() == ETIMEDOUT);
	test(mm_loop_delete(loop, &fd) == 0);
	/* Registration fails asynchronously. Both the current waiter and a
	 * later wait on the retired socket must get the registration errno. */
	mm_io_t *io = mm_io_create();
	test(io != NULL);
	io->handle.fd = pair[0];
	test(mm_io_attach(io) == 0);
	close(pair[0]);
	test(mm_io_wait(io, 2000) == -1 && machine_errno() == EBADF);
	test(mm_io_wait(io, 0) == -1 && machine_errno() == EBADF);
	test(mm_io_detach(io) == 0);
	mm_io_free(io);
	close(pair[1]);
}

static void count_read(mm_fd_t *fd)
{
	(*(int *)fd->on_read_arg)++;
}

/* Produce multiple readable edges before one drain, then a fresh edge on
 * the next step. Readiness is coalesced per batch, not across steps. */
static void check_coalesced_reads(void)
{
	mm_loop_t loop;
	test(mm_loop_init(&loop, &mm_uring_if) == 0);
	mm_poll_t *p = loop.poll;
	mm_fd_t fd = { .fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC) };
	int calls = 0;
	test(fd.fd >= 0);
	test(mm_loop_add_ro(&loop, &fd, count_read, &calls) == 0);
	test(p->iface->step(p, 0) == 0);
	for (int batch = 1; batch <= 2; batch++) {
		eventfd_t value;
		for (int i = 0; i < 8; i++) {
			test(eventfd_write(fd.fd, 1) == 0);
			test(eventfd_read(fd.fd, &value) == 0);
		}
		test(eventfd_write(fd.fd, 1) == 0);
		test(p->iface->step(p, 100) == 0 && calls == batch);
		test(eventfd_read(fd.fd, &value) == 0);
	}
	test(mm_loop_delete(&loop, &fd) == 0);
	test(mm_loop_shutdown(&loop) == 0);
	close(fd.fd);
}

struct ready_pair {
	mm_loop_t loop;
	mm_fd_t fd[2];
	int calls;
};

static void detach_peer(mm_fd_t *fd)
{
	struct ready_pair *pair = fd->on_read_arg;
	pair->calls++;
	mm_fd_t *peer = fd == &pair->fd[0] ? &pair->fd[1] : &pair->fd[0];
	test(mm_loop_delete(&pair->loop, peer) == 0);
}

/* Whichever ready callback runs first detaches the other watch. */
static void check_detach_ready_peer(void)
{
	struct ready_pair pair = { 0 };
	test(mm_loop_init(&pair.loop, &mm_uring_if) == 0);
	for (int i = 0; i < 2; i++) {
		pair.fd[i].fd = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
		test(pair.fd[i].fd >= 0);
		test(mm_loop_add_ro(&pair.loop, &pair.fd[i], detach_peer,
				    &pair) == 0);
	}
	mm_poll_t *p = pair.loop.poll;
	test(p->iface->step(p, 100) == 0);
	test(p->iface->step(p, 10) == 0);
	test(pair.calls == 1);
	for (int i = 0; i < 2; i++) {
		test(mm_loop_delete(&pair.loop, &pair.fd[i]) == 0);
		close(pair.fd[i].fd);
	}
	test(mm_loop_shutdown(&pair.loop) == 0);
}

static void count_write(mm_fd_t *fd)
{
	(*(int *)fd->on_write_arg)++;
}

/* Leave a live handle attached at shutdown, then register it in a fresh
 * loop with the same interest mask and again with a different mask. */
static void check_watch_reuse_after_shutdown(void)
{
	mm_fd_t fd = { .fd = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC) };
	test(fd.fd >= 0);
	for (int i = 0; i < 3; i++) {
		mm_loop_t loop;
		int calls = 0;
		test(mm_loop_init(&loop, &mm_uring_if) == 0);
		if (i < 2) {
			test(mm_loop_add_ro(&loop, &fd, count_read, &calls) ==
			     0);
		} else {
			test(mm_loop_add(&loop, &fd, NULL, NULL, count_write,
					 &calls, NULL, NULL, NULL, NULL) == 0);
		}
		test(loop.poll->iface->step(loop.poll, 100) == 0);
		test(calls == 1);
		test(mm_loop_shutdown(&loop) == 0);
		test(fd.mask == 0 && fd.poll_data == NULL);
	}
	close(fd.fd);
}

static void detach_on_read(mm_fd_t *fd)
{
	mm_loop_t *loop = fd->on_read_arg;
	test(mm_loop_delete(loop, fd) == 0);
}

/* Drive a separate poller so we control exactly when queued events dispatch. */
static void check_watch_reuse(void)
{
	mm_loop_t loop;
	test(mm_loop_init(&loop, &mm_uring_if) == 0);
	mm_poll_t *p = loop.poll;
	int a[2], b[2], old = 0, fresh = 0, writable = 0;
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, a) == 0);
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, b) == 0);
	mm_fd_t fd = { .fd = a[0] };
	test(mm_loop_add(&loop, &fd, count_read, &old, count_write, &writable,
			 NULL, NULL, NULL, NULL) == 0);
	test(p->iface->step(p, 100) == 0 && writable == 1);
	test(p->iface->step(p, 10) == 0 && writable == 1);

	/* The old read event can already be in the CQ. Reuse both the number
	 * and the handle before submitting its cancel or consuming the CQE. */
	test(send(a[1], "x", 1, 0) == 1);
	test(mm_loop_delete(&loop, &fd) == 0);
	test(dup2(b[0], fd.fd) == fd.fd);
	test(mm_loop_add_ro(&loop, &fd, count_read, &fresh) == 0);
	for (int i = 0; i < 3; i++) {
		test(p->iface->step(p, 10) == 0);
	}
	test(old == 0 && fresh == 0);
	test(send(b[1], "y", 1, 0) == 1);
	test(p->iface->step(p, 100) == 0 && fresh == 1);
	/* Change interests on the live handle. The replacement gets both
	 * IN and OUT; detaching in its read callback suppresses write. */
	test(mm_loop_add(&loop, &fd, detach_on_read, &loop, count_write,
			 &writable, NULL, NULL, NULL, NULL) == 0);
	for (int i = 0; i < 3 && fd.mask; i++) {
		test(p->iface->step(p, 100) == 0);
	}
	test(!fd.mask && writable == 1);
	test(mm_loop_shutdown(&loop) == 0);
	close(a[0]);
	close(a[1]);
	close(b[0]);
	close(b[1]);
}

/* Only the final watch is ready; a full first SQ must not block waiting
 * for one of its idle registrations. Also cancel more than one SQ of fds. */
static void check_watch_batch(void)
{
	mm_loop_t loop;
	test(mm_loop_init(&loop, &mm_uring_if) == 0);
	mm_fd_t fds[80] = { 0 };
	int calls = 0;
	for (int i = 0; i < 80; i++) {
		fds[i].fd = eventfd(i == 79, EFD_NONBLOCK | EFD_CLOEXEC);
		test(fds[i].fd >= 0);
		test(mm_loop_add_ro(&loop, &fds[i], count_read, &calls) == 0);
	}
	/* Drop a registration before its SQE is prepared. */
	test(mm_loop_delete(&loop, &fds[0]) == 0);
	for (int i = 0; i < 3 && calls == 0; i++) {
		test(loop.poll->iface->step(loop.poll, 100) == 0);
	}
	test(calls == 1);
	for (int i = 0; i < 80; i++) {
		test(mm_loop_delete(&loop, &fds[i]) == 0);
		close(fds[i].fd);
	}
	for (int i = 0; i < 5; i++) {
		test(loop.poll->iface->step(loop.poll, 10) == 0);
	}
	test(calls == 1);
	test(mm_loop_shutdown(&loop) == 0);
}

static void poll_failed(mm_fd_t *fd)
{
	test(fd->poll_error == EBADF && fd->last_event == MM_ERR);
	test(fd->mask == 0 && fd->poll_data == NULL);
	(*(int *)fd->on_err_arg)++;
}

static void check_poll_failure(void)
{
	mm_loop_t loop;
	test(mm_loop_init(&loop, &mm_uring_if) == 0);
	mm_poll_t *p = loop.poll;
	mm_fd_t good = { .fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC) };
	test(good.fd >= 0);
	int ready = 0, errors = 0;
	test(mm_loop_add_ro(&loop, &good, count_read, &ready) == 0);
	mm_fd_t bad = { .fd = dup(good.fd) };
	test(bad.fd >= 0);
	test(mm_loop_add(&loop, &bad, count_read, &ready, NULL, NULL,
			 poll_failed, &errors, NULL, NULL) == 0);
	test(close(bad.fd) == 0); /* POLL_ADD will complete with -EBADF */
	test(p->iface->step(p, 100) == 0 && errors == 1 && ready == 0);
	test(eventfd_write(good.fd, 1) == 0);
	test(p->iface->step(p, 100) == 0 && ready == 1 && errors == 1);

	/* No error handler: fail the step instead of invoking on_read. */
	test(mm_loop_add_ro(&loop, &bad, count_read, &ready) == 0);
	test(p->iface->step(p, 100) == -1 && errno == EBADF && ready == 1);
	test(mm_loop_shutdown(&loop) == 0);
	close(good.fd);
}

void machinarium_test_uring_poll(void)
{
	test(machinarium_init() == 0);
	check_poll_failure();
	check_coalesced_reads();
	check_detach_ready_peer();
	check_watch_reuse();
	check_watch_reuse_after_shutdown();
	check_watch_batch();
	int64_t id = mm_machine_create(-1, "uring_poll", check_poll, NULL,
				       &mm_uring_if);
	test(id >= 0);
	test(machine_wait(id) == 0);
	machinarium_free();
}

struct eof_recv {
	mm_uring_op_t op;
	char byte;
	int done;
};

static void complete_eof(mm_uring_op_t *op, int result, unsigned flags)
{
	(void)flags;
	struct eof_recv *r = mm_container_of(op, struct eof_recv, op);
	test(result == 0 && !r->done);
	r->done = 1;
}

/* Arm more than two CQs of reads, then complete them all before draining.
 * No new SQEs or blocking waits can help publish the overflow. */
static void check_cq_overflow(void)
{
	mm_loop_t loop;
	test(mm_loop_init(&loop, &mm_uring_if) == 0);
	mm_poll_t *p = loop.poll;
	int pair[2];
	test(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	enum { n = 288 };
	struct eof_recv reads[n] = { 0 };
	for (int i = 0; i < n; i++) {
		reads[i].op.complete = complete_eof;
		test(mm_uring_recv(p, &reads[i].op, pair[0], &reads[i].byte,
				   1) == 0);
		if ((i + 1) % 32 == 0) {
			test(p->iface->step(p, 0) == 0);
		}
	}
	test(shutdown(pair[1], SHUT_WR) == 0);
	for (int i = 0; i < 4; i++) {
		test(p->iface->step(p, 0) == 0);
	}
	for (int i = 0; i < n; i++) {
		test(reads[i].done);
	}
	test(mm_loop_shutdown(&loop) == 0);
	close(pair[0]);
	close(pair[1]);
}

void machinarium_test_uring_batch(void)
{
	struct pipe_end ends[2] = { { .payload = 'a' }, { .payload = 'b' } };
	for (int i = 0; i < 2; i++) {
		test(socketpair(AF_UNIX, SOCK_STREAM, 0, ends[i].fd) == 0);
		test(send(ends[i].fd[1], &ends[i].payload, 1, 0) == 1);
	}
	ends[0].dst = ends[1].fd[0];
	ends[1].dst = ends[0].fd[0];

	test(machinarium_init() == 0);
	check_cq_overflow();
	int64_t id =
		mm_machine_create(-1, "uring_batch", start, ends, &mm_uring_if);
	test(id >= 0);
	test(machine_wait(id) == 0);
	machinarium_free();

	for (int i = 0; i < 2; i++) {
		char byte;
		test(recv(ends[i].fd[1], &byte, 1, MSG_DONTWAIT) == 1);
		test(byte == ends[1 - i].payload);
		close(ends[i].fd[0]);
		close(ends[i].fd[1]);
	}
}
