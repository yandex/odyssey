
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

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_stat.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_list.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od.h"
#include "od_io.h"
#include "od_pooler.h"
#include "od_fe.h"

void od_feclose(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;
	if (client->route) {
		od_route_t *route = client->route;
		od_clientpool_set(&route->client_pool, client, OD_CUNDEF);
		client->route = NULL;
	}
	if (client->io) {
		mm_close(client->io);
		client->io = NULL;
	}
	od_clientlist_unlink(&pooler->client_list, client);
	od_clientfree(client);
}

int od_feerror(od_client_t *client, char *fmt, ...)
{
	od_pooler_t *pooler = client->pooler;

	char message[512];
	va_list args;
	va_start(args, fmt);
	int len;
	len = vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	od_error(&pooler->od->log, client->io, "C: %s", message);

	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_error(stream, message, len);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

static int
od_festartup_read(od_client_t *client)
{
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	for (;;) {
		uint32_t pos_size = so_stream_used(stream);
		uint8_t *pos_data = stream->s;
		uint32_t len;
		int to_read;
		to_read = so_read_startup(&len, &pos_data, &pos_size);
		if (to_read == 0)
			break;
		if (to_read == -1)
			return -1;
		int rc = so_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = mm_read(client->io, to_read, 0);
		if (rc < 0)
			return -1;
		char *data_pointer = mm_read_buf(client->io);
		memcpy(stream->p, data_pointer, to_read);
		so_stream_advance(stream, to_read);
	}
	return 0;
}

int od_festartup(od_client_t *client)
{
	int rc;
	rc = od_festartup_read(client);
	if (rc == -1)
		return -1;
	so_stream_t *stream = &client->stream;
	rc = so_beread_startup(&client->startup, stream->s,
	                       so_stream_used(stream));
	if (rc == -1)
		return -1;
	return 0;
}

int od_fekey(od_client_t *client)
{
	client->key.key_pid = client->id;
	client->key.key = 1 + rand();
	return 0;
}

int od_fesetup(od_client_t *client)
{
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_backend_key_data(stream, client->key.key_pid,
	                                 client->key.key);
	if (rc == -1)
		return -1;
	rc = so_bewrite_parameter_status(stream, "", 1, "", 1);
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}

int od_feready(od_client_t *client)
{
	so_stream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_ready(stream, 'I');
	if (rc == -1)
		return -1;
	rc = od_write(client->io, stream);
	return rc;
}
