
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <fcntl.h>
#if defined(__linux__)
#include <sys/eventfd.h>
#endif
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <machinarium/machinarium.h>

#if defined(__linux__)
#define MM_TCP_KEEPIDLE TCP_KEEPIDLE
#elif defined(__APPLE__)
#define MM_TCP_KEEPIDLE TCP_KEEPALIVE
#elif defined(TCP_KEEPIDLE)
#define MM_TCP_KEEPIDLE TCP_KEEPIDLE
#endif

int mm_socket(int domain, int type, int protocol)
{
	/* get and return file descriptor of env socket */
	int fd;
#if defined(SOCK_CLOEXEC)
	fd = socket(domain, type | SOCK_CLOEXEC, protocol);
#else
	fd = socket(domain, type, protocol);
	if (fd != -1) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	}
#endif
	return fd;
}

#if defined(__linux__)

int mm_socket_eventfd(unsigned int initval, int *write_fd)
{
	int fd = eventfd(initval, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd == -1) {
		return -1;
	}
	if (write_fd) {
		*write_fd = fd;
	}
	return fd;
}

#else
int mm_socket_eventfd(unsigned int initval, int *write_fd)
{
	int fds[2];
#if defined(__APPLE__)
	if (pipe(fds) == -1) {
		return -1;
	}
	fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	fcntl(fds[1], F_SETFD, FD_CLOEXEC);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	fcntl(fds[1], F_SETFL, O_NONBLOCK);
#else
	if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) == -1) {
		return -1;
	}
#endif
	for (unsigned int i = 0; i < initval; i++) {
		char byte = 1;
		write(fds[1], &byte, 1);
	}
	if (write_fd) {
		*write_fd = fds[1];
	}
	return fds[0];
}
#endif

int mm_socket_set_nonblock(int fd, int enable)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}
	if (enable) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}
	int rc;
	rc = fcntl(fd, F_SETFL, flags);
	return rc;
}

int mm_socket_set_nodelay(int fd, int enable)
{
	int rc;
	rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
	return rc;
}

int mm_socket_set_keepalive(int fd, int enable, int delay, int interval,
			    int keep_count, int usr_timeout)
{
	int rc;
	rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
	if (rc == MM_NOTOK_RETCODE) {
		return MM_NOTOK_RETCODE;
	}

	if (enable) {
		rc = setsockopt(fd, IPPROTO_TCP, MM_TCP_KEEPIDLE, &delay,
				sizeof(delay));
		if (rc == MM_NOTOK_RETCODE) {
			return MM_NOTOK_RETCODE;
		}

		rc = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
				sizeof(interval));
		if (rc == MM_NOTOK_RETCODE) {
			return MM_NOTOK_RETCODE;
		}

		rc = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count,
				sizeof(keep_count));
		if (rc == MM_NOTOK_RETCODE) {
			return MM_NOTOK_RETCODE;
		}

#if defined(TCP_USER_TIMEOUT)
		rc = setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &usr_timeout,
				sizeof(usr_timeout));
		if (rc == MM_NOTOK_RETCODE) {
			return MM_NOTOK_RETCODE;
		}
#else
		(void)usr_timeout;
#endif
	}
	return MM_OK_RETCODE;
}

int mm_socket_advice_keepalive_usr_timeout(int delay, int interval,
					   int keep_count)
{
	/*
	 * https://habr.com/ru/articles/700470/
	 * delay, interval are in seconds
	 * usr timeout in milliseconds
	 * see man 7 tcp
	 */
	return 1000 * (delay + interval * keep_count) - 500;
}

int mm_socket_set_nosigpipe(int fd, int enable)
{
#if defined(SO_NOSIGPIPE)
	int rc;
	rc = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable));
	if (rc == -1) {
		return -1;
	}
	(void)fd;
	(void)enable;
	return 0;
#else
	(void)fd;
	(void)enable;
	return 0;
#endif
	(void)fd;
	(void)enable;
	return 0;
}

int mm_socket_set_reuseaddr(int fd, int enable)
{
	int rc;
#ifdef SO_REUSEADDR
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#else
	/* ignore reuse addr in case of not enable */
	rc = enable ? -1 : 0;
#endif

	return rc;
}

int mm_socket_set_reuseport(int fd, int enable)
{
	int rc;
#ifdef SO_REUSEPORT
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
#else
	/* ignore reuse port in case of not enable */
	rc = enable ? -1 : 0;
#endif

	return rc;
}

int mm_socket_set_nolinger(int fd)
{
	int rc;
	struct linger linger = { .l_linger = 0, .l_onoff = 1 };
	rc = setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
	return rc;
}

int mm_socket_set_ipv6only(int fd, int enable)
{
	int rc;
	rc = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(enable));
	return rc;
}

int mm_socket_error(int fd)
{
	int error;
	socklen_t errorsize = sizeof(error);
	int rc;
	rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errorsize);
	if (rc == -1) {
		return -1;
	}
	return error;
}

int mm_socket_connect(int fd, struct sockaddr *sa)
{
	int addrlen;
	if (sa->sa_family == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
	} else if (sa->sa_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	} else if (sa->sa_family == AF_UNIX) {
		addrlen = sizeof(struct sockaddr_un);
	} else {
		errno = EINVAL;
		return -1;
	}
	int rc;
	rc = connect(fd, sa, addrlen);
	return rc;
}

int mm_socket_bind(int fd, struct sockaddr *sa)
{
	int addrlen;
	if (sa->sa_family == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
	} else if (sa->sa_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	} else if (sa->sa_family == AF_UNIX) {
		addrlen = sizeof(struct sockaddr_un);
	} else {
		errno = EINVAL;
		return -1;
	}
	int rc;
	rc = bind(fd, sa, addrlen);
	return rc;
}

int mm_socket_listen(int fd, int backlog)
{
	int rc;
	rc = listen(fd, backlog);
	return rc;
}

int mm_socket_accept(int fd, struct sockaddr *sa, socklen_t *slen)
{
	int rc;
#if defined(__linux__)
	rc = accept4(fd, sa, slen, SOCK_CLOEXEC | SOCK_NONBLOCK);
#else
	rc = accept(fd, sa, slen);
	if (rc != -1) {
		fcntl(rc, F_SETFD, FD_CLOEXEC);
		mm_socket_set_nonblock(rc, 1);
	}
#endif
	return rc;
}

int mm_socket_write(int fd, const void *buf, int size)
{
	int rc;
	rc = write(fd, buf, size);
	return rc;
}

int mm_socket_writev(int fd, const struct iovec *iov, int iovc)
{
	int rc;
	rc = writev(fd, iov, iovc);
	return rc;
}

int mm_socket_read(int fd, void *buf, int size)
{
	int rc;
	rc = read(fd, buf, size);
	return rc;
}

int mm_socket_read_pending(int fd)
{
	int rc;
	rc = ioctl(fd, FIONREAD, &rc);
	if (rc == -1) {
		return -1;
	}

	return rc > 0;
}

int mm_socket_getsockname(int fd, struct sockaddr *sa, socklen_t *salen)
{
	int rc;
	rc = getsockname(fd, sa, salen);
	return rc;
}

int mm_socket_getpeername(int fd, struct sockaddr *sa, socklen_t *salen)
{
	int rc;
	rc = getpeername(fd, sa, salen);
	return rc;
}

int mm_socket_getaddrinfo(char *node, char *service, struct addrinfo *hints,
			  struct addrinfo **res)
{
	int rc;
	rc = getaddrinfo(node, service, hints, res);
	return rc;
}
