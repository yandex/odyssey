
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/io.h"

int od_read(machine_io_t *io, shapito_stream_t *stream, int time_ms)
{
	uint32_t request_start = shapito_stream_used(stream);
	uint32_t request_size = 0;
	for (;;)
	{
		char *request_data = stream->start + request_start;
		uint32_t len;
		int to_read;
		to_read = shapito_read(&len, &request_data, &request_size);
		if (to_read == 0)
			break;
		if (to_read == -1)
			return -1;
		int rc = shapito_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(io, stream->pos, to_read, time_ms);
		if (rc == -1)
			return -1;
		shapito_stream_advance(stream, to_read);
		request_size += to_read;
	}
	return request_start;
}

int od_write(machine_io_t *io, shapito_stream_t *stream)
{
	int rc;
	rc = machine_write(io, stream->start, shapito_stream_used(stream),
	                   UINT32_MAX);
	return rc;
}

static int
od_getsockaddrname(struct sockaddr *sa, char *buf, int size,
                   int add_addr,
                   int add_port)
{
	char addr[128];
	if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)sa;
		inet_ntop(sa->sa_family, &sin->sin_addr, addr, sizeof(addr));
		if (add_addr && add_port)
			snprintf(buf, size, "%s:%d", addr, ntohs(sin->sin_port));
		else
		if (add_addr)
			snprintf(buf, size, "%s", addr);
		else
		if (add_port)
			snprintf(buf, size, "%d", ntohs(sin->sin_port));
		return 0;
	}
	if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)sa;
		inet_ntop(sa->sa_family, &sin->sin6_addr, addr, sizeof(addr));
		if (add_addr && add_port)
			snprintf(buf, size, "[%s]:%d", addr, ntohs(sin->sin6_port));
		else
		if (add_addr)
			snprintf(buf, size, "%s", addr);
		else
		if (add_port)
			snprintf(buf, size, "%d", ntohs(sin->sin6_port));
		return 0;
	}
	snprintf(buf, size, "%s", "");
	return -1;
}

int od_getaddrname(struct addrinfo *ai, char *buf, int size, int add_addr, int add_port)
{
	return od_getsockaddrname(ai->ai_addr, buf, size, add_addr, add_port);
}

int od_getpeername(machine_io_t *io, char *buf, int size, int add_addr, int add_port)
{
	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	int rc = machine_getpeername(io, (struct sockaddr*)&sa, &salen);
	if (rc < 0) {
		snprintf(buf, size, "%s", "");
		return -1;
	}
	return od_getsockaddrname((struct sockaddr*)&sa, buf, size, add_addr, add_port);
}

int od_getsockname(machine_io_t *io, char *buf, int size, int add_addr, int add_port)
{
	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	int rc = machine_getsockname(io, (struct sockaddr*)&sa, &salen);
	if (rc < 0) {
		snprintf(buf, size, "%s", "");
		return -1;
	}
	return od_getsockaddrname((struct sockaddr*)&sa, buf, size, add_addr, add_port);
}
