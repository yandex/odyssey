
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>

#include "sources/macro.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/log_file.h"
#include "sources/log_system.h"
#include "sources/logger.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"

void od_scheme_init(od_scheme_t *scheme)
{
	scheme->daemonize = 0;
	scheme->log_debug = 0;
	scheme->log_to_stdout = 1;
	scheme->log_config = 0;
	scheme->log_session = 1;
	scheme->log_file = NULL;
	scheme->log_statistics = 0;
	scheme->log_format_name = NULL;
	scheme->log_format = OD_LOGFORMAT_TEXT;
	scheme->pid_file = NULL;
	scheme->syslog = 0;
	scheme->syslog_ident = NULL;
	scheme->syslog_facility = NULL;
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
	scheme->tls_mode = OD_TLS_DISABLE;
	scheme->tls = NULL;
	scheme->tls_ca_file = NULL;
	scheme->tls_key_file = NULL;
	scheme->tls_cert_file = NULL;
	scheme->tls_protocols = NULL;
	od_list_init(&scheme->storages);
	od_list_init(&scheme->routes);
}

static void
od_schemestorage_free(od_schemestorage_t*);

void od_scheme_free(od_scheme_t *scheme)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&scheme->routes, i, n) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		od_schemeroute_free(route);
	}
	if (scheme->log_file)
		free(scheme->log_file);
	if (scheme->log_format_name)
		free(scheme->log_format_name);
	if (scheme->pid_file)
		free(scheme->pid_file);
	if (scheme->syslog_ident)
		free(scheme->syslog_ident);
	if (scheme->syslog_facility)
		free(scheme->syslog_facility);
	if (scheme->host)
		free(scheme->host);
	if (scheme->tls)
		free(scheme->tls);
	if (scheme->tls_ca_file)
		free(scheme->tls_ca_file);
	if (scheme->tls_key_file)
		free(scheme->tls_key_file);
	if (scheme->tls_cert_file)
		free(scheme->tls_cert_file);
	if (scheme->tls_protocols)
		free(scheme->tls_protocols);
}

static inline od_schemestorage_t*
od_schemestorage_allocate(void)
{
	od_schemestorage_t *storage;
	storage = (od_schemestorage_t*)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	od_list_init(&storage->link);
	return storage;
}

static void
od_schemestorage_free(od_schemestorage_t *storage)
{
	if (storage->name)
		free(storage->name);
	if (storage->type)
		free(storage->type);
	if (storage->host)
		free(storage->host);
	if (storage->tls)
		free(storage->tls);
	if (storage->tls_ca_file)
		free(storage->tls_ca_file);
	if (storage->tls_key_file)
		free(storage->tls_key_file);
	if (storage->tls_cert_file)
		free(storage->tls_cert_file);
	if (storage->tls_protocols)
		free(storage->tls_protocols);
	od_list_unlink(&storage->link);
	free(storage);
}

od_schemestorage_t*
od_schemestorage_add(od_scheme_t *scheme)
{
	od_schemestorage_t *storage;
	storage = od_schemestorage_allocate();
	if (storage == NULL)
		return NULL;
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

od_schemestorage_t*
od_schemestorage_match_latest(od_scheme_t *scheme, char *name)
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

od_schemestorage_t*
od_schemestorage_copy(od_schemestorage_t *storage)
{
	od_schemestorage_t *copy;
	copy = od_schemestorage_allocate();
	if (copy == NULL)
		return NULL;
	copy->storage_type = storage->storage_type;
	copy->name = strdup(storage->name);
	if (copy->name == NULL)
		goto error;
	copy->type = strdup(storage->type);
	if (copy->type == NULL)
		goto error;
	if (storage->host) {
		copy->host = strdup(storage->host);
		if (copy->host == NULL)
			goto error;
	}
	copy->port = storage->port;
	copy->tls_mode = storage->tls_mode;
	if (storage->tls) {
		copy->tls = strdup(storage->tls);
		if (copy->tls == NULL)
			goto error;
	}
	if (storage->tls_ca_file) {
		copy->tls_ca_file = strdup(storage->tls_ca_file);
		if (copy->tls_ca_file == NULL)
			goto error;
	}
	if (storage->tls_key_file) {
		copy->tls_key_file = strdup(storage->tls_key_file);
		if (copy->tls_key_file == NULL)
			goto error;
	}
	if (storage->tls_cert_file) {
		copy->tls_cert_file = strdup(storage->tls_cert_file);
		if (copy->tls_cert_file == NULL)
			goto error;
	}
	if (storage->tls_protocols) {
		copy->tls_protocols = strdup(storage->tls_protocols);
		if (copy->tls_protocols == NULL)
			goto error;
	}
	return copy;
error:
	od_schemestorage_free(copy);
	return NULL;
}

static inline int
od_schemestorage_compare(od_schemestorage_t *a, od_schemestorage_t *b)
{
	/* type */
	if (a->storage_type != b->storage_type)
		return 0;

	/* host */
	if (a->host && b->host) {
		if (strcmp(a->host, b->host) != 0)
			return 0;
	} else
	if (a->host || b->host) {
		return 0;
	}

	/* port */
	if (a->port != b->port)
		return 0;

	/* tls_mode */
	if (a->tls_mode != b->tls_mode)
		return 0;

	/* tls_ca_file */
	if (a->tls_ca_file && b->tls_ca_file) {
		if (strcmp(a->tls_ca_file, b->tls_ca_file) != 0)
			return 0;
	} else
	if (a->tls_ca_file || b->tls_ca_file) {
		return 0;
	}

	/* tls_key_file */
	if (a->tls_key_file && b->tls_key_file) {
		if (strcmp(a->tls_key_file, b->tls_key_file) != 0)
			return 0;
	} else
	if (a->tls_key_file || b->tls_key_file) {
		return 0;
	}

	/* tls_cert_file */
	if (a->tls_cert_file && b->tls_cert_file) {
		if (strcmp(a->tls_cert_file, b->tls_cert_file) != 0)
			return 0;
	} else
	if (a->tls_cert_file || b->tls_cert_file) {
		return 0;
	}

	/* tls_protocols */
	if (a->tls_protocols && b->tls_protocols) {
		if (strcmp(a->tls_protocols, b->tls_protocols) != 0)
			return 0;
	} else
	if (a->tls_protocols || b->tls_protocols) {
		return 0;
	}

	return 1;
}

od_schemeroute_t*
od_schemeroute_add(od_scheme_t *scheme, int version)
{
	od_schemeroute_t *route;
	route = (od_schemeroute_t*)malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	memset(route, 0, sizeof(*route));
	route->version = version;
	route->pool_size = 100;
	route->pool_cancel = 1;
	route->pool_discard = 1;
	route->pool_rollback = 1;
	od_list_init(&route->link);
	od_list_append(&scheme->routes, &route->link);
	return route;
}

void od_schemeroute_free(od_schemeroute_t *route)
{
	assert(route->refs == 0);
	if (route->db_name)
		free(route->db_name);
	if (route->user_name)
		free(route->user_name);
	if (route->password)
		free(route->password);
	if (route->auth)
		free(route->auth);
	if (route->storage)
		od_schemestorage_free(route->storage);
	if (route->storage_name)
		free(route->storage_name);
	if (route->storage_db)
		free(route->storage_db);
	if (route->storage_user)
		free(route->storage_user);
	if (route->storage_password)
		free(route->storage_password);
	if (route->pool_sz)
		free(route->pool_sz);
	od_list_unlink(&route->link);
	free(route);
}

static inline void
od_schemeroute_cmpswap(od_schemeroute_t **dest, od_schemeroute_t *next)
{
	/* update dest if (a) it is not set or (b) previous version is lower
	 * then new version */
	od_schemeroute_t *prev = *dest;
	if (prev == NULL) {
		*dest = next;
		return;
	}
	assert( prev->version != next->version);
	if (prev->version < next->version)
		*dest = next;
}

od_schemeroute_t*
od_schemeroute_forward(od_scheme_t *scheme, char *db_name, char *user_name)
{
	od_schemeroute_t *route_db_user = NULL;
	od_schemeroute_t *route_db_default = NULL;
	od_schemeroute_t *route_default_user = NULL;
	od_schemeroute_t *route_default_default = NULL;

	od_list_t *i;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (route->db_is_default) {
			if (route->user_is_default)
				od_schemeroute_cmpswap(&route_default_default, route);
			else
			if (strcmp(route->user_name, user_name) == 0)
				od_schemeroute_cmpswap(&route_default_user, route);
		} else
		if (strcmp(route->db_name, db_name) == 0) {
			if (route->user_is_default)
				od_schemeroute_cmpswap(&route_db_default, route);
			else
			if (strcmp(route->user_name, user_name) == 0)
				od_schemeroute_cmpswap(&route_db_user, route);
		}
	}

	if (route_db_user)
		return route_db_user;

	if (route_db_default)
		return route_db_default;

	if (route_default_user)
		return route_default_user;

	return route_default_default;
}

od_schemeroute_t*
od_schemeroute_match(od_scheme_t *scheme, char *db_name, char *user_name)
{
	od_list_t *i;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (strcmp(route->db_name, db_name) == 0 &&
		    strcmp(route->user_name, user_name) == 0)
			return route;
	}
	return NULL;
}

od_schemeroute_t*
od_schemeroute_match_latest(od_scheme_t *scheme, char *db_name, char *user_name)
{
	/* match latest route scheme version */
	od_schemeroute_t *match = NULL;
	od_list_t *i;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		if (strcmp(route->db_name, db_name) != 0 ||
		    strcmp(route->user_name, user_name) != 0)
			continue;
		if (match) {
			if (match->version < route->version)
				match = route;
		} else {
			match = route;
		}
	}
	return match;
}

int od_schemeroute_compare(od_schemeroute_t *a, od_schemeroute_t *b)
{
	/* db default */
	if (a->db_is_default != b->db_is_default)
		return 0;

	/* user default */
	if (a->user_is_default != b->user_is_default)
		return 0;

	/* password */
	if (a->password && b->password) {
		if (strcmp(a->password, b->password) != 0)
			return 0;
	} else
	if (a->password || b->password) {
		return 0;
	}

	/* auth */
	if (a->auth_mode != b->auth_mode)
		return 0;

	/* storage */
	if (strcmp(a->storage_name, b->storage_name) != 0)
		return 0;

	if (! od_schemestorage_compare(a->storage, b->storage))
		return 0;

	/* storage_db */
	if (a->storage_db && b->storage_db) {
		if (strcmp(a->storage_db, b->storage_db) != 0)
			return 0;
	} else
	if (a->storage_db || b->storage_db) {
		return 0;
	}

	/* storage_user */
	if (a->storage_user && b->storage_user) {
		if (strcmp(a->storage_user, b->storage_user) != 0)
			return 0;
	} else
	if (a->storage_user || b->storage_user) {
		return 0;
	}

	/* storage_password */
	if (a->storage_password && b->storage_password) {
		if (strcmp(a->storage_password, b->storage_password) != 0)
			return 0;
	} else
	if (a->storage_password || b->storage_password) {
		return 0;
	}

	/* pool */
	if (a->pool != b->pool)
		return 0;

	/* pool_size */
	if (a->pool_size != b->pool_size)
		return 0;

	/* pool_timeout */
	if (a->pool_timeout != b->pool_timeout)
		return 0;

	/* pool_ttl */
	if (a->pool_ttl != b->pool_ttl)
		return 0;

	/* pool_cancel */
	if (a->pool_cancel != b->pool_cancel)
		return 0;

	/* pool_discard */
	if (a->pool_discard != b->pool_discard)
		return 0;

	/* pool_rollback*/
	if (a->pool_rollback != b->pool_rollback)
		return 0;

	/* client_max */
	if (a->client_max != b->client_max)
		return 0;

	return 1;
}

int od_scheme_validate(od_scheme_t *scheme, od_logger_t *logger)
{
	/* log format */
	if (scheme->log_format_name) {
		if (strcmp(scheme->log_format_name, "text") == 0) {
			scheme->log_format = OD_LOGFORMAT_TEXT;
		} else
		if (strcmp(scheme->log_format_name, "tskv") == 0) {
			scheme->log_format = OD_LOGFORMAT_TSKV;
		} else {
			od_error(logger, "config", "unknown log format");
			return -1;
		}
	}

	/* workers */
	if (scheme->workers == 0) {
		od_error(logger, "config", "bad workers number");
		return -1;
	}

	/* listen */
	if (scheme->host == NULL) {
		od_error(logger, "config", "listen host is not defined");
		return -1;
	}

	/* tls */
	if (scheme->tls) {
		if (strcmp(scheme->tls, "disable") == 0) {
			scheme->tls_mode = OD_TLS_DISABLE;
		} else
		if (strcmp(scheme->tls, "allow") == 0) {
			scheme->tls_mode = OD_TLS_ALLOW;
		} else
		if (strcmp(scheme->tls, "require") == 0) {
			scheme->tls_mode = OD_TLS_REQUIRE;
		} else
		if (strcmp(scheme->tls, "verify_ca") == 0) {
			scheme->tls_mode = OD_TLS_VERIFY_CA;
		} else
		if (strcmp(scheme->tls, "verify_full") == 0) {
			scheme->tls_mode = OD_TLS_VERIFY_FULL;
		} else {
			od_error(logger, "config", "unknown tls mode");
			return -1;
		}
	}

	/* storages */
	if (od_list_empty(&scheme->storages)) {
		od_error(logger, "config", "no storages defined");
		return -1;
	}
	od_list_t *i;
	od_list_foreach(&scheme->storages, i) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		if (storage->type == NULL) {
			od_error(logger, "config", "storage '%s': no type is specified",
			         storage->name);
			return -1;
		}
		if (strcmp(storage->type, "remote") == 0) {
			storage->storage_type = OD_STORAGETYPE_REMOTE;
		} else
		if (strcmp(storage->type, "local") == 0) {
			storage->storage_type = OD_STORAGETYPE_LOCAL;
		} else {
			od_error(logger, "config", "unknown storage type");
			return -1;
		}
		if (storage->storage_type == OD_STORAGETYPE_REMOTE &&
		    storage->host == NULL) {
			od_error(logger, "config", "storage '%s': no remote host is specified",
			         storage->name);
			return -1;
		}
		if (storage->tls) {
			if (strcmp(storage->tls, "disable") == 0) {
				storage->tls_mode = OD_TLS_DISABLE;
			} else
			if (strcmp(storage->tls, "allow") == 0) {
				storage->tls_mode = OD_TLS_ALLOW;
			} else
			if (strcmp(storage->tls, "require") == 0) {
				storage->tls_mode = OD_TLS_REQUIRE;
			} else
			if (strcmp(storage->tls, "verify_ca") == 0) {
				storage->tls_mode = OD_TLS_VERIFY_CA;
			} else
			if (strcmp(storage->tls, "verify_full") == 0) {
				storage->tls_mode = OD_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "config", "unknown storage tls mode");
				return -1;
			}
		}
	}

	/* routes */
	if (od_list_empty(&scheme->routes)) {
		od_error(logger, "config", "no routes defined");
		return -1;
	}
	od_schemeroute_t *route_default_default = NULL;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);

		/* ensure route default.default exists */
		if (route->db_is_default && route->user_is_default) {
			assert(! route_default_default);
			route_default_default = route;
		}

		/* match storage and make a copy of in the user scheme */
		if (route->storage_name == NULL) {
			od_error(logger, "config", "route '%s.%s': no route storage is specified",
			         route->db_name, route->user_name);
			return -1;
		}
		od_schemestorage_t *storage;
		storage = od_schemestorage_match(scheme, route->storage_name);
		if (storage == NULL) {
			od_error(logger, "config", "route '%s.%s': no route storage '%s' found",
			         route->db_name, route->user_name);
			return -1;
		}
		route->storage = od_schemestorage_copy(storage);
		if (route->storage == NULL)
			return -1;

		/* pooling mode */
		if (! route->pool_sz) {
			od_error(logger, "config", "route '%s.%s': pooling mode is not set",
			         route->db_name, route->user_name);
			return -1;
		}
		if (strcmp(route->pool_sz, "session") == 0) {
			route->pool = OD_POOLING_SESSION;
		} else
		if (strcmp(route->pool_sz, "transaction") == 0) {
			route->pool = OD_POOLING_TRANSACTION;
		} else {
			od_error(logger, "config", "route '%s.%s': unknown pooling mode",
			         route->db_name, route->user_name);
			return -1;
		}

		/* auth */
		if (! route->auth) {
			od_error(logger, "config", "route '%s.%s': authentication mode is not defined",
			         route->db_name, route->user_name);
			return -1;
		}
		if (strcmp(route->auth, "none") == 0) {
			route->auth_mode = OD_AUTH_NONE;
		} else
		if (strcmp(route->auth, "block") == 0) {
			route->auth_mode = OD_AUTH_BLOCK;
		} else
		if (strcmp(route->auth, "clear_text") == 0) {
			route->auth_mode = OD_AUTH_CLEAR_TEXT;
			if (route->password == NULL) {
				od_error(logger, "config", "route '%s.%s': password is not set",
				         route->db_name, route->user_name);
				return -1;
			}
		} else
		if (strcmp(route->auth, "md5") == 0) {
			route->auth_mode = OD_AUTH_MD5;
			if (route->password == NULL) {
				od_error(logger, "config", "route '%s.%s': password is not set",
				         route->db_name, route->user_name);
				return -1;
			}
		} else {
			od_error(logger, "config", "route '%s.%s': has unknown authentication mode",
			         route->db_name, route->user_name);
			return -1;
		}
	}
	if (! route_default_default) {
		od_error(logger, "config", "route 'default.default': not defined");
		return -1;
	}

	/* cleanup declarative storages scheme data */
	od_list_t *n;
	od_list_foreach_safe(&scheme->storages, i, n) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		od_schemestorage_free(storage);
	}
	od_list_init(&scheme->storages);
	return 0;
}

static inline char*
od_scheme_yes_no(int value) {
	return value ? "yes" : "no";
}

void od_scheme_print(od_scheme_t *scheme, od_logger_t *logger, int routes_only)
{
	if (routes_only)
		goto log_routes;

	if (scheme->log_file)
		od_log(logger, "log_file        %s", scheme->log_file);
	if (scheme->log_format_name)
		od_log(logger, "log_format      %s", scheme->log_format_name);
	if (scheme->log_debug)
		od_log(logger, "log_debug       %s",
		       od_scheme_yes_no(scheme->log_debug));
	if (scheme->log_config)
		od_log(logger, "log_config      %s",
		       od_scheme_yes_no(scheme->log_config));
	if (scheme->log_session)
		od_log(logger, "log_session     %s",
		       od_scheme_yes_no(scheme->log_session));
	if (scheme->log_statistics)
		od_log(logger, "log_statistics  %d", scheme->log_statistics);
	if (scheme->pid_file)
		od_log(logger, "pid_file        %s", scheme->pid_file);
	if (scheme->syslog)
		od_log(logger, "syslog          %d", scheme->syslog);
	if (scheme->syslog_ident)
		od_log(logger, "syslog_ident    %s", scheme->syslog_ident);
	if (scheme->syslog_facility)
		od_log(logger, "syslog_facility %s", scheme->syslog_facility);
	if (scheme->daemonize)
		od_log(logger, "daemonize       %s",
		       od_scheme_yes_no(scheme->daemonize));
	od_log(logger, "readahead       %d", scheme->readahead);
	od_log(logger, "pipelining      %d", scheme->server_pipelining);
	if (scheme->client_max_set)
		od_log(logger, "client_max      %d", scheme->client_max);
	od_log(logger, "workers         %d", scheme->workers);
	od_log(logger, "");
	od_log(logger, "listen");
	od_log(logger, "  host          %s", scheme->host);
	od_log(logger, "  port          %d", scheme->port);
	od_log(logger, "  backlog       %d", scheme->backlog);
	od_log(logger, "  nodelay       %d", scheme->nodelay);
	od_log(logger, "  keepalive     %d", scheme->keepalive);
	if (scheme->tls)
		od_log(logger, "  tls           %s", scheme->tls);
	if (scheme->tls_ca_file)
		od_log(logger, "  tls_ca_file   %s", scheme->tls_ca_file);
	if (scheme->tls_key_file)
		od_log(logger, "  tls_key_file  %s", scheme->tls_key_file);
	if (scheme->tls_cert_file)
		od_log(logger, "  tls_cert_file %s", scheme->tls_cert_file);
	if (scheme->tls_protocols)
		od_log(logger, "  tls_protocols %s", scheme->tls_protocols);

log_routes:;
	od_log(logger, "");
	od_list_t *i;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		od_log(logger, "route %s.%s.%d %s",
		       route->db_name,
		       route->user_name, route->version,
		       route->is_obsolete ? "(obsolete)" : "");
		od_log(logger, "  authentication %s", route->auth);
		od_log(logger, "  pool           %s", route->pool_sz);
		od_log(logger, "  pool_size      %d", route->pool_size);
		od_log(logger, "  pool_timeout   %d", route->pool_timeout);
		od_log(logger, "  pool_ttl       %d", route->pool_ttl);
		od_log(logger, "  pool_cancel    %s",
			   route->pool_cancel ? "yes" : "no");
		od_log(logger, "  pool_rollback  %s",
			   route->pool_rollback ? "yes" : "no");
		od_log(logger, "  pool_discard   %s",
			   route->pool_discard ? "yes" : "no");
		if (route->client_max_set)
			od_log(logger, "  client_max     %d", route->client_max);
		od_log(logger, "  storage        %s", route->storage_name);
		od_log(logger, "  type           %s", route->storage->type);
		if (route->storage->host)
			od_log(logger, "  host           %s", route->storage->host);
		if (route->storage->port)
			od_log(logger, "  port           %d", route->storage->port);
		if (route->storage->tls)
			od_log(logger, "  tls            %s", route->storage->tls);
		if (route->storage->tls_ca_file)
			od_log(logger, "  tls_ca_file    %s", route->storage->tls_ca_file);
		if (route->storage->tls_key_file)
			od_log(logger, "  tls_key_file   %s", route->storage->tls_key_file);
		if (route->storage->tls_cert_file)
			od_log(logger, "  tls_cert_file  %s", route->storage->tls_cert_file);
		if (route->storage->tls_protocols)
			od_log(logger, "  tls_protocols  %s", route->storage->tls_protocols);
		if (route->storage_db)
			od_log(logger, "  storage_db     %s", route->storage_db);
		if (route->storage_user)
			od_log(logger, "  storage_user   %s", route->storage_user);
		od_log(logger, "");
	}
}

int od_scheme_merge(od_scheme_t *scheme, od_logger_t *logger, od_scheme_t *src)
{
	int count_obsolete = 0;
	int count_deleted = 0;
	int count_new = 0;

	/* mark all routes obsolete */
	od_list_t *i;
	od_list_foreach(&scheme->routes, i) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);
		route->is_obsolete = 1;
		count_obsolete++;
	}

	/* select new routes */
	od_list_t *n;
	od_list_foreach_safe(&src->routes, i, n) {
		od_schemeroute_t *route;
		route = od_container_of(i, od_schemeroute_t, link);

		/* find and compare origin route */
		od_schemeroute_t *origin;
		origin = od_schemeroute_match_latest(scheme, route->db_name, route->user_name);
		if (origin) {
			if (od_schemeroute_compare(origin, route)) {
				origin->is_obsolete = 0;
				count_obsolete--;
				continue;
			}

			/* add new version, origin version still exists */
			od_log(logger, "(config) update route %s.%s.%d -> %s.%s.%d",
			       origin->db_name, origin->user_name,
			       origin->version,
			       route->db_name, route->user_name,
			       route->version);
		} else {
			/* add new version */
			od_log(logger, "(config) new route %s.%s.%d",
			       route->db_name, route->user_name,
			       route->version);
		}

		od_list_unlink(&route->link);
		od_list_init(&route->link);
		od_list_append(&scheme->routes, &route->link);

		count_new++;
	}

	/* try to free obsolete schemes, which are unused by any
	 * route at the moment */
	if (count_obsolete) {
		od_list_foreach_safe(&scheme->routes, i, n) {
			od_schemeroute_t *route;
			route = od_container_of(i, od_schemeroute_t, link);
			if (route->is_obsolete && route->refs == 0) {
				od_schemeroute_free(route);
				count_deleted++;
				count_obsolete--;
			}
		}
	}

	od_log(logger, "(config) %d routes added, %d removed, %d scheduled for removal",
	       count_new, count_deleted,
	       count_obsolete);

	return count_new + count_obsolete + count_deleted;
}
