
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int od_auth_query(od_client_t *client, char *peer)
{
	od_global_t *global = client->global;
	od_rule_t *rule = client->rule;
	kiwi_var_t *user = &client->startup.user;
	kiwi_password_t *password = &client->password;
	od_instance_t *instance = global->instance;
	od_router_t *router = global->router;

	/* create internal auth client */
	od_client_t *auth_client;
	auth_client = od_client_allocate();
	if (auth_client == NULL)
		return -1;
	auth_client->global = global;
	auth_client->type = OD_POOL_CLIENT_INTERNAL;
	od_id_generate(&auth_client->id, "a");

	/* set auth query route user and database */
	kiwi_var_set(&auth_client->startup.user, KIWI_VAR_UNDEF,
		     rule->auth_query_user, strlen(rule->auth_query_user) + 1);

	kiwi_var_set(&auth_client->startup.database, KIWI_VAR_UNDEF,
		     rule->auth_query_db, strlen(rule->auth_query_db) + 1);

	/* route */
	od_router_status_t status;
	status = od_router_route(router, auth_client);
	if (status != OD_ROUTER_OK) {
		od_client_free(auth_client);
		return -1;
	}

	/* attach */
	status = od_router_attach(router, auth_client, false);
	if (status != OD_ROUTER_OK) {
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		return -1;
	}
	od_server_t *server;
	server = auth_client->server;

	od_debug(&instance->logger, "auth_query", NULL, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	/* connect to server, if necessary */
	int rc;
	if (server->io.io == NULL) {
		rc = od_backend_connect(server, "auth_query", NULL, NULL);
		if (rc == -1) {
			od_router_close(router, auth_client);
			od_router_unroute(router, auth_client);
			od_client_free(auth_client);
			return -1;
		}
	}

	/* preformat and execute query */
	char query[OD_QRY_MAX_SZ];
	char *format_pos = rule->auth_query;
	char *format_end = rule->auth_query + strlen(rule->auth_query);
	od_query_format(format_pos, format_end, user, peer, query, sizeof(query));

	rc = od_query_do(server, query, user, password);
	if (rc == -1) {
		od_router_close(router, auth_client);
		od_router_unroute(router, auth_client);
		od_client_free(auth_client);
		return -1;
	}

	/* detach and unroute */
	od_router_detach(router, auth_client);
	od_router_unroute(router, auth_client);
	od_client_free(auth_client);
	return 0;
}
