
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_pid_init(od_pid_t *pid)
{
	pid->pid = getpid();
	pid->pid_len = od_snprintf(pid->pid_sz, sizeof(pid->pid_sz), "%d", (int)pid->pid);
}

int
od_pid_create(od_pid_t *pid, char *path)
{
	char buffer[32];
	size_t size = od_snprintf(buffer, sizeof(buffer), "%d\n", pid->pid);
	int rc;
	rc = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (rc == -1)
		return -1;
	int fd = rc;
	rc = write(fd, buffer, size);
	if (rc != size) {
		close(fd);
		return -1;
	}
	rc = close(fd);
	return rc;
}

int
od_pid_unlink(od_pid_t *pid, char *path)
{
	(void)pid;
	int rc = unlink(path);
	return rc;
}
