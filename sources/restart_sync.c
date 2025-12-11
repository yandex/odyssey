
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <restart_sync.h>
#include <global.h>
#include <instance.h>
#include <logger.h>
#include <od_memory.h>

#define ODYSSEY_PARENT_PID_ENV_NAME "ODY_INHERIT_PPID"

pid_t od_restart_get_ppid()
{
	const char *ppid_str = getenv(ODYSSEY_PARENT_PID_ENV_NAME);
	if (ppid_str == NULL) {
		return -1;
	}

	char *end;
	long pid = strtol(ppid_str, &end, 10);
	if (*end != 0) {
		/* finished not on terminate byte - some fmt error */
		return -1;
	}

	return (pid_t)pid;
}

void send_sigterm_to_parent()
{
	/*
	 * if getppid has changed - parent already dead
	 */
	pid_t target = od_global_get_instance()->pid.restart_ppid;

	if (getppid() != target) {
		return;
	}

	online_restart_log("send SIGTERM to parent instance %d", target);

	kill(target, SIGTERM);
}

void od_restart_terminate_parent()
{
	if (od_global_get_instance()->pid.restart_ppid == -1) {
		return;
	}

	static pthread_once_t parent_term_ctrl = PTHREAD_ONCE_INIT;
	(void)pthread_once(&parent_term_ctrl, send_sigterm_to_parent);
}

char **build_envp(char *inherit_val)
{
	od_instance_t *instance = od_global_get_instance();
	char **envp = instance->cmdline.envp;
	size_t name_len = strlen(ODYSSEY_PARENT_PID_ENV_NAME);

	int existed_idx = -1;
	int count;

	for (count = 0; envp[count] != NULL; ++count) {
		if (existed_idx != -1) {
			continue;
		}

		int name_diff = strncmp(envp[count],
					ODYSSEY_PARENT_PID_ENV_NAME, name_len);
		if (name_diff == 0 && envp[count][name_len] == '=') {
			existed_idx = count;
		}
	}

	int new_count = (existed_idx == -1 ? count + 1 : count);
	char **new_envp =
		od_malloc(sizeof(char *) * (new_count + 1 /* for NULL */));
	if (new_envp == NULL) {
		return NULL;
	}

	int i;
	for (i = 0; i < count; ++i) {
		if (i != existed_idx) {
			new_envp[i] = envp[i];
		} else {
			new_envp[i] = inherit_val;
		}
	}

	if (existed_idx == -1) {
		new_envp[i++] = inherit_val;
	}

	new_envp[i] = NULL;

	return new_envp;
}

pid_t od_restart_run_new_binary()
{
	char inherit_str[128];
	snprintf(inherit_str, sizeof(inherit_str), "%s=%d",
		 ODYSSEY_PARENT_PID_ENV_NAME,
		 od_global_get_instance()->pid.pid);

	od_instance_t *instance = od_global_get_instance();

	pid_t p = fork();
	if (p == -1) {
		online_restart_error("can't fork new binary: %s",
				     strerror(errno));
		return -1;
	}

	if (p != 0) {
		/* report pid of new odyssey instance */
		return p;
	}

	/* will not free any of allocations anyway */
	char **envp = build_envp(inherit_str);
	if (envp != NULL) {
		execve(instance->cmdline.argv[0], instance->cmdline.argv, envp);
	}

	online_restart_error("can't start new binary: %s", strerror(errno));
	abort();
}
