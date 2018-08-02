
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
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
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
#include "sources/stat.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/tls.h"
#include "sources/frontend.h"

machine_tls_t*
od_tls_frontend(od_configlisten_t *config)
{
	int rc;
	machine_tls_t *tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (config->tls_mode == OD_TLS_ALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (config->tls_mode == OD_TLS_REQUIRE)
		machine_tls_set_verify(tls, "peer");
	else
		machine_tls_set_verify(tls, "peer_strict");
	if (config->tls_ca_file) {
		rc = machine_tls_set_ca_file(tls, config->tls_ca_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (config->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, config->tls_cert_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (config->tls_key_file) {
		rc = machine_tls_set_key_file(tls, config->tls_key_file);
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
                       od_configlisten_t *config,
                       machine_tls_t *tls)
{
	shapito_stream_t *stream = client->stream;

	if (client->startup.is_ssl_request)
	{
		od_debug(logger, "tls", client, NULL, "ssl request");
		shapito_stream_reset(stream);
		int rc;
		if (config->tls_mode == OD_TLS_DISABLE) {
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
	switch (config->tls_mode) {
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
od_tls_backend(od_configstorage_t *config)
{
	int rc;
	machine_tls_t *tls;
	tls = machine_tls_create();
	if (tls == NULL)
		return NULL;
	if (config->tls_mode == OD_TLS_ALLOW)
		machine_tls_set_verify(tls, "none");
	else
	if (config->tls_mode == OD_TLS_REQUIRE)
		machine_tls_set_verify(tls, "peer");
	else
		machine_tls_set_verify(tls, "peer_strict");
	if (config->tls_ca_file) {
		rc = machine_tls_set_ca_file(tls, config->tls_ca_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (config->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, config->tls_cert_file);
		if (rc == -1) {
			machine_tls_free(tls);
			return NULL;
		}
	}
	if (config->tls_key_file) {
		rc = machine_tls_set_key_file(tls, config->tls_key_file);
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
                       shapito_stream_t *stream,
                       od_configstorage_t *config)
{
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
		if (config->tls_mode == OD_TLS_ALLOW) {
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
