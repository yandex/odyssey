
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <machinarium.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_id.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"

void od_scheme_init(od_scheme_t *scheme)
{
	scheme->config_file = NULL;
	scheme->daemonize = 0;
	scheme->log_debug = 0;
	scheme->log_config = 0;
	scheme->log_file = NULL;
	scheme->pid_file = NULL;
	scheme->syslog = 0;
	scheme->syslog_ident = NULL;
	scheme->syslog_facility = NULL;
	scheme->stats_period = 0;
	scheme->host = NULL;
	scheme->port = 6432;
	scheme->backlog = 128;
	scheme->nodelay = 1;
	scheme->keepalive = 7200;
	scheme->readahead = 8192;
	scheme->server_pipelining = 32768;
	scheme->workers = 1;
	scheme->client_max_set = 0;
	scheme->client_max = 0;
	scheme->tls_verify = OD_TDISABLE;
	scheme->tls_mode = NULL;
	scheme->tls_ca_file = NULL;
	scheme->tls_key_file = NULL;
	scheme->tls_cert_file = NULL;
	scheme->tls_protocols = NULL;
	scheme->routing_default = NULL;
	scheme->users_default = NULL;
	od_list_init(&scheme->storages);
	od_list_init(&scheme->routing_table);
	od_list_init(&scheme->users);
}

void od_scheme_free(od_scheme_t *scheme)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&scheme->storages, i, n) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		free(storage);
	}
	od_list_foreach_safe(&scheme->routing_table, i, n) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		free(route);
	}
	od_list_foreach_safe(&scheme->users, i, n) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		free(user);
	}
}

od_schemestorage_t*
od_schemestorage_add(od_scheme_t *scheme)
{
	od_schemestorage_t *storage;
	storage = (od_schemestorage_t*)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	od_list_init(&storage->link);
	od_list_append(&scheme->storages, &storage->link);
	return storage;
}

od_schemestorage_t*
od_schemestorage_match(od_scheme_t *scheme, char *name)
{
	od_list_t *i;
	od_list_foreach(&scheme->storages, i) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		if (strcmp(storage->name, name) == 0)
			return storage;
	}
	return NULL;
}

od_schemeroute_t*
od_schemeroute_match(od_scheme_t *scheme, char *name)
{
	od_list_t *i;
	od_list_foreach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (strcmp(route->target, name) == 0)
			return route;
	}
	return NULL;
}

static inline void
od_schemeroute_init(od_schemeroute_t *route)
{
	route->client_max_set = 0;
	route->client_max = 0;
	route->pool_size = 100;
	route->cancel = 1;
	route->discard = 1;
	route->rollback = 1;
	route->pool_mode_sz = NULL;
	route->pool_mode = OD_PUNDEF;
}

static inline void
od_schemeuser_init(od_schemeuser_t *user)
{
	user->auth_mode = OD_AUNDEF;
	user->auth = NULL;
}

od_schemeroute_t*
od_schemeroute_add(od_scheme_t *scheme)
{
	od_schemeroute_t *r =
		(od_schemeroute_t*)malloc(sizeof(*r));
	if (r == NULL)
		return NULL;
	memset(r, 0, sizeof(*r));
	od_schemeroute_init(r);
	od_list_init(&r->link);
	od_list_append(&scheme->routing_table, &r->link);
	return r;
}

od_schemeuser_t*
od_schemeuser_add(od_scheme_t *scheme)
{
	od_schemeuser_t *user =
		(od_schemeuser_t*)malloc(sizeof(*user));
	if (user == NULL)
		return NULL;
	memset(user, 0, sizeof(*user));
	od_schemeuser_init(user);
	od_list_init(&user->link);
	od_list_append(&scheme->users, &user->link);
	return user;
}

od_schemeuser_t*
od_schemeuser_match(od_scheme_t *scheme, char *name)
{
	od_list_t *i;
	od_list_foreach(&scheme->users, i) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		if (strcmp(user->user, name) == 0)
			return user;
	}
	return NULL;
}

int od_scheme_validate(od_scheme_t *scheme, od_log_t *log)
{
	/* workers */
	if (scheme->workers == 0) {
		od_error(log, "config", "bad workers number");
		return -1;
	}

	/* listen */
	if (scheme->host == NULL)
		scheme->host = "*";

	/* tls */
	if (scheme->tls_mode) {
		if (strcmp(scheme->tls_mode, "disable") == 0) {
			scheme->tls_verify = OD_TDISABLE;
		} else
		if (strcmp(scheme->tls_mode, "allow") == 0) {
			scheme->tls_verify = OD_TALLOW;
		} else
		if (strcmp(scheme->tls_mode, "require") == 0) {
			scheme->tls_verify = OD_TREQUIRE;
		} else
		if (strcmp(scheme->tls_mode, "verify_ca") == 0) {
			scheme->tls_verify = OD_TVERIFY_CA;
		} else
		if (strcmp(scheme->tls_mode, "verify_full") == 0) {
			scheme->tls_verify = OD_TVERIFY_FULL;
		} else {
			od_error(log, "config", "unknown tls mode");
			return -1;
		}
	}

	/* storages */
	if (od_list_empty(&scheme->storages)) {
		od_error(log, "config", "no storages defined");
		return -1;
	}
	od_list_t *i;
	od_list_foreach(&scheme->storages, i) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		if (storage->type == NULL) {
			od_error(log, "config", "storage '%s': no type is specified",
			         storage->name);
			return -1;
		}
		if (strcmp(storage->type, "remote") == 0) {
			storage->storage_type = OD_SREMOTE;
		} else
		if (strcmp(storage->type, "local") == 0) {
			storage->storage_type = OD_SLOCAL;
		} else {
			od_error(log, "config", "unknown storage type");
			return -1;
		}
		if (storage->host == NULL) {
			od_error(log, "config", "storage '%s': no host is specified",
			         storage->name);
			return -1;
		}
		if (storage->tls_mode) {
			if (strcmp(storage->tls_mode, "disable") == 0) {
				storage->tls_verify = OD_TDISABLE;
			} else
			if (strcmp(storage->tls_mode, "allow") == 0) {
				storage->tls_verify = OD_TALLOW;
			} else
			if (strcmp(storage->tls_mode, "require") == 0) {
				storage->tls_verify = OD_TREQUIRE;
			} else
			if (strcmp(storage->tls_mode, "verify_ca") == 0) {
				storage->tls_verify = OD_TVERIFY_CA;
			} else
			if (strcmp(storage->tls_mode, "verify_full") == 0) {
				storage->tls_verify = OD_TVERIFY_FULL;
			} else {
				od_error(log, "config", "unknown storage tls mode");
				return -1;
			}
		}
	}

	od_schemeroute_t *default_route = NULL;

	/* routing table */
	od_list_foreach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (route->route == NULL) {
			od_error(log, "config", "route '%s': no route storage is specified",
			         route->target);
			return -1;
		}
		/* pooling mode */
		if (route->pool_mode_sz == NULL) {
			od_error(log, "config", "route '%s': pooling mode is not set",
			         route->target);
			return -1;
		}
		if (strcmp(route->pool_mode_sz, "session") == 0)
			route->pool_mode = OD_PSESSION;
		else
		if (strcmp(route->pool_mode_sz, "transaction") == 0)
			route->pool_mode = OD_PTRANSACTION;

		if (route->pool_mode == OD_PUNDEF) {
			od_error(log, "config", "route '%s': unknown pooling mode",
			         route->target);
			return -1;
		}
		/* match storage */
		route->storage = od_schemestorage_match(scheme, route->route);
		if (route->storage == NULL) {
			od_error(log, "config", "route '%s': no route storage '%s' found",
			         route->target);
			return -1;
		}
		if (route->is_default) {
			if (default_route) {
				od_error(log, "config", "more than one default route");
				return -1;
			}
			default_route = route;
		}
	}
	scheme->routing_default = default_route;

	/* users */
	if (od_list_empty(&scheme->users)) {
		od_error(log, "config", "no users defined");
		return -1;
	}

	od_schemeuser_t *default_user = NULL;

	od_list_foreach(&scheme->users, i) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		if (! user->auth) {
			if (user->is_default)
				od_error(log, "config", "default user authentication mode is not defined");
			 else
				od_error(log, "config", "user '%s' authentication mode is not defined",
				         user->user);
			return -1;
		}
		if (strcmp(user->auth, "none") == 0) {
			user->auth_mode = OD_ANONE;
		} else
		if (strcmp(user->auth, "clear_text") == 0) {
			user->auth_mode = OD_ACLEAR_TEXT;
			if (user->password == NULL) {
				od_error(log, "config", "user '%s' password is not set",
				         user->user);
				return -1;
			}
		} else
		if (strcmp(user->auth, "md5") == 0) {
			user->auth_mode = OD_AMD5;
			if (user->password == NULL) {
				od_error(log, "config", "user '%s' password is not set",
				         user->user);
				return -1;
			}
		} else {
			od_error(log, "config", "user '%s' has unknown authentication mode",
			         user->user);
			return -1;
		}
		if (user->is_default) {
			if (default_user) {
				od_error(log, "config", "more than one default user");
				return -1;
			}
			default_user = user;
		}
	}
	scheme->users_default = default_user;
	return 0;
}

static inline char*
od_scheme_yes_no(int value) {
	return value ? "yes" : "no";
}

void od_scheme_print(od_scheme_t *scheme, od_log_t *log)
{
	if (scheme->log_debug)
		od_log(log, "log_debug       %s",
		       od_scheme_yes_no(scheme->log_debug));
	if (scheme->log_config)
		od_log(log, "log_config      %s",
		       od_scheme_yes_no(scheme->log_config));
	if (scheme->log_file)
		od_log(log, "log_file        %s", scheme->log_file);
	if (scheme->pid_file)
		od_log(log, "pid_file        %s", scheme->pid_file);
	if (scheme->syslog)
		od_log(log, "syslog          %d", scheme->syslog);
	if (scheme->syslog_ident)
		od_log(log, "syslog_ident    %s", scheme->syslog_ident);
	if (scheme->syslog_facility)
		od_log(log, "syslog_facility %s", scheme->syslog_facility);
	if (scheme->stats_period)
		od_log(log, "stats_period    %d", scheme->stats_period);
	if (scheme->daemonize)
		od_log(log, "daemonize       %s",
		       od_scheme_yes_no(scheme->daemonize));
	od_log(log, "readahead       %d", scheme->readahead);
	od_log(log, "pipelining      %d", scheme->server_pipelining);
	if (scheme->client_max_set)
		od_log(log, "client_max      %d", scheme->client_max);
	od_log(log, "workers         %d", scheme->workers);
	od_log(log, "");
	od_log(log, "listen");
	od_log(log, "  host          %s ", scheme->host);
	od_log(log, "  port          %d", scheme->port);
	od_log(log, "  backlog       %d", scheme->backlog);
	od_log(log, "  nodelay       %d", scheme->nodelay);
	od_log(log, "  keepalive     %d", scheme->keepalive);
	if (scheme->tls_mode)
	od_log(log, "  tls_mode      %s", scheme->tls_mode);
	if (scheme->tls_ca_file)
	od_log(log, "  tls_ca_file   %s", scheme->tls_ca_file);
	if (scheme->tls_key_file)
	od_log(log, "  tls_key_file  %s", scheme->tls_key_file);
	if (scheme->tls_cert_file)
	od_log(log, "  tls_cert_file %s", scheme->tls_cert_file);
	if (scheme->tls_protocols)
	od_log(log, "  tls_protocols %s", scheme->tls_protocols);
	od_log(log, "");

	od_list_t *i;
	od_list_foreach(&scheme->storages, i) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		if (storage->is_default)
			od_log(log, "storage default");
		else
			od_log(log, "storage %s", storage->name);
		od_log(log, "  type          %s", storage->type);
		od_log(log, "  host          %s", storage->host);
		od_log(log, "  port          %d", storage->port);
		if (storage->tls_mode)
			od_log(log, "  tls_mode      %s", storage->tls_mode);
		if (storage->tls_ca_file)
			od_log(log, "  tls_ca_file   %s", storage->tls_ca_file);
		if (storage->tls_key_file)
			od_log(log, "  tls_key_file  %s", storage->tls_key_file);
		if (storage->tls_cert_file)
			od_log(log, "  tls_cert_file %s", storage->tls_cert_file);
		if (storage->tls_protocols)
			od_log(log, "  tls_protocols %s", storage->tls_protocols);
		od_log(log, "");
	}

	od_list_foreach(&scheme->routing_table, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (route->is_default)
			od_log(log, "route default");
		else
			od_log(log, "route %s", route->target);
		od_log(log, "  storage       %s", route->route);
		if (route->database)
		od_log(log, "  database      %s", route->database);
		if (route->user)
		od_log(log, "  user          %s", route->user);
		od_log(log, "  cancel        %s",
		       route->discard ? "yes" : "no");
		od_log(log, "  rollback      %s",
			   route->discard ? "yes" : "no");
		od_log(log, "  discard       %s",
		       route->discard ? "yes" : "no");
		if (route->client_max_set)
			od_log(log, "  client_max    %d", route->client_max);
		od_log(log, "  pool_mode     %s", route->pool_mode_sz);
		od_log(log, "  pool_size     %d", route->pool_size);
		od_log(log, "  pool_timeout  %d", route->pool_timeout);
		od_log(log, "  pool_ttl      %d", route->pool_ttl);
		od_log(log, "");
	}

	od_list_foreach(&scheme->users, i) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		if (user->is_default)
			od_log(log, "user default");
		else
			od_log(log, "user %s", user->user);
		if (user->is_deny)
			od_log(log, "  deny");
		od_log(log, "  authentication %s", user->auth);
		od_log(log, "");
	}
}
