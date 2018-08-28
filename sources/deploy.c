
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

static inline int
od_deploy_add(kiwi_params_t *params, char *query, int size,
              char *name, int name_len)
{
	kiwi_param_t *client_param;
	client_param = kiwi_params_find(params, name, name_len);
	if (client_param == NULL)
		return 0;
	char quote_value[256];
	int rc;
	rc = kiwi_enquote(kiwi_param_value(client_param), quote_value,
	                  sizeof(quote_value));
	if (rc == -1)
		return 0;
	return od_snprintf(query, size, "SET %s=%s;",
	                   kiwi_param_name(client_param),
	                   quote_value);
}

int
od_deploy_write(od_server_t *server, char *context, kiwi_params_t *params)
{
	od_instance_t *instance = server->global->instance;

	/* discard */
	int  query_count = 1;
	char query_discard[] = "DISCARD ALL";

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(query_discard, sizeof(query_discard));
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(server->io, msg);
	if (rc == -1)
		return -1;

	/* parameters */
	char query[512];
	int  size = 0;
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "TimeZone", 9);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "DateStyle", 10);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "client_encoding", 16);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "application_name", 17);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "extra_float_digits", 19);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "standard_conforming_strings", 28);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "statement_timeout", 18);
	size += od_deploy_add(params, query + size, sizeof(query) - size,
	                      "search_path", 12);
	if (size == 0) {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", "no need to configure");
	} else {
		od_debug(&instance->logger, context, server->client, server,
		         "%s", query);
		size++;
		query_count++;
		msg = kiwi_fe_write_query(query, size);
		if (msg == NULL)
			return -1;
		rc = machine_write(server->io, msg);
		if (rc == -1)
			return -1;
	}

	return query_count;
}
