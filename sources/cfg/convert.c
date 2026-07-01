/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <cfg/convert.h>
#include <od_memory.h>
#include <shared_pool.h>
#include <affinity.h>
#include <config_reader.h>
#include <hba_reader.h>
#include <tsa.h>

#ifdef PROMHTTP_FOUND
#include <prom_metrics.h>
#endif

#define COPY_STR(field, out, diags)                                     \
	do {                                                            \
		if ((field).seen.is_set) {                              \
			out = od_strdup((field).value);                 \
			if (out == NULL) {                              \
				od_cfg_diag_error(                      \
					diags, (field).seen.location,   \
					"can't allocate string value"); \
				goto error;                             \
			}                                               \
		}                                                       \
	} while (0)

#define COPY_INT(field, out)                 \
	do {                                 \
		if ((field).seen.is_set) {   \
			out = (field).value; \
		}                            \
	} while (0)

#define COPY_BOOL(field, out)                \
	do {                                 \
		if ((field).seen.is_set) {   \
			out = (field).value; \
		}                            \
	} while (0)

static int parse_tsa(const char *s, od_target_session_attrs_t *out)
{
	if (strcmp(s, "read-write") == 0) {
		*out = OD_TARGET_SESSION_ATTRS_RW;
		return 0;
	} else if (strcmp(s, "any") == 0) {
		*out = OD_TARGET_SESSION_ATTRS_ANY;
		return 0;
	} else if (strcmp(s, "read-only") == 0) {
		*out = OD_TARGET_SESSION_ATTRS_RO;
		return 0;
	}

	return 1;
}

#define COPY_TSA(field, out, diags)                                                   \
	do {                                                                          \
		if ((field).seen.is_set) {                                            \
			int rc = parse_tsa((field).value, &out);                      \
			if (rc) {                                                     \
				od_cfg_diag_error(                                    \
					diags, (field).seen.location,                 \
					"can't parse target session attrs from '%s'", \
					(field).value);                               \
				goto error;                                           \
			}                                                             \
		}                                                                     \
	} while (0)

static inline void ldap_not_supported(od_cfg_diag_list_t *diags, od_cfg_location_t location)
{
	od_cfg_diag_error(diags, location, "LDAP is not supported by this build");
}

#define CHECK_LDAP_NOT_SET(field, diags) \
	do { \
		if ((field).seen.is_set) { \
			ldap_not_supported((diags), (field).seen.location); \
			goto error; \
		} \
	} while(0)

static int import_hba(const od_cfg_string_field_t *hba_file,
		      od_config_t *config, od_hba_rules_t *hba_rules,
		      od_cfg_diag_list_t *diags)
{
	/* TODO: rewrite hba parsing */
	od_error_t error;
	od_config_reader_t reader;
	memset(&reader, 0, sizeof(reader));
	reader.config = config;
	reader.error = &error;
	reader.hba_rules = hba_rules;

	int rc;
	rc = od_config_reader_open(&reader, hba_file->value);
	if (rc != 0) {
		od_cfg_diag_error(diags, hba_file->seen.location,
				  "can't open hba_file: %s", error.error);
		return rc;
	}

	rc = od_hba_reader_parse(&reader);
	od_config_reader_close(&reader);

	if (rc != 0) {
		od_cfg_diag_error(diags, hba_file->seen.location,
				  "can't parse hba_file: %s", error.error);
		return rc;
	}

	return 0;
}

int convert_global(const od_cfg_global_t *cfg, od_config_t *config,
		   od_global_t *global, od_hba_rules_t *hba_rules,
		   od_cfg_diag_list_t *diags)
{
	COPY_BOOL(cfg->daemonize, config->daemonize);
	COPY_BOOL(cfg->sequential_routing, config->sequential_routing);
	COPY_BOOL(cfg->enable_online_restart,
		  config->enable_online_restart_feature);
	COPY_BOOL(cfg->virtual_processing, config->virtual_processing);
	COPY_BOOL(cfg->bindwith_reuseport, config->bindwith_reuseport);
	COPY_BOOL(cfg->enable_host_watcher, config->host_watcher_enabled);
	COPY_BOOL(cfg->log_debug, config->log_debug);
	COPY_BOOL(cfg->log_to_stdout, config->log_to_stdout);
	COPY_BOOL(cfg->log_config, config->log_config);
	COPY_BOOL(cfg->log_session, config->log_session);
	COPY_BOOL(cfg->log_query, config->log_query);
	COPY_BOOL(cfg->log_stats, config->log_stats);
	COPY_BOOL(cfg->log_async, config->log_async);
	COPY_BOOL(cfg->log_syslog, config->log_syslog);
	COPY_BOOL(cfg->smart_search_path_enquoting,
		  config->smart_search_path_enquoting);
	COPY_BOOL(cfg->nodelay, config->nodelay);
	COPY_BOOL(cfg->disable_nolinger, config->disable_nolinger);
	COPY_BOOL(cfg->log_general_stats_prom, config->log_general_stats_prom);
	COPY_BOOL(cfg->log_route_stats_prom, config->log_route_stats_prom);

	COPY_INT(cfg->priority, config->priority);
	COPY_INT(cfg->graceful_shutdown_timeout_ms,
		 config->graceful_shutdown_timeout_ms);
	COPY_INT(cfg->log_queue_depth, config->log_queue_depth);
	COPY_INT(cfg->stats_interval, config->stats_interval);
	COPY_INT(cfg->client_max, config->client_max);
	COPY_INT(cfg->client_max_routing, config->client_max_routing);
	COPY_INT(cfg->server_login_retry, config->server_login_retry);
	COPY_INT(cfg->readahead, config->readahead);
	COPY_INT(cfg->keepalive, config->keepalive);
	COPY_INT(cfg->keepalive_keep_interval, config->keepalive_keep_interval);
	COPY_INT(cfg->keepalive_probes, config->keepalive_probes);
	COPY_INT(cfg->keepalive_usr_timeout, config->keepalive_usr_timeout);
	COPY_INT(cfg->max_sigterms_to_die, config->max_sigterms_to_die);
	COPY_INT(cfg->backend_connect_timeout_ms,
		 config->backend_connect_timeout_ms);
	COPY_INT(cfg->cancel_timeout_ms, config->cancel_timeout_ms);
	COPY_INT(cfg->cancel_queue_timeout_ms, config->cancel_queue_timeout_ms);
	COPY_INT(cfg->cancel_max_inflight, config->cancel_max_inflight);
	COPY_INT(cfg->resolvers, config->resolvers);
	COPY_INT(cfg->dns_cache_ttl, config->dns_ttl_ms);
	COPY_INT(cfg->cache_msg_gc_size, config->cache_msg_gc_size);
	COPY_INT(cfg->cache_coroutine, config->cache_coroutine);
	COPY_INT(cfg->coroutine_stack_size, config->coroutine_stack_size);
	COPY_INT(cfg->group_checker_interval, config->group_checker_interval);
	COPY_INT(cfg->workers, config->workers);

	if (config->keepalive_usr_timeout < 0) {
		config->keepalive_usr_timeout =
			mm_io_advice_keepalive_usr_timeout(
				config->keepalive,
				config->keepalive_keep_interval,
				config->keepalive_probes);
	}

	if (!config->client_max_routing) {
		config->client_max_routing = config->workers * 64;
	}

	if (cfg->promhttp_server_port.seen.is_set) {
#ifdef PROMHTTP_FOUND
		if (od_prom_set_port(cfg->promhttp_server_port.value,
				     global->cron->metrics) != OK_RESPONSE) {
			od_cfg_diag_error(diags,
					  cfg->promhttp_server_port.location,
					  "can't set prom http server port %d",
					  cfg->promhttp_server_port.value);
			return -1;
		}
#else
		(void)global;
		od_cfg_diag_error(
			diags, cfg->promhttp_server_port.seen.location,
			"can't set prom http server port: current build does not support PROMHTTP");
		return -1;
#endif
	}

	COPY_STR(cfg->pid_file, config->pid_file, diags);
	COPY_STR(cfg->unix_socket_dir, config->unix_socket_dir, diags);
	COPY_STR(cfg->unix_socket_mode, config->unix_socket_mode, diags);
	COPY_STR(cfg->locks_dir, config->locks_dir, diags);
	COPY_STR(cfg->external_auth_socket_path,
		 config->external_auth_socket_path, diags);
	COPY_STR(cfg->log_file, config->log_file, diags);
	COPY_STR(cfg->log_format, config->log_format, diags);
	COPY_STR(cfg->log_syslog_ident, config->log_syslog_ident, diags);
	COPY_STR(cfg->log_syslog_facility, config->log_syslog_facility, diags);

	if (cfg->availability_zone.seen.is_set) {
		const char *val = cfg->availability_zone.value;
		size_t len = strlen(val);
		if (len > OD_MAX_AVAILABILITY_ZONE_LENGTH - 1) {
			od_cfg_diag_error(
				diags, cfg->availability_zone.seen.location,
				"availability_zone string is too large (%lu, but max is %u)",
				len, OD_MAX_AVAILABILITY_ZONE_LENGTH - 1);
			return -1;
		}
		strcpy(config->availability_zone, val);
	}

	if (cfg->cpu_affinity.seen.is_set) {
		config->cpu_affinity = od_malloc(sizeof(od_affinity_config_t));
		if (config->cpu_affinity == NULL) {
			od_cfg_diag_error(diags,
					  cfg->cpu_affinity.seen.location,
					  "can't allocate cpu_affinity");
			return -1;
		}
		od_affinity_config_init(config->cpu_affinity);

		char buf[256];
		int rc = od_affinity_config_parse(
			cfg->cpu_affinity.value,
			strlen(cfg->cpu_affinity.value), config->cpu_affinity,
			buf, sizeof(buf));
		if (rc != 0) {
			od_cfg_diag_error(diags,
					  cfg->cpu_affinity.seen.location,
					  "can't parse cpu_affinity: %s", buf);
			return -1;
		}
	}

	if (cfg->hba_file.seen.is_set) {
		int rc = import_hba(&cfg->hba_file, config, hba_rules, diags);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;

error:
	return -1;
}

static int parse_quantiles(od_cfg_diag_list_t *diags,
						const od_cfg_string_field_t *field,
				       double **quantiles, int *count)
{
	int comma_cnt = 1;
	const char *c = field->value;
	while (*c) {
		if (*c == ',') {
			comma_cnt++;
		}
		c++;
	}
	*quantiles = od_malloc(sizeof(double) * comma_cnt);
	if (*quantiles == NULL) {
		od_cfg_diag_error(diags, field->seen.location, "can't allocate quantiles");
		return -1;
	}
	double *array = *quantiles;
	*count = 0;
	c = field->value;
	while (*c) {
		int length = sscanf(c, "%lf", array + *count);
		if (length != 1 || array[*count] > 1 || array[*count] < 0) {
			od_cfg_diag_error(diags, field->seen.location, "can't parse quantiles from '%s'", field->value);
			od_free(*quantiles);
			return -1;
		}
		*count += 1;
		while (*c != '\0' && *c != ',') {
			c++;
		}
		if (*c == '\0') {
			break;
		}
		c++;
	}

	return 0;
}

static int convert_rule_settings(const od_cfg_route_t *cfg, od_list_t *spools, od_rules_t *rules, od_rule_t *rule, od_cfg_diag_list_t *diags)
{
	COPY_STR(cfg->authentication, rule->auth, diags);

	for (size_t i = 0; i < cfg->auth_common_names_count; ++i) {
		od_cfg_rule_auth_common_name_t *name = cfg->auth_common_names[i];
		if (name->is_default) {
			rule->auth_common_name_default = 1;
			continue;
		}

		od_rule_auth_t *auth = od_rules_auth_add(rule);
		if (auth == NULL) {
			od_cfg_diag_error(diags, name->location, "can't allocate rule auth");
			goto error;
		}

		auth->common_name = od_strdup(name->name);
		if (auth->common_name == NULL) {
			od_cfg_diag_error(diags, name->location, "can't allocate name");
			goto error;
		}
	}

	COPY_STR(cfg->auth_module, rule->auth_module, diags);
	COPY_BOOL(cfg->enable_mdb_iamproxy_auth, rule->enable_mdb_iamproxy_auth);
	COPY_STR(cfg->mdb_iamproxy_socket_path, rule->mdb_iamproxy_socket_path, diags);
#ifdef PAM_FOUND
	COPY_STR(cfg->auth_pam_service, rule->auth_pam_service, diags);
#else
	if (cfg->auth_pam_service.seen.is_set) {
		od_cfg_diag_error(diags, cfg->auth_pam_service.seen.location, "PAM is not supported by this build");
		goto error;
	}
#endif
	COPY_TSA(cfg->target_session_attrs, rule->target_session_attrs, diags);
	COPY_STR(cfg->auth_query, rule->auth_query, diags);
	COPY_STR(cfg->auth_query_db, rule->auth_query_db, diags);
	COPY_STR(cfg->auth_query_user, rule->auth_query_user, diags);
	COPY_BOOL(cfg->password_passthrough, rule->enable_password_passthrough);
	COPY_STR(cfg->password, rule->password, diags);
	if (rule->password != NULL) {
		rule->password_len = strlen(rule->password);
	}

	if (cfg->role.seen.is_set) {
		if (strcmp(cfg->role.value, "admin") == 0) {
			rule->user_role = OD_RULE_ROLE_ADMIN;
		} else if (strcmp(cfg->role.value, "stat") == 0) {
			rule->user_role = OD_RULE_ROLE_STAT;
		} else if (strcmp(cfg->role.value, "notallow") == 0) {
			rule->user_role = OD_RULE_ROLE_NOTALLOW;
		} else {
			od_cfg_diag_error(diags, cfg->role.seen.location, "role type '%s' is unknown", cfg->role.value);
			goto error;
		}
	}

	COPY_INT(cfg->client_max, rule->client_max);
	if (rule->client_max > 0) {
		rule->client_max_set = 1;
	}
	COPY_BOOL(cfg->client_fwd_error, rule->client_fwd_error);
	COPY_BOOL(cfg->client_show_id, rule->client_show_id);
	COPY_BOOL(cfg->reserve_session_server_connection, rule->reserve_session_server_connection);
	if (cfg->quantiles.seen.is_set) {
		int rc = parse_quantiles(diags, &cfg->quantiles, &rule->quantiles, &rule->quantiles_count);
		if (rc != 0) {
			goto error;
		}
	}
	COPY_BOOL(cfg->application_name_add_host, rule->application_name_add_host);
	COPY_BOOL(cfg->server_drop_on_backend_plan_error, rule->server_drop_on_cached_plan_error);
	if (cfg->server_lifetime.seen.is_set) {
		rule->server_lifetime_us = cfg->server_lifetime.value * 1000000L;
	}

	/* ignore ldap_pool_* */
	COPY_BOOL(cfg->maintain_params, rule->maintain_params);
	COPY_STR(cfg->pool, rule->pool->pool_type_str, diags);
	COPY_STR(cfg->pool_routing, rule->pool->routing_type, diags);
	COPY_INT(cfg->min_pool_size, rule->pool->min_size);
	COPY_INT(cfg->pool_size, rule->pool->size);
	COPY_INT(cfg->pool_timeout, rule->pool->timeout);
	COPY_INT(cfg->pool_ttl, rule->pool->ttl);
	COPY_BOOL(cfg->pool_discard, rule->pool->discard);
	COPY_BOOL(cfg->pool_smart_discard, rule->pool->smart_discard);
	COPY_STR(cfg->pool_discard_query, rule->pool->discard_query, diags);
	COPY_BOOL(cfg->pool_cancel, rule->pool->cancel);
	COPY_BOOL(cfg->pool_rollback, rule->pool->rollback);
	COPY_INT(cfg->pool_reset_timeout_ms, rule->pool->reset_timeout_ms);
	COPY_BOOL(cfg->pool_reserve_prepared_statement, rule->pool->reserve_prepared_statement);
	COPY_BOOL(cfg->pool_pin_on_listen, rule->pool->pin_on_listen);
	COPY_BOOL(cfg->pool_attach_check, rule->pool->attach_check);
	COPY_BOOL(cfg->pool_acquire_fail_fast, rule->pool->acquire_fail_fast);
	COPY_INT(cfg->pool_notice_after_waiting_ms, rule->pool->notice_after_waiting_ms);
	if (cfg->pool_client_idle_timeout.seen.is_set) {
		rule->pool->client_idle_timeout = cfg->pool_client_idle_timeout.value * interval_usec;
	}
	if (cfg->pool_idle_in_transaction_timeout.seen.is_set) {
		rule->pool->idle_in_transaction_timeout = cfg->pool_idle_in_transaction_timeout.value * interval_usec;
	}

	if (cfg->shared_pool.seen.is_set) {
		od_list_t *i;
		od_list_foreach(spools, i) {
			od_shared_pool_t *sp = od_container_of(i, od_shared_pool_t, link);
			if (strcmp(cfg->shared_pool.value, sp->name) == 0) {
				rule->shared_pool = od_shared_pool_ref(sp);
				break;
			}
		}

		if (rule->shared_pool == NULL) {
			od_cfg_diag_error(diags, cfg->shared_pool.seen.location, "unknown shared_pool \"%s\"", cfg->shared_pool.value);
			goto error;
		}
	}

	COPY_STR(cfg->storage, rule->storage_name, diags);
	COPY_STR(cfg->storage_database, rule->storage_db, diags);
	COPY_STR(cfg->storage_user, rule->storage_user, diags);
	if (rule->storage_user != NULL) {
		rule->storage_user_len = strlen(rule->storage_user);
	}
	COPY_STR(cfg->storage_password, rule->storage_password, diags);
	if (rule->storage_password != NULL) {
		rule->storage_password_len = strlen(rule->storage_password);
	}
	COPY_BOOL(cfg->log_debug, rule->log_debug);
	COPY_BOOL(cfg->log_query, rule->log_query);
	COPY_STR(cfg->ldap_endpoint_name, rule->ldap_endpoint_name, diags);
#ifdef LDAP_FOUND
	if (cfg->ldap_endpoint_name.seen.is_set) {
		od_ldap_endpoint_t *le = od_ldap_endpoint_find(&rules->ldap_endpoints, cfg->ldap_endpoint_name.value);
		if (le == NULL) {
			od_cfg_diag_error(diags, cfg->ldap_endpoint_name.seen.location, "LDAP endpoint \"%s\" is unknown", cfg->ldap_endpoint_name.value);
			goto error;
		}
		rule->ldap_endpoint = od_ldap_endpoint_ref(le);
	}
#else
	CHECK_LDAP_NOT_SET(cfg->ldap_endpoint_name, diags);
#endif

#ifdef LDAP_FOUND
	COPY_STR(cfg->ldap_storage_credentials_attr, rule->ldap_storage_credentials_attr, diags);
#else
	CHECK_LDAP_NOT_SET(cfg->ldap_storage_credentials_attr, diags);
#endif

	for (size_t i = 0; i < cfg->ldap_storage_credentials_count; ++i) {
		od_cfg_ldap_storage_credentials_t *c = cfg->ldap_storage_credentials[i];

#ifdef LDAP_FOUND
	 	od_ldap_storage_credentials_t *lsc = od_ldap_storage_credentials_alloc();
		if (lsc == NULL) {
			od_cfg_diag_error(diags, c->location, "can't allocate LDAP storage cred");
			goto error;
		}

		lsc->name = od_strdup(c->name);
		if (lsc->name == NULL) {
			od_cfg_diag_error(diags, c->location, "can't allocate LDAP storage cred name");
			goto error;
		}

		if (strlen(lsc->name) == 0) {
			od_cfg_diag_error(diags, c->location, "empty LDAP storage credentials definition");
			goto error;
		}

		if (od_ldap_storage_credentials_find(&rule->ldap_storage_creds_list, lsc->name) != NULL) {
			od_cfg_diag_error(diags, c->location, "redefinition of LDAP storage creds \"%s\"", lsc->name);
			goto error;
		}

		od_rule_ldap_storage_credentials_add(rule, lsc);
		od_ldap_storage_credentials_free(lsc);

		COPY_STR(c->ldap_storage_username, lsc->lsc_username, diags);
		COPY_STR(c->ldap_storage_password, lsc->lsc_password, diags);

		if (lsc->lsc_username == NULL || strlen(lsc->lsc_username) == 0) {
			od_cfg_diag_error(diags, c->location, "username in ldap_storage_credentials '%s' is not defined or is empty", lsc->name);
			goto error;
		}

		if (lsc->lsc_password == NULL || strlen(lsc->lsc_password) == 0) {
			od_cfg_diag_error(diags, c->location, "password in ldap_storage_credentials '%s' is not defined or is empty", lsc->name);
			goto error;
		}
#else
		ldap_not_supported(diags, c->location);
		goto error;
#endif
	}

	if (rule->ldap_storage_credentials_attr != NULL) {
		if (rule->ldap_endpoint == NULL) {
			od_cfg_diag_error(diags, cfg->ldap_storage_credentials_attr.seen.location, "ldap endpoint is not set, but storage cred attrs is");
			goto error;
		}
	}

	COPY_INT(cfg->catchup_timeout, rule->catchup_timeout);
	if (rule->catchup_timeout != 0) {
		/* TODO: remove catchup_checks */
		rule->catchup_checks = 1;
	}

	for (size_t i = 0; i < cfg->pgoptions.pairs_count; ++i) {
		od_cfg_string_kvp_t *kvp = cfg->pgoptions.pairs[i];
		kiwi_vars_update(&rule->vars, kvp->key, strlen(kvp->key) + 1, kvp->value, strlen(kvp->value) + 1);
	}

	if (cfg->backend_startup_options.pairs_count > 0) {
		rule->backend_startup_vars_sz = cfg->backend_startup_options.pairs_count;
		rule->backend_startup_vars = od_malloc(sizeof(untyped_kiwi_var_t) * rule->backend_startup_vars_sz);
		if (rule->backend_startup_vars == NULL) {
			od_cfg_diag_error(diags, cfg->location, "can't allocate backend startup options");
			goto error;
		}

		for (size_t i = 0; i < cfg->backend_startup_options.pairs_count; ++i) {
			untyped_kiwi_var_t *ptr = &rule->backend_startup_vars[rule->backend_startup_vars_sz];

			ptr->name = od_strdup(cfg->backend_startup_options.pairs[i]->key);
			if (ptr->name == NULL) {
				od_cfg_diag_error(diags, cfg->location, "can't allocate backend startup options");
				goto error;
			}
			ptr->name_len = strlen(ptr->name);

			ptr->value = od_strdup(cfg->backend_startup_options.pairs[i]->value);
			if (ptr->value == NULL) {
				od_cfg_diag_error(diags, cfg->location, "can't allocate backend startup options");
				goto error;
			}
			ptr->value_len = strlen(ptr->value);
		}
	}

	return 0;

error:
	return -1;
}

static int convert_shared_pool(const od_cfg_shared_pool_t *cfg, od_list_t *pools, od_cfg_diag_list_t *diags)
{
	od_list_t *i;
	od_list_foreach(pools, i) {
		od_shared_pool_t *s = od_container_of(i, od_shared_pool_t, link);
		if (strcmp(s->name, cfg->name) == 0) {
			od_cfg_diag_error(diags, cfg->name_location, "redefinition of shared pool \"%s\"", cfg->name);
			return -1;
		}
	}

	od_shared_pool_t *sp = od_shared_pool_create(cfg->name);
	if (sp == NULL) {
		od_cfg_diag_error(diags, cfg->location, "can't allocate shared pool");
		return -1;
	}

	COPY_INT(cfg->pool_size, sp->pool_size);

	od_list_append(pools, &sp->link);

	return 0;
}

static int convert_shared_pools(const od_cfg_model_t *cfg, od_list_t *pools, od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < cfg->shared_pools_count; ++i) {
		const od_cfg_shared_pool_t *sp = cfg->shared_pools[i];
		int rc = convert_shared_pool(sp, pools, diags);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

static int convert_listen(const od_cfg_listen_t *cfg, od_config_t *config,
			  od_cfg_diag_list_t *diags)
{
	od_config_listen_t *listen = od_config_listen_add(config);
	if (listen == NULL) {
		od_cfg_diag_error(diags, cfg->location,
				  "can't allocate listen");
		return -1;
	}

	COPY_STR(cfg->host, listen->host, diags);
	COPY_INT(cfg->port, listen->port);
	COPY_TSA(cfg->target_session_attrs, listen->target_session_attrs,
		 diags);
	COPY_INT(cfg->client_login_timeout, listen->client_login_timeout);
	COPY_INT(cfg->backlog, listen->backlog);
	COPY_STR(cfg->tls, listen->tls_opts->tls, diags);
	COPY_STR(cfg->tls_ca_file, listen->tls_opts->tls_ca_file, diags);
	COPY_STR(cfg->tls_key_file, listen->tls_opts->tls_key_file, diags);
	COPY_STR(cfg->tls_cert_file, listen->tls_opts->tls_cert_file, diags);
	COPY_STR(cfg->tls_protocols, listen->tls_opts->tls_protocols, diags);

	/* no compression */

	return 0;

error:
	return -1;
}

static int convert_listens(const od_cfg_model_t *model, od_config_t *config,
			   od_cfg_diag_list_t *diags)
{
	if (model->listens_count == 0) {
		od_cfg_diag_error(diags, model->location,
				  "no listen sections defined");
		return -1;
	}

	for (size_t i = 0; i < model->listens_count; ++i) {
		od_cfg_listen_t *l = model->listens[i];
		int rc = convert_listen(l, config, diags);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

static int convert_storage(const od_cfg_storage_t *cfg, od_list_t *spools, od_rules_t *rules,
			   od_global_t *global, od_cfg_diag_list_t *diags)
{
	od_rule_storage_t *storage = od_rules_storage_allocate();
	if (storage == NULL) {
		od_cfg_diag_error(diags, cfg->location,
				  "can't allocate storage");
		return -1;
	}

	if (od_rules_storage_match(rules, cfg->name) != NULL) {
		od_cfg_diag_error(diags, cfg->location,
				  "duplicate storage '%s' definition",
				  cfg->name);
		goto error;
	}

	storage->name = od_strdup(cfg->name);
	if (storage->name == NULL) {
		od_cfg_diag_error(diags, cfg->name_location,
				  "can't allocate name");
		goto error;
	}

	od_rules_storage_add(rules, storage);

	COPY_STR(cfg->type, storage->type, diags);
	COPY_STR(cfg->host, storage->host, diags);
	COPY_INT(cfg->port, storage->port);
	COPY_STR(cfg->tls, storage->tls_opts->tls, diags);
	COPY_STR(cfg->tls_ca_file, storage->tls_opts->tls_ca_file, diags);
	COPY_STR(cfg->tls_key_file, storage->tls_opts->tls_key_file, diags);
	COPY_STR(cfg->tls_cert_file, storage->tls_opts->tls_cert_file, diags);
	COPY_STR(cfg->tls_protocols, storage->tls_opts->tls_protocols, diags);
	COPY_INT(cfg->server_max_routing, storage->server_max_routing);
	COPY_INT(cfg->endpoints_status_poll_interval_ms,
		 storage->endpoints_status_poll_interval_ms);

	if (cfg->host.seen.is_set) {
		int rc = od_storage_parse_endpoints(cfg->host.value,
						    &storage->endpoints,
						    &storage->endpoints_count);
		if (rc != 0) {
			od_cfg_diag_error(diags, cfg->host.seen.location,
					  "can't parse endpoints from '%s'",
					  cfg->host.value);
			goto error;
		}

		for (size_t i = 0; i < storage->endpoints_count; ++i) {
			od_storage_endpoint_status_init(
				&storage->endpoints[i].status);
		}
	}

	if (cfg->balancing.seen.is_set) {
		if (cfg->balancing.method.seen.is_set) {
			int mlen = (int)strlen(cfg->balancing.method.name);
			od_balancing_method_t method =
				od_balancing_method_from_str(
					cfg->balancing.method.name, mlen);
			if (method == OD_BALANCING_METHOD_UNDEF) {
				od_cfg_diag_error(
					diags,
					cfg->balancing.method.seen.location,
					"unexpected balancing method '%s'",
					cfg->balancing.method.name);
				goto error;
			}

			if (method != OD_BALANCING_METHOD_ROUNDROBIN &&
			    method != OD_BALANCING_METHOD_LEASTCONN) {
				od_cfg_diag_error(
					diags,
					cfg->balancing.method.seen.location,
					"not implemented balancing method '%s'",
					cfg->balancing.method.name);
				goto error;
			}

			storage->balancing.method.type = method;
		}

		COPY_BOOL(cfg->balancing.show_notice_messages,
			  storage->balancing.debug_notice);
	}

	if (cfg->watchdog != NULL) {
		storage->watchdog = od_storage_watchdog_allocate(global);
		if (storage->watchdog == NULL) {
			od_cfg_diag_error(diags, cfg->watchdog->location,
					  "can't allocate watchdog");
			goto error;
		}

		od_storage_watchdog_t *watchdog = storage->watchdog;

		watchdog->route_usr = "watchdog_int";
		watchdog->route_db = "watchdog_int";

		od_rule_t *rule;
		od_address_range_t address_range = od_address_range_create_default();
		rule = od_rules_add_new_rule(rules, watchdog->route_db,
						0 /* db_is_default */, watchdog->route_usr,
						0 /* user_is_default */, &address_range,
						OD_RULE_CONN_TYPE_DEFAULT,
						1 /* pool_internal */);
		od_address_range_destroy(&address_range);

		if (rule == NULL) {
			od_cfg_diag_error(diags, cfg->watchdog->location, "route '%s.%s': is redefined", watchdog->route_db, watchdog->route_usr);
			goto error;
		}

		int rc = convert_rule_settings(cfg->watchdog, spools, rules, rule, diags);
		if (rc != 0) {
			goto error;
		}

		COPY_STR(cfg->watchdog->watchdog_lag_query, watchdog->query, diags);
		COPY_INT(cfg->watchdog->watchdog_lag_interval, watchdog->interval);

		watchdog->storage_db = rule->storage_db;
		watchdog->storage_user = rule->storage_user;
		rule->pool->routing = OD_RULE_POOL_INTERNAL;
		watchdog->storage = storage;
	}

error:
	od_rules_storage_free(storage);
	return -1;
}

static int convert_storages(const od_cfg_model_t *model, od_list_t *spools, od_rules_t *rules,
			    od_global_t *global, od_cfg_diag_list_t *diags)
{
	if (model->shared_pools_count == 0) {
		od_cfg_diag_error(diags, model->location,
				  "no storage sections defined");
		return -1;
	}

	for (size_t i = 0; i < model->storages_count; ++i) {
		od_cfg_storage_t *cfg = model->storages[i];
		int rc = convert_storage(cfg, spools, rules, global, diags);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

static int convert_conn_drop_opts(const od_cfg_conn_drop_options_t *cfg,
				  od_config_t *config)
{
	if (!cfg->seen.is_set) {
		return 0;
	}

	od_config_conn_drop_options_t *opts = &config->conn_drop_options;

	COPY_BOOL(cfg->drop_enabled, opts->drop_enabled);
	COPY_INT(cfg->rate, opts->rate);
	COPY_INT(cfg->interval_ms, opts->interval_ms);

	return 0;
}

struct sig_name_num {
	const char *name;
	int num;
};

static int parse_signal(const char *name) {
	static const struct sig_name_num sigs[] = {
#ifdef SIGHUP
		{ "SIGHUP", SIGHUP },
#endif
#ifdef SIGINT
		{ "SIGINT", SIGINT },
#endif
#ifdef SIGQUIT
		{ "SIGQUIT", SIGQUIT },
#endif
#ifdef SIGABRT
		{ "SIGABRT", SIGABRT },
#endif
#ifdef SIGKILL
		{ "SIGKILL", SIGKILL },
#endif
#ifdef SIGUSR1
		{ "SIGUSR1", SIGUSR1 },
#endif
#ifdef SIGUSR2
		{ "SIGUSR2", SIGUSR2 },
#endif
#ifdef SIGTERM
		{ "SIGTERM", SIGTERM },
#endif
#ifdef SIGSTOP
		{ "SIGSTOP", SIGSTOP },
#endif
#ifdef SIGTSTP
		{ "SIGTSTP", SIGTSTP },
#endif
	};

	for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) {
		if (strcasecmp(sigs[i].name, name) == 0) {
			return sigs[i].num;
		}

		if (strncasecmp(name, "SIG", 3) != 0) {
			if (strcasecmp(sigs[i].name + 3, name) == 0) {
				return sigs[i].num;
			}
		}
	}

	return -1;
}

static int convert_soft_oom(const od_cfg_soft_oom_t *cfg, od_config_t *config, od_cfg_diag_list_t *diags)
{
	if (!cfg->seen.is_set) {
		return 0;
	}

	od_config_soft_oom_t *soft_oom = &config->soft_oom;

	if (!cfg->limit.seen.is_set) {
		od_cfg_diag_error(diags, cfg->location,
				  "limits is not set in soft_oom");
		goto error;
	}

	COPY_INT(cfg->limit, soft_oom->limit_bytes);
	COPY_INT(cfg->check_interval_ms, soft_oom->check_interval_ms);

	if (cfg->drop.seen.is_set) {
		soft_oom->drop.enabled = 1;
		COPY_INT(cfg->drop.max_rate, soft_oom->drop.max_rate);

		if (cfg->drop.signal.seen.is_set) {
			soft_oom->drop.signal = parse_signal(cfg->drop.signal.value);
			if (soft_oom->drop.signal == -1) {
				od_cfg_diag_error(diags, cfg->drop.signal.seen.location,
					"can't parse signal from '%s'", cfg->drop.signal.value);
				goto error;
			}
		}
	}

	if (cfg->process.seen.is_set) {
		size_t l = strlen(cfg->process.value);
		if (l >= sizeof(soft_oom->process)) {
			od_cfg_diag_error(diags, cfg->process.seen.location,
				  "too long process name, max is %lu", sizeof(soft_oom->process));
			goto error;
		}
		strcpy(soft_oom->process, cfg->process.value);
	}

error:
	return -1;
}

static int convert_db(const od_cfg_database_t *db, od_list_t *spools, od_rule_t *rules, od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < db->users_count; ++i) {
		const od_cfg_route_t *rcfg = db->users[i];

		od_address_range_t address_range = od_address_range_create_default();
		od_rule_conn_type_t conn_type = OD_RULE_CONN_TYPE_DEFAULT;

		
	}
}

static int convert_rules(const od_cfg_model_t *cfg, od_list_t *spools, od_rules_t *rules, od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < cfg->databases_count; ++i) {
		od_cfg_database_t *db = cfg->databases[i];
		int rc = convert_db(db, spools, rules, diags);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

int od_cfg_convert_model(const od_cfg_model_t *model, od_config_t *config,
			 od_rules_t *rules, od_global_t *global,
			 od_hba_rules_t *hba_rules, od_cfg_diag_list_t *diags)
{
	(void)rules;

	od_list_t *i, *s;
	od_list_t shared_pools;
	od_list_init(&shared_pools);

	int rc;

	rc = convert_shared_pools(model, &shared_pools, diags);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_soft_oom(&model->soft_oom, config, diags);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_conn_drop_opts(&model->conn_drop_options, config);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_global(&model->global, config, global, hba_rules, diags);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_storages(model, &shared_pools, rules, global, diags);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_listens(model, config, diags);
	if (rc != 0) {
		goto to_return;
	}

	rc = convert_rules(model, &shared_pools, rules, diags);
	if (rc != 0) {
		goto to_return;
	}

to_return:
	od_list_foreach_safe(&shared_pools, i, s) {
		od_shared_pool_t *sp = od_container_of(i, od_shared_pool_t, link);
		od_list_unlink(&sp->link);
		od_shared_pool_unref(sp);
	}

	return rc;
}
