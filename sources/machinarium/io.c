
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>
#include <sys/poll.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/machine.h>
#include <machinarium/tls.h>
#include <machinarium/socket.h>
#include <machinarium/compression.h>

static void mm_io_on_read_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_read_arg;
	mm_cond_signal(&io->cond);
}

static void mm_io_on_write_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;
	mm_cond_signal(&io->cond);
}

static void mm_io_on_err_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;

	mm_cond_signal(&io->cond);

	io->errored = 1;
	io->error = mm_socket_error(handle->fd);
	io->connected = 0;
}

static void mm_io_on_close_cb(mm_fd_t *handle)
{
	mm_io_t *io = handle->on_write_arg;

	mm_cond_signal(&io->cond);

	io->connected = 0;
}

MACHINE_API machine_tls_t *machine_tls_create(void)
{
	mm_errno_set(0);
	mm_tls_t *tls;
	tls = mm_malloc(sizeof(*tls));
	if (tls == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	tls->verify = MM_TLS_NONE;
	tls->server = NULL;
	tls->protocols = NULL;
	tls->ca_path = NULL;
	tls->ca_file = NULL;
	tls->cert_file = NULL;
	tls->key_file = NULL;
	return (machine_tls_t *)tls;
}

MACHINE_API void machine_tls_free(machine_tls_t *obj)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	if (tls->protocols) {
		mm_free(tls->protocols);
	}
	if (tls->server) {
		mm_free(tls->server);
	}
	if (tls->ca_path) {
		mm_free(tls->ca_path);
	}
	if (tls->ca_file) {
		mm_free(tls->ca_file);
	}
	if (tls->cert_file) {
		mm_free(tls->cert_file);
	}
	if (tls->key_file) {
		mm_free(tls->key_file);
	}
	mm_free(tls);
}

MACHINE_API int machine_tls_set_verify(machine_tls_t *obj, char *mode)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	if (strcasecmp(mode, "none") == 0) {
		tls->verify = MM_TLS_NONE;
	} else if (strcasecmp(mode, "peer") == 0) {
		tls->verify = MM_TLS_PEER;
	} else if (strcasecmp(mode, "peer_strict") == 0) {
		tls->verify = MM_TLS_PEER_STRICT;
	} else {
		return -1;
	}
	return 0;
}

MACHINE_API int machine_tls_set_server(machine_tls_t *obj, char *name)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(name);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->server) {
		mm_free(tls->server);
	}
	tls->server = string;
	return 0;
}

MACHINE_API int machine_tls_set_protocols(machine_tls_t *obj, char *protocols)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(protocols);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->protocols) {
		mm_free(tls->protocols);
	}
	tls->protocols = string;
	return 0;
}

MACHINE_API int machine_tls_set_ca_path(machine_tls_t *obj, char *path)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(path);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->ca_path) {
		mm_free(tls->ca_path);
	}
	tls->ca_path = string;
	return 0;
}

MACHINE_API int machine_tls_set_ca_file(machine_tls_t *obj, char *path)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(path);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->ca_file) {
		mm_free(tls->ca_file);
	}
	tls->ca_file = string;
	return 0;
}

MACHINE_API int machine_tls_set_cert_file(machine_tls_t *obj, char *path)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(path);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->cert_file) {
		mm_free(tls->cert_file);
	}
	tls->cert_file = string;
	return 0;
}

MACHINE_API int machine_tls_set_key_file(machine_tls_t *obj, char *path)
{
	mm_tls_t *tls = mm_cast(mm_tls_t *, obj);
	mm_errno_set(0);
	char *string = mm_strdup(path);
	if (string == NULL) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	if (tls->key_file) {
		mm_free(tls->key_file);
	}
	tls->key_file = string;
	return 0;
}

int mm_io_set_tls(mm_io_t *io, machine_tls_t *tls, uint32_t timeout)
{
	if (io->tls) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	io->tls = mm_cast(mm_tls_t *, tls);
	return mm_tls_handshake(io, timeout);
}

int mm_io_is_tls(mm_io_t *io)
{
	return io->tls != NULL;
}

int mm_io_set_compression(mm_io_t *io, char algorithm)
{
	if (io->zpq_stream) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}

	int impl = mm_zpq_get_algorithm_impl(algorithm);
	if (impl >= 0) {
		io->zpq_stream =
			zpq_create(impl, (mm_zpq_tx_func)mm_io_write,
				   (mm_zpq_rx_func)mm_io_read, io, NULL, 0);
		return 0;
	}
	return -1;
}

mm_io_t *mm_io_create(void)
{
	mm_errno_set(0);
	mm_io_t *io = mm_malloc(sizeof(*io));
	if (io == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	memset(io, 0, sizeof(*io));
	io->fd = -1;
	mm_cond_init(&io->cond);
	mm_tls_init(io);
	return io;
}

MACHINE_API void mm_io_free(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	mm_tls_free(io);
	mm_compression_free(io);
	mm_free(io);
}

char *mm_io_error(mm_io_t *io)
{
	if (io->tls_error) {
		return io->tls_error_msg;
	}
	if (io->error) {
		return strerror(io->error);
	}
	int errno_ = mm_errno_get();
	if (errno_) {
		return strerror(errno_);
	}
	errno_ = errno;
	if (errno_) {
		return strerror(errno_);
	}
	return NULL;
}

int mm_io_fd(mm_io_t *io)
{
	return io->fd;
}

int mm_io_set_nodelay(mm_io_t *io, int enable)
{
	mm_errno_set(0);
	io->opt_nodelay = enable;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_nodelay(io->fd, enable);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	return 0;
}

int mm_io_set_nolinger(mm_io_t *io)
{
	mm_errno_set(0);

	int rc;
	rc = mm_socket_set_nolinger(io->fd);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}

	return 0;
}

int mm_io_set_keepalive(mm_io_t *io, int enable, int delay, int interval,
			int probes, int usr_timeout)
{
	mm_errno_set(0);
	io->opt_keepalive = enable;
	io->opt_keepalive_delay = delay;
	io->opt_keepalive_interval = interval;
	io->opt_keepalive_probes = probes;
	io->opt_keepalive_usr_timeout = usr_timeout;
	if (io->fd != -1) {
		int rc;
		rc = mm_socket_set_keepalive(io->fd, enable, delay, interval,
					     probes, usr_timeout);
		if (rc == -1) {
			mm_errno_set(errno);
			return -1;
		}
	}
	return 0;
}

int mm_io_advice_keepalive_usr_timeout(int delay, int interval, int probes)
{
	return mm_socket_advice_keepalive_usr_timeout(delay, interval, probes);
}

int mm_io_attach(mm_io_t *io)
{
	mm_errno_set(0);
	if (io->attached) {
		mm_errno_set(EINPROGRESS);
		return -1;
	}
	int rc;
	rc = mm_loop_add(&mm_self->loop, &io->handle, mm_io_on_read_cb, io,
			 mm_io_on_write_cb, io, mm_io_on_err_cb, io,
			 mm_io_on_close_cb, io);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	io->attached = 1;
	return 0;
}

int mm_io_detach(mm_io_t *io)
{
	mm_errno_set(0);
	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}
	int rc;
	rc = mm_loop_delete(&mm_self->loop, &io->handle);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	io->attached = 0;
	return 0;
}

int mm_io_verify(mm_io_t *io, char *common_name)
{
	mm_errno_set(0);
	if (io->tls == NULL) {
		mm_errno_set(EINVAL);
		return -1;
	}
	int rc;
	rc = mm_tls_verify_common_name(io, common_name);
	return rc;
}

int mm_io_socket_set(mm_io_t *io, int fd)
{
	io->fd = fd;
	int rc;
	rc = mm_socket_set_nosigpipe(io->fd, 1);
	if (rc == -1) {
		mm_errno_set(errno);
		return -1;
	}
	if (!io->is_unix_socket) {
		if (io->opt_nodelay) {
			rc = mm_socket_set_nodelay(io->fd, 1);
			if (rc == -1) {
				mm_errno_set(errno);
				return -1;
			}
		}
		if (io->opt_keepalive) {
			rc = mm_socket_set_keepalive(
				io->fd, 1, io->opt_keepalive_delay,
				io->opt_keepalive_interval,
				io->opt_keepalive_probes,
				io->opt_keepalive_usr_timeout);
			if (rc == -1) {
				mm_errno_set(errno);
				return -1;
			}
		}
	}
	io->handle.fd = io->fd;
	return 0;
}

int mm_io_socket(mm_io_t *io, struct sockaddr *sa)
{
	if (sa->sa_family == AF_UNIX) {
		io->is_unix_socket = 1;
	}
	int fd;
	fd = mm_socket(sa->sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (fd == -1) {
		mm_errno_set(errno);
		return -1;
	}
	return mm_io_socket_set(io, fd);
}

ssize_t mm_io_write(mm_io_t *io, const void *buf, size_t size)
{
	mm_scheduler_register_io();

	mm_errno_set(0);
	ssize_t rc;
	if (mm_tls_is_active(io)) {
		rc = mm_tls_write(io, buf, size);
	} else {
		rc = mm_socket_write(io->fd, buf, size);
	}
	if (rc > 0) {
		return rc;
	}
	int errno_ = errno;
	mm_errno_set(errno_);
	if (machine_errno_retryable(errno_)) {
		return -1;
	}
	io->connected = 0;
	return -1;
}

ssize_t mm_io_read(mm_io_t *io, void *buf, size_t size)
{
	mm_scheduler_register_io();

	mm_errno_set(0);
	ssize_t rc;
	if (mm_tls_is_active(io)) {
		rc = mm_tls_read(io, buf, size);
	} else {
		rc = mm_socket_read(io->fd, buf, size);
	}

	if (rc > 0) {
		return rc;
	}
	if (rc < 0) {
		int errno_ = errno;
		mm_errno_set(errno_);
		if (machine_errno_retryable(errno_)) {
			return -1;
		}
	}
	/* error of eof */
	io->connected = 0;
	return rc;
}

int mm_io_read_pending(mm_io_t *io)
{
	if (mm_tls_is_active(io)) {
		return mm_tls_read_pending(io);
	}

	return mm_socket_read_pending(io->fd);
}

static inline int format_inet_socket_addr(struct sockaddr *sa, socklen_t sa_len,
					  char *buf, size_t buflen)
{
	static MM_THREAD_LOCAL char host[NI_MAXHOST];
	static MM_THREAD_LOCAL char serv[NI_MAXSERV];
	int rc = getnameinfo(sa, sa_len, host, sizeof(host), serv, sizeof(serv),
			     NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc != 0) {
		snprintf(buf, buflen, "getnameinfo error: %s",
			 gai_strerror(rc));
		return -1;
	}

	if (sa->sa_family == AF_INET6) {
		snprintf(buf, buflen, "[%s]:%s", host, serv);
	} else {
		snprintf(buf, buflen, "%s:%s", host, serv);
	}

	return 0;
}

static inline int format_unix_socket_addr(char *buf, size_t buflen)
{
	/* TODO: implement unix sock name formatting */
	snprintf(buf, buflen, "unix");

	return 0;
}

int mm_io_format_socket_addr(mm_io_t *io, char *buf, size_t buflen)
{
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (getsockname(io->handle.fd, (struct sockaddr *)&sa, &sa_len) < 0) {
		return -1;
	}

	if (sa.ss_family == AF_INET || sa.ss_family == AF_INET6) {
		return format_inet_socket_addr((struct sockaddr *)&sa, sa_len,
					       buf, buflen);
	} else if (sa.ss_family == AF_UNIX) {
		return format_unix_socket_addr(buf, buflen);
	}

	snprintf(buf, buflen, "(unknown family %d)", sa.ss_family);
	return -1;
}

int mm_io_wait(mm_io_t *io, uint32_t timeout_ms)
{
	if (mm_unlikely(!io->attached)) {
		abort();
	}

	return mm_cond_wait(&io->cond, timeout_ms);
}

void mm_io_set_peer(mm_io_t *io, mm_io_t *peer)
{
	if (mm_unlikely(peer->cond.propagate != NULL)) {
		abort();
	}

	mm_cond_propagate(&peer->cond, &io->cond);
}

void mm_io_remove_peer(mm_io_t *io, mm_io_t *peer)
{
	if (mm_unlikely(peer->cond.propagate != &io->cond)) {
		abort();
	}

	mm_cond_propagate(&peer->cond, NULL);
}

int mm_tls_is_active(mm_io_t *io)
{
	return io->tls_ssl != NULL;
}

int mm_io_last_event(mm_io_t *io)
{
	return io->handle.last_event;
}

void mm_io_set_deadline(mm_io_t *io, uint32_t timeout_ms)
{
	uint64_t now_ms = machine_time_ms();
	uint64_t deadline_ms;

	if (timeout_ms != UINT32_MAX) {
		if (now_ms > UINT64_MAX - timeout_ms) {
			deadline_ms = UINT64_MAX;
		} else {
			deadline_ms = now_ms + timeout_ms;
		}
	} else {
		deadline_ms = UINT64_MAX;
	}

	io->deadline_ms = deadline_ms;
}

int mm_io_wait_deadline(mm_io_t *io)
{
	assert(io->deadline_ms > 0);

	uint64_t now_ms = machine_time_ms();
	if (now_ms >= io->deadline_ms) {
		mm_errno_set(ETIMEDOUT);
		return MM_COND_WAIT_FAIL;
	}

	uint32_t left_ms;
	if (io->deadline_ms != UINT64_MAX) {
		if (io->deadline_ms - now_ms >= UINT32_MAX) {
			left_ms = UINT32_MAX;
		} else {
			left_ms = (uint32_t)(io->deadline_ms - now_ms);
		}
	} else {
		left_ms = UINT32_MAX;
	}

	return mm_io_wait(io, left_ms);
}

int mm_io_poll(mm_io_t *io)
{
	/* no need to check IN/OUT - we call read/write before awaiting anyway */

	if (!io->attached) {
		mm_errno_set(ENOTCONN);
		return -1;
	}

	mm_fd_t *fd = &io->handle;

	struct pollfd pfd;
	memset(&pfd, 0, sizeof(struct pollfd));
	pfd.fd = fd->fd;
	pfd.events = POLLERR | POLLHUP | POLLRDHUP;

	int rc = poll(&pfd, 1, 0);
	if (rc < 0) {
		mm_errno_set(errno);
		return rc;
	}

	if (rc == 0) {
		/* no events, ok */
		return 0;
	}

	assert(rc == 1);

	int events = pfd.events;
	int err = events & POLLERR;
	int close = events & POLLHUP || events & POLLRDHUP;

	if (err && fd->mask & MM_ERR) {
		fd->on_err(&io->handle);
	}

	if (close && fd->mask & MM_CLOSE) {
		fd->on_close(&io->handle);
	}

	return 0;
}
