
/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "od_macro.h"
#include "od_log.h"

int od_loginit(odlog_t *l)
{
	l->pid = getpid();
	l->fd = -1;
	return 0;
}

int od_logopen(odlog_t *l, char *path)
{
	int rc = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (rc == -1)
		return -1;
	l->fd = rc;
	return 0;
}

int od_logclose(odlog_t *l)
{
	if (l->fd == -1)
		return 0;
	int rc = close(l->fd);
	l->fd = -1;
	return rc;
}

int od_log(odlog_t *l, char *fmt, ...)
{
	char buffer[512];
	/* pid */
	int len = snprintf(buffer, sizeof(buffer), "%d ", l->pid);
	va_list args;
	va_start(args, fmt);
	/* time */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	len += strftime(buffer + len, sizeof(buffer) - len, "%d %b %H:%M:%S.",
	                localtime(&tv.tv_sec));
	len += snprintf(buffer + len, sizeof(buffer) - len, "%03d  ",
	                (int)tv.tv_usec / 1000);
	/* message */
	len += vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
	va_end(args);
	len += snprintf(buffer + len, sizeof(buffer), "\n");
	if (l->fd >= 0) {
		if (write(l->fd, buffer, len) == -1)
			return -1;
	} else {
		puts(buffer);
	}
	return 0;
}
