
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <fcntl.h>
#include <unistd.h>

#include <machinarium/file.h>

#include <pid.h>
#include <restart_sync.h>
#include <util.h>

void od_pid_init(od_pid_t *pid)
{
	pid->pid = getpid();
	pid->restart_ppid = od_restart_get_ppid();
	atomic_init(&pid->restart_new_pid, -1);
	pid->pid_len = od_snprintf(pid->pid_sz, sizeof(pid->pid_sz), "%d",
				   (int)pid->pid);
}

void od_pid_restart_new_set(od_pid_t *p, pid_t pid)
{
	atomic_store(&p->restart_new_pid, pid);
}

pid_t od_pid_restart_new_get(od_pid_t *p)
{
	return atomic_load(&p->restart_new_pid);
}

int od_pid_create(od_pid_t *pid, char *path)
{
	char buffer[32];
	int size = od_snprintf(buffer, sizeof(buffer), "%d\n", pid->pid);
	int rc;
	rc = mm_file_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (rc == -1) {
		return -1;
	}
	int fd = rc;
	rc = mm_file_write_all(fd, buffer, (size_t)size);
	if (rc == -1) {
		mm_file_close(fd);
		return -1;
	}
	return mm_file_close(fd);
}

int od_pid_unlink(od_pid_t *pid, char *path)
{
	(void)pid;
	return mm_file_unlink(path);
}
