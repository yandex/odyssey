
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/signalfd.h>
#else
#include <fcntl.h>
#endif

#include <machinarium/machinarium.h>
#include <machinarium/signal_mgr.h>
#include <machinarium/fd.h>
#include <machinarium/machine.h>
#include <machinarium/socket.h>

static void mm_signalmgr_on_read(mm_fd_t *handle)
{
	mm_signalmgr_t *mgr = handle->on_read_arg;

	/* do one-time wakeup and detach all readers */
	mm_list_t *i, *n;
	mm_list_foreach_safe (&mgr->readers, i, n) {
		mm_signalrd_t *reader;
		reader = mm_container_of(i, mm_signalrd_t, link);
		mm_scheduler_wakeup(&mm_self->scheduler,
				    reader->call.coroutine);
		mm_list_unlink(&reader->link);
	}
}

#if defined(__linux__)

int mm_signalmgr_init(mm_signalmgr_t *mgr, mm_loop_t *loop)
{
	mm_list_init(&mgr->readers);
	memset(&mgr->fd, 0, sizeof(mgr->fd));
	mgr->fd.fd = -1;

	sigset_t mask;
	sigemptyset(&mask);
	int rc;
	rc = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (rc == -1) {
		return -1;
	}
	mgr->fd.fd = rc;

	rc = mm_loop_add_ro(loop, &mgr->fd, mm_signalmgr_on_read, mgr);
	if (rc == -1) {
		close(mgr->fd.fd);
		mgr->fd.fd = -1;
		return -1;
	}
	return 0;
}

void mm_signalmgr_free(mm_signalmgr_t *mgr, mm_loop_t *loop)
{
	if (mgr->fd.fd == -1) {
		return;
	}
	mm_loop_delete(loop, &mgr->fd);
	close(mgr->fd.fd);
	mgr->fd.fd = -1;
}

int mm_signalmgr_set(mm_signalmgr_t *mgr, sigset_t *set, sigset_t *ignore)
{
	int rc;
	rc = signalfd(mgr->fd.fd, set, SFD_NONBLOCK | SFD_CLOEXEC);
	if (rc == -1) {
		return -1;
	}
	assert(rc == mgr->fd.fd);
	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
	pthread_sigmask(SIG_BLOCK, set, NULL);
	pthread_sigmask(SIG_BLOCK, ignore, NULL);
	return 0;
}

int mm_signalmgr_wait(mm_signalmgr_t *mgr, uint32_t time_ms)
{
	mm_errno_set(0);

	while (1) {
		struct signalfd_siginfo fdsi;
		memset(&fdsi, 0, sizeof(fdsi));
		int rc = read(mgr->fd.fd, &fdsi, sizeof(fdsi));
		if (rc == sizeof(fdsi)) {
			return fdsi.ssi_signo;
		}

		int err = errno;

		if (machine_errno_retryable(err)) {
			mm_signalrd_t reader;
			mm_list_init(&reader.link);
			mm_list_append(&mgr->readers, &reader.link);

			mm_call(&reader.call, MM_CALL_SIGNAL, time_ms);

			if (reader.call.status != 0) {
				/* timedout or cancel */
				if (!reader.signal) {
					mm_list_unlink(&reader.link);
				}
				return -1;
			}

			continue;
		}

		mm_errno_set(err);
		return -1;
	}

	abort();
}

#else

static volatile int mm_signalmgr_write_fd = -1;

static void mm_signalmgr_handler(int signo)
{
	if (mm_signalmgr_write_fd != -1) {
		uint8_t byte = (uint8_t)signo;
		ssize_t rc = write(mm_signalmgr_write_fd, &byte, 1);
		(void)rc;
	}
}

int mm_signalmgr_init(mm_signalmgr_t *mgr, mm_loop_t *loop)
{
	mm_list_init(&mgr->readers);
	memset(&mgr->fd, 0, sizeof(mgr->fd));
	mgr->fd.fd = -1;
	mgr->write_fd = -1;

	int fds[2];
	if (pipe(fds) == -1) {
		return -1;
	}
	fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	fcntl(fds[1], F_SETFD, FD_CLOEXEC);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	fcntl(fds[1], F_SETFL, O_NONBLOCK);

	mgr->fd.fd = fds[0];
	mgr->write_fd = fds[1];

	int rc;
	rc = mm_loop_add_ro(loop, &mgr->fd, mm_signalmgr_on_read, mgr);
	if (rc == -1) {
		close(mgr->fd.fd);
		close(mgr->write_fd);
		mgr->fd.fd = -1;
		mgr->write_fd = -1;
		return -1;
	}

	mm_signalmgr_write_fd = mgr->write_fd;
	return 0;
}

void mm_signalmgr_free(mm_signalmgr_t *mgr, mm_loop_t *loop)
{
	if (mgr->fd.fd == -1) {
		return;
	}
	mm_loop_delete(loop, &mgr->fd);
	close(mgr->fd.fd);
	close(mgr->write_fd);
	mgr->fd.fd = -1;
	mgr->write_fd = -1;
	mm_signalmgr_write_fd = -1;
}

int mm_signalmgr_set(mm_signalmgr_t *mgr, sigset_t *set, sigset_t *ignore)
{
	(void)mgr;

	/*
	 * make sure none of the requested signals stay blocked, so that
	 * our handler actually gets invoked on delivery
	 */
	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

	int sig;
	for (sig = 1; sig < NSIG; sig++) {
		if (set && sigismember(set, sig)) {
			struct sigaction sa;
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = mm_signalmgr_handler;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			if (sigaction(sig, &sa, NULL) == -1) {
				/*
				 * some signals (e.g. SIGKILL/SIGSTOP)
				 * cannot be handled, ignore failure
				 */
			}
		}
		if (ignore && sigismember(ignore, sig)) {
			signal(sig, SIG_IGN);
		}
	}

	return 0;
}

int mm_signalmgr_wait(mm_signalmgr_t *mgr, uint32_t time_ms)
{
	mm_errno_set(0);

	while (1) {
		uint8_t byte = 0;
		int rc = read(mgr->fd.fd, &byte, sizeof(byte));
		if (rc == sizeof(byte)) {
			return (int)byte;
		}

		int err = errno;

		if (machine_errno_retryable(err)) {
			mm_signalrd_t reader;
			mm_list_init(&reader.link);
			mm_list_append(&mgr->readers, &reader.link);

			mm_call(&reader.call, MM_CALL_SIGNAL, time_ms);

			if (reader.call.status != 0) {
				/* timedout or cancel */
				if (!reader.signal) {
					mm_list_unlink(&reader.link);
				}
				return -1;
			}

			continue;
		}

		mm_errno_set(err);
		return -1;
	}

	abort();
}

#endif

MACHINE_API int machine_signal_init(sigset_t *set, sigset_t *ignore)
{
	return mm_signalmgr_set(&mm_self->signal_mgr, set, ignore);
}

MACHINE_API int machine_signal_wait(uint32_t time_ms)
{
	return mm_signalmgr_wait(&mm_self->signal_mgr, time_ms);
}
