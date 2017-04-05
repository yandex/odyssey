
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
#include "od_pooler.h"
#include "od_periodic.h"
#include "od_router.h"

static int
od_pooler_tls_init(od_pooler_t *pooler)
{
	od_scheme_t *scheme = &pooler->od->scheme;
	int rc;
	pooler->tls = NULL;
	if (scheme->tls_verify == OD_TDISABLE)
		return 0;
	pooler->tls = machine_create_tls(pooler->env);
	if (pooler->tls == NULL)
		return -1;
	if (scheme->tls_verify == OD_TALLOW)
		machine_tls_set_verify(pooler->tls, "none");
	else
	if (scheme->tls_verify == OD_TREQUIRE)
		machine_tls_set_verify(pooler->tls, "peer");
	else
		machine_tls_set_verify(pooler->tls, "peer_strict");

	if (scheme->tls_ca_file) {
		rc = machine_tls_set_ca_file(pooler->tls, scheme->tls_ca_file);
		if (rc == -1)
			goto error;
	}
	if (scheme->tls_cert_file) {
		rc = machine_tls_set_cert_file(pooler->tls, scheme->tls_cert_file);
		if (rc == -1)
			goto error;
	}
	if (scheme->tls_key_file) {
		rc = machine_tls_set_key_file(pooler->tls, scheme->tls_key_file);
		if (rc == -1)
			goto error;
	}
	return 0;
error:
	machine_free_tls(pooler->tls);
	pooler->tls = NULL;
	return -1;
}

static inline void
od_pooler(void *arg)
{
	od_pooler_t *pooler = arg;
	od_t *env = pooler->od;

	/* init pooler tls */
	int rc;
	rc = od_pooler_tls_init(pooler);
	if (rc == -1)
		return;

	/* listen '*' */
	struct addrinfo *hints_ptr = NULL;
	struct addrinfo  hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	char *host = env->scheme.host;
	if (strcmp(env->scheme.host, "*") == 0) {
		hints_ptr = &hints;
		host = NULL;
	}

	/* resolve listen address and port */
	char port[16];
	snprintf(port, sizeof(port), "%d", env->scheme.port);
	struct addrinfo *ai = NULL;
	rc = machine_getaddrinfo(pooler->server, host, port,
	                         hints_ptr, &ai, 0);
	if (rc < 0) {
		od_error(&env->log, NULL, "failed to resolve %s:%d",
		         env->scheme.host,
		         env->scheme.port);
		return;
	}
	assert(ai != NULL);

	/* bind to listen address and port */
	rc = machine_bind(pooler->server, ai->ai_addr);
	freeaddrinfo(ai);
	if (rc < 0) {
		od_error(&env->log, NULL, "bind %s:%d failed",
		         env->scheme.host,
		         env->scheme.port);
		return;
	}

	/* starting periodic task scheduler fiber */
	rc = machine_create_fiber(pooler->env, od_periodic, pooler);
	if (rc < 0) {
		od_error(&env->log, NULL, "failed to create periodic fiber");
		return;
	}

	od_log(&env->log, NULL, "listening on %s:%d",
	       env->scheme.host, env->scheme.port);
	od_log(&env->log, NULL, "");

	/* accept loop */
	while (machine_active(pooler->env))
	{
		machine_io_t client_io;
		rc = machine_accept(pooler->server, env->scheme.backlog, &client_io);
		if (rc < 0) {
			od_error(&env->log, NULL, "accept failed");
			continue;
		}
		if (pooler->client_list.count >= env->scheme.client_max) {
			od_log(&pooler->od->log, client_io,
			       "C: pooler client_max reached (%d), closing connection",
			       env->scheme.client_max);
			machine_close(client_io);
			continue;
		}
		machine_set_nodelay(client_io, env->scheme.nodelay);
		if (env->scheme.keepalive > 0)
			machine_set_keepalive(client_io, 1, env->scheme.keepalive);
		od_client_t *client = od_clientalloc();
		if (client == NULL) {
			od_error(&env->log, NULL, "failed to allocate client object");
			machine_close(client_io);
			continue;
		}
		int64_t id_fiber;
		id_fiber = machine_create_fiber(pooler->env, od_router, client);
		if (id_fiber < 0) {
			od_error(&env->log, NULL, "failed to create client fiber");
			machine_close(client_io);
			od_clientfree(client);
			continue;
		}
		client->id_fiber = id_fiber;
		client->id = pooler->client_seq++;
		client->pooler = pooler;
		client->io = client_io;
		od_clientlist_add(&pooler->client_list, client);
	}
}

int od_pooler_init(od_pooler_t *pooler, od_t *od)
{
	pooler->env = machine_create();
	if (pooler->env == NULL)
		return -1;
	pooler->server = machine_create_io(pooler->env);
	if (pooler->server == NULL) {
		machine_free(pooler->env);
		return -1;
	}
	pooler->client_seq = 0;
	pooler->od = od;
	od_routepool_init(&pooler->route_pool);
	od_clientlist_init(&pooler->client_list);
	return 0;
}

int od_pooler_start(od_pooler_t *pooler)
{
	int rc;
	rc = machine_create_fiber(pooler->env, od_pooler, pooler);
	if (rc < 0) {
		od_error(&pooler->od->log, NULL,
		         "failed to create pooler fiber");
		return -1;
	}
	machine_start(pooler->env);
	return 0;
}
