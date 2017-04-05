
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
od_tls_client(od_pooler_t *pooler, od_scheme_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_create_tls(pooler->env);
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

machine_tls_t
od_tls_server(od_pooler_t *pooler, od_schemeserver_t *scheme)
{
	int rc;
	machine_tls_t tls;
	tls = machine_create_tls(pooler->env);
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
