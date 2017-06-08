
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
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_route_pool.h"
#include "od_io.h"

#include "od_router.h"
#include "od_pooler.h"
#include "od_relay.h"
#include "od_tls.h"
#include "od_frontend.h"

machine_tls_t
od_tls_frontend(od_scheme_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (scheme->tls_verify == OD_TALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (scheme->tls_verify == OD_TREQUIRE)
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
                       od_log_t *log,
                       od_scheme_t *scheme,
                       machine_tls_t tls)
{
	so_stream_t *stream = &client->stream;

	if (client->startup.is_ssl_request)
	{
		od_debug_client(log, client->id, "tls", "ssl request");
		so_stream_reset(stream);
		int rc;
		if (scheme->tls_verify == OD_TDISABLE) {
			/* not supported 'N' */
			so_stream_write8(stream, 'N');
			rc = od_write(client->io, stream);
			if (rc == -1) {
				od_error_client(log, client->id, "tls", "write error: %s",
				                machine_error(client->io));
				return -1;
			}
			od_log_client(log, client->id, "tls", "disabled, closing");
			od_frontend_error(client, SO_ERROR_FEATURE_NOT_SUPPORTED,
			                  "SSL is not supported");
			return -1;
		}
		/* supported 'S' */
		so_stream_write8(stream, 'S');
		rc = od_write(client->io, stream);
		if (rc == -1) {
			od_error_client(log, client->id, "tls", "write error: %s",
			                machine_error(client->io));
			return -1;
		}
		rc = machine_set_tls(client->io, tls);
		if (rc == -1) {
			od_error_client(log, client->id, "tls", "error: %s",
			                machine_error(client->io));
			return -1;
		}
		od_debug_client(log, client->id, "tls", "ok");
		return 0;
	}
	switch (scheme->tls_verify) {
	case OD_TDISABLE:
	case OD_TALLOW:
		break;
	default:
		od_log_client(log, client->id, "tls", "required, closing");
		od_frontend_error(client, SO_ERROR_PROTOCOL_VIOLATION,
		                  "SSL is required");
		return -1;
	}
	return 0;
}

machine_tls_t
od_tls_backend(od_schemeserver_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (scheme->tls_verify == OD_TALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (scheme->tls_verify == OD_TREQUIRE)
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
                       od_log_t *log,
                       od_schemeserver_t *scheme)
{
	so_stream_t *stream = &server->stream;

	od_debug_server(log, server->id, "tls", "init");

	/* SSL Request */
	so_stream_reset(stream);
	int rc;
	rc = so_fewrite_ssl_request(stream);
	if (rc == -1)
		return -1;
	rc = od_write(server->io, stream);
	if (rc == -1) {
		od_error_server(log, server->id, "tls", "write error: %s",
		                machine_error(server->io));
		return -1;
	}

	/* read server reply */
	so_stream_reset(stream);
	rc = machine_read(server->io, (char*)stream->p, 1, UINT32_MAX);
	if (rc < 0) {
		od_error_server(log, server->id, "tls", "read error: %s",
		                machine_error(server->io));
		return -1;
	}
	switch (*stream->p) {
	case 'S':
		/* supported */
		od_debug_server(log, server->id, "tls", "supported");
		rc = machine_set_tls(server->io, server->tls);
		if (rc == -1) {
			od_error_server(log, server->id, "tls", "error: %s",
			                machine_error(server->io));
			return -1;
		}
		od_debug_server(log, server->id, "tls", "ok");
		break;
	case 'N':
		/* not supported */
		if (scheme->tls_verify == OD_TALLOW) {
			od_debug_server(log, server->id, "tls", "not supported, continue (allow)");
		} else {
			od_error_server(log, server->id, "tls", "not supported, closing");
			return -1;
		}
		break;
	default:
		od_error_server(log, server->id, "tls", "unexpected status reply");
		return -1;
	}
	return 0;
}
