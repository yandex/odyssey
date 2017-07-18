
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
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"

void od_scheme_init(od_scheme_t *scheme)
{
	scheme->daemonize = 0;
	scheme->log_debug = 0;
	scheme->log_config = 0;
	scheme->log_session = 1;
	scheme->log_file = NULL;
	scheme->log_statistics = 0;
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
	scheme->tls_verify = OD_TDISABLE;
	scheme->tls = NULL;
	scheme->tls_ca_file = NULL;
	scheme->tls_key_file = NULL;
	scheme->tls_cert_file = NULL;
	scheme->tls_protocols = NULL;
	scheme->db_default = NULL;
	od_list_init(&scheme->dbs);
	od_list_init(&scheme->storages);
}

static void
od_schemestorage_free(od_schemestorage_t*);

void od_scheme_free(od_scheme_t *scheme)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&scheme->dbs, i, n) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);
		od_schemedb_free(db);
	}
	od_list_foreach_safe(&scheme->storages, i, n) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		od_schemestorage_free(storage);
	}
	if (scheme->log_file)
		free(scheme->log_file);
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

od_schemestorage_t*
od_schemestorage_add(od_scheme_t *scheme, int version)
{
	od_schemestorage_t *storage;
	storage = (od_schemestorage_t*)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	od_list_init(&storage->link);
	od_list_append(&scheme->storages, &storage->link);
	storage->version = version;
	return storage;
}

od_schemestorage_t*
od_schemestorage_match(od_scheme_t *scheme, char *name, int version)
{
	/* match maximum storage scheme version which is
	 * lower then 'version' */
	od_schemestorage_t *match = NULL;
	od_list_t *i;
	od_list_foreach(&scheme->storages, i) {
		od_schemestorage_t *storage;
		storage = od_container_of(i, od_schemestorage_t, link);
		if (strcmp(storage->name, name) != 0)
			continue;
		if (storage->version > version)
			continue;
		if (match) {
			if (match->version < storage->version)
				match = storage;
		} else {
			match = storage;
		}
	}
	return match;
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

static inline int
od_schemestorage_compare(od_schemestorage_t *a, od_schemestorage_t *b)
{
	/* type */
	if (a->type != b->type)
		return 0;

	/* host */
	if (strcmp(a->host, b->host) != 0)
		return 0;

	/* port */
	if (a->port != b->port)
		return 0;

	/* tls_verify */
	if (a->tls_verify != b->tls_verify)
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

static inline void
od_schemedb_mark_obsolete(od_schemedb_t *db)
{
	assert(! db->is_obsolete);
	db->is_obsolete = 1;
}

od_schemedb_t*
od_schemedb_add(od_scheme_t *scheme, int version)
{
	od_schemedb_t *db;
	db = (od_schemedb_t*)malloc(sizeof(*db));
	if (db == NULL)
		return NULL;
	memset(db, 0, sizeof(*db));
	od_list_init(&db->users);
	od_list_init(&db->link);
	od_list_append(&scheme->dbs, &db->link);
	db->version = version;
	return db;
}

od_schemedb_t*
od_schemedb_match(od_scheme_t *scheme, char *name, int version)
{
	/* match maximum db scheme version which is
	 * lower then 'version' */
	od_schemedb_t *match = NULL;
	od_list_t *i;
	od_list_foreach(&scheme->dbs, i) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);
		if (db->is_default)
			continue;
		if (strcmp(db->name, name) != 0)
			continue;
		if (db->version > version)
			continue;
		if (match) {
			if (match->version < db->version)
				match = db;
		} else {
			match = db;
		}
	}
	return match;
}

static inline int
od_schemeuser_compare(od_schemeuser_t*, od_schemeuser_t*);

static inline int
od_schemedb_compare(od_schemedb_t *scheme, od_schemedb_t *src)
{
	od_list_t *i;
	od_list_foreach(&scheme->users, i) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		od_schemeuser_t *user_new;
		user_new = od_schemeuser_match(src, user->user);
		if (user_new == NULL)
			return 0;
		if (! od_schemeuser_compare(user, user_new))
			return 0;
	}
	return 1;
}

static void
od_schemeuser_free(od_schemeuser_t*);

void od_schemedb_free(od_schemedb_t *db)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&db->users, i, n) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		od_schemeuser_free(user);
	}
	if (db->name)
		free(db->name);
	od_list_unlink(&db->link);
	free(db);
}

od_schemeuser_t*
od_schemeuser_add(od_schemedb_t *db)
{
	od_schemeuser_t *user;
	user = (od_schemeuser_t*)malloc(sizeof(*user));
	if (user == NULL)
		return NULL;
	memset(user, 0, sizeof(*user));
	user->pool_size = 100;
	user->pool_cancel = 1;
	user->pool_discard = 1;
	user->pool_rollback = 1;
	od_list_init(&user->link);
	od_list_append(&db->users, &user->link);
	return user;
}

od_schemeuser_t*
od_schemeuser_match(od_schemedb_t *db, char *name)
{
	od_list_t *i;
	od_list_foreach(&db->users, i) {
		od_schemeuser_t *user;
		user = od_container_of(i, od_schemeuser_t, link);
		if (user->is_default)
			continue;
		if (strcmp(user->user, name) == 0)
			return user;
	}
	return NULL;
}

static void
od_schemeuser_free(od_schemeuser_t *user)
{
	if (user->user)
		free(user->user);
	if (user->user_password)
		free(user->user_password);
	if (user->auth)
		free(user->auth);
	if (user->storage)
		od_schemestorage_free(user->storage);
	if (user->storage_name)
		free(user->storage_name);
	if (user->storage_db)
		free(user->storage_db);
	if (user->storage_user)
		free(user->storage_user);
	if (user->storage_password)
		free(user->storage_password);
	if (user->pool_sz)
		free(user->pool_sz);
	free(user);
}

static inline int
od_schemestorage_compare(od_schemestorage_t*, od_schemestorage_t*);

static inline int
od_schemeuser_compare(od_schemeuser_t *a, od_schemeuser_t *b)
{
	/* user_password */
	if (a->user_password && b->user_password) {
		if (strcmp(a->user_password, b->user_password) != 0)
			return 0;
	} else
	if (a->user_password || b->user_password) {
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
	if (a->storage_db || b->storage) {
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

	/* default */
	if (a->is_default != b->is_default)
		return 0;

	return 1;
}

int od_scheme_validate(od_scheme_t *scheme, od_log_t *log)
{
	/* workers */
	if (scheme->workers == 0) {
		od_error(log, "config", "bad workers number");
		return -1;
	}

	/* listen */
	if (scheme->host == NULL) {
		od_error(log, "config", "listen host is not defined");
		return -1;
	}

	/* tls */
	if (scheme->tls) {
		if (strcmp(scheme->tls, "disable") == 0) {
			scheme->tls_verify = OD_TDISABLE;
		} else
		if (strcmp(scheme->tls, "allow") == 0) {
			scheme->tls_verify = OD_TALLOW;
		} else
		if (strcmp(scheme->tls, "require") == 0) {
			scheme->tls_verify = OD_TREQUIRE;
		} else
		if (strcmp(scheme->tls, "verify_ca") == 0) {
			scheme->tls_verify = OD_TVERIFY_CA;
		} else
		if (strcmp(scheme->tls, "verify_full") == 0) {
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
		if (storage->storage_type == OD_SREMOTE &&
		    storage->host == NULL) {
			od_error(log, "config", "storage '%s': no remote host is specified",
			         storage->name);
			return -1;
		}
		if (storage->tls) {
			if (strcmp(storage->tls, "disable") == 0) {
				storage->tls_verify = OD_TDISABLE;
			} else
			if (strcmp(storage->tls, "allow") == 0) {
				storage->tls_verify = OD_TALLOW;
			} else
			if (strcmp(storage->tls, "require") == 0) {
				storage->tls_verify = OD_TREQUIRE;
			} else
			if (strcmp(storage->tls, "verify_ca") == 0) {
				storage->tls_verify = OD_TVERIFY_CA;
			} else
			if (strcmp(storage->tls, "verify_full") == 0) {
				storage->tls_verify = OD_TVERIFY_FULL;
			} else {
				od_error(log, "config", "unknown storage tls mode");
				return -1;
			}
		}
	}

	/* databases */
	od_list_foreach(&scheme->dbs, i) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);
		if (db->is_default) {
			if (scheme->db_default) {
				od_error(log, "config", "more than one default db");
				return -1;
			}
			scheme->db_default = db;
		}

		/* routing table (per db and user) */
		if (od_list_empty(&scheme->dbs)) {
			od_error(log, "config", "no databases defined");
			return -1;
		}
		od_list_t *j;
		od_list_foreach(&db->users, j) {
			od_schemeuser_t *user;
			user = od_container_of(j, od_schemeuser_t, link);
			if (user->is_default) {
				if (db->user_default) {
					od_error(log, "config", "more than one default user for db '%s'",
					         db->name);
					return -1;
				}
				db->user_default = user;
			}
			if (user->storage_name == NULL) {
				od_error(log, "config", "db '%s' user '%s': no route storage is specified",
				         db->name, user->user);
				return -1;
			}
			/* match storage and make a reference */
			user->storage = od_schemestorage_match(scheme, user->storage_name, db->version);
			if (user->storage == NULL) {
				od_error(log, "config", "db '%s' user '%s': no route storage '%s' found",
				         db->name, user->user);
				return -1;
			}

			/* pooling mode */
			if (! user->pool_sz) {
				od_error(log, "config", "db '%s' user '%s': pooling mode is not set",
				         db->name, user->user);
				return -1;
			}
			if (strcmp(user->pool_sz, "session") == 0) {
				user->pool = OD_PSESSION;
			} else
			if (strcmp(user->pool_sz, "transaction") == 0) {
				user->pool = OD_PTRANSACTION;
			} else {
				od_error(log, "config", "db '%s' user '%s': unknown pooling mode",
				         db->name, user->user);
				return -1;
			}

			if (! user->auth) {
				od_error(log, "config", "db '%s' user '%s' authentication mode is not defined",
				         db->name, user->user);
				return -1;
			}
			/* auth */
			if (strcmp(user->auth, "none") == 0) {
				user->auth_mode = OD_ANONE;
			} else
			if (strcmp(user->auth, "block") == 0) {
				user->auth_mode = OD_ABLOCK;
			} else
			if (strcmp(user->auth, "clear_text") == 0) {
				user->auth_mode = OD_ACLEAR_TEXT;
				if (user->user_password == NULL) {
					od_error(log, "config", "db '%s' user '%s' password is not set",
					         db->name, user->user);
					return -1;
				}
			} else
			if (strcmp(user->auth, "md5") == 0) {
				user->auth_mode = OD_AMD5;
				if (user->user_password == NULL) {
					od_error(log, "config", "db '%s' user '%s' password is not set",
					         db->name, user->user);
					return -1;
				}
			} else {
				od_error(log, "config", "db '%s' user '%s' has unknown authentication mode",
				         db->name, user->user);
				return -1;
			}
		}
		if (od_list_empty(&db->users)) {
			od_error(log, "config", "no users defined for db %s", db->name);
			return -1;
		}
	}

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
	if (scheme->log_session)
		od_log(log, "log_session     %s",
		       od_scheme_yes_no(scheme->log_session));
	if (scheme->log_statistics)
		od_log(log, "log_statistics  %d", scheme->log_statistics);
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
	od_log(log, "  host          %s", scheme->host);
	od_log(log, "  port          %d", scheme->port);
	od_log(log, "  backlog       %d", scheme->backlog);
	od_log(log, "  nodelay       %d", scheme->nodelay);
	od_log(log, "  keepalive     %d", scheme->keepalive);
	if (scheme->tls)
		od_log(log, "  tls           %s", scheme->tls);
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
		od_log(log, "storage %s", storage->name);
		od_log(log, "  type          %s", storage->type);
		if (storage->host)
			od_log(log, "  host          %s", storage->host);
		if (storage->port)
			od_log(log, "  port          %d", storage->port);
		if (storage->tls)
			od_log(log, "  tls           %s", storage->tls);
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

	od_list_foreach(&scheme->dbs, i) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);
		od_log(log, "database %s", db->name);
		od_list_t *j;
		od_list_foreach(&db->users, j) {
			od_schemeuser_t *user;
			user = od_container_of(j, od_schemeuser_t, link);
			od_log(log, "  user %s", user->user);
			od_log(log, "    authentication %s", user->auth);
			od_log(log, "    storage        %s", user->storage_name);
			if (user->storage_db)
				od_log(log, "    storage_db     %s", user->storage_db);
			if (user->storage_user)
				od_log(log, "    storage_user   %s", user->storage_user);
			od_log(log, "    pool           %s", user->pool_sz);
			od_log(log, "    pool_size      %d", user->pool_size);
			od_log(log, "    pool_timeout   %d", user->pool_timeout);
			od_log(log, "    pool_ttl       %d", user->pool_ttl);
			od_log(log, "    pool_cancel    %s",
			       user->pool_cancel ? "yes" : "no");
			od_log(log, "    pool_rollback  %s",
			       user->pool_rollback ? "yes" : "no");
			od_log(log, "    pool_discard   %s",
			       user->pool_discard ? "yes" : "no");
			if (user->client_max_set)
				od_log(log, "    client_max     %d", user->client_max);
			od_log(log, "");
		}
	}
}

void od_scheme_merge(od_scheme_t *scheme, od_log_t *log, od_scheme_t *src)
{
	int count_obsolete = 0;
	int count_new = 0;

	/* mark all databases obsolete */
	od_list_t *i, *n;
	od_list_foreach(&scheme->dbs, i) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);
		od_schemedb_mark_obsolete(db);
		count_obsolete++;
	}

	/* select new databases */
	od_list_foreach_safe(&src->dbs, i, n) {
		od_schemedb_t *db;
		db = od_container_of(i, od_schemedb_t, link);

		/* find and compare current database */
		od_schemedb_t *origin;
		origin = od_schemedb_match(scheme, db->name, INT_MAX);
		if (origin) {
			if (od_schemedb_compare(origin, db)) {
				origin->is_obsolete = 0;
				count_obsolete--;
				continue;
			}
			/* add new version, origin version still exists */
		} else {
			/* add new version */
		}
		od_list_unlink(&db->link);
		od_list_init(&db->link);
		od_list_append(&scheme->dbs, &db->link);
		count_new++;
	}

	od_log(log, "configuration: added %d and obsolete %d databases",
	       count_new, count_obsolete);
}
