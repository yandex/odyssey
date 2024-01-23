
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

void od_rules_init(od_rules_t *rules)
{
	pthread_mutex_init(&rules->mu, NULL);
	od_list_init(&rules->storages);
#ifdef LDAP_FOUND
	od_list_init(&rules->ldap_endpoints);
#endif
	od_list_init(&rules->rules);
}

void od_rules_rule_free(od_rule_t *);

void od_rules_free(od_rules_t *rules)
{
	pthread_mutex_destroy(&rules->mu);
	od_list_t *i, *n;
	od_list_foreach_safe(&rules->rules, i, n)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		od_rules_rule_free(rule);
	}
}

#ifdef LDAP_FOUND
od_ldap_endpoint_t *od_rules_ldap_endpoint_add(od_rules_t *rules,
					       od_ldap_endpoint_t *ldap)
{
	od_list_append(&rules->ldap_endpoints, &ldap->link);
	return ldap;
}

od_ldap_storage_credentials_t *
od_rule_ldap_storage_credentials_add(od_rule_t *rule,
				     od_ldap_storage_credentials_t *lsc)
{
	od_list_append(&rule->ldap_storage_creds_list, &lsc->link);
	return lsc;
}
#endif

od_rule_storage_t *od_rules_storage_add(od_rules_t *rules,
					od_rule_storage_t *storage)
{
	od_list_append(&rules->storages, &storage->link);
	return storage;
}

od_rule_storage_t *od_rules_storage_match(od_rules_t *rules, char *name)
{
	od_list_t *i;
	od_list_foreach(&rules->storages, i)
	{
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		if (strcmp(storage->name, name) == 0)
			return storage;
	}
	return NULL;
}

od_retcode_t od_rules_storages_watchdogs_run(od_logger_t *logger,
					     od_rules_t *rules)
{
	od_list_t *i;
	od_list_foreach(&rules->storages, i)
	{
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		if (storage->watchdog) {
			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create(
				od_storage_watchdog_watch, storage->watchdog);
			if (coroutine_id == INVALID_COROUTINE_ID) {
				od_error(logger, "system", NULL, NULL,
					 "failed to start watchdog coroutine");
				return NOT_OK_RESPONSE;
			}
		}
	}
	return OK_RESPONSE;
}

od_rule_auth_t *od_rules_auth_add(od_rule_t *rule)
{
	od_rule_auth_t *auth;
	auth = (od_rule_auth_t *)malloc(sizeof(*auth));
	if (auth == NULL)
		return NULL;
	memset(auth, 0, sizeof(*auth));
	od_list_init(&auth->link);
	od_list_append(&rule->auth_common_names, &auth->link);
	rule->auth_common_names_count++;
	return auth;
}

void od_rules_auth_free(od_rule_auth_t *auth)
{
	if (auth->common_name)
		free(auth->common_name);
	free(auth);
}

static inline od_rule_auth_t *od_rules_auth_find(od_rule_t *rule, char *name)
{
	od_list_t *i;
	od_list_foreach(&rule->auth_common_names, i)
	{
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		if (!strcasecmp(auth->common_name, name))
			return auth;
	}
	return NULL;
}

od_group_t *od_rules_group_allocate(od_global_t *global)
{
	/* Allocate and force defaults */
	od_group_t *group;
	group = (od_group_t *)malloc(sizeof(*group));
	if (group == NULL)
		return NULL;
	memset(group, 0, sizeof(*group));
	group->global = global;
	group->check_retry = 10;
	group->online = 1;

	od_list_init(&group->link);
	return group;
}

static inline int od_rule_update_auth(od_route_t *route, void **argv)
{
	od_rule_t *rule = (od_rule_t *)argv[0];
	od_rule_t *group_rule = (od_rule_t *)argv[1];

	/* auth */
	rule->auth = group_rule->auth;
	rule->auth_mode = group_rule->auth_mode;
	rule->auth_query = group_rule->auth_query;
	rule->auth_query_db = group_rule->auth_query_db;
	rule->auth_query_user = group_rule->auth_query_user;
	rule->auth_common_name_default = group_rule->auth_common_name_default;
	rule->auth_common_names = group_rule->auth_common_names;
	rule->auth_common_names_count = group_rule->auth_common_names_count;

#ifdef PAM_FOUND
	rule->auth_pam_service = group_rule->auth_pam_service;
	rule->auth_pam_data = group_rule->auth_pam_data;
#endif

#ifdef LDAP_FOUND
	rule->ldap_endpoint_name = group_rule->ldap_endpoint_name;
	rule->ldap_endpoint = group_rule->ldap_endpoint;
	rule->ldap_pool_timeout = group_rule->ldap_pool_timeout;
	rule->ldap_pool_size = group_rule->ldap_pool_size;
	rule->ldap_pool_ttl = group_rule->ldap_pool_ttl;
	rule->ldap_storage_creds_list = group_rule->ldap_storage_creds_list;
	rule->ldap_storage_credentials_attr = group_rule->ldap_storage_credentials_attr;
#endif

	rule->auth_module = group_rule->auth_module;

	/* password */
	rule->password = group_rule->password;
	rule->password_len = group_rule->password_len;

	rule->is_group_member = 1;

	return 0;
}

void od_rules_group_checker_run(void *arg) 
{ 
	od_group_checker_run_args *args = (od_group_checker_run_args *)arg;
	od_rule_t *group_rule = args->rule;
	od_group_t *group = group_rule->group;
	od_rules_t *rules = args->rules;
	od_list_t *i_copy = args->i_copy;
	od_global_t *global = group->global;
	od_router_t *router = global->router;
	od_instance_t *instance = global->instance;

	od_debug(&instance->logger, "group_checker", NULL, NULL,
		 "start group checking");
	
	/* create internal auth client */
	od_client_t *group_checker_client;
	group_checker_client =
		od_client_allocate_internal(global, "rule-group-checker");
	if (group_checker_client == NULL) {
		od_error(&instance->logger, "group_checker", NULL, NULL,
			 "route rule group_checker failed to allocate client");
		return;
	}

	group_checker_client->global = global;
	group_checker_client->type = OD_POOL_CLIENT_INTERNAL;
	od_id_generate(&group_checker_client->id, "a");

	/* set storage user and database */
	kiwi_var_set(&group_checker_client->startup.user, KIWI_VAR_UNDEF,
		     group->route_usr, strlen(group->route_usr) + 1);

	kiwi_var_set(&group_checker_client->startup.database, KIWI_VAR_UNDEF,
		     group->route_db, strlen(group->route_db) + 1);

	machine_msg_t *msg;
	int is_group_member;
	int rc;

	/* route */
	od_router_status_t status;
	status = od_router_route(router, group_checker_client);
	od_debug(&instance->logger, "group_checker", group_checker_client, NULL,
		 "routing to internal group_checker route status: %s",
		 od_router_status_to_str(status));

	if (status != OD_ROUTER_OK) {
		od_error(&instance->logger, "group_checker", group_checker_client, NULL,
			 "route rule group_checker failed: %s",
			 od_router_status_to_str(status));
		return;
	}

	for (;;) {
		od_list_t *i = i_copy;
		od_list_foreach_with_start(&rules->rules, i)
		{
			od_rule_t *rule;
			rule = od_container_of(i, od_rule_t, link);

			if (rule->obsolete) {
				continue;
			}
			if (rule->pool->routing == OD_RULE_POOL_INTERVAL) {
				continue;
			}
			if (rule->pool->routing != OD_RULE_POOL_CLIENT_VISIBLE) {
				continue;
			}

			/* attach client to some route */
			status = od_router_attach(router, group_checker_client, false);
			od_debug(&instance->logger, "group_checker", group_checker_client, NULL,
				"attaching group_checker client to backend connection status: %s",
				od_router_status_to_str(status));

			if (status != OD_ROUTER_OK) {
				/* 1 second soft interval */
				machine_sleep(1000);
				continue;
			}
			od_server_t *server;
			server = group_checker_client->server;
			od_debug(&instance->logger, "group_checker", group_checker_client, server,
				"attached to server %s%.*s", server->id.id_prefix,
				(int)sizeof(server->id.id), server->id.id);
			
			/* connect to server, if necessary */
			if (server->io.io == NULL) {
				rc = od_backend_connect(server, "group_checker", NULL,
							group_checker_client);
				if (rc == NOT_OK_RESPONSE) {
					od_debug(
						&instance->logger, "group_checker",
						group_checker_client, server,
						"backend connect failed, retry after 1 sec");
					od_router_close(router, group_checker_client);
					/* 1 second soft interval */
					machine_sleep(1000);
					continue;
				}
			}

			for (int retry = 0; retry < group->check_retry; ++retry) {
				char* qry = (char*)malloc(OD_QRY_MAX_SZ * sizeof(char));
				od_group_qry_format(qry, group->group_query, rule->user_name);
				
				msg = od_query_do(server, "group_checker", qry, NULL);
				free(qry);

				if (msg != NULL) {
					rc = od_group_parse_val_datarow(msg, &is_group_member);
					machine_msg_free(msg);
					od_router_close(router, group_checker_client);
				} else {
					od_debug(
						&instance->logger, "group_checker",
						group_checker_client, server,
						"receive msg failed, closing backend connection");
					rc = NOT_OK_RESPONSE;
					od_router_close(router, group_checker_client);
					break;
				}

				if (rc == OK_RESPONSE) {
					od_debug(
						&instance->logger, "group_checker",
						group_checker_client, server,
						"group check result is %d",
						is_group_member);

					if (is_group_member && rule->is_group_member == 0) {
						void *argv[] = { rule, group_rule };
						od_router_foreach(router,
								od_rule_update_auth,
								argv);	
					}

					break;
				}
			}
			// retry
		}

		/* detach and unroute */
		if (group_checker_client->server) {
			od_router_detach(router, group_checker_client);
		}

		if (group->online == 0) {
			od_debug(&instance->logger, "group_checker", group_checker_client,
				 NULL,
				 "deallocating obsolete group_checker");
			od_client_free(group_checker_client);
			od_group_free(group);
			return;
		}

		/* 1 second soft interval */
		machine_sleep(1000);
	}
}

od_retcode_t od_rules_groups_checkers_run(od_logger_t *logger,
          od_rules_t *rules)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->group) {
			od_group_checker_run_args *args = malloc(sizeof(od_group_checker_run_args));
			args->rules = rules;
			args->rule = rule;
			args->i_copy = i->next;

			int64_t coroutine_id;
			coroutine_id = machine_coroutine_create(
				od_rules_group_checker_run, args);
			if (coroutine_id == INVALID_COROUTINE_ID) {
				od_error(logger, "system", NULL, NULL,
				"failed to start group_checker coroutine");
				return NOT_OK_RESPONSE;
			}

			machine_sleep(1000);
		}
	}

	machine_sleep(2000);

	return OK_RESPONSE;
}

od_rule_t *od_rules_add(od_rules_t *rules)
{
	od_rule_t *rule;
	rule = (od_rule_t *)malloc(sizeof(*rule));
	if (rule == NULL)
		return NULL;
	memset(rule, 0, sizeof(*rule));
	/* pool */
	rule->pool = od_rule_pool_alloc();
	if (rule->pool == NULL) {
		free(rule);
		return NULL;
	}

	rule->user_role = OD_RULE_ROLE_UNDEF;

	rule->obsolete = 0;
	rule->mark = 0;
	rule->refs = 0;

	rule->auth_common_name_default = 0;
	rule->auth_common_names_count = 0;
	rule->server_lifetime_us = 3600 * 1000000L;
	rule->reserve_session_server_connection = 1;
#ifdef PAM_FOUND
	rule->auth_pam_data = od_pam_auth_data_create();
#endif

#ifdef LDAP_FOUND
	rule->ldap_endpoint_name = NULL;
	rule->ldap_endpoint = NULL;
	rule->ldap_storage_credentials_attr = NULL;
	od_list_init(&rule->ldap_storage_creds_list);
#endif

	kiwi_vars_init(&rule->vars);

	rule->enable_password_passthrough = 0;

	rule->is_group_member = 0;
	od_list_init(&rule->auth_common_names);
	od_list_init(&rule->link);
	od_list_append(&rules->rules, &rule->link);

	rule->quantiles = NULL;
	return rule;
}

void od_rules_rule_free(od_rule_t *rule)
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
	if (rule->pool)
		od_rule_pool_free(rule->pool);
	if (rule->group)
		rule->group->online = 0;

	od_list_t *i, *n;
	od_list_foreach_safe(&rule->auth_common_names, i, n)
	{
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		od_rules_auth_free(auth);
	}
#ifdef PAM_FOUND
	od_pam_auth_data_free(rule->auth_pam_data);
#endif
#ifdef LDAP_FOUND
	if (rule->ldap_endpoint_name)
		free(rule->ldap_endpoint_name);
	if (rule->ldap_storage_credentials_attr)
		free(rule->ldap_storage_credentials_attr);
	if (rule->ldap_endpoint)
		od_ldap_endpoint_free(rule->ldap_endpoint);
	if (&rule->ldap_storage_creds_list) {
		od_list_foreach_safe(&rule->ldap_storage_creds_list, i, n)
		{
			od_ldap_storage_credentials_t *lsc;
			lsc = od_container_of(i, od_ldap_storage_credentials_t,
					      link);
			od_ldap_storage_credentials_free(lsc);
		}
	}
#endif
	if (rule->auth_module) {
		free(rule->auth_module);
	}
	if (rule->quantiles) {
		free(rule->quantiles);
	}
	od_list_unlink(&rule->link);
	free(rule);
}

void od_rules_ref(od_rule_t *rule)
{
	rule->refs++;
}

void od_rules_unref(od_rule_t *rule)
{
	assert(rule->refs > 0);
	rule->refs--;
	if (!rule->obsolete)
		return;
	if (rule->refs == 0)
		od_rules_rule_free(rule);
}

od_rule_t *od_rules_forward(od_rules_t *rules, char *db_name, char *user_name,
			    int pool_internal)
{
	od_rule_t *rule_db_user = NULL;
	od_rule_t *rule_db_default = NULL;
	od_rule_t *rule_default_user = NULL;
	od_rule_t *rule_default_default = NULL;

	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->obsolete)
			continue;
		if (pool_internal) {
			if (rule->pool->routing != OD_RULE_POOL_INTERVAL) {
				continue;
			}
		} else {
			if (rule->pool->routing !=
			    OD_RULE_POOL_CLIENT_VISIBLE) {
				continue;
			}
		}
		if (rule->db_is_default) {
			if (rule->user_is_default)
				rule_default_default = rule;
			else if (strcmp(rule->user_name, user_name) == 0)
				rule_default_user = rule;
		} else if (strcmp(rule->db_name, db_name) == 0) {
			if (rule->user_is_default)
				rule_db_default = rule;
			else if (strcmp(rule->user_name, user_name) == 0)
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

od_rule_t *od_rules_match(od_rules_t *rules, char *db_name, char *user_name,
			  int db_is_default, int user_is_default,
			  int pool_internal)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		/* filter out internal or client-vidible rules */
		if (pool_internal) {
			if (rule->pool->routing != OD_RULE_POOL_INTERVAL) {
				continue;
			}
		} else {
			if (rule->pool->routing !=
			    OD_RULE_POOL_CLIENT_VISIBLE) {
				continue;
			}
		}
		if (strcmp(rule->db_name, db_name) == 0 &&
		    strcmp(rule->user_name, user_name) == 0 &&
		    rule->db_is_default == db_is_default &&
		    rule->user_is_default == user_is_default)
			return rule;
	}
	return NULL;
}

static inline od_rule_t *od_rules_match_active(od_rules_t *rules, char *db_name,
					       char *user_name)
{
	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
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

static inline int od_rules_storage_compare(od_rule_storage_t *a,
					   od_rule_storage_t *b)
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
	} else if (a->host || b->host) {
		return 0;
	}

	/* port */
	if (a->port != b->port)
		return 0;

	/* tls_opts->tls_mode */
	if (a->tls_opts->tls_mode != b->tls_opts->tls_mode)
		return 0;

	/* tls_opts->tls_ca_file */
	if (a->tls_opts->tls_ca_file && b->tls_opts->tls_ca_file) {
		if (strcmp(a->tls_opts->tls_ca_file,
			   b->tls_opts->tls_ca_file) != 0)
			return 0;
	} else if (a->tls_opts->tls_ca_file || b->tls_opts->tls_ca_file) {
		return 0;
	}

	/* tls_opts->tls_key_file */
	if (a->tls_opts->tls_key_file && b->tls_opts->tls_key_file) {
		if (strcmp(a->tls_opts->tls_key_file,
			   b->tls_opts->tls_key_file) != 0)
			return 0;
	} else if (a->tls_opts->tls_key_file || b->tls_opts->tls_key_file) {
		return 0;
	}

	/* tls_opts->tls_cert_file */
	if (a->tls_opts->tls_cert_file && b->tls_opts->tls_cert_file) {
		if (strcmp(a->tls_opts->tls_cert_file,
			   b->tls_opts->tls_cert_file) != 0)
			return 0;
	} else if (a->tls_opts->tls_cert_file || b->tls_opts->tls_cert_file) {
		return 0;
	}

	/* tls_opts->tls_protocols */
	if (a->tls_opts->tls_protocols && b->tls_opts->tls_protocols) {
		if (strcmp(a->tls_opts->tls_protocols,
			   b->tls_opts->tls_protocols) != 0)
			return 0;
	} else if (a->tls_opts->tls_protocols || b->tls_opts->tls_protocols) {
		return 0;
	}

	return 1;
}

int od_rules_rule_compare(od_rule_t *a, od_rule_t *b)
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
	} else if (a->password || b->password) {
		return 0;
	}

	/* role */
	if (a->user_role != b->user_role)
		return 0;

	/* quantiles changed */
	if (a->quantiles_count == b->quantiles_count) {
		if (a->quantiles_count != 0 &&
		    memcmp(a->quantiles, b->quantiles,
			   sizeof(double) * a->quantiles_count) != 0)
			return 0;
	} else {
		return 0;
	}

	/* auth */
	if (a->auth_mode != b->auth_mode)
		return 0;

	/* auth query */
	if (a->auth_query && b->auth_query) {
		if (strcmp(a->auth_query, b->auth_query) != 0)
			return 0;
	} else if (a->auth_query || b->auth_query) {
		return 0;
	}

	/* auth query db */
	if (a->auth_query_db && b->auth_query_db) {
		if (strcmp(a->auth_query_db, b->auth_query_db) != 0)
			return 0;
	} else if (a->auth_query_db || b->auth_query_db) {
		return 0;
	}

	/* auth query user */
	if (a->auth_query_user && b->auth_query_user) {
		if (strcmp(a->auth_query_user, b->auth_query_user) != 0)
			return 0;
	} else if (a->auth_query_user || b->auth_query_user) {
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
	od_list_foreach(&a->auth_common_names, i)
	{
		od_rule_auth_t *auth;
		auth = od_container_of(i, od_rule_auth_t, link);
		if (!od_rules_auth_find(b, auth->common_name))
			return 0;
	}

	/* storage */
	if (strcmp(a->storage_name, b->storage_name) != 0)
		return 0;

	if (!od_rules_storage_compare(a->storage, b->storage))
		return 0;

	/* storage_db */
	if (a->storage_db && b->storage_db) {
		if (strcmp(a->storage_db, b->storage_db) != 0)
			return 0;
	} else if (a->storage_db || b->storage_db) {
		return 0;
	}

	/* storage_user */
	if (a->storage_user && b->storage_user) {
		if (strcmp(a->storage_user, b->storage_user) != 0)
			return 0;
	} else if (a->storage_user || b->storage_user) {
		return 0;
	}

	/* storage_password */
	if (a->storage_password && b->storage_password) {
		if (strcmp(a->storage_password, b->storage_password) != 0)
			return 0;
	} else if (a->storage_password || b->storage_password) {
		return 0;
	}

	/* pool */
	if (!od_rule_pool_compare(a->pool, b->pool)) {
		return 0;
	}

	/* client_fwd_error */
	if (a->client_fwd_error != b->client_fwd_error)
		return 0;

	/* reserve_session_server_connection */
	if (a->reserve_session_server_connection !=
	    b->reserve_session_server_connection) {
		return 0;
	}

	if (a->catchup_timeout != b->catchup_timeout) {
		return 0;
	}

	if (a->catchup_checks != b->catchup_checks) {
		return 0;
	}

	/* client_max */
	if (a->client_max != b->client_max)
		return 0;

	/* server_lifetime */
	if (a->server_lifetime_us != b->server_lifetime_us) {
		return 0;
	}

	return 1;
}

int od_rules_rule_compare_to_drop(od_rule_t *a, od_rule_t *b)
{
	/* role */
	if (a->user_role < b->user_role)
		return 0;

	return 1;
}

__attribute__((hot)) int od_rules_merge(od_rules_t *rules, od_rules_t *src,
					od_list_t *added, od_list_t *deleted,
					od_list_t *to_drop)
{
	int count_mark = 0;
	int count_deleted = 0;
	int count_new = 0;

	/* mark all rules for obsoletion */
	od_list_t *i;
	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		rule->mark = 1;
		count_mark++;

		od_hashmap_empty(rule->storage->acache);
	}

	/* select dropped rules */
	od_list_t *n;
	od_list_foreach_safe(&rules->rules, i, n)
	{
		od_rule_t *rule_old;
		rule_old = od_container_of(i, od_rule_t, link);

		int ok = 0;

		od_list_t *m;
		od_list_t *j;
		od_list_foreach_safe(&src->rules, j, m)
		{
			od_rule_t *rule_new;
			rule_new = od_container_of(j, od_rule_t, link);
			if (strcmp(rule_old->user_name, rule_new->user_name) ==
				    0 &&
			    strcmp(rule_old->db_name, rule_new->db_name) == 0) {
				ok = 1;
				break;
			}
		}

		if (!ok) {
			od_rule_key_t *rk = malloc(sizeof(od_rule_key_t));

			od_rule_key_init(rk);

			rk->usr_name = strndup(rule_old->user_name,
					       rule_old->user_name_len);
			rk->db_name = strndup(rule_old->db_name,
					      rule_old->db_name_len);

			od_list_append(deleted, &rk->link);
		}
	};

	/* select added rules */
	od_list_foreach_safe(&src->rules, i, n)
	{
		od_rule_t *rule_new;
		rule_new = od_container_of(i, od_rule_t, link);

		int ok = 0;

		od_list_t *m;
		od_list_t *j;
		od_list_foreach_safe(&rules->rules, j, m)
		{
			od_rule_t *rule_old;
			rule_old = od_container_of(j, od_rule_t, link);
			if (strcmp(rule_old->user_name, rule_new->user_name) ==
				    0 &&
			    strcmp(rule_old->db_name, rule_new->db_name) == 0) {
				ok = 1;
				break;
			}
		}

		if (!ok) {
			od_rule_key_t *rk = malloc(sizeof(od_rule_key_t));

			od_rule_key_init(rk);

			rk->usr_name = strndup(rule_new->user_name,
					       rule_new->user_name_len);
			rk->db_name = strndup(rule_new->db_name,
					      rule_new->db_name_len);

			od_list_append(added, &rk->link);
		}
	};

	/* select new rules */
	od_list_foreach_safe(&src->rules, i, n)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);

		/* find and compare origin rule */
		od_rule_t *origin;
		origin = od_rules_match_active(rules, rule->db_name,
					       rule->user_name);
		if (origin) {
			if (od_rules_rule_compare(origin, rule)) {
				origin->mark = 0;
				count_mark--;
				continue;
				/* select rules with changes what needed disconnect */
			} else if (!od_rules_rule_compare_to_drop(origin,
								  rule)) {
				od_rule_key_t *rk =
					malloc(sizeof(od_rule_key_t));

				od_rule_key_init(rk);

				rk->usr_name = strndup(origin->user_name,
						       origin->user_name_len);
				rk->db_name = strndup(origin->db_name,
						      origin->db_name_len);
				od_list_append(to_drop, &rk->link);
			}

			/* add new version, origin version still exists */
		} else {
			/* add new version */

			//			od_list_append(added, &rule->link);
		}

		od_list_unlink(&rule->link);
		od_list_init(&rule->link);
		od_list_append(&rules->rules, &rule->link);
#ifdef PAM_FOUND
		rule->auth_pam_data = od_pam_auth_data_create();
#endif
		count_new++;
	}

	/* try to free obsolete schemes, which are unused by any
	 * rule at the moment */
	if (count_mark > 0) {
		od_list_foreach_safe(&rules->rules, i, n)
		{
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

int od_pool_validate(od_logger_t *logger, od_rule_pool_t *pool, char *db_name,
		     char *user_name)
{
	/* pooling mode */
	if (!pool->type) {
		od_error(logger, "rules", NULL, NULL,
			 "rule '%s.%s': pooling mode is not set", db_name,
			 user_name);
		return NOT_OK_RESPONSE;
	}
	if (strcmp(pool->type, "session") == 0) {
		pool->pool = OD_RULE_POOL_SESSION;
	} else if (strcmp(pool->type, "transaction") == 0) {
		pool->pool = OD_RULE_POOL_TRANSACTION;
	} else if (strcmp(pool->type, "statement") == 0) {
		pool->pool = OD_RULE_POOL_STATEMENT;
	} else {
		od_error(logger, "rules", NULL, NULL,
			 "rule '%s.%s': unknown pooling mode", db_name,
			 user_name);
		return NOT_OK_RESPONSE;
	}

	pool->routing = OD_RULE_POOL_CLIENT_VISIBLE;
	if (!pool->routing_type) {
		od_debug(
			logger, "rules", NULL, NULL,
			"rule '%s.%s': pool routing mode is not set, assuming \"client_visible\" by default",
			db_name, user_name);
	} else if (strcmp(pool->routing_type, "internal") == 0) {
		pool->routing = OD_RULE_POOL_INTERVAL;
	} else if (strcmp(pool->routing_type, "client_visible") == 0) {
		pool->routing = OD_RULE_POOL_CLIENT_VISIBLE;
	} else {
		od_error(logger, "rules", NULL, NULL,
			 "rule '%s.%s': unknown pool routing mode", db_name,
			 user_name);
		return NOT_OK_RESPONSE;
	}

	// reserve prepare statement feature
	if (pool->reserve_prepared_statement &&
	    pool->pool == OD_RULE_POOL_SESSION) {
		od_error(
			logger, "rules", NULL, NULL,
			"rule '%s.%s': prepared statements support in session pool makes no sence",
			db_name, user_name);
		return NOT_OK_RESPONSE;
	}

	if (pool->reserve_prepared_statement && pool->discard) {
		od_error(
			logger, "rules", NULL, NULL,
			"rule '%s.%s': pool discard is forbidden when using prepared statements support",
			db_name, user_name);
		return NOT_OK_RESPONSE;
	}

	if (pool->smart_discard && !pool->reserve_prepared_statement) {
		od_error(
			logger, "rules", NULL, NULL,
			"rule '%s.%s': pool smart discard is forbidden without using prepared statements support",
			db_name, user_name);
		return NOT_OK_RESPONSE;
	}

	if (pool->discard_query && pool->reserve_prepared_statement) {
		if (strcasestr(pool->discard_query, "DEALLOCATE ALL")) {
			od_error(
				logger, "rules", NULL, NULL,
				"rule '%s.%s': cannot support prepared statements when 'DEALLOCATE ALL' present in discard string",
				db_name, user_name);
			return NOT_OK_RESPONSE;
		}
	}

	return OK_RESPONSE;
}

int od_rules_autogenerate_defaults(od_rules_t *rules, od_logger_t *logger)
{
	od_rule_t *rule;
	od_rule_t *default_rule;
	od_list_t *i;
	bool need_autogen = false;
	/* rules */
	od_list_foreach(&rules->rules, i)
	{
		rule = od_container_of(i, od_rule_t, link);

		/* match storage and make a copy of in the user rules */
		if (rule->auth_query != NULL &&
		    !od_rules_match(rules, rule->db_name, rule->user_name,
				    rule->db_is_default, rule->user_is_default,
				    1)) {
			need_autogen = true;
			break;
		}
	}

	if (!need_autogen ||
	    od_rules_match(rules, "default_db", "default_user", 1, 1, 1)) {
		return OK_RESPONSE;
	}

	default_rule =
		od_rules_match(rules, "default_db", "default_user", 1, 1, 0);
	if (!default_rule) {
		od_log(logger, "config", NULL, NULL,
		       "skipping default internal rule auto-generation: no default rule provided");
		return OK_RESPONSE;
	}

	if (!default_rule->storage) {
		od_log(logger, "config", NULL, NULL,
		       "skipping default internal rule auto-generation: default rule storage not set");
		return OK_RESPONSE;
	}

	if (!default_rule->storage_password) {
		od_log(logger, "config", NULL, NULL,
		       "skipping default internal rule auto-generation: default rule storage password not set");
		return OK_RESPONSE;
	}

	rule = od_rules_add(rules);
	if (rule == NULL) {
		return NOT_OK_RESPONSE;
	}
	rule->user_is_default = 1;
	rule->user_name_len = sizeof("default_user");

	/* we need malloc'd string here */
	rule->user_name = strdup("default_user");
	if (rule->user_name == NULL)
		return NOT_OK_RESPONSE;
	rule->db_is_default = 1;
	rule->db_name_len = sizeof("default_db");
	/* we need malloc'd string here */
	rule->db_name = strdup("default_db");
	if (rule->db_name == NULL)
		return NOT_OK_RESPONSE;

/* force several default settings */
#define OD_DEFAULT_INTERNAL_POLL_SZ 0
	rule->pool->type = strdup("transaction");
	rule->pool->pool = OD_RULE_POOL_TRANSACTION;

	rule->pool->routing_type = strdup("internal");
	rule->pool->routing = OD_RULE_POOL_INTERVAL;

	rule->pool->size = OD_DEFAULT_INTERNAL_POLL_SZ;
	rule->enable_password_passthrough = true;
	rule->storage = od_rules_storage_copy(default_rule->storage);
	if (!rule->storage) {
		// oom
		return NOT_OK_RESPONSE;
	}

	rule->storage_password = strdup(default_rule->storage_password);
	rule->storage_password_len = default_rule->storage_password_len;

	od_log(logger, "config", NULL, NULL,
	       "default internal rule auto-generated");
	return OK_RESPONSE;
}

int od_rules_validate(od_rules_t *rules, od_config_t *config,
		      od_logger_t *logger)
{
	/* storages */
	if (od_list_empty(&rules->storages)) {
		od_error(logger, "rules", NULL, NULL, "no storage defined");
		return -1;
	}

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
		} else if (strcmp(storage->type, "local") == 0) {
			storage->storage_type = OD_RULE_STORAGE_LOCAL;
		} else {
			od_error(logger, "rules", NULL, NULL,
				 "unknown storage type");
			return -1;
		}
		if (storage->storage_type == OD_RULE_STORAGE_REMOTE) {
			if (storage->host == NULL) {
				if (config->unix_socket_dir == NULL) {
					od_error(
						logger, "rules", NULL, NULL,
						"storage '%s': no host specified and "
						"unix_socket_dir is not set",
						storage->name);
					return -1;
				}
			} else {
				for (size_t i = 0; i < storage->endpoints_count;
				     ++i) {
					if (storage->endpoints[i].port == 0) {
						/* forse default port */
						storage->endpoints[i].port =
							storage->port;
					}
				}
			}
		}
		if (storage->tls_opts->tls) {
			if (strcmp(storage->tls_opts->tls, "disable") == 0) {
				storage->tls_opts->tls_mode =
					OD_CONFIG_TLS_DISABLE;
			} else if (strcmp(storage->tls_opts->tls, "allow") ==
				   0) {
				storage->tls_opts->tls_mode =
					OD_CONFIG_TLS_ALLOW;
			} else if (strcmp(storage->tls_opts->tls, "require") ==
				   0) {
				storage->tls_opts->tls_mode =
					OD_CONFIG_TLS_REQUIRE;
			} else if (strcmp(storage->tls_opts->tls,
					  "verify_ca") == 0) {
				storage->tls_opts->tls_mode =
					OD_CONFIG_TLS_VERIFY_CA;
			} else if (strcmp(storage->tls_opts->tls,
					  "verify_full") == 0) {
				storage->tls_opts->tls_mode =
					OD_CONFIG_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "rules", NULL, NULL,
					 "unknown storage tls_opts->tls mode");
				return -1;
			}
		}
	}

	/* rules */
	if (od_list_empty(&rules->rules)) {
		od_error(logger, "rules", NULL, NULL, "no rules defined");
		return -1;
	}

	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);

		/* match storage and make a copy of in the user rules */
		if (rule->storage_name == NULL) {
			od_error(logger, "rules", NULL, NULL,
				 "rule '%s.%s': no rule storage is specified",
				 rule->db_name, rule->user_name);
			return NOT_OK_RESPONSE;
		}

		od_rule_storage_t *storage;
		storage = od_rules_storage_match(rules, rule->storage_name);
		if (storage == NULL) {
			od_error(logger, "rules", NULL, NULL,
				 "rule '%s.%s': no rule storage '%s' found",
				 rule->db_name, rule->user_name,
				 rule->storage_name);
			return NOT_OK_RESPONSE;
		}

		rule->storage = od_rules_storage_copy(storage);
		if (rule->storage == NULL) {
			return NOT_OK_RESPONSE;
		}

		if (od_pool_validate(logger, rule->pool, rule->db_name,
				     rule->user_name) == NOT_OK_RESPONSE) {
			return NOT_OK_RESPONSE;
		}

		if (rule->storage->storage_type != OD_RULE_STORAGE_LOCAL) {
			if (rule->user_role != OD_RULE_ROLE_UNDEF) {
				od_error(
					logger, "rules validate", NULL, NULL,
					"rule '%s.%s': role set for non-local storage",
					rule->db_name, rule->user_name);
				return NOT_OK_RESPONSE;
			}
		} else {
			if (rule->user_role == OD_RULE_ROLE_UNDEF) {
				od_error(
					logger, "rules validate", NULL, NULL,
					"rule '%s.%s': force stat role for local storage",
					rule->db_name, rule->user_name);
				rule->user_role = OD_RULE_ROLE_STAT;
			}
		}

		/* auth */
		if (!rule->auth) {
			od_error(
				logger, "rules", NULL, NULL,
				"rule '%s.%s': authentication mode is not defined",
				rule->db_name, rule->user_name);
			return -1;
		}
		if (strcmp(rule->auth, "none") == 0) {
			rule->auth_mode = OD_RULE_AUTH_NONE;
		} else if (strcmp(rule->auth, "block") == 0) {
			rule->auth_mode = OD_RULE_AUTH_BLOCK;
		} else if (strcmp(rule->auth, "clear_text") == 0) {
			rule->auth_mode = OD_RULE_AUTH_CLEAR_TEXT;

#ifdef PAM_FOUND
			if (rule->auth_query != NULL &&
			    rule->auth_pam_service != NULL) {
				od_error(
					logger, "rules", NULL, NULL,
					"auth query and pam service auth method cannot be "
					"used simultaneously",
					rule->db_name, rule->user_name);
				return -1;
			}
#endif

			if (rule->password == NULL && rule->auth_query == NULL
#ifdef PAM_FOUND
			    && rule->auth_pam_service == NULL
#endif
			    && rule->auth_module == NULL
#ifdef LDAP_FOUND
			    && rule->ldap_endpoint == NULL
#endif
			) {

				od_error(logger, "rules", NULL, NULL,
					 "rule '%s.%s': password is not set",
					 rule->db_name, rule->user_name);
				return -1;
			}
		} else if (strcmp(rule->auth, "md5") == 0) {
			rule->auth_mode = OD_RULE_AUTH_MD5;
			if (rule->password == NULL &&
			    rule->auth_query == NULL) {
				od_error(logger, "rules", NULL, NULL,
					 "rule '%s.%s': password is not set",
					 rule->db_name, rule->user_name);
				return -1;
			}
		} else if (strcmp(rule->auth, "scram-sha-256") == 0) {
			rule->auth_mode = OD_RULE_AUTH_SCRAM_SHA_256;
			if (rule->password == NULL &&
			    rule->auth_query == NULL) {
				od_error(logger, "rules", NULL, NULL,
					 "rule '%s.%s': password is not set",
					 rule->db_name, rule->user_name);
				return -1;
			}
		} else if (strcmp(rule->auth, "cert") == 0) {
			rule->auth_mode = OD_RULE_AUTH_CERT;
		} else {
			od_error(
				logger, "rules", NULL, NULL,
				"rule '%s.%s': has unknown authentication mode",
				rule->db_name, rule->user_name);
			return -1;
		}

		/* auth_query */
		if (rule->auth_query) {
			if (rule->auth_query_user == NULL) {
				od_error(
					logger, "rules", NULL, NULL,
					"rule '%s.%s': auth_query_user is not set",
					rule->db_name, rule->user_name);
				return -1;
			}
			if (rule->auth_query_db == NULL) {
				od_error(
					logger, "rules", NULL, NULL,
					"rule '%s.%s': auth_query_db is not set",
					rule->db_name, rule->user_name);
				return -1;
			}
		}
	}

	return 0;
}

int od_rules_cleanup(od_rules_t *rules)
{
	/* cleanup declarative storages rules data */
	od_list_t *n, *i;
	od_list_foreach_safe(&rules->storages, i, n)
	{
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);
		od_rules_storage_free(storage);
	}
	od_list_init(&rules->storages);
#ifdef LDAP_FOUND

	/* TODO: cleanup ldap 
	od_list_foreach_safe(&rules->storages, i, n)
	{
		od_ldap_endpoint_t *endp;
		storage = od_container_of(i, od_ldap_endpoint_t, link);
		od_ldap_endpoint_free(endp);
	}
	*/

	od_list_init(&rules->ldap_endpoints);
#endif

	return 0;
}

static inline char *od_rules_yes_no(int value)
{
	return value ? "yes" : "no";
}

void od_rules_print(od_rules_t *rules, od_logger_t *logger)
{
	od_list_t *i;
	od_log(logger, "config", NULL, NULL, "storages");

	od_list_foreach(&rules->storages, i)
	{
		od_rule_storage_t *storage;
		storage = od_container_of(i, od_rule_storage_t, link);

		od_log(logger, "storage", NULL, NULL,
		       "  storage types           %s",
		       storage->storage_type == OD_RULE_STORAGE_REMOTE ?
				     "remote" :
				     "local");

		od_log(logger, "storage", NULL, NULL, "  host          %s",
		       storage->host ? storage->host : "<unix socket>");

		od_log(logger, "storage", NULL, NULL, "  port          %d",
		       storage->port);

		if (storage->tls_opts->tls)
			od_log(logger, "storage", NULL, NULL,
			       "  tls             %s", storage->tls_opts->tls);
		if (storage->tls_opts->tls_ca_file)
			od_log(logger, "storage", NULL, NULL,
			       "  tls_ca_file     %s",
			       storage->tls_opts->tls_ca_file);
		if (storage->tls_opts->tls_key_file)
			od_log(logger, "storage", NULL, NULL,
			       "  tls_key_file    %s",
			       storage->tls_opts->tls_key_file);
		if (storage->tls_opts->tls_cert_file)
			od_log(logger, "storage", NULL, NULL,
			       "  tls_cert_file   %s",
			       storage->tls_opts->tls_cert_file);
		if (storage->tls_opts->tls_protocols)
			od_log(logger, "storage", NULL, NULL,
			       "  tls_protocols   %s",
			       storage->tls_opts->tls_protocols);
		if (storage->watchdog) {
			if (storage->watchdog->query)
				od_log(logger, "storage", NULL, NULL,
				       "  watchdog query   %s",
				       storage->watchdog->query);
			if (storage->watchdog->interval)
				od_log(logger, "storage", NULL, NULL,
				       "  watchdog interval   %d",
				       storage->watchdog->interval);
		}
		od_log(logger, "storage", NULL, NULL, "");
	}

	od_list_foreach(&rules->rules, i)
	{
		od_rule_t *rule;
		rule = od_container_of(i, od_rule_t, link);
		if (rule->obsolete)
			continue;
		od_log(logger, "rules", NULL, NULL, "<%s.%s>", rule->db_name,
		       rule->user_name);
		od_log(logger, "rules", NULL, NULL,
		       "  authentication                    %s", rule->auth);
		if (rule->auth_common_name_default)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_common_name default");
		od_list_t *j;
		od_list_foreach(&rule->auth_common_names, j)
		{
			od_rule_auth_t *auth;
			auth = od_container_of(j, od_rule_auth_t, link);
			od_log(logger, "rules", NULL, NULL,
			       "  auth_common_name %s", auth->common_name);
		}
		if (rule->auth_query)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query                        %s",
			       rule->auth_query);
		if (rule->auth_query_db)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query_db                     %s",
			       rule->auth_query_db);
		if (rule->auth_query_user)
			od_log(logger, "rules", NULL, NULL,
			       "  auth_query_user                   %s",
			       rule->auth_query_user);

		/* pool  */
		od_log(logger, "rules", NULL, NULL,
		       "  pool                              %s",
		       rule->pool->type);
		od_log(logger, "rules", NULL, NULL,
		       "  pool routing                      %s",
		       rule->pool->routing_type == NULL ?
				     "client visible" :
				     rule->pool->routing_type);
		od_log(logger, "rules", NULL, NULL,
		       "  pool size                         %d",
		       rule->pool->size);
		od_log(logger, "rules", NULL, NULL,
		       "  pool timeout                      %d",
		       rule->pool->timeout);
		od_log(logger, "rules", NULL, NULL,
		       "  pool ttl                          %d",
		       rule->pool->ttl);
		od_log(logger, "rules", NULL, NULL,
		       "  pool discard                      %s",
		       rule->pool->discard ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool smart discard                %s",
		       rule->pool->smart_discard ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool cancel                       %s",
		       rule->pool->cancel ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool rollback                     %s",
		       rule->pool->rollback ? "yes" : "no");
		od_log(logger, "rules", NULL, NULL,
		       "  pool client_idle_timeout          %d",
		       rule->pool->client_idle_timeout);
		od_log(logger, "rules", NULL, NULL,
		       "  pool idle_in_transaction_timeout  %d",
		       rule->pool->idle_in_transaction_timeout);
		if (rule->pool->pool != OD_RULE_POOL_SESSION) {
			od_log(logger, "rules", NULL, NULL,
			       "  pool prepared statement support   %s",
			       rule->pool->reserve_prepared_statement ? "yes" :
									      "no");
		}

		if (rule->client_max_set)
			od_log(logger, "rules", NULL, NULL,
			       "  client_max                        %d",
			       rule->client_max);
		od_log(logger, "rules", NULL, NULL,
		       "  client_fwd_error                  %s",
		       od_rules_yes_no(rule->client_fwd_error));
		od_log(logger, "rules", NULL, NULL,
		       "  reserve_session_server_connection %s",
		       od_rules_yes_no(
			       rule->reserve_session_server_connection));
#ifdef LDAP_FOUND
		if (rule->ldap_endpoint_name) {
			od_log(logger, "rules", NULL, NULL,
			       "  ldap_endpoint_name                %s",
			       rule->ldap_endpoint_name);
		}
		if (rule->ldap_storage_credentials_attr != NULL) {
			od_log(logger, "rules", NULL, NULL,
			       "  ldap_storage_credentials_attr     %s",
			       rule->ldap_storage_credentials_attr);
		}
		if (&rule->ldap_storage_creds_list) {
			od_list_t *f;
			od_list_foreach(&rule->ldap_storage_creds_list, f)
			{
				od_ldap_storage_credentials_t *lsc;
				lsc = od_container_of(
					f, od_ldap_storage_credentials_t, link);
				if (lsc->name) {
					od_log(logger, "rule", NULL, NULL,
					       "  lsc_name                %s",
					       lsc->name);
				}
				if (lsc->lsc_username) {
					od_log(logger, "rule", NULL, NULL,
					       "  lsc_username                %s",
					       lsc->lsc_username);
				}
				if (lsc->lsc_password) {
					od_log(logger, "rule", NULL, NULL,
					       "  lsc_password                %s",
					       lsc->lsc_password);
				}
			}
		}
#endif
		od_log(logger, "rules", NULL, NULL,
		       "  storage                           %s",
		       rule->storage_name);
		od_log(logger, "rules", NULL, NULL,
		       "  type                              %s",
		       rule->storage->type);
		od_log(logger, "rules", NULL, NULL,
		       "  host                              %s",
		       rule->storage->host ? rule->storage->host :
						   "<unix socket>");
		od_log(logger, "rules", NULL, NULL,
		       "  port                              %d",
		       rule->storage->port);
		if (rule->storage->tls_opts->tls)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_opts->tls                               %s",
			       rule->storage->tls_opts->tls);
		if (rule->storage->tls_opts->tls_ca_file)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_opts->tls_ca_file                       %s",
			       rule->storage->tls_opts->tls_ca_file);
		if (rule->storage->tls_opts->tls_key_file)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_opts->tls_key_file                      %s",
			       rule->storage->tls_opts->tls_key_file);
		if (rule->storage->tls_opts->tls_cert_file)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_opts->tls_cert_file                     %s",
			       rule->storage->tls_opts->tls_cert_file);
		if (rule->storage->tls_opts->tls_protocols)
			od_log(logger, "rules", NULL, NULL,
			       "  tls_opts->tls_protocols                     %s",
			       rule->storage->tls_opts->tls_protocols);
		if (rule->storage_db)
			od_log(logger, "rules", NULL, NULL,
			       "  storage_db                        %s",
			       rule->storage_db);
		if (rule->storage_user)
			od_log(logger, "rules", NULL, NULL,
			       "  storage_user                      %s",
			       rule->storage_user);
		if (rule->catchup_checks)
			od_log(logger, "rules", NULL, NULL,
			       "  catchup timeout   %d", rule->catchup_timeout);
		if (rule->catchup_checks)
			od_log(logger, "rules", NULL, NULL,
			       "  catchup timeout   %d", rule->catchup_checks);

		od_log(logger, "rules", NULL, NULL,
		       "  log_debug                         %s",
		       od_rules_yes_no(rule->log_debug));
		od_log(logger, "rules", NULL, NULL,
		       "  log_query                         %s",
		       od_rules_yes_no(rule->log_query));

		od_log(logger, "rules", NULL, NULL,
		       "  options:                         %s", "todo");

		od_log(logger, "rules", NULL, NULL, "");
	}
}
