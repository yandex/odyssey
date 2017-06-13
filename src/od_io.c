
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
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

#include "od_macro.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_io.h"

int od_read(machine_io_t *io, so_stream_t *stream, int time_ms)
{
	uint32_t request_start = so_stream_used(stream);
	uint32_t request_size = 0;
	for (;;)
	{
		uint8_t *request_data = stream->s + request_start;
		uint32_t len;
		int to_read;
		to_read = so_read(&len, &request_data, &request_size);
		if (to_read == 0)
			break;
		if (to_read == -1)
			return -1;
		int rc = so_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = machine_read(io, (char*)stream->p, to_read, time_ms);
		if (rc < 0)
			return -1;
		so_stream_advance(stream, to_read);
		request_size += to_read;
	}
	return request_start;
}

int od_write(machine_io_t *io, so_stream_t *stream)
{
	int rc;
	rc = machine_write(io, (char*)stream->s,
	                   so_stream_used(stream),
	                   UINT32_MAX);
	if (rc < 0)
		return -1;
	return 0;
}

int od_getpeername(machine_io_t *io, char *buf, int size)
{
	char addr[128];
	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	int rc = machine_getpeername(io, (struct sockaddr*)&sa, &salen);
	if (rc < 0)
		goto error;

	if (sa.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)&sa;
		inet_ntop(sa.ss_family, &sin->sin_addr, addr, sizeof(addr));
		snprintf(buf, size, "%s:%d", addr, ntohs(sin->sin_port));
		return 0;
	}
	if (sa.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)&sa;
		inet_ntop(sa.ss_family, &sin->sin6_addr, addr, sizeof(addr));
		snprintf(buf, size, "[%s]:%d", addr, ntohs(sin->sin6_port));
		return 0;
	}

error:
	snprintf(buf, size, "unknown");
	return -1;
}
