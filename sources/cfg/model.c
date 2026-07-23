/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <od_memory.h>
#include <balancing.h>
#include <cfg/model.h>

void od_cfg_model_init(od_cfg_model_t *model)
{
	memset(model, 0, sizeof(*model));
}

static void print_depth(FILE *file, int depth)
{
	for (int i = 0; i < depth; ++i) {
		fputc('\t', file);
	}
}

static void dump_bool(FILE *file, const char *name, int depth,
		      const od_cfg_bool_field_t *f)
{
	if (f->seen.is_set) {
		print_depth(file, depth);
		fprintf(file, "%s %s\n", name, f->value ? "yes" : "no");
	}
}

static void dump_string(FILE *file, const char *name, int depth,
			const od_cfg_string_field_t *f)
{
	if (f->seen.is_set) {
		print_depth(file, depth);
		fprintf(file, "%s \"%s\"\n", name, f->value);
	}
}

static void dump_int(FILE *file, const char *name, int depth,
		     const od_cfg_int_field_t *f)
{
	if (f->seen.is_set) {
		print_depth(file, depth);
		fprintf(file, "%s %d\n", name, f->value);
	}
}

static void dump_uint64(FILE *file, const char *name, int depth,
			const od_cfg_u64_field_t *f)
{
	if (f->seen.is_set) {
		print_depth(file, depth);
		fprintf(file, "%s %llu\n", name, (unsigned long long)f->value);
	}
}

static void dump_shared_pool(FILE *file, const od_cfg_shared_pool_t *sp)
{
	fprintf(file, "shared_pool \"%s\" {\n", sp->name);
	dump_int(file, "pool_size", 1, &sp->pool_size);
	fprintf(file, "}\n");
}

static void dump_conn_drop(FILE *file, const od_cfg_conn_drop_options_t *cd)
{
	if (cd->seen.is_set) {
		fprintf(file, "conn_drop_options {\n");
		dump_bool(file, "drop_enabled", 1, &cd->drop_enabled);
		dump_int(file, "rate", 1, &cd->rate);
		dump_int(file, "interval_ms", 1, &cd->interval_ms);
		fprintf(file, "}\n");
	}
}

static void dump_listen(FILE *file, const od_cfg_listen_t *l)
{
	fprintf(file, "listen {\n");
	dump_string(file, "host", 1, &l->host);
	dump_int(file, "port", 1, &l->port);
	dump_string(file, "target_session_attrs", 1, &l->target_session_attrs);
	dump_int(file, "client_login_timeout", 1, &l->client_login_timeout);
	dump_int(file, "backlog", 1, &l->backlog);
	dump_string(file, "tls", 1, &l->tls);
	dump_string(file, "tls_ca_file", 1, &l->tls_ca_file);
	dump_string(file, "tls_key_file", 1, &l->tls_key_file);
	dump_string(file, "tls_cert_file", 1, &l->tls_cert_file);
	dump_string(file, "tls_protocols", 1, &l->tls_protocols);
	dump_int(file, "catchup_timeout", 1, &l->catchup_timeout);
	fprintf(file, "}\n");
}

static void dump_user(FILE *file, const char *type, const od_cfg_route_t *user);

static void dump_storage(FILE *file, const od_cfg_storage_t *s)
{
	fprintf(file, "storage \"%s\" {\n", s->name);

	dump_string(file, "type", 1, &s->type);
	dump_string(file, "host", 1, &s->host);
	dump_int(file, "port", 1, &s->port);
	dump_string(file, "tls", 1, &s->tls);
	dump_string(file, "tls_ca_file", 1, &s->tls_ca_file);
	dump_string(file, "tls_key_file", 1, &s->tls_key_file);
	dump_string(file, "tls_cert_file", 1, &s->tls_cert_file);
	dump_string(file, "tls_protocols", 1, &s->tls_protocols);
	dump_int(file, "server_max_routing", 1, &s->server_max_routing);
	dump_int(file, "endpoints_status_poll_interval_ms", 1,
		 &s->endpoints_status_poll_interval_ms);

	if (s->balancing.seen.is_set) {
		fprintf(file, "\tbalancing {\n");
		if (s->balancing.method.seen.is_set) {
			fprintf(file, "\t\tmethod \"%s\" {}\n",
				s->balancing.method.name);
		}
		dump_bool(file, "show_notice_messages", 2,
			  &s->balancing.show_notice_messages);
		fprintf(file, "\t}\n");
	}

	if (s->watchdog != NULL) {
		dump_user(file, "watchdog", s->watchdog);
	}

	fprintf(file, "}\n");
}

static void dump_soft_oom(FILE *file, const od_cfg_soft_oom_t *s)
{
	if (!s->seen.is_set) {
		return;
	}

	fprintf(file, "soft_oom {\n");
	dump_uint64(file, "limit", 1, &s->limit);
	dump_string(file, "process", 1, &s->process);
	dump_int(file, "check_interval_ms", 1, &s->check_interval_ms);
	if (s->drop.seen.is_set) {
		fprintf(file, "\tdrop {\n");
		dump_string(file, "signal", 2, &s->drop.signal);
		dump_int(file, "max_rate", 2, &s->drop.max_rate);
		fprintf(file, "\t}\n");
	}
	fprintf(file, "}\n");
}

static void dump_ldap_endpoint(FILE *file, const od_cfg_ldap_endpoint_t *e)
{
	fprintf(file, "ldap_endpoint \"%s\" {\n", e->name);

	dump_string(file, "ldapserver", 1, &e->ldapserver);
	dump_uint64(file, "ldapport", 1, &e->ldapport);
	dump_string(file, "ldapprefix", 1, &e->ldapprefix);
	dump_string(file, "ldapsuffix", 1, &e->ldapsuffix);
	dump_string(file, "ldapsearchattribute", 1, &e->ldapsearchattribute);
	dump_string(file, "ldapscope", 1, &e->ldapscope);
	dump_string(file, "ldapscheme", 1, &e->ldapscheme);
	dump_string(file, "ldapbasedn", 1, &e->ldapbasedn);
	dump_string(file, "ldapbinddn", 1, &e->ldapbinddn);
	dump_string(file, "ldapbindpasswd", 1, &e->ldapbindpasswd);
	dump_string(file, "ldapsearchfilter", 1, &e->ldapsearchfilter);

	fprintf(file, "}\n");
}

static void dump_string_kvp_list(FILE *file, const char *name, int depth,
				 const od_cfg_string_kvp_list_t *list)
{
	if (!list->seen.is_set || list->pairs_count == 0) {
		return;
	}

	print_depth(file, depth);
	fprintf(file, "%s {\n", name);
	for (size_t i = 0; i < list->pairs_count; ++i) {
		print_depth(file, depth + 1);
		fprintf(file, "\"%s\" \"%s\"\n", list->pairs[i]->key,
			list->pairs[i]->value);
	}
	print_depth(file, depth);
	fprintf(file, "}\n");
}

static void
dump_ldap_storage_credentials(FILE *file, int depth,
			      const od_cfg_ldap_storage_credentials_t *lsc)
{
	print_depth(file, depth);
	fprintf(file, "ldap_storage_credentials \"%s\" {\n", lsc->name);
	dump_string(file, "ldap_storage_username", depth + 1,
		    &lsc->ldap_storage_username);
	dump_string(file, "ldap_storage_password", depth + 1,
		    &lsc->ldap_storage_password);
	print_depth(file, depth);
	fprintf(file, "}\n");
}

static void dump_user(FILE *file, const char *type, const od_cfg_route_t *user)
{
	if (user->is_default) {
		fprintf(file, "\t%s default ", type);
	} else {
		if (user->name != NULL) {
			fprintf(file, "\t%s \"%s\" ", type, user->name);
		} else {
			fprintf(file, "\t%s ", type);
		}
	}

	if (user->address_range != NULL) {
		fprintf(file, "\"%s\" ", user->address_range);
	}

	if (user->conn_type != OD_CFG_CONN_TYPE_DEFAULT) {
		switch (user->conn_type) {
		case OD_CFG_CONN_TYPE_LOCAL:
			fprintf(file, "local ");
			break;
		case OD_CFG_CONN_TYPE_HOST:
			fprintf(file, "host ");
			break;
		case OD_CFG_CONN_TYPE_HOSTSSL:
			fprintf(file, "hostssl ");
			break;
		case OD_CFG_CONN_TYPE_HOSTNOSSL:
			fprintf(file, "hostnossl ");
			break;
		default:
			abort();
		}
	}

	fprintf(file, "{\n");

	for (size_t i = 0; i < user->auth_common_names_count; ++i) {
		print_depth(file, 2);
		if (user->auth_common_names[i]->is_default) {
			fprintf(file, "auth_common_name default\n");
		} else {
			fprintf(file, "auth_common_name \"%s\"\n",
				user->auth_common_names[i]->name);
		}
	}

	dump_string(file, "authentication", 2, &user->authentication);
	dump_string(file, "auth_module", 2, &user->auth_module);
	dump_bool(file, "enable_mdb_iamproxy_auth", 2,
		  &user->enable_mdb_iamproxy_auth);
	dump_string(file, "mdb_iamproxy_socket_path", 2,
		    &user->mdb_iamproxy_socket_path);
	dump_string(file, "auth_pam_service", 2, &user->auth_pam_service);
	dump_string(file, "target_session_attrs", 2,
		    &user->target_session_attrs);
	dump_string(file, "auth_query", 2, &user->auth_query);
	dump_string(file, "auth_query_db", 2, &user->auth_query_db);
	dump_string(file, "auth_query_user", 2, &user->auth_query_user);
	dump_bool(file, "password_passthrough", 2, &user->password_passthrough);
	dump_string(file, "password", 2, &user->password);
	dump_string(file, "role", 2, &user->role);

	dump_int(file, "client_max", 2, &user->client_max);
	dump_bool(file, "client_fwd_error", 2, &user->client_fwd_error);
	dump_bool(file, "client_show_id", 2, &user->client_show_id);
	dump_bool(file, "reserve_session_server_connection", 2,
		  &user->reserve_session_server_connection);
	dump_string(file, "quantiles", 2, &user->quantiles);
	dump_bool(file, "application_name_add_host", 2,
		  &user->application_name_add_host);
	dump_bool(file, "server_drop_on_backend_plan_error", 2,
		  &user->server_drop_on_backend_plan_error);
	dump_uint64(file, "server_lifetime", 2, &user->server_lifetime);

	if (user->ldap_pool_size.seen.is_set) {
		print_depth(file, 2);
		fprintf(file, "# deprecated: ldap_pool_size %d\n",
			user->ldap_pool_size.value);
	}
	if (user->ldap_pool_timeout.seen.is_set) {
		print_depth(file, 2);
		fprintf(file, "# deprecated: ldap_pool_timeout %d\n",
			user->ldap_pool_timeout.value);
	}
	if (user->ldap_pool_ttl.seen.is_set) {
		print_depth(file, 2);
		fprintf(file, "# deprecated: ldap_pool_ttl %d\n",
			user->ldap_pool_ttl.value);
	}

	dump_bool(file, "maintain_params", 2, &user->maintain_params);

	dump_string(file, "pool", 2, &user->pool);
	dump_string(file, "pool_routing", 2, &user->pool_routing);
	dump_int(file, "min_pool_size", 2, &user->min_pool_size);
	dump_int(file, "pool_size", 2, &user->pool_size);
	dump_int(file, "pool_timeout", 2, &user->pool_timeout);
	dump_int(file, "pool_ttl", 2, &user->pool_ttl);
	dump_bool(file, "pool_discard", 2, &user->pool_discard);
	dump_bool(file, "pool_smart_discard", 2, &user->pool_smart_discard);
	dump_string(file, "pool_discard_query", 2, &user->pool_discard_query);
	dump_bool(file, "pool_cancel", 2, &user->pool_cancel);
	dump_bool(file, "pool_rollback", 2, &user->pool_rollback);
	dump_uint64(file, "pool_reset_timeout_ms", 2,
		    &user->pool_reset_timeout_ms);
	dump_bool(file, "pool_reserve_prepared_statement", 2,
		  &user->pool_reserve_prepared_statement);
	dump_bool(file, "pool_pin_on_listen", 2, &user->pool_pin_on_listen);
	dump_bool(file, "pool_attach_check", 2, &user->pool_attach_check);
	dump_bool(file, "pool_acquire_fail_fast", 2,
		  &user->pool_acquire_fail_fast);
	dump_uint64(file, "pool_notice_after_waiting_ms", 2,
		    &user->pool_notice_after_waiting_ms);
	dump_uint64(file, "pool_client_idle_timeout", 2,
		    &user->pool_client_idle_timeout);
	dump_uint64(file, "pool_idle_in_transaction_timeout", 2,
		    &user->pool_idle_in_transaction_timeout);

	dump_string(file, "shared_pool", 2, &user->shared_pool);
	dump_string(file, "storage", 2, &user->storage);
	dump_string(file, "storage_user", 2, &user->storage_user);
	dump_string(file, "storage_password", 2, &user->storage_password);
	dump_string(file, "storage_database", 2, &user->storage_database);
	dump_bool(file, "log_debug", 2, &user->log_debug);
	dump_bool(file, "log_query", 2, &user->log_query);
	dump_string(file, "ldap_endpoint_name", 2, &user->ldap_endpoint_name);
	dump_string(file, "ldap_storage_credentials_attr", 2,
		    &user->ldap_storage_credentials_attr);
	for (size_t i = 0; i < user->ldap_storage_credentials_count; ++i) {
		dump_ldap_storage_credentials(
			file, 2, user->ldap_storage_credentials[i]);
	}

	dump_string(file, "watchdog_lag_query", 2, &user->watchdog_lag_query);
	dump_int(file, "watchdog_lag_interval", 2,
		 &user->watchdog_lag_interval);
	dump_int(file, "catchup_timeout", 2, &user->catchup_timeout);

	dump_string_kvp_list(file, "pgoptions", 2, &user->pgoptions);
	dump_string_kvp_list(file, "backend_startup_options", 2,
			     &user->backend_startup_options);

	dump_string(file, "group_query", 2, &user->group_query);
	dump_string(file, "group_query_user", 2, &user->group_query_user);
	dump_string(file, "group_query_db", 2, &user->group_query_db);

	fprintf(file, "\t}\n");
}

static void dump_database(FILE *file, const od_cfg_database_t *db)
{
	if (db->is_default) {
		fprintf(file, "database default {\n");
	} else {
		fprintf(file, "database \"%s\" {\n", db->name);
	}

	for (size_t i = 0; i < db->users_count; ++i) {
		dump_user(file, "user", db->users[i]);
	}

	for (size_t i = 0; i < db->groups_count; ++i) {
		dump_user(file, "group", db->groups[i]);
	}

	fprintf(file, "}\n");
}

void od_cfg_model_dumpf(FILE *file, const od_cfg_model_t *model)
{
	dump_bool(file, "daemonize", 0, &model->global.daemonize);
	dump_bool(file, "sequential_routing", 0,
		  &model->global.sequential_routing);
	dump_bool(file, "enable_online_restart", 0,
		  &model->global.enable_online_restart);
	dump_bool(file, "virtual_processing", 0,
		  &model->global.virtual_processing);
	dump_bool(file, "bindwith_reuseport", 0,
		  &model->global.bindwith_reuseport);
	dump_bool(file, "enable_host_watcher", 0,
		  &model->global.enable_host_watcher);
	dump_bool(file, "log_debug", 0, &model->global.log_debug);
	dump_bool(file, "log_to_stdout", 0, &model->global.log_to_stdout);
	dump_bool(file, "log_config", 0, &model->global.log_config);
	dump_bool(file, "log_session", 0, &model->global.log_session);
	dump_bool(file, "log_query", 0, &model->global.log_query);
	dump_bool(file, "log_stats", 0, &model->global.log_stats);
	dump_bool(file, "log_async", 0, &model->global.log_async);
	dump_bool(file, "log_syslog", 0, &model->global.log_syslog);
	dump_bool(file, "log_general_stats_prom", 0,
		  &model->global.log_general_stats_prom);
	dump_bool(file, "log_route_stats_prom", 0,
		  &model->global.log_route_stats_prom);
	dump_bool(file, "smart_search_path_enquoting", 0,
		  &model->global.smart_search_path_enquoting);
	dump_bool(file, "nodelay", 0, &model->global.nodelay);
	dump_bool(file, "disable_nolinger", 0, &model->global.disable_nolinger);

	dump_string(file, "pid_file", 0, &model->global.pid_file);
	dump_string(file, "unix_socket_dir", 0, &model->global.unix_socket_dir);
	dump_string(file, "unix_socket_mode", 0,
		    &model->global.unix_socket_mode);
	dump_string(file, "locks_dir", 0, &model->global.locks_dir);
	dump_string(file, "external_auth_socket_path", 0,
		    &model->global.external_auth_socket_path);
	dump_string(file, "availability_zone", 0,
		    &model->global.availability_zone);
	dump_string(file, "log_file", 0, &model->global.log_file);
	dump_string(file, "log_format", 0, &model->global.log_format);
	dump_string(file, "log_syslog_ident", 0,
		    &model->global.log_syslog_ident);
	dump_string(file, "log_syslog_facility", 0,
		    &model->global.log_syslog_facility);
	dump_string(file, "cpu_affinity", 0, &model->global.cpu_affinity);
	dump_string(file, "hba_file", 0, &model->global.hba_file);

	dump_int(file, "priority", 0, &model->global.priority);
	dump_int(file, "graceful_shutdown_timeout_ms", 0,
		 &model->global.graceful_shutdown_timeout_ms);
	dump_int(file, "log_queue_depth", 0, &model->global.log_queue_depth);
	dump_int(file, "stats_interval", 0, &model->global.stats_interval);
	dump_int(file, "client_max", 0, &model->global.client_max);
	dump_int(file, "client_max_routing", 0,
		 &model->global.client_max_routing);
	dump_int(file, "server_login_retry", 0,
		 &model->global.server_login_retry);
	dump_int(file, "readahead", 0, &model->global.readahead);
	dump_int(file, "keepalive", 0, &model->global.keepalive);
	dump_int(file, "keepalive_keep_interval", 0,
		 &model->global.keepalive_keep_interval);
	dump_int(file, "keepalive_probes", 0, &model->global.keepalive_probes);
	dump_int(file, "keepalive_usr_timeout", 0,
		 &model->global.keepalive_usr_timeout);
	dump_int(file, "max_sigterms_to_die", 0,
		 &model->global.max_sigterms_to_die);
	dump_int(file, "backend_connect_timeout_ms", 0,
		 &model->global.backend_connect_timeout_ms);
	dump_int(file, "cancel_timeout_ms", 0,
		 &model->global.cancel_timeout_ms);
	dump_int(file, "cancel_queue_timeout_ms", 0,
		 &model->global.cancel_queue_timeout_ms);
	dump_int(file, "cancel_max_inflight", 0,
		 &model->global.cancel_max_inflight);
	dump_int(file, "resolvers", 0, &model->global.resolvers);
	dump_int(file, "dns_cache_ttl", 0, &model->global.dns_cache_ttl);
	dump_int(file, "cache_msg_gc_size", 0,
		 &model->global.cache_msg_gc_size);
	dump_int(file, "cache_coroutine", 0, &model->global.cache_coroutine);
	dump_int(file, "coroutine_stack_size", 0,
		 &model->global.coroutine_stack_size);
	dump_int(file, "promhttp_server_port", 0,
		 &model->global.promhttp_server_port);
	dump_int(file, "group_checker_interval", 0,
		 &model->global.group_checker_interval);
	dump_int(file, "workers", 0, &model->global.workers);

	for (size_t i = 0; i < model->listens_count; ++i) {
		dump_listen(file, model->listens[i]);
	}

	for (size_t i = 0; i < model->storages_count; ++i) {
		dump_storage(file, model->storages[i]);
	}

	for (size_t i = 0; i < model->databases_count; ++i) {
		dump_database(file, model->databases[i]);
	}

	for (size_t i = 0; i < model->shared_pools_count; ++i) {
		dump_shared_pool(file, model->shared_pools[i]);
	}

	for (size_t i = 0; i < model->ldap_endpoints_count; ++i) {
		dump_ldap_endpoint(file, model->ldap_endpoints[i]);
	}

	dump_conn_drop(file, &model->conn_drop_options);

	dump_soft_oom(file, &model->soft_oom);
}

static void od_cfg_string_field_free(od_cfg_string_field_t *field)
{
	od_free(field->value);
	field->value = NULL;
}

static void od_cfg_listen_free(od_cfg_listen_t *listen)
{
	if (listen == NULL) {
		return;
	}

	od_cfg_string_field_free(&listen->host);

	od_cfg_string_field_free(&listen->target_session_attrs);
	od_cfg_string_field_free(&listen->tls);
	od_cfg_string_field_free(&listen->tls_ca_file);
	od_cfg_string_field_free(&listen->tls_key_file);
	od_cfg_string_field_free(&listen->tls_cert_file);
	od_cfg_string_field_free(&listen->tls_protocols);

	od_free(listen);
}

static void od_cfg_user_route_free(od_cfg_route_t *user);

static void od_cfg_storage_free(od_cfg_storage_t *storage)
{
	if (storage == NULL) {
		return;
	}

	if (storage->watchdog) {
		od_cfg_user_route_free(storage->watchdog);
	}

	od_free(storage->name);

	od_cfg_string_field_free(&storage->type);
	od_cfg_string_field_free(&storage->host);
	od_cfg_string_field_free(&storage->tls);
	od_cfg_string_field_free(&storage->tls_ca_file);
	od_cfg_string_field_free(&storage->tls_key_file);
	od_cfg_string_field_free(&storage->tls_cert_file);
	od_cfg_string_field_free(&storage->tls_protocols);

	od_free(storage->balancing.method.name);

	od_free(storage);
}

static void od_cfg_shared_pool_free(od_cfg_shared_pool_t *pool)
{
	if (pool == NULL) {
		return;
	}

	od_free(pool->name);
	od_free(pool);
}

static void od_cfg_ldap_endpoint_free(od_cfg_ldap_endpoint_t *endpoint)
{
	if (endpoint == NULL) {
		return;
	}

	od_free(endpoint->name);

	od_cfg_string_field_free(&endpoint->ldapserver);
	od_cfg_string_field_free(&endpoint->ldapprefix);
	od_cfg_string_field_free(&endpoint->ldapsuffix);
	od_cfg_string_field_free(&endpoint->ldapsearchattribute);
	od_cfg_string_field_free(&endpoint->ldapscope);
	od_cfg_string_field_free(&endpoint->ldapscheme);
	od_cfg_string_field_free(&endpoint->ldapbasedn);
	od_cfg_string_field_free(&endpoint->ldapbinddn);
	od_cfg_string_field_free(&endpoint->ldapbindpasswd);
	od_cfg_string_field_free(&endpoint->ldapsearchfilter);

	od_free(endpoint);
}

static void od_cfg_string_kvp_free(od_cfg_string_kvp_t *kvp)
{
	od_free(kvp->key);
	od_free(kvp->value);
}

static void od_cfg_string_kvp_list_free(od_cfg_string_kvp_list_t *l)
{
	for (size_t i = 0; i < l->pairs_count; ++i) {
		od_cfg_string_kvp_free(l->pairs[i]);
		od_free(l->pairs[i]);
	}
	od_free(l->pairs);
}

static void od_cfg_user_route_free(od_cfg_route_t *user)
{
	if (user == NULL) {
		return;
	}

	od_free(user->name);
	od_free(user->address_range);

	for (size_t i = 0; i < user->auth_common_names_count; ++i) {
		od_free(user->auth_common_names[i]->name);
		od_free(user->auth_common_names[i]);
	}
	od_free(user->auth_common_names);

	for (size_t i = 0; i < user->ldap_storage_credentials_count; ++i) {
		od_free(user->ldap_storage_credentials[i]->name);
		od_cfg_string_field_free(&user->ldap_storage_credentials[i]
						  ->ldap_storage_username);
		od_cfg_string_field_free(&user->ldap_storage_credentials[i]
						  ->ldap_storage_password);
		od_free(user->ldap_storage_credentials[i]);
	}
	od_free(user->ldap_storage_credentials);

	od_cfg_string_field_free(&user->authentication);
	od_cfg_string_field_free(&user->auth_module);

	od_cfg_string_field_free(&user->mdb_iamproxy_socket_path);
	od_cfg_string_field_free(&user->auth_pam_service);
	od_cfg_string_field_free(&user->target_session_attrs);
	od_cfg_string_field_free(&user->auth_query);
	od_cfg_string_field_free(&user->auth_query_db);
	od_cfg_string_field_free(&user->auth_query_user);
	od_cfg_string_field_free(&user->password);
	od_cfg_string_field_free(&user->role);
	od_cfg_string_field_free(&user->quantiles);
	od_cfg_string_field_free(&user->pool);
	od_cfg_string_field_free(&user->pool_routing);
	od_cfg_string_field_free(&user->pool_discard_query);
	od_cfg_string_field_free(&user->shared_pool);
	od_cfg_string_field_free(&user->storage);
	od_cfg_string_field_free(&user->storage_database);
	od_cfg_string_field_free(&user->storage_user);
	od_cfg_string_field_free(&user->storage_password);
	od_cfg_string_field_free(&user->ldap_endpoint_name);
	od_cfg_string_field_free(&user->ldap_storage_credentials_attr);
	od_cfg_string_field_free(&user->watchdog_lag_query);
	od_cfg_string_field_free(&user->group_query);
	od_cfg_string_field_free(&user->group_query_user);
	od_cfg_string_field_free(&user->group_query_db);

	od_cfg_string_kvp_list_free(&user->pgoptions);
	od_cfg_string_kvp_list_free(&user->backend_startup_options);

	od_free(user);
}

static void od_cfg_database_free(od_cfg_database_t *db)
{
	if (db == NULL) {
		return;
	}

	for (size_t i = 0; i < db->users_count; ++i) {
		od_cfg_user_route_free(db->users[i]);
	}
	od_free(db->users);

	for (size_t i = 0; i < db->groups_count; ++i) {
		od_cfg_user_route_free(db->groups[i]);
	}
	od_free(db->groups);

	od_free(db->name);

	od_free(db);
}

void od_cfg_model_free(od_cfg_model_t *model)
{
	od_cfg_string_field_free(&model->global.pid_file);
	od_cfg_string_field_free(&model->global.unix_socket_dir);
	od_cfg_string_field_free(&model->global.unix_socket_mode);
	od_cfg_string_field_free(&model->global.locks_dir);
	od_cfg_string_field_free(&model->global.external_auth_socket_path);
	od_cfg_string_field_free(&model->global.availability_zone);
	od_cfg_string_field_free(&model->global.log_file);
	od_cfg_string_field_free(&model->global.log_format);
	od_cfg_string_field_free(&model->global.log_syslog_ident);
	od_cfg_string_field_free(&model->global.log_syslog_facility);
	od_cfg_string_field_free(&model->global.cpu_affinity);
	od_cfg_string_field_free(&model->global.hba_file);

	if (model->soft_oom.seen.is_set) {
		od_cfg_string_field_free(&model->soft_oom.process);

		if (model->soft_oom.drop.seen.is_set) {
			od_cfg_string_field_free(&model->soft_oom.drop.signal);
		}
	}

	for (size_t i = 0; i < model->databases_count; ++i) {
		od_cfg_database_free(model->databases[i]);
	}
	od_free(model->databases);

	for (size_t i = 0; i < model->storages_count; ++i) {
		od_cfg_storage_free(model->storages[i]);
	}
	od_free(model->storages);

	for (size_t i = 0; i < model->listens_count; ++i) {
		od_cfg_listen_free(model->listens[i]);
	}
	od_free(model->listens);

	for (size_t i = 0; i < model->shared_pools_count; ++i) {
		od_cfg_shared_pool_free(model->shared_pools[i]);
	}
	od_free(model->shared_pools);

	for (size_t i = 0; i < model->ldap_endpoints_count; ++i) {
		od_cfg_ldap_endpoint_free(model->ldap_endpoints[i]);
	}
	od_free(model->ldap_endpoints);

	memset(model, 0, sizeof(*model));
}

od_cfg_route_t *od_cfg_storage_add_watchdog(od_cfg_storage_t *s,
					    od_cfg_location_t loc)
{
	s->watchdog = od_malloc(sizeof(*s->watchdog));
	if (s->watchdog == NULL) {
		return NULL;
	}
	memset(s->watchdog, 0, sizeof(*s->watchdog));

	s->watchdog->location = loc;

	return s->watchdog;
}

od_cfg_listen_t *od_cfg_model_add_listen(od_cfg_model_t *model,
					 od_cfg_location_t location)
{
	if (model->listens_count == model->listens_capacity) {
		size_t new_capacity = model->listens_capacity == 0 ?
					      16 :
					      model->listens_capacity * 2;

		od_cfg_listen_t **new_items =
			od_realloc(model->listens,
				   sizeof(od_cfg_listen_t *) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		model->listens = new_items;
		model->listens_capacity = new_capacity;
	}

	od_cfg_listen_t *listen = od_malloc(sizeof(*listen));
	if (listen == NULL) {
		return NULL;
	}
	memset(listen, 0, sizeof(*listen));

	listen->location = location;

	model->listens[model->listens_count++] = listen;
	return listen;
}

od_cfg_string_kvp_t *
od_cfg_string_kvp_list_add(od_cfg_string_kvp_list_t *list,
			   od_cfg_location_t location, char *key,
			   od_cfg_location_t key_location, char *value,
			   od_cfg_location_t value_location)
{
	if (list->pairs_count == list->pairs_capacity) {
		size_t new_capacity = list->pairs_capacity == 0 ?
					      16 :
					      list->pairs_capacity * 2;
		od_cfg_string_kvp_t **new_items =
			od_realloc(list->pairs, sizeof(od_cfg_string_kvp_t *) *
							new_capacity);

		if (new_items == NULL) {
			return NULL;
		}

		list->pairs = new_items;
		list->pairs_capacity = new_capacity;
	}

	od_cfg_string_kvp_t *kvp = od_malloc(sizeof(*kvp));
	if (kvp == NULL) {
		return NULL;
	}
	memset(kvp, 0, sizeof(*kvp));

	kvp->location = location;

	kvp->key = key;
	kvp->key_location = key_location;

	kvp->value = value;
	kvp->value_location = value_location;

	list->pairs[list->pairs_count++] = kvp;
	return kvp;
}

od_cfg_ldap_storage_credentials_t *
od_cfg_user_route_add_ldap_creds(od_cfg_route_t *r, od_cfg_location_t location,
				 char *name)
{
	if (r->ldap_storage_credentials_count ==
	    r->ldap_storage_credentials_capacity) {
		size_t new_capacity =
			r->ldap_storage_credentials_capacity == 0 ?
				16 :
				r->ldap_storage_credentials_capacity * 2;
		od_cfg_ldap_storage_credentials_t **new_items =
			od_realloc(r->ldap_storage_credentials,
				   sizeof(od_cfg_ldap_storage_credentials_t *) *
					   new_capacity);

		if (new_items == NULL) {
			return NULL;
		}

		r->ldap_storage_credentials = new_items;
		r->ldap_storage_credentials_capacity = new_capacity;
	}

	od_cfg_ldap_storage_credentials_t *c = od_malloc(sizeof(*c));
	if (c == NULL) {
		return NULL;
	}
	memset(c, 0, sizeof(*c));

	c->location = location;
	c->name = name;
	c->name_location = location;

	r->ldap_storage_credentials[r->ldap_storage_credentials_count++] = c;
	return c;
}

od_cfg_rule_auth_common_name_t *od_cfg_user_route_add_auth_common_name(
	od_cfg_route_t *r, char *name, int is_default,
	od_cfg_location_t location, od_cfg_location_t name_location)
{
	if (r->auth_common_names_count == r->auth_common_names_capacity) {
		size_t new_capacity = r->auth_common_names_capacity == 0 ?
					      16 :
					      r->auth_common_names_capacity * 2;
		od_cfg_rule_auth_common_name_t **new_items =
			od_realloc(r->auth_common_names,
				   sizeof(od_cfg_rule_auth_common_name_t *) *
					   new_capacity);

		if (new_items == NULL) {
			return NULL;
		}

		r->auth_common_names = new_items;
		r->auth_common_names_capacity = new_capacity;
	}

	od_cfg_rule_auth_common_name_t *cn = od_malloc(sizeof(*cn));
	if (cn == NULL) {
		return NULL;
	}
	memset(cn, 0, sizeof(*cn));

	cn->name = name;
	cn->location = location;
	cn->name_location = name_location;
	cn->is_default = is_default;

	r->auth_common_names[r->auth_common_names_count++] = cn;
	return cn;
}

od_cfg_route_t *od_cfg_database_add_user(
	od_cfg_database_t *db, char *name, int is_default,
	od_cfg_location_t name_location, od_cfg_location_t location,
	char *addr_range, od_cfg_location_t addr_range_location,
	od_cfg_conn_type_t conn_type, od_cfg_location_t conn_type_location)
{
	if (db->users_count == db->users_capacity) {
		size_t new_capacity =
			db->users_capacity == 0 ? 16 : db->users_capacity * 2;

		od_cfg_route_t **new_items = od_realloc(
			db->users, sizeof(od_cfg_route_t *) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		db->users = new_items;
		db->users_capacity = new_capacity;
	}

	if (is_default) {
		name = od_strdup("default_user");
		if (name == NULL) {
			return NULL;
		}
	}

	od_cfg_route_t *user = od_malloc(sizeof(*user));
	if (user == NULL) {
		if (is_default) {
			od_free(name);
		}
		return NULL;
	}
	memset(user, 0, sizeof(*user));

	user->name = name;
	user->name_location = name_location;
	user->is_default = is_default;
	user->location = location;

	user->address_range = addr_range;
	user->address_range_location = addr_range_location;

	user->conn_type = conn_type;
	user->conn_type_location = conn_type_location;

	db->users[db->users_count++] = user;
	return user;
}

od_cfg_route_t *od_cfg_database_add_group(od_cfg_database_t *db, char *name,
					  od_cfg_location_t name_location,
					  od_cfg_location_t location,
					  char *addr_range,
					  od_cfg_location_t addr_range_location,
					  od_cfg_conn_type_t conn_type,
					  od_cfg_location_t conn_type_location)
{
	if (db->groups_count == db->groups_capacity) {
		size_t new_capacity =
			db->groups_capacity == 0 ? 16 : db->groups_capacity * 2;

		od_cfg_route_t **new_items = od_realloc(
			db->groups, sizeof(od_cfg_route_t *) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		db->groups = new_items;
		db->groups_capacity = new_capacity;
	}

	od_cfg_route_t *group = od_malloc(sizeof(*group));
	if (group == NULL) {
		return NULL;
	}
	memset(group, 0, sizeof(*group));

	group->name = name;
	group->name_location = name_location;
	group->location = location;

	group->address_range = addr_range;
	group->address_range_location = addr_range_location;

	group->conn_type = conn_type;
	group->conn_type_location = conn_type_location;

	db->groups[db->groups_count++] = group;
	return group;
}

od_cfg_database_t *od_cfg_model_add_database(od_cfg_model_t *model, char *name,
					     int is_default,
					     od_cfg_location_t name_location,
					     od_cfg_location_t location)
{
	if (model->databases_count == model->databases_capacity) {
		size_t new_capacity = model->databases_capacity == 0 ?
					      16 :
					      model->databases_capacity * 2;

		od_cfg_database_t **new_items =
			od_realloc(model->databases,
				   sizeof(od_cfg_database_t *) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		model->databases = new_items;
		model->databases_capacity = new_capacity;
	}

	if (is_default) {
		name = od_strdup("default_db");
		if (name == NULL) {
			return NULL;
		}
	}

	od_cfg_database_t *database = od_malloc(sizeof(*database));
	if (database == NULL) {
		if (is_default) {
			od_free(name);
		}
		return NULL;
	}
	memset(database, 0, sizeof(*database));

	database->location = location;
	database->name = name;
	database->is_default = is_default;
	database->name_location = name_location;

	model->databases[model->databases_count++] = database;
	return database;
}

od_cfg_storage_t *od_cfg_model_add_storage(od_cfg_model_t *model, char *name,
					   od_cfg_location_t name_location,
					   od_cfg_location_t location)
{
	if (model->storages_count == model->storages_capacity) {
		size_t new_capacity = model->storages_capacity == 0 ?
					      16 :
					      model->storages_capacity * 2;

		od_cfg_storage_t **new_items =
			od_realloc(model->storages,
				   sizeof(od_cfg_storage_t *) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		model->storages = new_items;
		model->storages_capacity = new_capacity;
	}

	od_cfg_storage_t *storage = od_malloc(sizeof(*storage));
	if (storage == NULL) {
		return NULL;
	}
	memset(storage, 0, sizeof(*storage));

	storage->location = location;
	storage->name = name;
	storage->name_location = name_location;

	model->storages[model->storages_count++] = storage;
	return storage;
}

static int od_cfg_check_duplicate(od_cfg_diag_list_t *diags,
				  od_cfg_seen_t *seen,
				  od_cfg_location_t location,
				  const char *field_name)
{
	if (!seen->is_set) {
		return 0;
	}

	od_cfg_diag_error(diags, location, "duplicate option '%s'", field_name);

	/*
	 * TODO: add hint
	 * previously defined at file:line:column
	 */

	return -1;
}

int od_cfg_set_bool(od_cfg_diag_list_t *diags, od_cfg_bool_field_t *field,
		    int value, od_cfg_location_t location,
		    const char *field_name)
{
	if (od_cfg_check_duplicate(diags, &field->seen, location, field_name) !=
	    0) {
		return -1;
	}

	field->value = value ? 1 : 0;
	field->seen.is_set = 1;
	field->seen.location = location;
	return 0;
}

int od_cfg_set_int(od_cfg_diag_list_t *diags, od_cfg_int_field_t *field,
		   int value, od_cfg_location_t location,
		   const char *field_name)
{
	if (od_cfg_check_duplicate(diags, &field->seen, location, field_name) !=
	    0) {
		return -1;
	}

	field->value = value;
	field->seen.is_set = 1;
	field->seen.location = location;
	return 0;
}

int od_cfg_set_int_from_i64(od_cfg_diag_list_t *diags,
			    od_cfg_int_field_t *field, int64_t value,
			    od_cfg_location_t location, const char *field_name)
{
	if (value < INT_MIN || value > INT_MAX) {
		od_cfg_diag_error(diags, location,
				  "value for option '%s' is out of int range",
				  field_name);
		return -1;
	}

	return od_cfg_set_int(diags, field, (int)value, location, field_name);
}

int od_cfg_set_int_range_from_i64(od_cfg_diag_list_t *diags,
				  od_cfg_int_field_t *field, int64_t value,
				  int min, int max, od_cfg_location_t location,
				  const char *field_name)
{
	if (min > max) {
		od_cfg_diag_error(
			diags, location,
			"internal error: invalid range for option '%s'",
			field_name);
		return -1;
	}

	if (value < (int64_t)INT_MIN || value > (int64_t)INT_MAX) {
		od_cfg_diag_error(diags, location,
				  "value '%s' = %lld is out of integer bounds",
				  field_name, (long long)value);
		return -1;
	}

	if (value < min || value > max) {
		od_cfg_diag_error(
			diags, location,
			"value for option '%s' must be between %d and %d",
			field_name, min, max);
		return -1;
	}

	return od_cfg_set_int(diags, field, (int)value, location, field_name);
}

int od_cfg_set_u64(od_cfg_diag_list_t *diags, od_cfg_u64_field_t *field,
		   uint64_t value, od_cfg_location_t location,
		   const char *field_name)
{
	if (od_cfg_check_duplicate(diags, &field->seen, location, field_name) !=
	    0) {
		return -1;
	}

	field->value = value;
	field->seen.is_set = 1;
	field->seen.location = location;
	return 0;
}

static int od_cfg_validate_shared_pools(od_cfg_model_t *model,
					od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < model->shared_pools_count; i++) {
		od_cfg_shared_pool_t *pool = model->shared_pools[i];

		if (pool->name == NULL || pool->name[0] == 0) {
			od_cfg_diag_error(diags, pool->name_location,
					  "shared_pool name cannot be empty");
		}

		if (!pool->pool_size.seen.is_set) {
			od_cfg_diag_error(
				diags, pool->location,
				"pool_size is required in shared_pool '%s'",
				pool->name ? pool->name : "<unknown>");
		} else if (pool->pool_size.value <= 0) {
			od_cfg_diag_error(
				diags, pool->pool_size.seen.location,
				"shared_pool '%s' pool_size must be positive",
				pool->name);
		}

		for (size_t j = 0; j < i; j++) {
			od_cfg_shared_pool_t *prev = model->shared_pools[j];

			if (strcmp(prev->name, pool->name) == 0) {
				od_cfg_diag_error(
					diags, pool->name_location,
					"duplicate shared_pool definition '%s'",
					pool->name);
				break;
			}
		}
	}

	return od_cfg_diag_has_errors(diags) ? -1 : 0;
}

static int od_cfg_validate_ldap_endpoints(od_cfg_model_t *model,
					  od_cfg_diag_list_t *diags)
{
	if (model->ldap_endpoints_count == 0) {
		return 0;
	}

#ifndef LDAP_FOUND
	for (size_t i = 0; i < model->ldap_endpoints_count; i++) {
		od_cfg_diag_error(diags, model->ldap_endpoints[i]->location,
				  "LDAP is not supported by this build");
	}
	return -1;
#endif

	for (size_t i = 0; i < model->ldap_endpoints_count; i++) {
		od_cfg_ldap_endpoint_t *endpoint = model->ldap_endpoints[i];

		if (endpoint->name == NULL || endpoint->name[0] == 0) {
			od_cfg_diag_error(diags, endpoint->name_location,
					  "ldap_endpoint name cannot be empty");
			return -1;
		}

		for (size_t j = 0; j < i; j++) {
			od_cfg_ldap_endpoint_t *prev = model->ldap_endpoints[j];

			if (strcmp(prev->name, endpoint->name) == 0) {
				od_cfg_diag_error(
					diags, endpoint->name_location,
					"duplicate ldap endpoint definition: %s",
					endpoint->name);
				return -1;
			}
		}

		if (endpoint->ldapport.seen.is_set &&
		    endpoint->ldapport.value > 65535) {
			od_cfg_diag_error(
				diags, endpoint->ldapport.seen.location,
				"ldapport must be between 0 and 65535");
			return -1;
		}
	}

	return 0;
}

static int od_cfg_validate_conn_drop_options(od_cfg_model_t *model,
					     od_cfg_diag_list_t *diags)
{
	od_cfg_conn_drop_options_t *opts = &model->conn_drop_options;

	if (!opts->seen.is_set) {
		return 0;
	}

	if (opts->interval_ms.seen.is_set && opts->interval_ms.value <= 0) {
		od_cfg_diag_error(
			diags, opts->interval_ms.seen.location,
			"interval for conn_drop_options must be positive");
		return -1;
	}

	return 0;
}

static int od_cfg_validate_soft_oom(od_cfg_model_t *model,
				    od_cfg_diag_list_t *diags)
{
	od_cfg_soft_oom_t *so = &model->soft_oom;

	if (!so->seen.is_set) {
		return 0;
	}

	if (so->seen.is_set &&
	    (!so->limit.seen.is_set || so->limit.value == 0)) {
		od_cfg_diag_error(diags, so->location,
				  "limit is not set for soft_oom");
		return -1;
	}

	return 0;
}

static int od_cfg_validate_listens(od_cfg_model_t *model,
				   od_cfg_diag_list_t *diags)
{
	if (model->listens_count == 0) {
		od_cfg_diag_error(diags, model->location,
				  "no listen sections defined");
		return -1;
	}

	for (size_t i = 0; i < model->listens_count; i++) {
		od_cfg_listen_t *l = model->listens[i];

		if (!l->port.seen.is_set) {
			od_cfg_diag_error(diags, l->location,
					  "listen section is missing 'port'");
		}

		if (l->tls.seen.is_set) {
			const char *tls = l->tls.value;
			if (strcmp(tls, "disable") != 0 &&
			    strcmp(tls, "allow") != 0 &&
			    strcmp(tls, "require") != 0 &&
			    strcmp(tls, "verify_ca") != 0 &&
			    strcmp(tls, "verify_full") != 0) {
				od_cfg_diag_error(diags, l->tls.seen.location,
						  "unknown tls mode '%s'", tls);
			}
		}
	}

	return od_cfg_diag_has_errors(diags) ? -1 : 0;
}

static int od_cfg_validate_storages(od_cfg_model_t *model,
				    od_cfg_diag_list_t *diags)
{
	if (model->storages_count == 0) {
		od_cfg_diag_error(diags, model->location,
				  "no storage sections defined");
		return -1;
	}

	for (size_t i = 0; i < model->storages_count; i++) {
		od_cfg_storage_t *s = model->storages[i];

		if (s->name == NULL || s->name[0] == '\0') {
			od_cfg_diag_error(diags, s->location,
					  "storage name cannot be empty");
			continue;
		}

		if (!s->type.seen.is_set) {
			od_cfg_diag_error(diags, s->location,
					  "storage '%s': 'type' is required",
					  s->name);
		} else if (strcmp(s->type.value, "remote") != 0 &&
			   strcmp(s->type.value, "local") != 0) {
			od_cfg_diag_error(diags, s->type.seen.location,
					  "storage '%s': unknown type '%s'",
					  s->name, s->type.value);
		}

		/* duplicate name check */
		for (size_t j = 0; j < i; j++) {
			if (strcmp(model->storages[j]->name, s->name) == 0) {
				od_cfg_diag_error(
					diags, s->location,
					"duplicate storage definition '%s'",
					s->name);
				break;
			}
		}
	}

	return od_cfg_diag_has_errors(diags) ? -1 : 0;
}

static int od_cfg_validate_route(const od_cfg_route_t *route,
				 od_cfg_model_t *model,
				 od_cfg_diag_list_t *diags)
{
	/* storage reference must exist */
	if (route->storage.seen.is_set) {
		int found = 0;
		for (size_t k = 0; k < model->storages_count; k++) {
			if (strcmp(model->storages[k]->name,
				   route->storage.value) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			od_cfg_diag_error(diags, route->storage.seen.location,
					  "unknown storage '%s'",
					  route->storage.value);
		}
	}

	/* shared_pool reference must exist */
	if (route->shared_pool.seen.is_set) {
		int found = 0;
		for (size_t k = 0; k < model->shared_pools_count; k++) {
			if (strcmp(model->shared_pools[k]->name,
				   route->shared_pool.value) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			od_cfg_diag_error(diags,
					  route->shared_pool.seen.location,
					  "unknown shared_pool '%s'",
					  route->shared_pool.value);
		}
	}

	/* ldap_endpoint reference must exist */
	if (route->ldap_endpoint_name.seen.is_set) {
		int found = 0;
		for (size_t k = 0; k < model->ldap_endpoints_count; k++) {
			if (strcmp(model->ldap_endpoints[k]->name,
				   route->ldap_endpoint_name.value) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			od_cfg_diag_error(
				diags, route->ldap_endpoint_name.seen.location,
				"unknown ldap_endpoint '%s'",
				route->ldap_endpoint_name.value);
		}
	}

	return 0;
}

static int od_cfg_validate_db(od_cfg_database_t *db, od_cfg_model_t *model,
			      od_cfg_diag_list_t *diags)
{
	if (db->name == NULL || db->name[0] == '\0') {
		od_cfg_diag_error(diags, db->location,
				  "invalid name for database");
		return -1;
	}

	for (size_t i = 0; i < db->users_count; i++) {
		od_cfg_validate_route(db->users[i], model, diags);
	}

	for (size_t i = 0; i < db->groups_count; i++) {
		od_cfg_validate_route(db->groups[i], model, diags);
	}

	return 0;
}

static int od_cfg_validate_dbs(od_cfg_model_t *model, od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < model->databases_count; ++i) {
		od_cfg_database_t *db = model->databases[i];

		od_cfg_validate_db(db, model, diags);
	}

	return od_cfg_diag_has_errors(diags) ? -1 : 0;
}

int od_cfg_validate_model(od_cfg_model_t *model, od_cfg_diag_list_t *diags)
{
	int rc;

	rc = od_cfg_validate_listens(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_storages(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_shared_pools(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_conn_drop_options(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_soft_oom(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_ldap_endpoints(model, diags);
	if (rc != 0) {
		return rc;
	}

	rc = od_cfg_validate_dbs(model, diags);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

int od_cfg_set_string(od_cfg_diag_list_t *diags, od_cfg_string_field_t *field,
		      char *value, od_cfg_location_t location,
		      const char *field_name)
{
	if (od_cfg_check_duplicate(diags, &field->seen, location, field_name) !=
	    0) {
		od_free(value);
		return -1;
	}

	field->value = value;
	field->seen.is_set = 1;
	field->seen.location = location;
	return 0;
}

od_cfg_shared_pool_t *
od_cfg_model_add_shared_pool(od_cfg_model_t *model, char *name,
			     od_cfg_location_t name_location,
			     od_cfg_location_t location)
{
	if (model->shared_pools_count == model->shared_pools_capacity) {
		size_t new_capacity = model->shared_pools_capacity == 0 ?
					      16 :
					      model->shared_pools_capacity * 2;

		od_cfg_shared_pool_t **new_items =
			od_realloc(model->shared_pools,
				   sizeof(*model->shared_pools) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		model->shared_pools = new_items;
		model->shared_pools_capacity = new_capacity;
	}

	od_cfg_shared_pool_t *pool = od_malloc(sizeof(*pool));
	if (pool == NULL) {
		return NULL;
	}

	memset(pool, 0, sizeof(*pool));
	pool->location = location;
	pool->name = name;
	pool->name_location = name_location;

	model->shared_pools[model->shared_pools_count++] = pool;
	return pool;
}

od_cfg_ldap_endpoint_t *
od_cfg_model_add_ldap_endpoint(od_cfg_model_t *model, char *name,
			       od_cfg_location_t name_location,
			       od_cfg_location_t location)
{
	if (model->ldap_endpoints_count == model->ldap_endpoints_capacity) {
		size_t new_capacity =
			model->ldap_endpoints_capacity == 0 ?
				16 :
				model->ldap_endpoints_capacity * 2;

		od_cfg_ldap_endpoint_t **new_items = od_realloc(
			model->ldap_endpoints,
			sizeof(*model->ldap_endpoints) * new_capacity);
		if (new_items == NULL) {
			return NULL;
		}

		model->ldap_endpoints = new_items;
		model->ldap_endpoints_capacity = new_capacity;
	}

	od_cfg_ldap_endpoint_t *endpoint = od_malloc(sizeof(*endpoint));
	if (endpoint == NULL) {
		return NULL;
	}

	memset(endpoint, 0, sizeof(*endpoint));

	endpoint->location = location;
	endpoint->name = name;
	endpoint->name_location = name_location;

	model->ldap_endpoints[model->ldap_endpoints_count++] = endpoint;
	return endpoint;
}
