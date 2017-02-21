
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
#include "od_router.h"
#include "od_router_session.h"
#include "od_router_transaction.h"
#include "od_router_copy.h"
#include "od_cancel.h"
#include "od_auth.h"
#include "od_fe.h"
#include "od_be.h"

od_routerstatus_t
od_router_copy_in(od_client_t *client)
{
	od_pooler_t *pooler = client->pooler;
	od_server_t *server = client->server;
	assert(! server->is_copy);
	server->is_copy = 1;

	int rc, type;
	so_stream_t *stream = &client->stream;
	for (;;) {
		rc = od_read(client->io, stream, 0);
		if (rc == -1)
			return OD_RS_ECLIENT_READ;
		type = *stream->s;
		od_debug(&pooler->od->log, client->io, "C (copy): %c", *stream->s);

		rc = od_write(server->io, stream);
		if (rc == -1)
			return OD_RS_ESERVER_WRITE;

		/* copy complete or fail */
		if (type == 'c' || type == 'f')
			break;
	}

	server->is_copy = 0;
	return OD_RS_OK;
}
