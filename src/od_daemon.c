
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_daemon.h"

int od_daemonize(void)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid > 0) {
		/* shutdown parent */
		_exit(0);
	}
	setsid();
	int fd;
	fd = open("/dev/null", O_RDWR);
	if (fd < 0)
		return -1;
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2)
		close(fd);
	return 0;
}
