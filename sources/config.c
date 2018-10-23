
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

void
od_config_init(od_config_t *config)
{
	config->daemonize = 0;
	config->log_debug = 0;
	config->log_to_stdout = 1;
	config->log_config = 0;
	config->log_session = 1;
	config->log_query = 0;
	config->log_file = NULL;
	config->log_stats = 1;
	config->stats_interval = 3;
	config->log_format = NULL;
	config->pid_file = NULL;
	config->unix_socket_dir = NULL;
	config->unix_socket_mode = NULL;
	config->log_syslog = 0;
	config->log_syslog_ident = NULL;
	config->log_syslog_facility = NULL;
	config->readahead = 8192;
	config->packet_read_size = INT_MAX;
	config->nodelay = 1;
	config->keepalive = 7200;
	config->workers = 1;
	config->resolvers = 1;
	config->client_max_set = 0;
	config->client_max = 0;
	config->cache_coroutine = 0;
	config->cache_msg_gc_size = 0;
	config->coroutine_stack_size = 4;
	od_list_init(&config->storages);
	od_list_init(&config->routes);
	od_list_init(&config->listen);
}

static void
od_config_listen_free(od_config_listen_t*);

void
od_config_free(od_config_t *config)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&config->routes, i, n) {
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		od_config_route_free(route);
	}
	od_list_foreach_safe(&config->listen, i, n) {
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		od_config_listen_free(listen);
	}
	if (config->log_file)
		free(config->log_file);
	if (config->log_format)
		free(config->log_format);
	if (config->pid_file)
		free(config->pid_file);
	if (config->unix_socket_dir)
		free(config->unix_socket_dir);
	if (config->log_syslog_ident)
		free(config->log_syslog_ident);
	if (config->log_syslog_facility)
		free(config->log_syslog_facility);
}

od_config_listen_t*
od_config_listen_add(od_config_t *config)
{
	od_config_listen_t *listen;
	listen = (od_config_listen_t*)malloc(sizeof(*config));
	if (listen == NULL)
		return NULL;
	memset(listen, 0, sizeof(*listen));
	listen->port = 6432;
	listen->backlog = 128;
	od_list_init(&listen->link);
	od_list_append(&config->listen, &listen->link);
	return listen;
}

static void
od_config_listen_free(od_config_listen_t *config)
{
	if (config->host)
		free(config->host);
	if (config->tls)
		free(config->tls);
	if (config->tls_ca_file)
		free(config->tls_ca_file);
	if (config->tls_key_file)
		free(config->tls_key_file);
	if (config->tls_cert_file)
		free(config->tls_cert_file);
	if (config->tls_protocols)
		free(config->tls_protocols);
	free(config);
}

static inline od_config_storage_t*
od_config_storage_allocate(void)
{
	od_config_storage_t *storage;
	storage = (od_config_storage_t*)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	od_list_init(&storage->link);
	return storage;
}

void
od_config_storage_free(od_config_storage_t *storage)
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

od_config_storage_t*
od_config_storage_add(od_config_t *config)
{
	od_config_storage_t *storage;
	storage = od_config_storage_allocate();
	if (storage == NULL)
		return NULL;
	od_list_append(&config->storages, &storage->link);
	return storage;
}

od_config_storage_t*
od_config_storage_match(od_config_t *config, char *name)
{
	od_list_t *i;
	od_list_foreach(&config->storages, i) {
		od_config_storage_t *storage;
		storage = od_container_of(i, od_config_storage_t, link);
		if (strcmp(storage->name, name) == 0)
			return storage;
	}
	return NULL;
}

od_config_storage_t*
od_config_storage_copy(od_config_storage_t *storage)
{
	od_config_storage_t *copy;
	copy = od_config_storage_allocate();
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
	od_config_storage_free(copy);
	return NULL;
}

od_config_auth_t*
od_config_auth_add(od_config_route_t *route)
{
	od_config_auth_t *auth;
	auth = (od_config_auth_t*)malloc(sizeof(*auth));
	if (auth == NULL)
		return NULL;
	memset(auth, 0, sizeof(*auth));
	od_list_init(&auth->link);
	od_list_append(&route->auth_common_names, &auth->link);
	route->auth_common_names_count++;
	return auth;
}

void
od_config_auth_free(od_config_auth_t *auth)
{
	if (auth->common_name)
		free(auth->common_name);
	free(auth);
}

static inline od_config_auth_t*
od_config_auth_find(od_config_route_t *route, char *name)
{
	od_list_t *i;
	od_list_foreach(&route->auth_common_names, i) {
		od_config_auth_t *auth;
		auth = od_container_of(i, od_config_auth_t, link);
		if (! strcasecmp(auth->common_name, name))
			return auth;
	}
	return NULL;
}

od_config_route_t*
od_config_route_add(od_config_t *config)
{
	od_config_route_t *route;
	route = (od_config_route_t*)malloc(sizeof(*route));
	if (route == NULL)
		return NULL;
	memset(route, 0, sizeof(*route));
	route->pool_size = 0;
	route->pool_timeout = 0;
	route->pool_cancel = 1;
	route->pool_rollback = 1;
	route->obsolete = 0;
	route->mark = 0;
	route->refs = 0;
	route->auth_common_name_default = 0;
	route->auth_common_names_count = 0;
	od_list_init(&route->auth_common_names);
	od_list_init(&route->link);
	od_list_append(&config->routes, &route->link);
	return route;
}

void
od_config_route_free(od_config_route_t *route)
{
	if (route->db_name)
		free(route->db_name);
	if (route->user_name)
		free(route->user_name);
	if (route->password)
		free(route->password);
	if (route->auth)
		free(route->auth);
	if (route->auth_query)
		free(route->auth_query);
	if (route->auth_query_db)
		free(route->auth_query_db);
	if (route->auth_query_user)
		free(route->auth_query_user);
	if (route->storage)
		od_config_storage_free(route->storage);
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
	od_list_t *i, *n;
	od_list_foreach_safe(&route->auth_common_names, i, n) {
		od_config_auth_t *auth;
		auth = od_container_of(i, od_config_auth_t, link);
		od_config_auth_free(auth);
	}
	od_list_unlink(&route->link);
	free(route);
}

void
od_config_route_ref(od_config_route_t *route)
{
	route->refs++;
}

void
od_config_route_unref(od_config_route_t *route)
{
	assert(route->refs > 0);
	route->refs--;
	if (! route->obsolete)
		return;
	if (route->refs == 0)
		od_config_route_free(route);
}

od_config_route_t*
od_config_route_forward(od_config_t *config, char *db_name, char *user_name)
{
	od_config_route_t *route_db_user = NULL;
	od_config_route_t *route_db_default = NULL;
	od_config_route_t *route_default_user = NULL;
	od_config_route_t *route_default_default = NULL;

	od_list_t *i;
	od_list_foreach(&config->routes, i) {
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		if (route->obsolete)
			continue;
		if (route->db_is_default) {
			if (route->user_is_default)
				route_default_default = route;
			else
			if (strcmp(route->user_name, user_name) == 0)
				route_default_user = route;
		} else
		if (strcmp(route->db_name, db_name) == 0) {
			if (route->user_is_default)
				route_db_default = route;
			else
			if (strcmp(route->user_name, user_name) == 0)
				route_db_user = route;
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

od_config_route_t*
od_config_route_match(od_config_t *config, char *db_name, char *user_name)
{
	od_list_t *i;
	od_list_foreach(&config->routes, i) {
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		if (strcmp(route->db_name, db_name) == 0 &&
		    strcmp(route->user_name, user_name) == 0)
			return route;
	}
	return NULL;
}

static inline od_config_route_t*
od_config_route_match_active(od_config_t *config, char *db_name, char *user_name)
{
	od_list_t *i;
	od_list_foreach(&config->routes, i) {
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		if (route->obsolete)
			continue;
		if (strcmp(route->db_name, db_name) == 0 &&
		    strcmp(route->user_name, user_name) == 0)
			return route;
	}
	return NULL;
}

static inline int
od_config_storage_compare(od_config_storage_t *a, od_config_storage_t *b)
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

int
od_config_route_compare(od_config_route_t *a, od_config_route_t *b)
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

	/* auth query */
	if (a->auth_query && b->auth_query) {
		if (strcmp(a->auth_query, b->auth_query) != 0)
			return 0;
	} else
	if (a->auth_query || b->auth_query) {
		return 0;
	}

	/* auth query db */
	if (a->auth_query_db && b->auth_query_db) {
		if (strcmp(a->auth_query_db, b->auth_query_db) != 0)
			return 0;
	} else
	if (a->auth_query_db || b->auth_query_db) {
		return 0;
	}

	/* auth query user */
	if (a->auth_query_user && b->auth_query_user) {
		if (strcmp(a->auth_query_user, b->auth_query_user) != 0)
			return 0;
	} else
	if (a->auth_query_user || b->auth_query_user) {
		return 0;
	}

	/* auth common name default */
	if (a->auth_common_name_default != b->auth_common_name_default)
		return 0;

	/* auth common names count */
	if (a->auth_common_names_count != b->auth_common_names_count)
		return 0;

	/* compare auth common names */
	od_list_t *i;
	od_list_foreach(&a->auth_common_names, i) {
		od_config_auth_t *auth;
		auth = od_container_of(i, od_config_auth_t, link);
		if (! od_config_auth_find(b, auth->common_name))
			return 0;
	}

	/* storage */
	if (strcmp(a->storage_name, b->storage_name) != 0)
		return 0;

	if (! od_config_storage_compare(a->storage, b->storage))
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

	/* pool_rollback*/
	if (a->pool_rollback != b->pool_rollback)
		return 0;

	/* client_fwd_error */
	if (a->client_fwd_error != b->client_fwd_error)
		return 0;

	/* client_max */
	if (a->client_max != b->client_max)
		return 0;

	return 1;
}

int
od_config_merge(od_config_t *config, od_logger_t *logger, od_config_t *src)
{
	int count_mark = 0;
	int count_deleted = 0;
	int count_new = 0;

	/* mark all routes for obsoletion */
	od_list_t *i;
	od_list_foreach(&config->routes, i) {
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		route->mark = 1;
		count_mark++;
	}

	/* select new routes */
	od_list_t *n;
	od_list_foreach_safe(&src->routes, i, n)
	{
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);

		/* find and compare origin route */
		od_config_route_t *origin;
		origin = od_config_route_match_active(config, route->db_name, route->user_name);
		if (origin) {
			if (od_config_route_compare(origin, route)) {
				origin->mark = 0;
				count_mark--;
				continue;
			}

			/* add new version, origin version still exists */
			od_log(logger, "config", NULL, NULL,
			       "route updated %s.%s", origin->db_name, origin->user_name);
		} else {
			/* add new version */
			od_log(logger, "config", NULL, NULL,
			       "route added %s.%s", route->db_name, route->user_name);
		}

		od_list_unlink(&route->link);
		od_list_init(&route->link);
		od_list_append(&config->routes, &route->link);
		count_new++;
	}

	/* try to free obsolete schemes, which are unused by any
	 * route at the moment */
	if (count_mark > 0) {
		od_list_foreach_safe(&config->routes, i, n) {
			od_config_route_t *route;
			route = od_container_of(i, od_config_route_t, link);

			int is_obsolete = route->obsolete || route->mark;
			route->mark = 0;
			route->obsolete = is_obsolete;

			if (is_obsolete && route->refs == 0) {
				od_config_route_free(route);
				count_deleted++;
				count_mark--;
			}
		}
	}

	od_log(logger, "config", NULL, NULL,
	       "%d routes added, %d removed, %d scheduled for removal",
	       count_new, count_deleted,
	       count_mark);

	return count_new + count_mark + count_deleted;
}

int
od_config_validate(od_config_t *config, od_logger_t *logger)
{
	/* workers */
	if (config->workers == 0) {
		od_error(logger, "config", NULL, NULL, "bad workers number");
		return -1;
	}

	/* resolvers */
	if (config->resolvers == 0) {
		od_error(logger, "config", NULL, NULL, "bad resolvers number");
		return -1;
	}

	/* coroutine_stack_size */
	if (config->coroutine_stack_size < 4) {
		od_error(logger, "config", NULL, NULL, "bad coroutine_stack_size number");
		return -1;
	}

	/* log format */
	if (config->log_format == NULL) {
		od_error(logger, "config", NULL, NULL, "log is not defined");
		return -1;
	}

	/* unix_socket_mode */
	if (config->unix_socket_dir) {
		if (config->unix_socket_mode == NULL) {
			od_error(logger, "config", NULL, NULL, "unix_socket_mode is not set");
			return -1;
		}
	}

	/* packet_read_size */
	if (config->packet_read_size == 0) {
		config->packet_read_size = UINT32_MAX;
	} else
	if (config->packet_read_size < 4096) {
		config->packet_read_size = 4096;
	}

	if (config->packet_read_size <= 0) {
		if (config->unix_socket_mode == NULL) {
			od_error(logger, "config", NULL, NULL, "unix_socket_mode is not set");
			return -1;
		}
	}

	/* listen */
	if (od_list_empty(&config->listen)) {
		od_error(logger, "config", NULL, NULL, "no listen servers defined");
		return -1;
	}

	od_list_t *i;
	od_list_foreach(&config->listen, i)
	{
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		if (listen->host == NULL) {
			if (config->unix_socket_dir == NULL) {
				od_error(logger, "config", NULL, NULL,
				         "listen host is not set and no unix_socket_dir is specified");
				return -1;
			}
		}

		/* tls */
		if (listen->tls) {
			if (strcmp(listen->tls, "disable") == 0) {
				listen->tls_mode = OD_TLS_DISABLE;
			} else
			if (strcmp(listen->tls, "allow") == 0) {
				listen->tls_mode = OD_TLS_ALLOW;
			} else
			if (strcmp(listen->tls, "require") == 0) {
				listen->tls_mode = OD_TLS_REQUIRE;
			} else
			if (strcmp(listen->tls, "verify_ca") == 0) {
				listen->tls_mode = OD_TLS_VERIFY_CA;
			} else
			if (strcmp(listen->tls, "verify_full") == 0) {
				listen->tls_mode = OD_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "config", NULL, NULL, "unknown tls mode");
				return -1;
			}
		}
	}

	/* storages */
	od_list_foreach(&config->storages, i)
	{
		od_config_storage_t *storage;
		storage = od_container_of(i, od_config_storage_t, link);
		if (storage->type == NULL) {
			od_error(logger, "config", NULL, NULL,
			         "storage '%s': no type is specified",
			         storage->name);
			return -1;
		}
		if (strcmp(storage->type, "remote") == 0) {
			storage->storage_type = OD_STORAGE_TYPE_REMOTE;
		} else
		if (strcmp(storage->type, "local") == 0) {
			storage->storage_type = OD_STORAGE_TYPE_LOCAL;
		} else {
			od_error(logger, "config", NULL, NULL, "unknown storage type");
			return -1;
		}
		if (storage->storage_type == OD_STORAGE_TYPE_REMOTE) {
			if (storage->host == NULL) {
				if (config->unix_socket_dir == NULL) {
					od_error(logger, "config", NULL, NULL,
					         "storage '%s': no host specified and unix_socket_dir is not set",
					         storage->name);
					return -1;
				}
			}
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
				od_error(logger, "config", NULL, NULL, "unknown storage tls mode");
				return -1;
			}
		}
	}

	/* routes */
	od_list_foreach(&config->routes, i)
	{
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);

		/* match storage and make a copy of in the user config */
		if (route->storage_name == NULL) {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': no route storage is specified",
			         route->db_name, route->user_name);
			return -1;
		}
		od_config_storage_t *storage;
		storage = od_config_storage_match(config, route->storage_name);
		if (storage == NULL) {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': no route storage '%s' found",
			         route->db_name, route->user_name, route->storage_name);
			return -1;
		}
		route->storage = od_config_storage_copy(storage);
		if (route->storage == NULL)
			return -1;

		/* pooling mode */
		if (! route->pool_sz) {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': pooling mode is not set",
			         route->db_name, route->user_name);
			return -1;
		}
		if (strcmp(route->pool_sz, "session") == 0) {
			route->pool = OD_POOL_TYPE_SESSION;
		} else
		if (strcmp(route->pool_sz, "transaction") == 0) {
			route->pool = OD_POOL_TYPE_TRANSACTION;
		} else {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': unknown pooling mode",
			         route->db_name, route->user_name);
			return -1;
		}

		/* auth */
		if (! route->auth) {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': authentication mode is not defined",
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
			if (route->password == NULL && route->auth_query == NULL) {
				od_error(logger, "config", NULL, NULL,
				         "route '%s.%s': password is not set",
				         route->db_name, route->user_name);
				return -1;
			}
		} else
		if (strcmp(route->auth, "md5") == 0) {
			route->auth_mode = OD_AUTH_MD5;
			if (route->password == NULL && route->auth_query == NULL) {
				od_error(logger, "config", NULL, NULL,
				         "route '%s.%s': password is not set",
				         route->db_name, route->user_name);
				return -1;
			}
		} else
		if (strcmp(route->auth, "cert") == 0) {
			route->auth_mode = OD_AUTH_CERT;
		} else {
			od_error(logger, "config", NULL, NULL,
			         "route '%s.%s': has unknown authentication mode",
			         route->db_name, route->user_name);
			return -1;
		}

		/* auth_query */
		if (route->auth_query) {
			if (route->auth_query_user == NULL) {
				od_error(logger, "config", NULL, NULL,
				         "route '%s.%s': auth_query_user is not set",
				         route->db_name, route->user_name);
				return -1;
			}
			if (route->auth_query_db == NULL) {
				od_error(logger, "config", NULL, NULL,
				         "route '%s.%s': auth_query_db is not set",
				         route->db_name, route->user_name);
				return -1;
			}
		}
	}

	/* cleanup declarative storages config data */
	od_list_t *n;
	od_list_foreach_safe(&config->storages, i, n) {
		od_config_storage_t *storage;
		storage = od_container_of(i, od_config_storage_t, link);
		od_config_storage_free(storage);
	}
	od_list_init(&config->storages);
	return 0;
}

static inline char*
od_config_yes_no(int value) {
	return value ? "yes" : "no";
}

void
od_config_print(od_config_t *config, od_logger_t *logger, int routes_only)
{
	od_log(logger, "config", NULL, NULL,
	       "daemonize            %s",
	       od_config_yes_no(config->daemonize));
	if (config->pid_file)
		od_log(logger, "config", NULL, NULL,
		       "pid_file             %s", config->pid_file);
	if (config->unix_socket_dir) {
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_dir      %s", config->unix_socket_dir);
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_mode     %s", config->unix_socket_mode);
	}
	if (routes_only)
		goto log_routes;
	if (config->log_format)
		od_log(logger, "config", NULL, NULL,
		       "log_format           %s", config->log_format);
	if (config->log_file)
		od_log(logger, "config", NULL, NULL,
		       "log_file             %s", config->log_file);
	od_log(logger, "config", NULL, NULL,
	       "log_to_stdout        %s",
	       od_config_yes_no(config->log_to_stdout));
	od_log(logger, "config", NULL, NULL,
	       "log_syslog           %s",
	       od_config_yes_no(config->log_syslog));
	if (config->log_syslog_ident)
		od_log(logger, "config", NULL, NULL,
		       "log_syslog_ident     %s", config->log_syslog_ident);
	if (config->log_syslog_facility)
		od_log(logger, "config", NULL, NULL,
		       "log_syslog_facility  %s", config->log_syslog_facility);
	od_log(logger, "config", NULL, NULL,
	       "log_debug            %s",
	       od_config_yes_no(config->log_debug));
	od_log(logger, "config", NULL, NULL,
	       "log_config           %s",
	       od_config_yes_no(config->log_config));
	od_log(logger, "config", NULL, NULL,
	       "log_session          %s",
	       od_config_yes_no(config->log_session));
	od_log(logger, "config", NULL, NULL,
	       "log_query            %s",
	       od_config_yes_no(config->log_query));
	od_log(logger, "config", NULL, NULL,
	       "log_stats            %s",
	       od_config_yes_no(config->log_stats));
	od_log(logger, "config", NULL, NULL,
	       "stats_interval       %d", config->stats_interval);
	od_log(logger, "config", NULL, NULL,
	       "readahead            %d", config->readahead);
	od_log(logger, "config", NULL, NULL,
	       "packet_read_size     %d", config->packet_read_size);
	od_log(logger, "config", NULL, NULL,
	       "nodelay              %s",
	       od_config_yes_no(config->nodelay));
	od_log(logger, "config", NULL, NULL,
	       "keepalive            %d", config->keepalive);
	if (config->client_max_set)
		od_log(logger, "config", NULL, NULL,
		       "client_max           %d", config->client_max);
	od_log(logger, "config", NULL, NULL,
	       "cache_msg_gc_size    %d", config->cache_msg_gc_size);
	od_log(logger, "config", NULL, NULL,
	       "cache_coroutine      %d", config->cache_coroutine);
	od_log(logger, "config", NULL, NULL,
	       "coroutine_stack_size %d", config->coroutine_stack_size);
	od_log(logger, "config", NULL, NULL,
	       "workers              %d", config->workers);
	od_log(logger, "config", NULL, NULL,
	       "resolvers            %d", config->resolvers);
	od_log(logger, "config", NULL, NULL, "");
	od_list_t *i;
	od_list_foreach(&config->listen, i)
	{
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		od_log(logger, "config", NULL, NULL, "listen");
		od_log(logger, "config", NULL, NULL,
		       "  host             %s", listen->host ? listen->host : "<unix socket>");
		od_log(logger, "config", NULL, NULL,
		       "  port             %d", listen->port);
		od_log(logger, "config", NULL, NULL,
		       "  backlog          %d", listen->backlog);
		if (listen->tls)
			od_log(logger, "config", NULL, NULL,
			       "  tls              %s", listen->tls);
		if (listen->tls_ca_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_ca_file      %s", listen->tls_ca_file);
		if (listen->tls_key_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_key_file     %s", listen->tls_key_file);
		if (listen->tls_cert_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_cert_file    %s", listen->tls_cert_file);
		if (listen->tls_protocols)
			od_log(logger, "config", NULL, NULL,
			       "  tls_protocols    %s", listen->tls_protocols);
		od_log(logger, "config", NULL, NULL, "");
	}
log_routes:;
	od_list_foreach(&config->routes, i)
	{
		od_config_route_t *route;
		route = od_container_of(i, od_config_route_t, link);
		if (route->obsolete)
			continue;
		od_log(logger, "config", NULL, NULL, "route %s.%s",
		       route->db_name,
		       route->user_name);
		od_log(logger, "config", NULL, NULL,
		       "  authentication   %s", route->auth);
		if (route->auth_common_name_default)
			od_log(logger, "config", NULL, NULL,
			       "  auth_common_name default");
		od_list_t *j;
		od_list_foreach(&route->auth_common_names, j) {
			od_config_auth_t *auth;
			auth = od_container_of(j, od_config_auth_t, link);
			od_log(logger, "config", NULL, NULL,
			       "  auth_common_name %s", auth->common_name);
		}
		if (route->auth_query)
			od_log(logger, "config", NULL, NULL,
			       "  auth_query       %s", route->auth_query);
		if (route->auth_query_db)
			od_log(logger, "config", NULL, NULL,
			       "  auth_query_db    %s", route->auth_query_db);
		if (route->auth_query_user)
			od_log(logger, "config", NULL, NULL,
			       "  auth_query_user  %s", route->auth_query_user);
		od_log(logger, "config", NULL, NULL,
		       "  pool             %s", route->pool_sz);
		od_log(logger, "config", NULL, NULL,
		       "  pool_size        %d", route->pool_size);
		od_log(logger, "config", NULL, NULL,
		       "  pool_timeout     %d", route->pool_timeout);
		od_log(logger, "config", NULL, NULL,
		       "  pool_ttl         %d", route->pool_ttl);
		od_log(logger, "config", NULL, NULL,
		       "  pool_cancel      %s",
			   route->pool_cancel ? "yes" : "no");
		od_log(logger, "config", NULL, NULL,
		       "  pool_rollback    %s",
			   route->pool_rollback ? "yes" : "no");
		if (route->client_max_set)
			od_log(logger, "config", NULL, NULL,
			       "  client_max       %d", route->client_max);
		od_log(logger, "config", NULL, NULL,
		       "  client_fwd_error %s",
		       od_config_yes_no(route->client_fwd_error));
		od_log(logger, "config", NULL, NULL,
		       "  storage          %s", route->storage_name);
		od_log(logger, "config", NULL, NULL,
		       "  type             %s", route->storage->type);
		od_log(logger, "config", NULL, NULL,
		       "  host             %s",
		       route->storage->host ? route->storage->host : "<unix socket>");
		od_log(logger, "config", NULL, NULL,
		       "  port             %d", route->storage->port);
		if (route->storage->tls)
			od_log(logger, "config", NULL, NULL,
			       "  tls              %s", route->storage->tls);
		if (route->storage->tls_ca_file)
			od_log(logger,"config", NULL, NULL,
			       "  tls_ca_file      %s", route->storage->tls_ca_file);
		if (route->storage->tls_key_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_key_file     %s", route->storage->tls_key_file);
		if (route->storage->tls_cert_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_cert_file    %s", route->storage->tls_cert_file);
		if (route->storage->tls_protocols)
			od_log(logger, "config", NULL, NULL,
			       "  tls_protocols    %s", route->storage->tls_protocols);
		if (route->storage_db)
			od_log(logger, "config", NULL, NULL,
			       "  storage_db       %s", route->storage_db);
		if (route->storage_user)
			od_log(logger, "config", NULL, NULL,
			       "  storage_user     %s", route->storage_user);
		od_log(logger, "config", NULL, NULL,
		       "  log_debug        %s",
		       od_config_yes_no(route->log_debug));
		od_log(logger, "config", NULL, NULL, "");
	}
}
