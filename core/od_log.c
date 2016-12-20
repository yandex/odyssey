
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

#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_io.h"

int od_loginit(od_log_t *l, od_pid_t *pid, od_syslog_t *syslog)
{
	l->pid = pid;
	l->syslog = syslog;
	l->fd = 0;
	return 0;
}

int od_logopen(od_log_t *l, char *path)
{
	int rc = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (rc == -1)
		return -1;
	l->fd = rc;
	return 0;
}

int od_logclose(od_log_t *l)
{
	if (l->fd == -1)
		return 0;
	int rc = close(l->fd);
	l->fd = -1;
	return rc;
}

int od_logv(od_log_t *l, od_syslogprio_t prio,
            mm_io_t peer,
            char *ident,
            char *fmt, va_list args)
{
	char buffer[512];
	/* pid */
	int len = snprintf(buffer, sizeof(buffer), "%d ", l->pid->pid);
	/* time */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	len += strftime(buffer + len, sizeof(buffer) - len, "%d %b %H:%M:%S.",
	                localtime(&tv.tv_sec));
	len += snprintf(buffer + len, sizeof(buffer) - len, "%03d  ",
	                (int)tv.tv_usec / 1000);
	/* message ident */
	if (ident)
		len += snprintf(buffer + len, sizeof(buffer) - len, "%s", ident);
	/* peer */
	if (peer) {
		char *peer_name = od_getpeername(peer);
		len += snprintf(buffer + len, sizeof(buffer) - len, "%s", peer_name);
	}
	/* message */
	len += vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
	va_end(args);
	len += snprintf(buffer + len, sizeof(buffer), "\n");
	int rc = write(l->fd, buffer, len);
	od_syslog(l->syslog, prio, buffer);
	return rc > 0;
}
