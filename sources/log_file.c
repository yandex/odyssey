
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
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
#include <sys/time.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/log_file.h"

void od_logfile_init(od_logfile_t *log)
{
	log->fd = -1;
}

int od_logfile_open(od_logfile_t *log, char *path)
{
	int rc = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (rc == -1)
		return -1;
	log->fd = rc;
	return 0;
}

void od_logfile_close(od_logfile_t *log)
{
	if (log->fd == -1)
		return;
	close(log->fd);
	log->fd = -1;
}

int od_logfile_write(od_logfile_t *log, char *msg, int len)
{
	if (log->fd == -1)
		return 0;
	int rc = write(log->fd, msg, len);
	if (rc != len)
		return -1;
	return 0;
}
