
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

int od_deploy(od_client_t *client, char *context)
{
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
	od_server_t *server = client->server;

	machine_msg_t *msg;
	int rc;

	/* discard */
	int query_count = 0;
	if (route->rule->pool_discard) {
		char discard[] = "DISCARD ALL";
		msg = kiwi_fe_write_query(discard, sizeof(discard));
		if (msg == NULL)
			return -1;
		rc = machine_write(server->io, msg);
		if (rc == -1)
			return -1;
		query_count++;
	}

	/* compare and set options which are differs from server */
	char query[512];
	int  query_size;
	query_size = kiwi_vars_cas(&client->vars, &server->vars, query,
	                           sizeof(query) - 1);
	if (query_size > 0)
	{
		query[query_size] = 0;
		query_size++;
		msg = kiwi_fe_write_query(query, query_size);
		if (msg == NULL)
			return -1;
		rc = machine_write(server->io, msg);
		if (rc == -1)
			return -1;
		query_count++;
	}

	if (route->rule->pool_discard || query_size > 0) {
		od_debug(&instance->logger, context, client, server,
		         "deploy: %s%.*s",
		        (route->rule->pool_discard) ? "DISCARD; " : "",
		         query_size, query);
	}

	return query_count;
}
