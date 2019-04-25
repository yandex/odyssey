
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <odyssey.h>

od_config_t *
od_config_allocate(mcxt_context_t mcxt)
{
	mcxt_context_t	config_mcxt = mcxt_new(mcxt);
	od_config_t	   *config;

	config = mcxt_alloc_mem(config_mcxt, sizeof(od_config_t), true);

	config->log_to_stdout        = 1;
	config->log_session          = 1;
	config->log_stats            = 1;
	config->stats_interval       = 3;
	config->log_syslog           = 0;
	config->readahead            = 8192;
	config->nodelay              = 1;
	config->keepalive            = 7200;
	config->workers              = 1;
	config->resolvers            = 1;
	config->coroutine_stack_size = 4;
	config->mcxt				 = config_mcxt;
	od_list_init(&config->listen);

	return config;
}

void
od_config_free(od_config_t *config)
{
	fprintf(stderr, "ok");
	mcxt_delete(config->mcxt);
}

od_config_listen_t*
od_config_listen_add(od_config_t *config)
{
	od_config_listen_t *listen;

	listen = mcxt_alloc_mem(config->mcxt, sizeof(*listen), true);

	if (listen == NULL)
		return NULL;

	listen->port = 6432;
	listen->backlog = 128;

	od_list_init(&listen->link);
	od_list_append(&config->listen, &listen->link);

	return listen;
}

int
od_config_validate(od_config_t *config, od_logger_t *logger)
{
	/* workers */
	if (config->workers <= 0) {
		od_error(logger, "config", NULL, NULL, "bad workers number");
		return -1;
	}

	/* resolvers */
	if (config->resolvers <= 0) {
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
				listen->tls_mode = OD_CONFIG_TLS_DISABLE;
			} else
			if (strcmp(listen->tls, "allow") == 0) {
				listen->tls_mode = OD_CONFIG_TLS_ALLOW;
			} else
			if (strcmp(listen->tls, "require") == 0) {
				listen->tls_mode = OD_CONFIG_TLS_REQUIRE;
			} else
			if (strcmp(listen->tls, "verify_ca") == 0) {
				listen->tls_mode = OD_CONFIG_TLS_VERIFY_CA;
			} else
			if (strcmp(listen->tls, "verify_full") == 0) {
				listen->tls_mode = OD_CONFIG_TLS_VERIFY_FULL;
			} else {
				od_error(logger, "config", NULL, NULL, "unknown tls mode");
				return -1;
			}
		}
	}

	return 0;
}

static inline char*
od_config_yes_no(int value) {
	return value ? "yes" : "no";
}

void
od_config_print(od_config_t *config, od_logger_t *logger)
{
	od_log(logger, "config", NULL, NULL,
	       "daemonize            %s",
	       od_config_yes_no(config->daemonize));
	od_log(logger, "config", NULL, NULL,
	       "priority             %d", config->priority);
	if (config->pid_file)
		od_log(logger, "config", NULL, NULL,
		       "pid_file             %s", config->pid_file);
	if (config->unix_socket_dir) {
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_dir      %s", config->unix_socket_dir);
		od_log(logger, "config", NULL, NULL,
		       "unix_socket_mode     %s", config->unix_socket_mode);
	}
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
		       "  host          %s", listen->host ? listen->host : "<unix socket>");
		od_log(logger, "config", NULL, NULL,
		       "  port          %d", listen->port);
		od_log(logger, "config", NULL, NULL,
		       "  backlog       %d", listen->backlog);
		if (listen->tls)
			od_log(logger, "config", NULL, NULL,
			       "  tls           %s", listen->tls);
		if (listen->tls_ca_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_ca_file   %s", listen->tls_ca_file);
		if (listen->tls_key_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_key_file  %s", listen->tls_key_file);
		if (listen->tls_cert_file)
			od_log(logger, "config", NULL, NULL,
			       "  tls_cert_file %s", listen->tls_cert_file);
		if (listen->tls_protocols)
			od_log(logger, "config", NULL, NULL,
			       "  tls_protocols %s", listen->tls_protocols);
		od_log(logger, "config", NULL, NULL, "");
	}
}
