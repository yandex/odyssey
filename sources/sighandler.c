/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

static inline od_retcode_t
od_system_gracefully_killer_invoke(od_system_t *system,
				   machine_channel_t *channel)
{
	od_instance_t *instance = system->global->instance;
	if (instance->shutdown_worker_id != INVALID_COROUTINE_ID) {
		return OK_RESPONSE;
	}

	od_grac_shutdown_worker_arg_t *arg =
		malloc(sizeof(od_grac_shutdown_worker_arg_t));
	arg->system = system;
	arg->channel = channel;

	int64_t mid;
	mid = machine_create("shutdowner", od_grac_shutdown_worker, arg);
	if (mid == -1) {
		od_error(&instance->logger, "gracefully_killer", NULL, NULL,
			 "failed to invoke gracefully killer coroutine");
		return NOT_OK_RESPONSE;
	}
	instance->shutdown_worker_id = mid;

	return OK_RESPONSE;
}

static inline void od_system_cleanup(od_system_t *system)
{
	od_instance_t *instance = system->global->instance;
	od_list_t *i;

	od_list_foreach(&instance->config.listen, i)
	{
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		if (listen->host)
			continue;
		/* remove unix socket files */
		char path[PATH_MAX];
		od_snprintf(path, sizeof(path), "%s/.s.PGSQL.%d",
			    instance->config.unix_socket_dir, listen->port);
		unlink(path);
	}
}

typedef struct waiter_arg {
	od_system_t *system;
	machine_channel_t *channel;
} waiter_arg_t;

static inline void od_signal_waiter(void *arg)
{
	waiter_arg_t *waiter_arg = mm_cast(waiter_arg_t *, arg);

	od_system_t *system = waiter_arg->system;
	machine_channel_t *channel = waiter_arg->channel;

	od_instance_t *instance = system->global->instance;

	for (;;) {
		int rc;
		rc = machine_signal_wait(UINT32_MAX);

		machine_msg_t *msg = machine_msg_create(sizeof(int));
		if (msg == NULL) {
			od_error(&instance->logger, "system", NULL, NULL,
				 "failed to create a message in sigwaiter");
			return;
		}

		int *data = machine_msg_data(msg);
		*data = rc;

		machine_msg_set_type(msg, OD_MSG_SIGNAL_RECEIVED);
		rc = machine_channel_write(channel, msg);
		if (rc != 0) {
			od_error(&instance->logger, "system", NULL, NULL,
				 "failed to write a message in sigwaiter");
			return;
		}
	}
}

void od_system_signal_handler(void *arg)
{
	od_signal_handler_arg_t *sarg = mm_cast(od_signal_handler_arg_t *, arg);

	od_system_t *system = sarg->system;
	od_instance_t *instance = system->global->instance;

	machine_wait_flag_t *is_finished = sarg->is_finished;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, OD_SIG_LOG_ROTATE);
	sigaddset(&mask, OD_SIG_GRACEFUL_SHUTDOWN);

	sigset_t ignore_mask;
	sigemptyset(&ignore_mask);
	sigaddset(&ignore_mask, SIGPIPE);

	int rc;
	rc = machine_signal_init(&mask, &ignore_mask);
	if (rc == -1) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to init signal handler (machine_signal_init)");
		return;
	}

	machine_channel_t *channel;
	channel = machine_channel_create();
	if (channel == NULL) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to init signal handler (channel creation)");
		return;
	}

	waiter_arg_t *waiter_arg = malloc(sizeof(waiter_arg_t));
	if (waiter_arg == NULL) {
		od_error(&instance->logger, "system", NULL, NULL,
			 "failed to init signal handler (waiter_arg malloc)");
		return;
	}

	waiter_arg->channel = channel;
	waiter_arg->system = system;
	int sigwaiter_id = machine_coroutine_create_named(
		od_signal_waiter, waiter_arg, "sigwaiter");
	if (sigwaiter_id == -1) {
		od_error(
			&instance->logger, "system", NULL, NULL,
			"failed to init signal handler (signal waiter creation)");
		return;
	}

	int term_count = 0;

	bool graceful_shutdown_finished = false;
	while (!graceful_shutdown_finished) {
		machine_msg_t *msg = machine_channel_read(channel, UINT32_MAX);
		if (msg == NULL) {
			od_error(&instance->logger, "system", NULL, NULL,
				 "NULL message in sighandler");
			break;
		}

		rc = *(int *)machine_msg_data(msg);
		if (rc == -1) {
			od_error(&instance->logger, "system", NULL, NULL,
				 "NOT_OK recieved from sigwaiter");
			break;
		}

		int type = machine_msg_type(msg);
		switch (type) {
		case OD_MSG_SIGNAL_RECEIVED:
			break;
		case OD_MSG_GRAC_SHUTDOWN_FINISHED:
			graceful_shutdown_finished = true;
			continue;
		default:
			assert(0);
		};

		switch (rc) {
		case SIGTERM:
		case SIGINT:
			if (++term_count >=
			    instance->config.max_sigterms_to_die) {
				exit(1);
			}

			od_system_gracefully_killer_invoke(system, channel);
			break;
		case SIGHUP:
			od_log(&instance->logger, "system", NULL, NULL,
			       "SIGHUP received");
			od_system_config_reload(system);
			break;
		case OD_SIG_LOG_ROTATE:
			if (instance->config.log_file) {
				od_log(&instance->logger, "system", NULL, NULL,
				       "SIGUSR1 received, reopening log");
				rc = od_logger_reopen(
					&instance->logger,
					instance->config.log_file);
				if (rc == -1) {
					od_error(
						&instance->logger, "system",
						NULL, NULL,
						"failed to reopen log file '%s'",
						instance->config.log_file);
				} else {
					od_log(&instance->logger, "system",
					       NULL, NULL, "log reopened");
				}
			}
			break;
		case OD_SIG_GRACEFUL_SHUTDOWN:
			if (instance->config.enable_online_restart_feature ||
			    instance->config.graceful_die_on_errors) {
				od_log(&instance->logger, "system", NULL, NULL,
				       "SIG_GRACEFUL_SHUTDOWN received");
				od_system_gracefully_killer_invoke(system,
								   channel);
			} else {
				od_log(&instance->logger, "system", NULL, NULL,
				       "SIGUSR2 received, but online restart feature not "
				       "enabled, doing nothing");
			}
			break;
		}
	}

	if (graceful_shutdown_finished) {
		machine_wait(instance->shutdown_worker_id);
	}
	machine_wait_flag_set(is_finished);
}
