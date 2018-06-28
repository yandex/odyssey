
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

int mm_socket(int domain, int type, int protocol)
{
	int rc;
	rc = socket(domain, type, protocol);
	return rc;
}

int mm_socket_set_nonblock(int fd, int enable)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	if (enable)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	int rc;
	rc = fcntl(fd, F_SETFL, flags);
	return rc;
}

int mm_socket_set_nodelay(int fd, int enable)
{
	int rc;
	rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable,
	                sizeof(enable));
	return rc;
}

int mm_socket_set_keepalive(int fd, int enable, int delay)
{
	int rc;
	rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable,
	                sizeof(enable));
	if (rc == -1)
		return -1;
#ifdef TCP_KEEPIDLE
	if (enable) {
		rc = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &delay,
		                sizeof(delay));
		if (rc == -1)
			return -1;
	}
#endif
	return 0;
}

int mm_socket_set_nosigpipe(int fd, int enable)
{
#if defined(SO_NOSIGPIPE)
	int enable = 1;
	rc = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enable,
	                sizeof(enable));
	if (rc == -1)
		return -1;
#endif
	(void)fd;
	(void)enable;
	return 0;
}

int mm_socket_set_reuseaddr(int fd, int enable)
{
	int rc;
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
	                sizeof(enable));
	return rc;
}

int mm_socket_set_ipv6only(int fd, int enable)
{
	int rc;
	rc = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &enable,
	                sizeof(enable));
	return rc;
}

int mm_socket_error(int fd)
{
	int error;
	socklen_t errorsize = sizeof(error);
	int rc;
	rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error,
	                &errorsize);
	if (rc == -1)
		return -1;
	return error;
}

int mm_socket_connect(int fd, struct sockaddr *sa)
{
	int addrlen;
	if (sa->sa_family == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
	} else
	if (sa->sa_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	} else
	if (sa->sa_family == AF_UNIX) {
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
	} else
	if (sa->sa_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	} else
	if (sa->sa_family == AF_UNIX) {
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
	rc = accept(fd, sa, slen);
	return rc;
}

int mm_socket_write(int fd, void *buf, int size)
{
	int rc;
	rc = write(fd, buf, size);
	return rc;
}

int mm_socket_read(int fd, void *buf, int size)
{
	int rc;
	rc = read(fd, buf, size);
	return rc;
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

int mm_socket_getaddrinfo(char *node,
                          char *service,
                          struct addrinfo *hints,
                          struct addrinfo **res)
{
	int rc;
	rc = getaddrinfo(node, service, hints, res);
	return rc;
}
