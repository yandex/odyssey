
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
#include "od_tls.h"

machine_tls_t
od_tlsfe(machine_t machine, od_scheme_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_create_tls(machine);
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
			machine_free_tls(tls);
			return NULL;
		}
	}
	if (scheme->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, scheme->tls_cert_file);
		if (rc == -1) {
			machine_free_tls(tls);
			return NULL;
		}
	}
	if (scheme->tls_key_file) {
		rc = machine_tls_set_key_file(tls, scheme->tls_key_file);
		if (rc == -1) {
			machine_free_tls(tls);
			return NULL;
		}
	}
	return tls;
}

int
od_tlsfe_accept(machine_t machine,
                machine_io_t io,
                machine_tls_t tls,
                so_stream_t *stream,
                od_log_t *log,
                char *prefix,
                od_scheme_t *scheme,
                so_bestartup_t *startup)
{
	if (startup->is_ssl_request)
	{
		od_debug(log, io, "%s (tls): ssl request", prefix);
		so_stream_reset(stream);
		int rc;
		if (scheme->tls_verify == OD_TDISABLE) {
			/* not supported 'N' */
			so_stream_write8(stream, 'N');
			rc = od_write(io, stream);
			if (rc == -1) {
				od_error(log, io, "%s (tls): write error: %s",
				         prefix, machine_error(io));
				return -1;
			}
			od_log(log, io, "%s (tls): disabled, closing", prefix);
			return -1;
		}
		/* supported 'S' */
		so_stream_write8(stream, 'S');
		rc = od_write(io, stream);
		if (rc == -1) {
			od_error(log, io, "%s (tls): write error: %s",
			         prefix, machine_error(io));
			return -1;
		}
		rc = machine_set_tls(io, tls);
		if (rc == -1) {
			od_error(log, io, "%s (tls): error: %s", prefix,
			         machine_error(io));
			return -1;
		}
		od_debug(log, io, "%s (tls): ok", prefix);
		return 0;
	}
	switch (scheme->tls_verify) {
	case OD_TDISABLE:
	case OD_TALLOW:
		break;
	default:
		od_log(log, io, "%s (tls): required, closing", prefix);
		return -1;
	}

	(void)machine;
	return 0;
}

machine_tls_t
od_tlsbe(machine_t machine, od_schemeserver_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_create_tls(machine);
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
			machine_free_tls(tls);
			return NULL;
		}
	}
	if (scheme->tls_cert_file) {
		rc = machine_tls_set_cert_file(tls, scheme->tls_cert_file);
		if (rc == -1) {
			machine_free_tls(tls);
			return NULL;
		}
	}
	if (scheme->tls_key_file) {
		rc = machine_tls_set_key_file(tls, scheme->tls_key_file);
		if (rc == -1) {
			machine_free_tls(tls);
			return NULL;
		}
	}
	return tls;
}

int
od_tlsbe_connect(machine_t machine,
                 machine_io_t io,
                 machine_tls_t tls,
                 so_stream_t *stream,
                 od_log_t *log,
                 char *prefix,
                 od_schemeserver_t *scheme)
{
	od_debug(log, io, "%s (tls): init", prefix);

	/* SSL Request */
	so_stream_reset(stream);
	int rc;
	rc = so_fewrite_ssl_request(stream);
	if (rc == -1)
		return -1;
	rc = od_write(io, stream);
	if (rc == -1) {
		od_error(log, io, "%s (tls): write error: %s",
		         prefix, machine_error(io));
		return -1;
	}

	/* read server reply */
	so_stream_reset(stream);
	rc = machine_read(io, (char*)stream->p, 1, 0);
	if (rc < 0) {
		od_error(log, io, "%s (tls): read error: %s",
		         prefix, machine_error(io));
		return -1;
	}
	switch (*stream->p) {
	case 'S':
		/* supported */
		od_debug(log, io, "%s (tls): supported", prefix);
		rc = machine_set_tls(io, tls);
		if (rc == -1) {
			od_error(log, io, "%s (tls): %s", prefix,
			         machine_error(machine));
			return -1;
		}
		od_debug(log, io, "%s (tls): ok", prefix);
		break;
	case 'N':
		/* not supported */
		if (scheme->tls_verify == OD_TALLOW) {
			od_debug(log, io, "%s (tls): not supported, continue (allow)",
			         prefix);
		} else {
			od_error(log, io, "%s (tls): not supported, closing",
			         prefix);
			return -1;
		}
		break;
	default:
		od_error(log, io, "%s (tls): unexpected status reply",
		         prefix);
		return -1;
	}
	return 0;
}
