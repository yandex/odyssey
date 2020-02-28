
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
od_rules_init(od_rules_t *rules)
{
	od_list_init(&rules->storages);
	od_list_init(&rules->rules);
}

static inline void
od_rules_rule_free(od_rule_t*);

void
od_rules_free(od_rules_t *rules)
{
	od_list_t *i, *n;
	od_list_foreach_safe(&rules->rules, i, n) {
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		od_rules_rule_free(rule);
	}
}

static inline od_rule_storage_t*
od_rules_storage_allocate(void)
{
	od_rule_storage_t *storage;
	storage = (od_rule_storage_t*)malloc(sizeof(*storage));
	if (storage == NULL)
		return NULL;
	memset(storage, 0, sizeof(*storage));
	od_list_init(&storage->link);
	return storage;
}

void
od_rules_storage_free(od_rule_storage_t *storage)
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

od_rule_storage_t*
od_rules_storage_add(od_rules_t *rules)
{
	od_rule_storage_t *storage;
	storage = od_rules_storage_allocate();
	if (storage == NULL)
		return NULL;
	od_list_append(&rules->storages, &storage->link);
	return storage;
}

od_rule_storage_t*
od_rules_storage_match(od_rules_t *rules, char *name)
{
	od_list_t *i;
	od_list_foreach(&rules->storages, i) {
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		if (strcmp(storage->name, name) == 0)
			return storage;
	}
	return NULL;
}

od_rule_storage_t*
od_rules_storage_copy(od_rule_storage_t *storage)
{
	od_rule_storage_t *copy;
	copy = od_rules_storage_allocate();
	if (copy == NULL)
		return NULL;
	copy->storage_type = storage->storage_type;
	copy->name = strdup(storage->name);
	copy->server_max_routing = storage->server_max_routing;
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
	od_rules_storage_free(copy);
	return NULL;
}

od_rule_auth_t*
od_rules_auth_add(od_rule_t *rule)
{
	od_rule_auth_t *auth;
	auth = (od_rule_auth_t*)malloc(sizeof(*auth));
	if (auth == NULL)
		return NULL;
	memset(auth, 0, sizeof(*auth));
	od_list_init(&auth->link);
	od_list_append(&rule->auth_common_names, &auth->link);
	rule->auth_common_names_count++;
	return auth;
}

void
od_rules_auth_free(od_rule_auth_t *auth)
{
	if (auth->common_name)
		free(auth->common_name);
	free(auth);
}

static inline od_rule_auth_t*
od_rules_auth_find(od_rule_t *rule, char *name)
{
	od_list_t *i;
	od_list_foreach(&rule->auth_common_names, i) {
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		if (! strcasecmp(auth->common_name, name))
			return auth;
	}
	return NULL;
}

od_rule_t*
od_rules_add(od_rules_t *rules)
{
	od_rule_t *rule;
	rule = (od_rule_t*)malloc(sizeof(*rule));
	if (rule == NULL)
		return NULL;
	memset(rule, 0, sizeof(*rule));
	rule->pool_size = 0;
	rule->pool_timeout = 0;
	rule->pool_discard = 1;
	rule->pool_cancel = 1;
	rule->pool_rollback = 1;
	rule->obsolete = 0;
	rule->mark = 0;
	rule->refs = 0;
	rule->auth_common_name_default = 0;
	rule->auth_common_names_count = 0;
	rule->server_lifetime_us = 3600 * 1000000L;
	od_list_init(&rule->auth_common_names);
	od_list_init(&rule->link);
	od_list_append(&rules->rules, &rule->link);
	return rule;
}

static inline void
od_rules_rule_free(od_rule_t *rule)
{
	if (rule->db_name)
		free(rule->db_name);
	if (rule->user_name)
		free(rule->user_name);
	if (rule->password)
		free(rule->password);
	if (rule->auth)
		free(rule->auth);
	if (rule->auth_query)
		free(rule->auth_query);
	if (rule->auth_query_db)
		free(rule->auth_query_db);
	if (rule->auth_query_user)
		free(rule->auth_query_user);
	if (rule->storage)
		od_rules_storage_free(rule->storage);
	if (rule->storage_name)
		free(rule->storage_name);
	if (rule->storage_db)
		free(rule->storage_db);
	if (rule->storage_user)
		free(rule->storage_user);
	if (rule->storage_password)
		free(rule->storage_password);
	if (rule->pool_sz)
		free(rule->pool_sz);
	od_list_t *i, *n;
	od_list_foreach_safe(&rule->auth_common_names, i, n) {
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		od_rules_auth_free(auth);
	}
	od_list_unlink(&rule->link);
	free(rule);
}

void
od_rules_ref(od_rule_t *rule)
{
	rule->refs++;
}

void
od_rules_unref(od_rule_t *rule)
{
	assert(rule->refs > 0);
	rule->refs--;
	if (! rule->obsolete)
		return;
	if (rule->refs == 0)
		od_rules_rule_free(rule);
}

od_rule_t*
od_rules_forward(od_rules_t *rules, char *db_name, char *user_name)
{
	od_rule_t *rule_db_user = NULL;
	od_rule_t *rule_db_default = NULL;
	od_rule_t *rule_default_user = NULL;
	od_rule_t *rule_default_default = NULL;

	od_list_t *i;
	od_list_foreach(&rules->rules, i) {
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->obsolete)
			continue;
		if (rule->db_is_default) {
			if (rule->user_is_default)
				rule_default_default = rule;
			else
			if (strcmp(rule->user_name, user_name) == 0)
				rule_default_user = rule;
		} else
		if (strcmp(rule->db_name, db_name) == 0) {
			if (rule->user_is_default)
				rule_db_default = rule;
			else
			if (strcmp(rule->user_name, user_name) == 0)
				rule_db_user = rule;
		}
	}

	if (rule_db_user)
		return rule_db_user;

	if (rule_db_default)
		return rule_db_default;

	if (rule_default_user)
		return rule_default_user;

	return rule_default_default;
}

od_rule_t*
od_rules_match(od_rules_t *rules, char *db_name, char *user_name)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i) {
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (strcmp(rule->db_name, db_name) == 0 &&
		    strcmp(rule->user_name, user_name) == 0)
			return rule;
	}
	return NULL;
}

static inline od_rule_t*
od_rules_match_active(od_rules_t *rules, char *db_name, char *user_name)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i) {
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->obsolete)
			continue;
		if (strcmp(rule->db_name, db_name) == 0 &&
		    strcmp(rule->user_name, user_name) == 0)
			return rule;
	}
	return NULL;
}

static inline int
od_rules_storage_compare(od_rule_storage_t *a, od_rule_storage_t *b)
{
	/* type */
	if (a->storage_type != b->storage_type)
		return 0;

	/* type */
	if (a->server_max_routing != b->server_max_routing)
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
od_rules_rule_compare(od_rule_t *a, od_rule_t *b)
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
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		if (! od_rules_auth_find(b, auth->common_name))
			return 0;
	}

	/* storage */
	if (strcmp(a->storage_name, b->storage_name) != 0)
		return 0;

	if (! od_rules_storage_compare(a->storage, b->storage))
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

	/* pool_discard */
	if (a->pool_discard != b->pool_discard)
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

__attribute__((hot)) int
od_rules_merge(od_rules_t *rules, od_rules_t *src)
{
	int count_mark = 0;
	int count_deleted = 0;
	int count_new = 0;

	/* mark all rules for obsoletion */
	od_list_t *i;
	od_list_foreach(&rules->rules, i) {
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		rule->mark = 1;
		count_mark++;
	}

	/* select new rules */
	od_list_t *n;
	od_list_foreach_safe(&src->rules, i, n)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);

		/* find and compare origin rule */
		od_rule_t *origin;
		origin = od_rules_match_active(rules, rule->db_name, rule->user_name);
		if (origin) {
			if (od_rules_rule_compare(origin, rule)) {
				origin->mark = 0;
				count_mark--;
				continue;
			}

			/* add new version, origin version still exists */
		} else {
			/* add new version */
		}

		od_list_unlink(&rule->link);
		od_list_init(&rule->link);
		od_list_append(&rules->rules, &rule->link);
		count_new++;
	}

	/* try to free obsolete schemes, which are unused by any
	 * rule at the moment */
	if (count_mark > 0) {
		od_list_foreach_safe(&rules->rules, i, n) {
			od_rule_t *rule;
			rule = od_container_of(i, od_rule_t, link);

			int is_obsolete = rule->obsolete || rule->mark;
			rule->mark = 0;
			rule->obsolete = is_obsolete;

			if (is_obsolete && rule->refs == 0) {
				od_rules_rule_free(rule);
				count_deleted++;
				count_mark--;
			}
		}
	}

	return count_new + count_mark + count_deleted;
}

int
od_rules_validate(od_rules_t *rules, od_config_t *config, od_logger_t *logger)
{
	/* storages */
	od_list_t *i;
	od_list_foreach(&rules->storages, i)
	{
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		if (storage->server_max_routing == 0)
			storage->server_max_routing = config->workers;
		if (storage->type == NULL) {
			od_error(logger, "rules", NULL, NULL,
			         "storage '%s': no type is specified",
			         storage->name);
			return -1;
		}
		if (strcmp(storage->type, "remote") == 0) {
			storage->storage_type = OD_RULE_STORAGE_REMOTE;
		} else
		if (strcmp(storage->type, "local") == 0) {
			storage->storage_type = OD_RULE_STORAGE_LOCAL;
		} else {
			od_error(logger, "rules", NULL, NULL, "unknown storage type");
			return -1;
		}
		if (storage->storage_type == OD_RULE_STORAGE_REMOTE) {
			if (storage->host == NULL) {
				if (config->unix_socket_dir == NULL) {
					od_error(logger, "rules", NULL, NULL,
					         "storage '%s': no host specified and unix_socket_dir is not set",
					         storage->name);
					return -1;
				}
			}
		}
		if (storage->tls) {
			if (strcmp(storage->tls, "disable") == 0) {
				storage->tls_mode = OD_RULE_TLS_DISABLE;
			} else
			if (strcmp(storage->tls, "allow") == 0) {
				storage->tls_mode = OD_RULE_TLS_ALLOW;
			} else
			if (strcmp(storage->tls, "require") == 0) {
				storage->tls_mode = OD_RULE_TLS_REQUIRE;
			} else
			if (strcmp(storage->tls, "verify_ca") == 0) {
				storage->tls_mode = OD_RULE_TLS_VERIFY_CA;
			} else
			if (strcmp(storage->tls, "verify_full") == 0) {
				storage->tls_mode = OD_RULE_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "rules", NULL, NULL, "unknown storage tls mode");
				return -1;
			}
		}
	}

	/* rules */
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);

		/* match storage and make a copy of in the user rules */
		if (rule->storage_name == NULL) {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': no rule storage is specified",
			         rule->db_name, rule->user_name);
			return -1;
		}
		od_rule_storage_t *storage;
		storage = od_rules_storage_match(rules, rule->storage_name);
		if (storage == NULL) {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': no rule storage '%s' found",
			         rule->db_name, rule->user_name, rule->storage_name);
			return -1;
		}
		rule->storage = od_rules_storage_copy(storage);
		if (rule->storage == NULL)
			return -1;

		/* pooling mode */
		if (! rule->pool_sz) {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': pooling mode is not set",
			         rule->db_name, rule->user_name);
			return -1;
		}
		if (strcmp(rule->pool_sz, "session") == 0) {
			rule->pool = OD_RULE_POOL_SESSION;
		} else
		if (strcmp(rule->pool_sz, "transaction") == 0) {
			rule->pool = OD_RULE_POOL_TRANSACTION;
		} else {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': unknown pooling mode",
			         rule->db_name, rule->user_name);
			return -1;
		}

		/* auth */
		if (! rule->auth) {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': authentication mode is not defined",
			         rule->db_name, rule->user_name);
			return -1;
		}
		if (strcmp(rule->auth, "none") == 0) {
			rule->auth_mode = OD_RULE_AUTH_NONE;
		} else
		if (strcmp(rule->auth, "block") == 0) {
			rule->auth_mode = OD_RULE_AUTH_BLOCK;
		} else
		if (strcmp(rule->auth, "clear_text") == 0) {
			rule->auth_mode = OD_RULE_AUTH_CLEAR_TEXT;

			if (rule->auth_query != NULL &&
			    rule->auth_pam_service != NULL) {
				od_error(logger, "rules", NULL, NULL, 
						"auth query and pam service auth method cannot be used simultaneously",
						rule->db_name, rule->user_name);
				return -1;
			}

			if (rule->password == NULL &&
			    rule->auth_query == NULL &&
			    rule->auth_pam_service == NULL) {
				od_error(logger, "rules", NULL, NULL,
				         "rule '%s.%s': password is not set",
				         rule->db_name, rule->user_name);
				return -1;
			}
		} else
		if (strcmp(rule->auth, "md5") == 0) {
			rule->auth_mode = OD_RULE_AUTH_MD5;
			if (rule->password == NULL && rule->auth_query == NULL) {
				od_error(logger, "rules", NULL, NULL,
				         "rule '%s.%s': password is not set",
				         rule->db_name, rule->user_name);
				return -1;
			}
		} else
		if (strcmp(rule->auth, "scram-sha-256") == 0) {
			rule->auth_mode = OD_RULE_AUTH_SCRAM_SHA_256;
			if (rule->password == NULL && rule->auth_query == NULL) {
				od_error(logger, "rules", NULL, NULL,
				         "rule '%s.%s': password is not set",
				         rule->db_name, rule->user_name);
				return -1;
			}
		} else
		if (strcmp(rule->auth, "cert") == 0) {
			rule->auth_mode = OD_RULE_AUTH_CERT;
		} else {
			od_error(logger, "rules", NULL, NULL,
			         "rule '%s.%s': has unknown authentication mode",
			         rule->db_name, rule->user_name);
			return -1;
		}

		/* auth_query */
		if (rule->auth_query) {
			if (rule->auth_query_user == NULL) {
				od_error(logger, "rules", NULL, NULL,
				         "rule '%s.%s': auth_query_user is not set",
				         rule->db_name, rule->user_name);
				return -1;
			}
			if (rule->auth_query_db == NULL) {
				od_error(logger, "rules", NULL, NULL,
				         "rule '%s.%s': auth_query_db is not set",
				         rule->db_name, rule->user_name);
				return -1;
			}
		}
	}

	/* cleanup declarative storages rules data */
	od_list_t *n;
	od_list_foreach_safe(&rules->storages, i, n) {
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		od_rules_storage_free(storage);
	}
	od_list_init(&rules->storages);
	return 0;
}

static inline char*
od_rules_yes_no(int value) {
	return value ? "yes" : "no";
}

void
od_rules_print(od_rules_t *rules, od_logger_t *logger)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->obsolete)
			continue;
		od_log(logger, "rules", NULL, NULL, "<%s.%s>",
		       rule->db_name,
		       rule->user_name);
		od_log(logger, "rules", NULL, NULL,
		       "  authentication   %s", rule->auth);
		if (rule->auth_common_name_default)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_common_name default");
		od_list_t *j;
		od_list_foreach(&rule->auth_common_names, j) {
			od_rule_auth_t *auth;
			auth = od_container_of(j, od_rule_auth_t, link);
			od_log(logger, "rules", NULL, NULL,
			       "  auth_common_name %s", auth->common_name);
		}
		if (rule->auth_query)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query       %s", rule->auth_query);
		if (rule->auth_query_db)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query_db    %s", rule->auth_query_db);
		if (rule->auth_query_user)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query_user  %s", rule->auth_query_user);
		od_log(logger, "rules", NULL, NULL,
		       "  pool             %s", rule->pool_sz);
		od_log(logger, "rules", NULL, NULL,
		       "  pool_size        %d", rule->pool_size);
		od_log(logger, "rules", NULL, NULL,
		       "  pool_timeout     %d", rule->pool_timeout);
		od_log(logger, "rules", NULL, NULL,
		       "  pool_ttl         %d", rule->pool_ttl);
		od_log(logger, "rules", NULL, NULL,
		       "  pool_discard     %s",
			   rule->pool_discard ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool_cancel      %s",
			   rule->pool_cancel ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool_rollback    %s",
			   rule->pool_rollback ? "yes" : "no");
		if (rule->client_max_set)
			od_log(logger, "rules", NULL, NULL,
			       "  client_max       %d", rule->client_max);
		od_log(logger, "rules", NULL, NULL,
		       "  client_fwd_error %s",
		       od_rules_yes_no(rule->client_fwd_error));
		od_log(logger, "rules", NULL, NULL,
		       "  storage          %s", rule->storage_name);
		od_log(logger, "rules", NULL, NULL,
		       "  type             %s", rule->storage->type);
		od_log(logger, "rules", NULL, NULL,
		       "  host             %s",
		       rule->storage->host ? rule->storage->host : "<unix socket>");
		od_log(logger, "rules", NULL, NULL,
		       "  port             %d", rule->storage->port);
		if (rule->storage->tls)
			od_log(logger, "rules", NULL, NULL,
			       "  tls              %s", rule->storage->tls);
		if (rule->storage->tls_ca_file)
			od_log(logger,"rules", NULL, NULL,
			       "  tls_ca_file      %s", rule->storage->tls_ca_file);
		if (rule->storage->tls_key_file)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_key_file     %s", rule->storage->tls_key_file);
		if (rule->storage->tls_cert_file)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_cert_file    %s", rule->storage->tls_cert_file);
		if (rule->storage->tls_protocols)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_protocols    %s", rule->storage->tls_protocols);
		if (rule->storage_db)
			od_log(logger, "rules", NULL, NULL,
			       "  storage_db       %s", rule->storage_db);
		if (rule->storage_user)
			od_log(logger, "rules", NULL, NULL,
			       "  storage_user     %s", rule->storage_user);
		od_log(logger, "rules", NULL, NULL,
		       "  log_debug        %s",
		       od_rules_yes_no(rule->log_debug));
		od_log(logger, "rules", NULL, NULL, "");
	}
}
