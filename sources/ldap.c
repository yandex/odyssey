/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

#define USE_POOL

od_retcode_t od_ldap_server_free(od_ldap_server_t *serv)
{
	od_list_unlink(&serv->link);
	/* free memory alloc from LDAP lib */
	if (serv->conn) {
		ldap_unbind(serv->conn);
	}

	if (serv->auth_user) {
		free(serv->auth_user);
	}
	free(serv);
	return OK_RESPONSE;
}

//#define LDAP_DBG

static inline od_retcode_t od_ldap_error_report_client(od_client_t *cl, int rc)
{
	switch (rc) {
	case LDAP_SUCCESS:
		return OK_RESPONSE;
	case LDAP_INVALID_CREDENTIALS:
		return NOT_OK_RESPONSE;
	case LDAP_UNAVAILABLE:
	case LDAP_UNWILLING_TO_PERFORM:
	case LDAP_BUSY: {
		od_frontend_error(cl, KIWI_SYNTAX_ERROR,
				  "LDAP auth failed: ldap server is down");
		return NOT_OK_RESPONSE;
	}
	case LDAP_INVALID_SYNTAX: {
		od_frontend_error(
			cl, KIWI_SYNTAX_ERROR,
			"LDAP auth failed: invalid attribute value was specified");
		return NOT_OK_RESPONSE;
	}
	default: {
#ifdef LDAP_DBG
		od_frontend_error(cl, KIWI_SYNTAX_ERROR,
				  "LDAP auth failed: %s %d",
				  ldap_err2string(rc), rc);
#else
		od_frontend_error(cl, KIWI_SYNTAX_ERROR,
				  "LDAP auth failed: unknown error");
#endif
		return NOT_OK_RESPONSE;
	}
	}
}

static inline int od_init_ldap_conn(LDAP **l, char *uri)
{
	od_retcode_t rc = ldap_initialize(l, uri);
	if (rc != LDAP_SUCCESS) {
		// set to null, we do not need to unbind this
		// ldap_initialize frees assosated memory
		*l = NULL;
		return NOT_OK_RESPONSE;
	}

	int ldapversion = LDAP_VERSION3;
	rc = ldap_set_option(*l, LDAP_OPT_PROTOCOL_VERSION, &ldapversion);

	if (rc != LDAP_SUCCESS) {
		// same as above
		*l = NULL;
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

od_retcode_t od_ldap_endpoint_prepare(od_ldap_endpoint_t *le)
{
	const char *scheme;
	scheme = le->ldapscheme; // ldap or ldaps
	if (scheme == NULL) {
		scheme = "ldap";
	}

	le->ldapurl = NULL;
	if (!le->ldapserver) {
		// TODO: support mulitple ldap servers
		return NOT_OK_RESPONSE;
	}

	if (od_asprintf(&le->ldapurl, "%s://%s:%d", scheme, le->ldapserver,
			le->ldapport) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

static inline od_retcode_t od_ldap_server_prepare(od_logger_t *logger,
						  od_ldap_server_t *serv,
						  od_client_t *client)
{
	od_retcode_t rc;
	char *auth_user = NULL;

	if (serv->endpoint->ldapbasedn) {
		// copy pasted from ./src/backend/libpq/auth.c:2635
		char *filter;
		LDAPMessage *search_message;
		LDAPMessage *entry;
		char *attributes[] = { LDAP_NO_ATTRS, NULL };
		char *dn;
		int count;

		rc = ldap_simple_bind_s(serv->conn,
					serv->endpoint->ldapbinddn ?
						serv->endpoint->ldapbinddn :
						"",
					serv->endpoint->ldapbindpasswd ?
						serv->endpoint->ldapbindpasswd :
						"");

		od_debug(logger, "auth_ldap", NULL, NULL,
			 "basednn simple bind result: %d", rc);

		if (rc != LDAP_SUCCESS) {
			return NOT_OK_RESPONSE;
		}

		/* Build a custom filter or a single attribute filter? */
		if (serv->endpoint->ldapsearchfilter) {
			// TODO: support;
			return NOT_OK_RESPONSE;
		} else if (serv->endpoint->ldapsearchattribute) {
			od_asprintf(&filter, "(%s=%s)",
				    serv->endpoint->ldapsearchattribute,
				    client->startup.user.value);
		} else {
			od_asprintf(&filter, "(uid=%s)",
				    client->startup.user.value);
		}

		rc = ldap_search_s(serv->conn, serv->endpoint->ldapbasedn,
				   LDAP_SCOPE_SUBTREE, filter, attributes, 0,
				   &search_message);

		od_debug(logger, "auth_ldap", NULL, NULL,
			 "basednn search result: %d", rc);

		if (rc != LDAP_SUCCESS) {
			free(filter);
			return NOT_OK_RESPONSE;
		}

		count = ldap_count_entries(serv->conn, search_message);
		od_debug(logger, "auth_ldap", NULL, NULL,
			 "basedn search entries count: %d", count);
		if (count != 1) {
			if (count == 0) {
				// TODO: report err 2 client
			} else {
			}

			free(filter);
			ldap_msgfree(search_message);
			return NOT_OK_RESPONSE;
		}

		entry = ldap_first_entry(serv->conn, search_message);
		dn = ldap_get_dn(serv->conn, entry);
		if (dn == NULL) {
			// TODO: report err
			return NOT_OK_RESPONSE;
		}

		auth_user = strdup(dn);

		free(filter);
		ldap_memfree(dn);
		ldap_msgfree(search_message);

	} else {
		od_asprintf(&auth_user, "%s%s%s",
			    serv->endpoint->ldapprefix ?
				    serv->endpoint->ldapprefix :
				    "",
			    client->startup.user.value,
			    serv->endpoint->ldapsuffix ?
				    serv->endpoint->ldapsuffix :
				    "");
	}

	serv->auth_user = auth_user;

	return OK_RESPONSE;
}

od_ldap_server_t *od_ldap_server_allocate()
{
	od_ldap_server_t *serv = malloc(sizeof(od_ldap_server_t));
	serv->conn = NULL;
	serv->endpoint = NULL;
	serv->auth_user = NULL;

	return serv;
}

static inline od_retcode_t od_ldap_server_init(od_logger_t *logger,
					       od_ldap_server_t *server,
					       od_route_t *route,
					       od_client_t *client)
{
	od_id_generate(&server->id, "ls");
	od_list_init(&server->link);

	server->global = NULL;
	server->route = route;

	od_ldap_endpoint_t *le = route->rule->ldap_endpoint;
	server->endpoint = le;

	if (od_init_ldap_conn(&server->conn, le->ldapurl) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	if (od_ldap_server_prepare(logger, server, client) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

static inline int od_ldap_server_auth(od_ldap_server_t *serv, od_client_t *cl,
				      kiwi_password_t *tok)
{
	int rc;
	rc = ldap_simple_bind_s(serv->conn, serv->auth_user, tok->password);

	od_route_t *route = cl->route;
	if (route->rule->client_fwd_error) {
		od_ldap_error_report_client(cl, rc);
	}

	return rc;
}

static inline od_ldap_server_t *od_ldap_server_attach(od_route_t *route,
						      od_client_t *client,
						      bool wait_for_idle)
{
	assert(route != NULL);
	od_instance_t *instance = client->global->instance;
	od_logger_t *logger = &instance->logger;

	od_ldap_server_t *server = NULL;
	od_route_lock(route);

#ifdef USE_POOL
	od_retcode_t rc;

	/* get client server from route server pool */
	for (;;) {
		server = od_ldap_server_pool_next(&route->ldap_pool,
						  OD_SERVER_IDLE);
		if (server) {
			break;
		}

		if (wait_for_idle) {
			/* special case, when we are interested only in an idle connection
			 * and do not want to start a new one */
			// NOT IMPL
			od_route_unlock(route);
			return NULL;
		} else {
			/* Maybe start new connection, if pool_size is zero */
			/* Maybe start new connection, if we still have capacity for it */

			int connections_in_pool =
				od_server_pool_total(&route->ldap_pool);
			int pool_size = route->rule->ldap_pool_size;

			if (pool_size == 0 || connections_in_pool < pool_size) {
				// TODO: better limit logic here
				// We are allowed to spun new server connection
				od_debug(
					logger, "auth_ldap", NULL, NULL,
					"spun new connection to ldap server %s",
					route->rule->ldap_endpoint_name);
				break;
			}
		}

		/*
		 * Wait wakeup condition for pool_timeout milliseconds.
		 *
		 * The condition triggered when a server connection
		 * put into idle state by DETACH events.
		 */
		od_route_unlock(route);

		uint32_t timeout = route->rule->ldap_pool_timeout;
		if (timeout == 0)
			timeout = UINT32_MAX;
		rc = od_route_wait(route, timeout);

		if (rc == -1) {
			return NULL;
		}

		od_route_lock(route);
	}
#endif

	if (server == NULL) {
		od_route_unlock(route);

		/* create new server object */
		server = od_ldap_server_allocate();

		int ldap_rc =
			od_ldap_server_init(logger, server, route, client);

		od_route_lock(route);
		od_ldap_server_pool_set(&route->ldap_pool, server,
					OD_SERVER_UNDEF);
		od_route_unlock(route);

		if (ldap_rc != LDAP_SUCCESS) {
			if (route->rule->client_fwd_error) {
				od_ldap_error_report_client(client, ldap_rc);
			}
			od_ldap_server_free(server);
			return NULL;
		}

		od_route_lock(route);
	}

	od_ldap_server_pool_set(&route->ldap_pool, server, OD_SERVER_ACTIVE);

	od_route_unlock(route);

	return server;
}

od_retcode_t od_auth_ldap(od_client_t *cl, kiwi_password_t *tok)
{
	od_route_t *route = cl->route;
	od_instance_t *instance = cl->global->instance;

	od_debug(&instance->logger, "auth_ldap", cl, NULL,
		 "%d connections are currently issued to ldap",
		 od_server_pool_active(&route->ldap_pool));

	od_ldap_server_t *serv = od_ldap_server_attach(route, cl, false);
	if (serv == NULL) {
		od_debug(&instance->logger, "auth_ldap", cl, NULL,
			 "failed to get ldap connection");
		return NOT_OK_RESPONSE;
	}

	int ldap_rc = od_ldap_server_auth(serv, cl, tok);

	switch (ldap_rc) {
	case LDAP_SUCCESS: {
#ifndef USE_POOL
		od_ldap_conn_close(route, serv);
#else
		od_route_lock(route);
		od_ldap_server_pool_set(&route->ldap_pool, serv,
					OD_SERVER_IDLE);
		od_route_unlock(route);
#endif
		return OK_RESPONSE;
	}
	case LDAP_INVALID_SYNTAX:
	case LDAP_INVALID_CREDENTIALS:
#ifndef USE_POOL
		od_ldap_conn_close(route, serv);
#else
		od_route_lock(route);
		od_ldap_server_pool_set(&route->ldap_pool, serv,
					OD_SERVER_IDLE);
		od_route_unlock(route);
#endif
		return NOT_OK_RESPONSE;
	default:
#ifndef USE_POOL
		od_ldap_conn_close(route, serv);
#else
		od_route_lock(route);
		/*Need to rebind */
		od_ldap_server_pool_set(&route->ldap_pool, serv,
					OD_SERVER_UNDEF);
		od_route_unlock(route);
#endif
		return NOT_OK_RESPONSE;
	}
}

od_retcode_t od_ldap_conn_close(od_attribute_unused() od_route_t *route,
				od_ldap_server_t *server)
{
	ldap_unbind(server->conn);
	od_list_unlink(&server->link);

	return OK_RESPONSE;
}

//----------------------------------------------------------------------------------------

/* ldap endpoints ADD/REMOVE API */
od_ldap_endpoint_t *od_ldap_endpoint_alloc()
{
	od_ldap_endpoint_t *le = malloc(sizeof(od_ldap_endpoint_t));
	if (le == NULL) {
		return NULL;
	}
	od_list_init(&le->link);

	le->name = NULL;

	le->ldapserver = NULL;
	le->ldapport = 0;

	le->ldapscheme = NULL;

	le->ldapprefix = NULL;
	le->ldapsuffix = NULL;
	le->ldapbindpasswd = NULL;
	le->ldapsearchfilter = NULL;
	le->ldapsearchattribute = NULL;
	le->ldapscope = NULL;
	le->ldapbasedn = NULL;
	le->ldapbinddn = NULL;
	// preparsed connect url
	le->ldapurl = NULL;

	return le;
}

od_retcode_t od_ldap_endpoint_free(od_ldap_endpoint_t *le)
{
	if (le->name) {
		free(le->name);
	}

	if (le->ldapserver) {
		free(le->ldapserver);
	}
	if (le->ldapscheme) {
		free(le->ldapscheme);
	}

	if (le->ldapprefix) {
		free(le->ldapprefix);
	}
	if (le->ldapsuffix) {
		free(le->ldapsuffix);
	}
	if (le->ldapbindpasswd) {
		free(le->ldapbindpasswd);
	}
	if (le->ldapsearchfilter) {
		free(le->ldapsearchfilter);
	}
	if (le->ldapsearchattribute) {
		free(le->ldapsearchattribute);
	}
	if (le->ldapscope) {
		free(le->ldapscope);
	}
	if (le->ldapbasedn) {
		free(le->ldapbasedn);
	}
	// preparsed connect url
	if (le->ldapurl) {
		free(le->ldapurl);
	}

	od_list_unlink(&le->link);

	free(le);

	return OK_RESPONSE;
}

od_retcode_t od_ldap_endpoint_add(od_ldap_endpoint_t *ldaps,
				  od_ldap_endpoint_t *target)
{
	od_list_t *i;

	od_list_foreach(&(ldaps->link), i)
	{
		od_ldap_endpoint_t *s =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(s->name, target->name) == 0) {
			/* already loaded */
			return NOT_OK_RESPONSE;
		}
	}

	od_list_append(&ldaps->link, &target->link);

	return OK_RESPONSE;
}

od_ldap_endpoint_t *od_ldap_endpoint_find(od_list_t *ldaps, char *name)
{
	od_list_t *i;

	od_list_foreach(ldaps, i)
	{
		od_ldap_endpoint_t *serv =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(serv->name, name) == 0) {
			return serv;
		}
	}

	/* target ldap server was not found */
	return NULL;
}

od_retcode_t od_ldap_endpoint_remove(od_ldap_endpoint_t *ldaps,
				     od_ldap_endpoint_t *target)
{
	od_list_t *i;

	od_list_foreach(&ldaps->link, i)
	{
		od_ldap_endpoint_t *serv =
			od_container_of(i, od_ldap_endpoint_t, link);
		if (strcmp(serv->name, target->name) == 0) {
			od_list_unlink(&target->link);
			return OK_RESPONSE;
		}
	}

	/* target ldap server was not found */
	return NOT_OK_RESPONSE;
}
