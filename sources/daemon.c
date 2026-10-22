
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <fcntl.h>
#include <unistd.h>

#include <machinarium/file.h>

#include <systemd_notify.h>

int od_daemonize(void)
{
	pid_t pid = fork();
	if (pid < 0) {
		return -1;
	}
	if (pid > 0) {
		/* shutdown parent */
		od_systemd_notify_mainpid(pid);
		_exit(0);
	}
	setsid();
	int fd;
	fd = mm_file_open_plain("/dev/null", O_RDWR, 0);
	if (fd < 0) {
		return -1;
	}
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2) {
		mm_file_close(fd);
	}
	return 0;
}
