
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/error.h"
#include "sources/atomic.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/tls.h"
#include "sources/frontend.h"

machine_tls_t*
od_tls_frontend(od_schemelisten_t *scheme)
{
	int rc;
	machine_tls_t *tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (scheme->tls_mode == OD_TLS_ALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (scheme->tls_mode == OD_TLS_REQUIRE)
		machine_tls_set_verify(tls, "peer");
	else
		machine_tls_set_verify(tls, "peer_strict");
	if (scheme->tls_ca_file) {
		rc = machine_tls_set_ca_file(tls, scheme->tls_ca_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (scheme->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, scheme->tls_cert_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (scheme->tls_key_file) {
		rc = machine_tls_set_key_file(tls, scheme->tls_key_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	return tls;
}

int
od_tls_frontend_accept(od_client_t *client,
                       od_logger_t *logger,
                       od_schemelisten_t *scheme,
                       machine_tls_t *tls)
{
	shapito_stream_t *stream = &client->stream;

	if (client->startup.is_ssl_request)
	{
		od_debug(logger, "tls", client, NULL, "ssl request");
		shapito_stream_reset(stream);
		int rc;
		if (scheme->tls_mode == OD_TLS_DISABLE) {
			/* not supported 'N' */
			shapito_stream_write8(stream, 'N');
			rc = od_write(client->io, stream);
			if (rc == -1) {
				od_error(logger, "tls", client, NULL, "write error: %s",
				         machine_error(client->io));
				return -1;
			}
			od_debug(logger, "tls", client, NULL, "is disabled, ignoring");
			return 0;
		}
		/* supported 'S' */
		shapito_stream_write8(stream, 'S');
		rc = od_write(client->io, stream);
		if (rc == -1) {
			od_error(logger, "tls", client, NULL, "write error: %s",
			         machine_error(client->io));
			return -1;
		}
		rc = machine_set_tls(client->io, tls);
		if (rc == -1) {
			od_error(logger, "tls", client, NULL, "error: %s",
			         machine_error(client->io));
			return -1;
		}
		od_debug(logger, "tls", client, NULL, "ok");
		return 0;
	}
	switch (scheme->tls_mode) {
	case OD_TLS_DISABLE:
	case OD_TLS_ALLOW:
		break;
	default:
		od_log(logger, "tls", client, NULL, "required, closing");
		od_frontend_error(client, SHAPITO_PROTOCOL_VIOLATION,
		                  "SSL is required");
		return -1;
	}
	return 0;
}

machine_tls_t*
od_tls_backend(od_schemestorage_t *scheme)
{
	int rc;
	machine_tls_t *tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (scheme->tls_mode == OD_TLS_ALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (scheme->tls_mode == OD_TLS_REQUIRE)
		machine_tls_set_verify(tls, "peer");
	else
		machine_tls_set_verify(tls, "peer_strict");
	if (scheme->tls_ca_file) {
		rc = machine_tls_set_ca_file(tls, scheme->tls_ca_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (scheme->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, scheme->tls_cert_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (scheme->tls_key_file) {
		rc = machine_tls_set_key_file(tls, scheme->tls_key_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	return tls;
}

int
od_tls_backend_connect(od_server_t *server,
                       od_logger_t *logger,
                       od_schemestorage_t *scheme)
{
	shapito_stream_t *stream = &server->stream;

	od_debug(logger, "tls", NULL, server, "init");

	/* SSL Request */
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_fe_write_ssl_request(stream);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error(logger, "tls", NULL, server, "write error: %s",
		         machine_error(server->io));
		return -1;
	}

	/* read server reply */
	shapito_stream_reset(stream);
	rc = machine_read(server->io, stream->pos, 1, UINT32_MAX);
	if (rc == -1) {
		od_error(logger, "tls", NULL, server, "read error: %s",
		         machine_error(server->io));
		return -1;
	}
	switch (*stream->pos) {
	case 'S':
		/* supported */
		od_debug(logger, "tls", NULL, server, "supported");
		rc = machine_set_tls(server->io, server->tls);
		if (rc == -1) {
			od_error(logger, "tls", NULL, server, "error: %s",
			         machine_error(server->io));
			return -1;
		}
		od_debug(logger, "tls", NULL, server, "ok");
		break;
	case 'N':
		/* not supported */
		if (scheme->tls_mode == OD_TLS_ALLOW) {
			od_debug(logger, "tls", NULL, server,
			         "not supported, continue (allow)");
		} else {
			od_error(logger, "tls", NULL, server, "not supported, closing");
			return -1;
		}
		break;
	default:
		od_error(logger, "tls", NULL, server, "unexpected status reply");
		return -1;
	}
	return 0;
}
