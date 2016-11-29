
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"

void od_schemeinit(od_scheme_t *scheme)
{
	scheme->config_file = NULL;
	scheme->daemonize = 0;
	scheme->log_file = NULL;
	scheme->pid_file = NULL;
	scheme->syslog = 0;
	scheme->syslog_ident = NULL;
	scheme->syslog_facility = NULL;
	scheme->host = NULL;
	scheme->port = 6432;
	scheme->backlog = 128;
	scheme->nodelay = 0;
	scheme->keepalive = 7200;
	scheme->workers = 1;
	scheme->client_max = 100;
	scheme->pooling = NULL;
	scheme->pooling_mode = OD_PUNDEF;
	scheme->routing = NULL;
	scheme->routing_mode = OD_RUNDEF;
	scheme->routing_default = NULL;
	scheme->server_id = 0;
	od_listinit(&scheme->servers);
	od_listinit(&scheme->routing_table);
}

void od_schemefree(od_scheme_t *scheme)
{
	od_list_t *i, *n;
	od_listforeach_safe(&scheme->servers, i, n) {
		od_schemeserver_t *server;
		server = od_container_of(i, od_schemeserver_t, link);
		free(server);
	}
	od_listforeach_safe(&scheme->routing_table, i, n) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		free(route);
	}
}

od_schemeserver_t*
od_schemeserver_add(od_scheme_t *scheme)
{
	od_schemeserver_t *s =
		(od_schemeserver_t*)malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	memset(s, 0, sizeof(*s));
	s->id = scheme->server_id++;
	od_listinit(&s->link);
	od_listappend(&scheme->servers, &s->link);
	return s;
}

od_schemeserver_t*
od_schemeserver_match(od_scheme_t *scheme, char *name)
{
	od_list_t *i;
	od_listforeach(&scheme->servers, i) {
		od_schemeserver_t *server;
		server = od_container_of(i, od_schemeserver_t, link);
		if (strcmp(server->name, name) == 0)
			return server;
	}
	return NULL;
}

od_schemeroute_t*
od_schemeroute_match(od_scheme_t *scheme, char *name)
{
	od_list_t *i;
	od_listforeach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (strcmp(route->target, name) == 0)
			return route;
	}
	return NULL;
}

od_schemeroute_t*
od_schemeroute_add(od_scheme_t *scheme)
{
	od_schemeroute_t *r =
		(od_schemeroute_t*)malloc(sizeof(*r));
	if (r == NULL)
		return NULL;
	memset(r, 0, sizeof(*r));
	od_listinit(&r->link);
	od_listappend(&scheme->routing_table, &r->link);
	return r;
}

int od_schemevalidate(od_scheme_t *scheme, od_log_t *log)
{
	/* pooling mode */
	if (scheme->pooling == NULL) {
		od_error(log, "pooling mode is not set");
		return -1;
	}
	if (strcmp(scheme->pooling, "session") == 0)
		scheme->pooling_mode = OD_PSESSION;
	else
	if (strcmp(scheme->pooling, "statement") == 0)
		scheme->pooling_mode = OD_PSTATEMENT;
	else
	if (strcmp(scheme->pooling, "transaction") == 0)
		scheme->pooling_mode = OD_PTRANSACTION;

	if (scheme->pooling_mode == OD_PUNDEF) {
		od_error(log, "unknown pooling mode");
		return -1;
	}

	/* routing mode */
	if (scheme->routing == NULL) {
		od_error(log, "routing mode is not set");
		return -1;
	}
	if (strcmp(scheme->routing, "forward") == 0)
		scheme->routing_mode = OD_RFORWARD;

	if (scheme->routing_mode == OD_RUNDEF) {
		od_error(log, "unknown routing mode");
		return -1;
	}

	/* listen */
	if (scheme->host == NULL)
		scheme->host = "127.0.0.1";

	/* servers */
	if (od_listempty(&scheme->servers)) {
		od_error(log, "no servers are defined");
		return -1;
	}
	od_list_t *i;
	od_listforeach(&scheme->servers, i) {
		od_schemeserver_t *server;
		server = od_container_of(i, od_schemeserver_t, link);
		if (server->host == NULL) {
			od_error(log, "server '%s': no host is specified",
			         server->name);
			return -1;
		}
	}

	od_schemeroute_t *default_route = NULL;

	/* routing table */
	od_listforeach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (route->route == NULL) {
			od_error(log, "route '%s': no route server is specified",
			         route->target);
			return -1;
		}
		route->server = od_schemeserver_match(scheme, route->route);
		if (route->server == NULL) {
			od_error(log, "route '%s': no route server '%s' found",
			         route->target);
			return -1;
		}
		if (route->is_default) {
			if (default_route) {
				od_error(log, "more than one default route");
				return -1;
			}
			default_route = route;
		}
	}
	scheme->routing_default = default_route;
	return 0;
}

void od_schemeprint(od_scheme_t *scheme, od_log_t *log)
{
	od_log(log, "using configuration file '%s'",
	       scheme->config_file);
	if (scheme->log_file)
		od_log(log, "log_file '%s'", scheme->log_file);
	if (scheme->pid_file)
		od_log(log, "pid_file '%s'", scheme->pid_file);
	if (scheme->syslog)
		od_log(log, "syslog %d", scheme->syslog);
	if (scheme->syslog_ident)
		od_log(log, "syslog_ident '%s'", scheme->syslog_ident);
	if (scheme->syslog_facility)
		od_log(log, "syslog_facility '%s'", scheme->syslog_facility);
	if (scheme->daemonize)
		od_log(log, "daemonize %s",
		       scheme->daemonize ? "yes" : "no");
	od_log(log, "");
	od_log(log, "pooling '%s'", scheme->pooling);
	od_log(log, "");
	od_log(log, "listen");
	od_log(log, "  host     '%s'", scheme->host);
	od_log(log, "  port      %d", scheme->port);
	od_log(log, "  backlog   %d", scheme->backlog);
	od_log(log, "  nodelay   %d", scheme->nodelay);
	od_log(log, "  keepalive %d", scheme->keepalive);
	od_log(log, "");
	od_log(log, "servers");
	od_list_t *i;
	od_listforeach(&scheme->servers, i) {
		od_schemeserver_t *server;
		server = od_container_of(i, od_schemeserver_t, link);
		od_log(log, "  <%s> %s",
		       server->name ? server->name : "",
		       server->is_default ? "default" : "");
		od_log(log, "    host '%s'", server->host);
		od_log(log, "    port  %d", server->port);

	}
	od_log(log, "");
	od_log(log, "routing");
	od_log(log, "  mode '%s'", scheme->routing);
	od_listforeach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		od_log(log, "  <%s>", route->target);
		od_log(log, "    route   '%s'", route->route);
		if (route->database)
			od_log(log, "    database '%s'", route->database);
		if (route->user)
			od_log(log, "    user '%s'", route->user);
		if (route->password)
			od_log(log, "    password '****'");
		od_log(log, "    ttl      %d", route->ttl);
		od_log(log, "    pool_min %d", route->pool_min);
		od_log(log, "    pool_max %d", route->pool_max);
	}
}
