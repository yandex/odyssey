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
	bool shutting_down = false;
	while (!shutting_down) {
		machine_msg_t *msg = machine_channel_read(channel, UINT32_MAX);

		/* canceled */
		if (msg == NULL) {
			od_log(&instance->logger, "system", NULL, NULL,
			       "NULL message in sighandler");
			break;
		}

		int type = machine_msg_type(msg);
		if (type != OD_MSG_SIGNAL_RECEIVED) {
			od_assert(0);
			machine_msg_free(msg);
			continue;
		}

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

			if (od_system_send_msg(system, OD_MSG_SHUTDOWN, NULL) !=
			    0) {
				od_error(&instance->logger, "system", NULL,
					 NULL,
					 "failed to send shutdown request, "
					 "forcing exit");
				exit(1);
			}
			shutting_down = true;
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
			mm_wait_flag_t *reload_done = mm_wait_flag_create();
			if (reload_done == NULL) {
				od_error(&instance->logger, "system", NULL,
					 NULL,
					 "failed to create wait flag for "
					 "reload, notifying ready anyway");
				od_systemd_notify_ready();
				break;
			}
			if (od_system_send_msg(system, OD_MSG_RELOAD,
					       reload_done) != 0) {
				mm_wait_flag_destroy(reload_done);
				od_error(&instance->logger, "system", NULL,
					 NULL,
					 "failed to send reload request, "
					 "notifying ready anyway");
				od_systemd_notify_ready();
				break;
			}
			mm_wait_flag_wait(reload_done, UINT32_MAX);
			mm_wait_flag_destroy(reload_done);
			od_systemd_notify_ready();
			break;
		case OD_SIG_LOG_ROTATE:
			if (instance->config.log_file) {
				od_log(&instance->logger, "system", NULL, NULL,
				       "SIGUSR1 received, reopening log");
				od_logger_reopen(&instance->logger);
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

	machine_cancel(sigwaiter_id);
	machine_join(sigwaiter_id);

	machine_channel_free(channel);
}
