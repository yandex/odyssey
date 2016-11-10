
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <flint.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od.h"
#include "od_pooler.h"
#include "od_router.h"

static inline int
od_read(ftio_t *io, sostream_t *stream)
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
		rc = ft_read(io, to_read, 0);
		if (rc < 0)
			return -1;
		char *data_pointer = ft_read_buf(io);
		memcpy(stream->p, data_pointer, to_read);
		so_stream_advance(stream, to_read);
	}
	return 0;
}

static void
od_feclose(odclient_t *client)
{
	odpooler_t *pooler = client->pooler;
	if (client->io) {
		ft_close(client->io);
		client->io = NULL;
	}
	od_clientpool_unlink(&pooler->client_pool, client);
}

static inline int
od_festartup_read(odclient_t *client)
{
	sostream_t *stream = &client->stream;
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
		rc = ft_read(client->io, to_read, 0);
		if (rc < 0)
			return -1;
		char *data_pointer = ft_read_buf(client->io);
		memcpy(stream->p, data_pointer, to_read);
		so_stream_advance(stream, to_read);
	}
	return 0;
}

static int
od_festartup(odclient_t *client)
{
	int rc;
	rc = od_festartup_read(client);
	if (rc == -1)
		return -1;
	sostream_t *stream = &client->stream;
	rc = so_beread_startup(&client->startup, stream->s,
	                       so_stream_used(stream));
	if (rc == -1)
		return -1;
	return 0;
}

static int
od_feauth(odclient_t *client)
{
	sostream_t *stream = &client->stream;
	so_stream_reset(stream);
	int rc;
	rc = so_bewrite_authentication(stream, 0);
	if (rc == -1)
		return -1;
	rc = so_bewrite_backend_key_data(stream, 0, 0);
	if (rc == -1)
		return -1;
	rc = so_bewrite_parameter_status(stream, "", 1, "", 1);
	if (rc == -1)
		return -1;
	rc = so_bewrite_ready(stream, 'I');
	if (rc == -1)
		return -1;
	rc = ft_write(client->io, (char*)stream->s, so_stream_used(stream), 0);
	return rc;
}

static odserver_t*
od_beconnect(odpooler_t *pooler, sobestartup_t *startup)
{
	(void)pooler;
	(void)startup;
	return NULL;
}

static odserver_t*
od_bepop(odpooler_t *pooler, sobestartup_t *startup)
{
	odserver_t *server = od_serverpool_pop(&pooler->server_pool);
	if (server) {
		od_serverpool_set(&pooler->server_pool, server, OD_SACTIVE);
		return server;
	}
	server = od_beconnect(pooler, startup);
	return server;
}

void od_router(void *arg)
{
	odclient_t *client = arg;
	odpooler_t *pooler = client->pooler;

	od_log(&pooler->od->log, "C: new connection");

	/* client startup */
	int rc = od_festartup(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}
	/* client cancel request */
	if (client->startup.is_cancel) {
		od_log(&pooler->od->log, "C: cancel request");
		od_feclose(client);
		return;
	}
	/* client auth */
	rc = od_feauth(client);
	if (rc == -1) {
		od_feclose(client);
		return;
	}

	/* get server connection */
	odserver_t *server = od_bepop(pooler, &client->startup);
	if (server == NULL) {
		od_feclose(client);
		return;
	}

	/* link server with client */

	while (1) {
		rc = od_read(client->io, &client->stream);
		if (rc == -1) {
			od_feclose(client);
			return;
		}
		char type = *client->stream.s;
		od_log(&pooler->od->log, "C: %c", type);

		/* write(server, packet) */
		while (1) {
			/* packet = read(server) */
			/* write(client, packet) */
			/* if Z break */
		}
	}
}
