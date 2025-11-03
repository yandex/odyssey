
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <odyssey.h>

od_instance_t *od_instance_create()
{
	od_instance_t *instance = od_malloc(sizeof(od_instance_t));
	if (instance == NULL) {
		return NULL;
	}

	od_pid_init(&instance->pid);

	od_logger_init(&instance->logger, &instance->pid);
	od_config_init(&instance->config);

	instance->config_file = NULL;
	instance->shutdown_worker_id = INVALID_COROUTINE_ID;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, OD_SIG_LOG_ROTATE);
	sigaddset(&mask, OD_SIG_GRACEFUL_SHUTDOWN);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGPIPE);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	return instance;
}

void od_instance_free(od_instance_t *instance)
{
	if (instance->config.pid_file) {
		od_pid_unlink(&instance->pid, instance->config.pid_file);
	}
	od_config_free(&instance->config);
	/* as mallocd on start */
	od_free(instance->config_file);
	od_free(instance->exec_path);
	od_logger_close(&instance->logger);
	machinarium_free();
	od_free(instance);
}

void od_usage(od_instance_t *instance, char *path)
{
	od_log(&instance->logger, "init", NULL, NULL, "odyssey (git: %s %s)",
	       OD_VERSION_GIT, OD_VERSION_BUILD);
	od_log(&instance->logger, "init", NULL, NULL, "usage: %s <config_file>",
	       path);
}

int od_config_testing(od_instance_t *instance)
{
	od_error_t error;
	od_router_t router;
	od_hba_t hba;
	od_global_t global;
	od_extension_t extensions;

	od_error_init(&error);
	od_router_init(&router, &global);
	od_hba_init(&hba);
	if (od_extensions_init(&extensions) != 0) {
		od_error(&instance->logger, "config", NULL, NULL,
			 "failed to init extensions");
		goto error;
	};

	int rc;
	rc = od_config_reader_import(&instance->config, &router.rules, &error,
				     &extensions, &global, &hba.rules,
				     instance->config_file);
	if (rc == -1) {
		od_error(&instance->logger, "config", NULL, NULL, "%s",
			 error.error);
		goto error;
	}

	/* validate configuration */
	rc = od_config_validate(&instance->config, &instance->logger);
	if (rc == -1) {
		goto error;
	}

	/* validate rules */
	rc = od_rules_validate(&router.rules, &instance->config,
			       &instance->logger);
	if (rc == -1) {
		goto error;
	}

	od_log(&instance->logger, "config", NULL, NULL, "config is valid");

	return 0;

error:
	od_router_free(&router);

	return 1;
}

static inline void od_bind_version()
{
	od_asprintf((char **__restrict)&argp_program_version,
		    "odyssey (git: %s %s %s)", OD_VERSION_NUMBER,
		    OD_VERSION_GIT, OD_VERSION_BUILD);
}

static inline od_retcode_t od_args_init(od_arguments_t *args,
					od_instance_t *instance)
{
	args->silent = 0;
	args->verbose = 0;
	args->console = 0;
	args->test = 0;
	args->instance = instance;
	return OK_RESPONSE;
}

int od_instance_main(od_instance_t *instance, int argc, char **argv)
{
	od_arguments_t args;
	memset(&args, 0, sizeof(args));
	struct argp argp;
	od_bind_args(&argp);
	od_bind_version();

	/* odyssey accept only ONE positional arg - to path config */
	if (od_args_init(&args, instance) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}
	instance->exec_path = strdup(argv[0]);
	if (instance->exec_path == NULL) {
		return NOT_OK_RESPONSE;
	}
	/* validate command line options */
	int argindx; /* index of first unparsed indx */
	if (argp_parse(&argp, argc, argv, 0, &argindx, &args) != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	od_log(&instance->logger, "startup", NULL, NULL, "Starting Odyssey");

	/* prepare system services */
	od_router_t router;
	od_cron_t cron;
	od_worker_pool_t worker_pool;
	od_hba_t hba;
	od_extension_t *extensions = NULL;
	od_system_t *system = NULL;
	od_global_t *global = NULL;

	extensions = od_extensions_create();
	if (extensions == NULL) {
		od_error(&instance->logger, "config", NULL, NULL,
			 "failed to extensions init");
		goto error;
	}

	system = od_system_create();
	if (system == NULL) {
		goto error;
	}

	od_router_init(&router, NULL /* will set global later */);

	if (od_cron_init(&cron) != 0) {
		od_error(&instance->logger, "config", NULL, NULL,
			 "failed to init cron");
		goto error;
	}

	od_worker_pool_init(&worker_pool);

	od_hba_init(&hba);

	global = od_global_create(instance, system, &router, &cron,
				  &worker_pool, extensions, &hba);
	if (global == NULL) {
		goto error;
	}

	router.global = global;

	/* read config file */
	od_error_t error;
	od_error_init(&error);
	int rc;
	rc = od_config_reader_import(&instance->config, &router.rules, &error,
				     extensions, global, &hba.rules,
				     instance->config_file);
	if (rc == -1) {
		od_error(&instance->logger, "config", NULL, NULL, "%s",
			 error.error);
		goto error;
	}

	rc = od_apply_validate_cli_args(&instance->logger, &instance->config,
					&args, &router.rules);
	if (rc != OK_RESPONSE) {
		goto error;
	}

	/* validate configuration */
	rc = od_config_validate(&instance->config, &instance->logger);
	if (rc == -1) {
		goto error;
	}

	/* validate rules */
	rc = od_rules_validate(&router.rules, &instance->config,
			       &instance->logger);
	if (rc == -1) {
		goto error;
	}

	/* auto-generate default rule for auth_query if none specified */
	rc = od_rules_autogenerate_defaults(&router.rules, &instance->logger);

	if (rc == -1) {
		goto error;
	}

	/* run as daemon */
	if (instance->config.daemonize) {
		rc = od_daemonize();
		if (rc == -1) {
			goto error;
		}
		/* update pid */
		od_pid_init(&instance->pid);
	}

#ifdef PROM_FOUND
	rc = od_prom_metrics_init(cron.metrics);
	if (rc != OK_RESPONSE) {
		od_error(&instance->logger, "metrics", NULL, NULL,
			 "failed to initialize metrics");
		goto error;
	}
#ifdef PROMHTTP_FOUND
	if (instance->config.log_route_stats_prom) {
		rc = od_prom_activate_route_metrics(cron.metrics);
		if (rc != OK_RESPONSE) {
			od_error(&instance->logger, "promhttp", NULL, NULL,
				 "%s", "could not activate prom_http server");
			goto error;
		}
	} else if (instance->config.log_general_stats_prom) {
		rc = od_prom_activate_general_metrics(cron.metrics);
		if (rc != OK_RESPONSE) {
			od_error(&instance->logger, "promhttp", NULL, NULL,
				 "%s", "could not activate prom_http server");
			goto error;
		}
	}
#endif
#endif

	/* reopen log file after config parsing */
	if (instance->config.log_file) {
		rc = od_logger_open(&instance->logger,
				    instance->config.log_file);
		if (rc == -1) {
			od_error(&instance->logger, "init", NULL, NULL,
				 "failed to open log file '%s'",
				 instance->config.log_file);
			goto error;
		}
	}

	/* configure logger */
	od_logger_set_format(&instance->logger, instance->config.log_format);
	od_logger_set_debug(&instance->logger, instance->config.log_debug);
	od_logger_set_stdout(&instance->logger, instance->config.log_to_stdout);

	/* syslog */
	if (instance->config.log_syslog) {
		od_logger_open_syslog(&instance->logger,
				      instance->config.log_syslog_ident,
				      instance->config.log_syslog_facility);
	}
	od_log(&instance->logger, "init", NULL, NULL, "odyssey (git: %s %s)",
	       OD_VERSION_GIT, OD_VERSION_BUILD);
	od_log(&instance->logger, "init", NULL, NULL, "");

	/* print configuration */
	od_log(&instance->logger, "init", NULL, NULL,
	       "using configuration file '%s'", instance->config_file);
	od_log(&instance->logger, "init", NULL, NULL, "");

	if (instance->config.log_config) {
		od_config_print(&instance->config, &instance->logger);
		od_rules_print(&router.rules, &instance->logger);
	}

	/* set process priority */
	if (instance->config.priority != 0) {
		int rc;
		rc = setpriority(PRIO_PROCESS, 0, instance->config.priority);
		if (rc == -1) {
			od_error(&instance->logger, "init", NULL, NULL,
				 "failed to set process priority: %s",
				 strerror(errno));
			goto error;
		}
	}

	/* initialize machinarium */
	machinarium_set_stack_size(instance->config.coroutine_stack_size);
	machinarium_set_pool_size(instance->config.resolvers);
	machinarium_set_coroutine_cache_size(instance->config.cache_coroutine);
	machinarium_set_msg_cache_gc_size(instance->config.cache_msg_gc_size);
	rc = machinarium_init();
	if (rc == -1) {
		od_error(&instance->logger, "init", NULL, NULL,
			 "failed to init machinarium");
		goto error;
	}

	/* create pid file */
	if (instance->config.pid_file) {
		rc = od_pid_create(&instance->pid, instance->config.pid_file);
		if (rc == -1) {
			od_error(&instance->logger, "init", NULL, NULL,
				 "failed to create pid file %s: %s",
				 instance->config.pid_file, strerror(errno));
			goto error;
		}
	}

	/* start aync logging thread if needed */
	od_logger_load(&instance->logger);

	if (instance->config.soft_oom.enabled) {
		rc = od_soft_oom_start_checker(&instance->config.soft_oom,
					       &global->soft_oom);
		if (rc != OK_RESPONSE) {
			goto error;
		}
	}

	if (instance->config.host_watcher_enabled) {
		rc = od_host_watcher_init(&global->host_watcher);
		if (rc != OK_RESPONSE) {
			goto error;
		}
	}

	/* start system machine thread */
	rc = od_system_start(system, global);
	if (rc == -1) {
		goto error;
	}

	rc = machine_wait(system->machine);

	od_soft_oom_stop_checker(&global->soft_oom);

	return rc;

error:
	od_router_free(&router);
	od_extension_free(&instance->logger, extensions);
	od_system_free(system);
	return NOT_OK_RESPONSE;
}
