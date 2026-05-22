/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <status.h>
#include <sighandler.h>
#include <system.h>
#include <extension.h>
#include <cron.h>
#include <global.h>
#include <router.h>
#include <instance.h>
#include <msg.h>
#include <setproctitle.h>
#include <worker_pool.h>
#include <restart_sync.h>
#include <systemd_notify.h>

typedef struct {
	od_system_t *system;
	machine_channel_t *channel;
} shutdown_worker_arg_t;

static void *killer(void *arg)
{
	uint64_t timeout_ms = (uint64_t)(uintptr_t)arg;

	struct timespec duration;
	memset(&duration, 0, sizeof(duration));
	duration.tv_sec = timeout_ms / 1000;
	duration.tv_nsec = (timeout_ms % 1000) * 1000000;

	struct timespec rem;
	memset(&rem, 0, sizeof(rem));

	while (1) {
		int rc = nanosleep(&duration, &rem);
		if (rc == 0 || errno != EINTR) {
			break;
		}

		duration = rem;
	}

	exit(124);
}

static void start_timeout_thread(uint64_t timeout_ms)
{
	/*
	 * start the detached plain pthread
	 * because we dont want mess with machinarium
	 * internals destroy
	 */

	pthread_t th;
	int rc = pthread_create(&th, NULL, killer,
				(void *)(uintptr_t)timeout_ms);
	if (rc == 0) {
		pthread_detach(th);
	} else {
		od_gerror("shutdown", NULL, NULL,
			  "can't start shutdown timeout killer: %d (%s)", rc,
			  strerror(rc));
	}
}

static void shutdown_servers(od_router_t *router)
{
	od_list_t *i;
	od_list_foreach (&router->servers, i) {
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		od_system_server_shutdown(server);
	}
}

static void shutdown_worker(void *arg)
{
	shutdown_worker_arg_t *warg = arg;

	od_worker_pool_t *worker_pool;
	od_system_t *system;
	od_instance_t *instance;
	od_router_t *router;
	machine_channel_t *channel;

	system = warg->system;
	worker_pool = system->global->worker_pool;
	instance = system->global->instance;
	router = system->global->router;
	channel = warg->channel;

	od_free(warg);

#ifdef ODYSSEY_VERSION_GIT
	od_setproctitlef(&instance->orig_argv_ptr, instance->orig_argv_ptr_len,
			 "odyssey %s (git %s) stop accepting any connections",
			 ODYSSEY_VERSION_NUMBER, ODYSSEY_VERSION_GIT);
#else
	od_setproctitlef(&instance->orig_argv_ptr, instance->orig_argv_ptr_len,
			 "odyssey %s stop accepting any connections",
			 ODYSSEY_VERSION_NUMBER);
#endif

	if (instance->config.graceful_shutdown_timeout_ms != 0) {
		start_timeout_thread(
			instance->config.graceful_shutdown_timeout_ms);
	}

	shutdown_servers(router);

	od_worker_pool_shutdown(worker_pool);
	od_worker_pool_wait_gracefully_shutdown(worker_pool);

	od_rules_stop_watchdogs(&router->rules);

	machine_msg_t *msg = machine_msg_create(0);
	if (msg == NULL) {
		od_fatal(&instance->logger, "system", NULL, NULL,
			 "failed to create a message in grac_shutdown_worker");
	}

	machine_msg_set_type(msg, OD_MSG_GRAC_SHUTDOWN_FINISHED);
	machine_channel_write(channel, msg);
}

static inline od_retcode_t run_shutdown_thread(od_system_t *system,
					       machine_channel_t *channel)
{
	/*
	 * run servers and workers closing in separated thread
	 * to still have an ability to receive signals
	 *
	 * all other cleanup will be done in system thread
	 */

	od_instance_t *instance = system->global->instance;
	int64_t shut_worker_id = od_instance_get_shutdown_worker_id(instance);
	if (shut_worker_id != INVALID_COROUTINE_ID) {
		return OK_RESPONSE;
	}

	/* freed in od_grac_shutdown_worker */
	shutdown_worker_arg_t *arg = od_malloc(sizeof(shutdown_worker_arg_t));
	if (arg == NULL) {
		od_fatal(&instance->logger, "gracefully_killer", NULL, NULL,
			 "failed to allocate shutdown_worker_arg");
		return NOT_OK_RESPONSE;
	}
	arg->system = system;
	arg->channel = channel;

	int64_t mid;
	mid = machine_create("shutdowner", shutdown_worker, arg);
	if (mid == -1) {
		od_fatal(&instance->logger, "gracefully_killer", NULL, NULL,
			 "failed to invoke gracefully killer coroutine");
		od_free(arg);
		return NOT_OK_RESPONSE;
	}
	od_instance_set_shutdown_worker_id(instance, mid);

	return OK_RESPONSE;
}

typedef struct waiter_arg {
	od_system_t *system;
	machine_channel_t *channel;
} waiter_arg_t;

static inline void od_signal_waiter(void *arg)
{
	waiter_arg_t *waiter_arg = arg;

	od_system_t *system = waiter_arg->system;
	machine_channel_t *channel = waiter_arg->channel;

	od_instance_t *instance = system->global->instance;

	for (;;) {
		int rc;
		rc = machine_signal_wait(UINT32_MAX);

		/* canceled */
		if (rc == -1) {
			break;
		}

		machine_msg_t *msg = machine_msg_create(sizeof(int));
		if (msg == NULL) {
			od_fatal(&instance->logger, "system", NULL, NULL,
				 "failed to create a message in sigwaiter");
		}

		int *data = machine_msg_data(msg);
		*data = rc;

		machine_msg_set_type(msg, OD_MSG_SIGNAL_RECEIVED);
		machine_channel_write(channel, msg);
	}
}

void od_system_signal_handler(void *arg)
{
	od_system_t *system = arg;
	od_instance_t *instance = system->global->instance;
	pid_t new_binary_pid = -1;
	pid_t wpid = -1;
	int wstatus = -1;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, OD_SIG_LOG_ROTATE);
	sigaddset(&mask, OD_SIG_ONLINE_RESTART);
	sigaddset(&mask, SIGWINCH);

	sigset_t ignore_mask;
	sigemptyset(&ignore_mask);
	sigaddset(&ignore_mask, SIGPIPE);

	int rc;
	rc = machine_signal_init(&mask, &ignore_mask);
	if (rc == -1) {
		od_fatal(&instance->logger, "system", NULL, NULL,
			 "failed to init signal handler (machine_signal_init)");
	}

	machine_channel_t *channel;
	channel = machine_channel_create();
	if (channel == NULL) {
		od_fatal(&instance->logger, "system", NULL, NULL,
			 "failed to init signal handler (channel creation)");
	}

	waiter_arg_t waiter_arg = { system, channel };
	int sigwaiter_id = machine_coroutine_create_named(
		od_signal_waiter, &waiter_arg, "sigwaiter");
	if (sigwaiter_id == -1) {
		od_fatal(
			&instance->logger, "system", NULL, NULL,
			"failed to init signal handler (signal waiter creation)");
	}

	int term_count = 0;

	bool graceful_shutdown_finished = false;
	while (!graceful_shutdown_finished) {
		machine_msg_t *msg = machine_channel_read(channel, UINT32_MAX);

		/* canceled */
		if (msg == NULL) {
			od_log(&instance->logger, "system", NULL, NULL,
			       "NULL message in sighandler");
			return;
		}

		int type = machine_msg_type(msg);
		switch (type) {
		case OD_MSG_SIGNAL_RECEIVED:
			break;
		case OD_MSG_GRAC_SHUTDOWN_FINISHED:
			graceful_shutdown_finished = true;
			machine_msg_free(msg);
			continue;
		default:
			assert(0);
		};

		int sig = *(int *)machine_msg_data(msg);
		machine_msg_free(msg);

		switch (sig) {
		case SIGTERM:
		case SIGINT:
			od_log(&instance->logger, "system", NULL, NULL,
			       "termination signal received, shutting down");

			if (++term_count >=
			    instance->config.max_sigterms_to_die) {
				exit(1);
			}

			/* 
			 * If we're being replaced by a new process (online restart),
			 * notify systemd of the new main PID before shutting down.
			 */
			if (new_binary_pid != -1) {
				od_systemd_notify_mainpid(new_binary_pid);

				/* signal the new binary to set ready */
				kill(new_binary_pid, SIGWINCH);
			} else {
				/* Notify systemd we're shutting down */
				od_systemd_notify_stopping();
			}

			run_shutdown_thread(system, channel);
			break;
		case SIGWINCH:
			/*
			 * old binary accepted our term signal and setup MAINPID
			 * now we must notify that we are ready
			 */

			if (instance->pid.restart_ppid == -1) {
				online_restart_log(
					"got unexpected SIGWINCH, ignored");
				break;
			}

			od_systemd_notify_ready();
			break;
		case SIGHUP:
			od_log(&instance->logger, "system", NULL, NULL,
			       "SIGHUP received");
			if (new_binary_pid != -1) {
				od_log(&instance->logger, "system", NULL, NULL,
				       "performing online restart now, SIGHUP is ignored");
				break;
			}
			od_systemd_notify_reloading("Reloading configuration");
			od_system_config_reload(system);
			od_systemd_notify_ready();
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
		case OD_SIG_ONLINE_RESTART:
			online_restart_log("online restart signal received");

			if (!instance->config.enable_online_restart_feature) {
				online_restart_error(
					"online restart signal ignored - feature is disabled in config");
				break;
			}

			if (new_binary_pid != -1) {
				online_restart_error(
					"online restart signal ignored - already spawning new binary");
				break;
			}

			if (getppid() == instance->pid.restart_ppid) {
				online_restart_error(
					"online restart signal ignored - parent odyssey process is still alive");
				break;
			}

			od_systemd_notify_reloading(
				"Graceful restart on new binary");

			new_binary_pid = od_restart_run_new_binary();
			if (new_binary_pid != -1) {
				online_restart_log("new binary pid = %d",
						   new_binary_pid);

				od_global_get_instance()->pid.restart_new_pid =
					new_binary_pid;
			} else {
				/* notify ready because no one else will - the child didnt start */
				od_systemd_notify_ready();
				online_restart_error(
					"running new binary failed - keep use old instance");
			}

			break;
		case SIGCHLD:
			wpid = waitpid(-1, &wstatus, WNOHANG);
			if (wpid == -1) {
				od_gerror("system", NULL, NULL,
					  "waitpid failed: %s",
					  strerror(errno));
				break;
			}

			if (wpid == 0) {
				break;
			}

			/* currently SIGCHLD is tracked to only catch if new binary failed to start */
			if (wpid != new_binary_pid) {
				od_glog("system", NULL, NULL,
					"waitpid returned unexpected pid(%d), ignore (the only expected is %d)",
					wpid, new_binary_pid);
				break;
			}

			/* notify ready because no one else will - the child crashed */
			od_systemd_notify_ready();

			if (WIFEXITED(wstatus)) {
				online_restart_error(
					"new binary exited(%d) - keep use old binary instance",
					WEXITSTATUS(wstatus));
				new_binary_pid = -1;
			} else if (WIFSIGNALED(wstatus)) {
				online_restart_error(
					"new binary was killed by signal(%d) - keep use old binary instance",
					WTERMSIG(wstatus));
				new_binary_pid = -1;
			}
			/* all other wait status is ignored */

			break;
		}
	}

	machine_wait(od_instance_get_shutdown_worker_id(instance));

	machine_cancel(sigwaiter_id);
	machine_join(sigwaiter_id);

	machine_channel_free(channel);
}
