
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_io.h"

int od_read(machine_io_t io, so_stream_t *stream, int time_ms)
{
	so_stream_reset(stream);
	for (;;) {
		uint32_t pos_size = so_stream_used(stream);
		uint8_t *pos_data = stream->s;
		uint32_t len;
		int to_read;
		to_read = so_read(&len, &pos_data, &pos_size);
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
	}
	return 0;
}

int od_write(machine_io_t io, so_stream_t *stream)
{
	int rc;
	rc = machine_write(io, (char*)stream->s, so_stream_used(stream), INT_MAX);
	if (rc < 0)
		return -1;
	return 0;
}

char *od_getpeername(machine_io_t io)
{
	static char sockname[128];
	char addr[128];
	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	int rc = machine_getpeername(io, (struct sockaddr*)&sa, &salen);
	if (rc < 0)
		goto unknown;
	if (sa.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)&sa;
		inet_ntop(sa.ss_family, &sin->sin_addr, addr, sizeof(addr));
		snprintf(sockname, sizeof(sockname), "%s:%d", addr, ntohs(sin->sin_port));
	} else
	if (sa.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)&sa;
		inet_ntop(sa.ss_family, &sin->sin6_addr, addr, sizeof(addr));
		snprintf(sockname, sizeof(sockname), "[%s]:%d", addr, ntohs(sin->sin6_port));
	} else {
		goto unknown;
	}
	return sockname;
unknown:
	snprintf(sockname, sizeof(sockname), "unknown");
	return sockname;
}
