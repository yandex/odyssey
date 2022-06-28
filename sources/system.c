
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static inline od_retcode_t od_system_server_pre_stop(od_system_server_t *server)
{
	/* shutdown */
	od_retcode_t rc;
	rc = machine_shutdown_receptions(server->io);

	if (rc == -1)
		return NOT_OK_RESPONSE;
	return OK_RESPONSE;
}

static inline void od_system_server(void *arg)
{
	od_system_server_t *server = arg;
	od_instance_t *instance = server->global->instance;
	od_router_t *router = server->global->router;

	for (;;) {
		/* do not accept new client */
		if (server->closed) {
			od_dbg_printf_on_dvl_lvl(1, "%s shutting receptions\n",
						 server->sid.id);
			od_system_server_pre_stop(server);
			server->pre_exited = true;
			break;
		}

		/* accepted client io is not attached to epoll context yet */
		machine_io_t *client_io;
		int rc;
		rc = machine_accept(server->io, &client_io,
				    server->config->backlog, 0, UINT32_MAX);
		if (rc == -1) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "accept failed: %s",
				 machine_error(server->io));
			int errno_ = machine_errno();
			if (errno_ == EADDRINUSE)
				break;
			continue;
		}

		/* set network options */
		machine_set_nodelay(client_io, instance->config.nodelay);
		if (instance->config.keepalive > 0)
			machine_set_keepalive(
				client_io, 1, instance->config.keepalive,
				instance->config.keepalive_keep_interval,
				instance->config.keepalive_probes,
				instance->config.keepalive_usr_timeout);

		machine_io_t *notify_io;
		notify_io = machine_io_create();
		if (notify_io == NULL) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "failed to allocate client io notify object");
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		rc = machine_eventfd(notify_io);
		if (rc == -1) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "failed to get eventfd for client: %s",
				 machine_error(client_io));
			machine_close(notify_io);
			machine_io_free(notify_io);
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}

		/* allocate new client */
		od_client_t *client = od_client_allocate();
		if (client == NULL) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "failed to allocate client object");
			machine_close(notify_io);
			machine_io_free(notify_io);
			machine_close(client_io);
			machine_io_free(client_io);
			continue;
		}
		od_id_generate(&client->id, "c");
		rc = od_io_prepare(&client->io, client_io,
				   instance->config.readahead);
		if (rc == -1) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "failed to allocate client io object");
			machine_close(notify_io);
			machine_io_free(notify_io);
			machine_close(client_io);
			machine_io_free(client_io);
			od_client_free(client);
			continue;
		}
		client->rule = NULL;
		client->config_listen = server->config;
		client->tls = server->tls;
		client->time_accept = 0;
		client->notify_io = notify_io;
		client->time_accept = machine_time_us();

		/* create new client event and pass it to worker pool */
		machine_msg_t *msg;
		msg = machine_msg_create(sizeof(od_client_t *));
		machine_msg_set_type(msg, OD_MSG_CLIENT_NEW);
		memcpy(machine_msg_data(msg), &client, sizeof(od_client_t *));

		od_worker_pool_t *worker_pool = server->global->worker_pool;
		od_atomic_u32_inc(&router->clients_routing);
		od_worker_pool_feed(worker_pool, msg);
		while (od_atomic_u32_of(&router->clients_routing) >=
		       (uint32_t)instance->config.client_max_routing) {
			machine_sleep(1);
		}
	}

	od_worker_pool_t *worker_pool = server->global->worker_pool;

	od_worker_pool_wait_gracefully_shutdown(worker_pool);
}

static inline int od_system_server_start(od_system_t *system,
					 od_config_listen_t *config,
					 struct addrinfo *addr)
{
	od_instance_t *instance = system->global->instance;
	od_system_server_t *server;
	server = malloc(sizeof(od_system_server_t));
	if (server == NULL) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to allocate system server object");
		return -1;
	}
	server->config = config;
	server->addr = addr;
	server->io = NULL;
	server->tls = NULL;
	server->global = system->global;
	od_id_generate(&server->sid, "sid");
	server->closed = false;
	server->pre_exited = false;

	/* create server tls */
	if (server->config->tls_opts->tls_mode != OD_CONFIG_TLS_DISABLE) {
		server->tls = od_tls_frontend(server->config);
		if (server->tls == NULL) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "failed to create tls handler");
			free(server);
			return -1;
		}
	}

	/* create server io */
	server->io = machine_io_create();
	if (server->io == NULL) {
		od_error(&instance->logger, "server", NULL, NULL,
			 "failed to create system io");
		if (server->tls)
			machine_tls_free(server->tls);
		free(server);
		return -1;
	}

	char addr_name[PATH_MAX];
	int addr_name_len;
	struct sockaddr_un saddr_un;
	struct sockaddr *saddr;
	if (server->addr) {
		/* resolve listen address and port */
		od_getaddrname(server->addr, addr_name, sizeof(addr_name), 1,
			       1);
		addr_name_len = strlen(addr_name);
		saddr = server->addr->ai_addr;
	} else {
		/* set unix socket path */
		memset(&saddr_un, 0, sizeof(saddr_un));
		saddr_un.sun_family = AF_UNIX;
		saddr = (struct sockaddr *)&saddr_un;
		addr_name_len = od_snprintf(addr_name, sizeof(addr_name),
					    "%s/.s.PGSQL.%d",
					    instance->config.unix_socket_dir,
					    config->port);
		strncpy(saddr_un.sun_path, addr_name, addr_name_len);
	}

	/* bind */
	int rc;
	if (instance->config.bindwith_reuseport) {
		rc = machine_bind(server->io, saddr,
				  MM_BINDWITH_SO_REUSEPORT |
					  MM_BINDWITH_SO_REUSEADDR);
	} else {
		rc = machine_bind(server->io, saddr, MM_BINDWITH_SO_REUSEADDR);
	}

	if (rc == -1) {
		od_error(&instance->logger, "server", NULL, NULL,
			 "bind to '%s' failed: %s", addr_name,
			 machine_error(server->io));
		if (server->tls)
			machine_tls_free(server->tls);
		machine_close(server->io);
		machine_io_free(server->io);
		free(server);
		return -1;
	}

	/* chmod */
	if (server->addr == NULL) {
		long mode;
		mode = strtol(instance->config.unix_socket_mode, NULL, 8);
		if ((errno == ERANGE &&
		     (mode == LONG_MAX || mode == LONG_MIN))) {
			od_error(&instance->logger, "server", NULL, NULL,
				 "incorrect unix_socket_mode");
		} else {
			rc = chmod(saddr_un.sun_path, mode);
			if (rc == -1) {
				od_error(&instance->logger, "server", NULL,
					 NULL, "chmod(%s, %d) failed",
					 saddr_un.sun_path,
					 instance->config.unix_socket_mode);
			}
		}
	}

	od_log(&instance->logger, "server", NULL, NULL, "listening on %s",
	       addr_name);

	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_system_server, server);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to start server coroutine");
		if (server->tls)
			machine_tls_free(server->tls);
		machine_close(server->io);
		machine_io_free(server->io);
		free(server);
		return -1;
	}

	/* register server in list for possible TLS reload */
	od_router_t *router = system->global->router;
	od_list_append(&router->servers, &server->link);
	od_dbg_printf_on_dvl_lvl(1, "server %s started successfully on %s\n",
				 server->sid.id, addr_name);
	return 0;
}

static inline int od_system_listen(od_system_t *system)
{
	od_instance_t *instance = system->global->instance;
	int binded = 0;
	od_list_t *i;
	od_list_foreach(&instance->config.listen, i)
	{
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);

		/* unix socket */
		int rc;
		if (listen->host == NULL) {
			rc = od_system_server_start(system, listen, NULL);
			if (rc == 0)
				binded++;
			continue;
		}

		/* listen '*' */
		struct addrinfo *hints_ptr = NULL;
		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = IPPROTO_TCP;
		char *host = listen->host;
		if (strcmp(listen->host, "*") == 0) {
			hints_ptr = &hints;
			host = NULL;
		}

		/* resolve listen address and port */
		char port[16];
		od_snprintf(port, sizeof(port), "%d", listen->port);
		struct addrinfo *ai = NULL;
		rc = machine_getaddrinfo(host, port, hints_ptr, &ai,
					 UINT32_MAX);
		if (rc != 0) {
			od_error(&instance->logger, "system", NULL, NULL,
				 "failed to resolve %s:%d", listen->host,
				 listen->port);
			continue;
		}

		/* listen resolved addresses */
		if (host) {
			rc = od_system_server_start(system, listen, ai);
			if (rc == 0)
				binded++;
			continue;
		}
		while (ai) {
			rc = od_system_server_start(system, listen, ai);
			if (rc == 0)
				binded++;
			ai = ai->ai_next;
		}
	}

	od_setproctitlef(
		&instance->orig_argv_ptr,
		"odyssey: version %s listening and accepting new connections ",
		OD_VERSION_NUMBER);

	return binded;
}

static inline int od_config_listen_host_cmp(char *host_listen,
					    char *host_server)
{
	if (host_listen == NULL && host_server == NULL) {
		return 0;
	}
	if (host_listen == NULL || host_server == NULL) {
		return 1;
	}
	return strcmp(host_listen, host_server);
}

void od_system_config_reload(od_system_t *system)
{
	od_instance_t *instance = system->global->instance;
	od_router_t *router = system->global->router;
	od_extention_t *extentions = system->global->extentions;
	od_hba_t *hba = system->global->hba;

	od_log(&instance->logger, "config", NULL, NULL,
	       "importing changes from '%s'", instance->config_file);

	pthread_mutex_lock(&router->rules.mu);

	od_rules_cleanup(&router->rules);

	od_error_t error;
	od_error_init(&error);

	od_config_t config;
	od_config_init(&config);

	od_rules_t rules;
	od_rules_init(&rules);

	od_hba_rules_t hba_rules;
	od_hba_rules_init(&hba_rules);

	int rc;
	rc = od_config_reader_import(&config, &rules, &error, extentions,
				     system->global, &hba_rules,
				     instance->config_file);
	if (rc == -1) {
		od_error(&instance->logger, "config", NULL, NULL, "%s",
			 error.error);
		pthread_mutex_unlock(&router->rules.mu);
		od_config_free(&config);
		od_rules_free(&rules);
		return;
	}

	rc = od_config_validate(&config, &instance->logger);
	if (rc == -1) {
		pthread_mutex_unlock(&router->rules.mu);
		od_config_free(&config);
		od_rules_free(&rules);
		return;
	}

	rc = od_rules_validate(&rules, &config, &instance->logger);
	if (rc == -1) {
		pthread_mutex_unlock(&router->rules.mu);
		od_config_free(&config);
		od_rules_free(&rules);
		return;
	}
	od_config_reload(&instance->config, &config);
	od_hba_reload(hba, &hba_rules);

	pthread_mutex_unlock(&router->rules.mu);

	/* Reload TLS certificates */
	od_list_t *i;
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		od_config_listen_t *listen_config = NULL;
		server = od_container_of(i, od_system_server_t, link);

		od_list_t *j;
		od_list_foreach(&config.listen, j)
		{
			listen_config =
				od_container_of(j, od_config_listen_t, link);
			if (listen_config->port == server->config->port &&
			    od_config_listen_host_cmp(listen_config->host,
						      server->config->host) ==
				    0) {
				// we have found matched listen config rule
				break;
			}
			listen_config = NULL;
		}

		if (listen_config == NULL) {
			od_log(&instance->logger, "reload-config", NULL, NULL,
			       "failed to match listen config for %s:%d",
			       server->config->host == NULL ?
				       "(NULL)" :
				       server->config->host,
			       server->config->port);
		} else if (server->config->tls_opts->tls_mode !=
			   listen_config->tls_opts->tls_mode) {
			od_log(&instance->logger, "reload-config", NULL, NULL,
			       "reloaded tls mode for %s:%d",
			       server->config->host == NULL ?
				       "(NULL)" :
				       server->config->host,
			       server->config->port);

			server->config->tls_opts->tls_mode =
				listen_config->tls_opts->tls_mode;
		}

		if (server->config->tls_opts->tls_mode !=
		    OD_CONFIG_TLS_DISABLE) {
			machine_tls_t *tls = od_tls_frontend(server->config);
			/* TODO: suppport changing cert files */
			if (tls != NULL) {
				server->tls = tls;
			}
		}
	}

	od_config_free(&config);
	od_hba_rules_free(&hba_rules);

	if (instance->config.log_config)
		od_rules_print(&rules, &instance->logger);

	/* Merge configuration changes.
	 *
	 * Add new routes or obsolete previous ones which are updated or not
	 * present in new config file.
	 *
	 * Force obsolete clients to disconnect.
	 */
	od_log(&instance->logger, "rules", NULL, NULL, "reconfigure rules");
	int updates;
	updates = od_router_reconfigure(router, &rules);

	od_log(&instance->logger, "rules", NULL, NULL,
	       "dispatching storage watchdogs");
	od_rules_storages_watchdogs_run(&instance->logger, &rules);

	/* free unused rules */
	od_rules_free(&rules);

	od_log(&instance->logger, "rules", NULL, NULL,
	       "%d routes created/deleted and scheduled for removal", updates);
}

static inline void od_system(void *arg)
{
	od_system_t *system = arg;
	od_instance_t *instance = system->global->instance;
	od_router_t *router = system->global->router;

	/* start cron coroutine */
	od_cron_t *cron = system->global->cron;
	int rc;
	rc = od_cron_start(cron, system->global);
	if (rc == -1)
		return;

	/* start worker threads */
	od_worker_pool_t *worker_pool = system->global->worker_pool;
	rc = od_worker_pool_start(worker_pool, system->global,
				  instance->config.workers);
	if (rc == -1)
		return;

	/* start signal handler coroutine */
	int64_t mid;
	mid = machine_create("sighandler", od_system_signal_handler, system);
	if (mid == -1) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to start signal handler");
		return;
	}

	/* start listen servers */
	rc = od_system_listen(system);
	if (rc == 0) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to bind any listen address");
		exit(1);
	}
	od_rules_storages_watchdogs_run(&instance->logger, &router->rules);

	if (instance->config.enable_online_restart_feature) {
		/* start watchdog coroutine */
		rc = od_watchdog_invoke(system);
		if (rc == NOT_OK_RESPONSE)
			return;
	}
}

void od_system_init(od_system_t *system)
{
	system->machine = -1;
	system->global = NULL;
}

int od_system_start(od_system_t *system, od_global_t *global)
{
	system->global = global;
	od_instance_t *instance = global->instance;
	system->machine = machine_create("system", od_system, system);
	if (system->machine == -1) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to create system thread");
		return -1;
	}
	return 0;
}
