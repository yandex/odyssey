#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>
#include <signal.h>

typedef struct od_pid od_pid_t;

struct od_pid {
	pid_t pid;
	pid_t restart_ppid;
	atomic_int_fast32_t restart_new_pid;
	char pid_sz[16];
	int pid_len;
};

void od_pid_init(od_pid_t *);
int od_pid_create(od_pid_t *, char *);
int od_pid_unlink(od_pid_t *, char *);

void od_pid_restart_new_set(od_pid_t *p, pid_t pid);
pid_t od_pid_restart_new_get(od_pid_t *p);

#define OD_SIG_LOG_ROTATE SIGUSR1
#define OD_SIG_ONLINE_RESTART SIGUSR2
