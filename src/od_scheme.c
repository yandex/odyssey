
/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"

void od_schemeinit(odscheme_t *scheme)
{
	scheme->config_file = NULL;
	scheme->daemonize = 0;
	scheme->log_file = NULL;
	scheme->pid_file = NULL;
	scheme->host = "127.0.0.1";
	scheme->port = 6432;
	scheme->workers = 1;
	scheme->client_max = 100;
	scheme->routing = "forward";
	od_listinit(&scheme->servers);
	od_listinit(&scheme->routing_table);
}

void od_schemefree(odscheme_t *scheme)
{
	odlist_t *i, *n;
	od_listforeach_safe(&scheme->servers, i, n) {
		odscheme_server_t *server;
		server = od_container_of(i, odscheme_server_t, link);
		free(server);
	}
	od_listforeach_safe(&scheme->servers, i, n) {
		odscheme_route_t *route;
		route = od_container_of(i, odscheme_route_t, link);
		free(route);
	}
}

odscheme_server_t*
od_scheme_addserver(odscheme_t *scheme)
{
	odscheme_server_t *s =
		(odscheme_server_t*)malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	memset(s, 0, sizeof(*s));
	od_listinit(&s->link);
	od_listappend(&scheme->servers, &s->link);
	return s;
}

odscheme_route_t*
od_scheme_addroute(odscheme_t *scheme)
{
	odscheme_route_t *r =
		(odscheme_route_t*)malloc(sizeof(*r));
	if (r == NULL)
		return NULL;
	memset(r, 0, sizeof(*r));
	od_listinit(&r->link);
	od_listappend(&scheme->routing_table, &r->link);
	return r;
}

void od_schemeprint(odscheme_t *scheme, odlog_t *log)
{
	if (scheme->config_file)
		od_log(log, "using configuration file '%s'",
		       scheme->config_file);
	else
		od_log(log, "using default settings");
	if (scheme->log_file)
		od_log(log, "log_file '%s'", scheme->log_file);
	if (scheme->pid_file)
		od_log(log, "pid_file '%s'", scheme->pid_file);
	if (scheme->daemonize)
		od_log(log, "daemonize %s",
		       scheme->daemonize ? "yes" : "no");
	od_log(log, "pooling '%s'", scheme->pooling);
	od_log(log, "");
	od_log(log, "servers");
	odlist_t *i;
	od_listforeach(&scheme->servers, i) {
		odscheme_server_t *server;
		server = od_container_of(i, odscheme_server_t, link);
		od_log(log, "  <%s> %s",
		       server->name ? server->name : "",
		       server->is_default ? "default" : "");
		od_log(log, "    host '%s'", server->host);
		od_log(log, "    port '%d'", server->port);

	}
	od_log(log, "routing");
	od_log(log, "  mode '%s'", scheme->routing);
	od_listforeach(&scheme->routing_table, i) {
		odscheme_route_t *route;
		route = od_container_of(i, odscheme_route_t, link);
		od_log(log, "  >> %s", route->database);
		if (route->user)
			od_log(log, "    user '%s'", route->user);
		if (route->password)
			od_log(log, "    password '****'");
		od_log(log, "    pool_min %d", route->pool_min);
		od_log(log, "    pool_max %d", route->pool_max);
	}
}
