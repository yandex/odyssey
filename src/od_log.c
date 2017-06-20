
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <machinarium.h>
#include <shapito.h>

#include "od_macro.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"

int od_log_init(od_log_t *log, od_pid_t *pid, od_syslog_t *syslog)
{
	log->debug = 0;
	log->pid = pid;
	log->syslog = syslog;
	log->fd = 0;
	return 0;
}

int od_log_open(od_log_t *log, char *path)
{
	int rc = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (rc == -1)
		return -1;
	log->fd = rc;
	return 0;
}

int od_log_close(od_log_t *log)
{
	if (log->fd == -1)
		return 0;
	int rc = close(log->fd);
	log->fd = -1;
	return rc;
}

int od_logv(od_log_t *log, od_syslogprio_t prio,
            char *ident,
            char *object,
            uint64_t object_id,
            char *context,
            char *fmt, va_list args)
{
	char buffer[512];

	/* pid */
	int len;
	len = snprintf(buffer, sizeof(buffer), "%d ", log->pid->pid);
	/* time */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	len += strftime(buffer + len, sizeof(buffer) - len, "%d %b %H:%M:%S.",
	                localtime(&tv.tv_sec));
	len += snprintf(buffer + len, sizeof(buffer) - len, "%03d  ",
	                (int)tv.tv_usec / 1000);
	/* ident */
	if (ident)
		len += snprintf(buffer + len, sizeof(buffer) - len, "%s ", ident);

	/* object and id */
	if (object) {
		len += snprintf(buffer + len, sizeof(buffer) - len, "%s%" PRIu64": ",
		                object, object_id);
	}

	/* context */
	if (context) {
		len += snprintf(buffer + len, sizeof(buffer) - len, "(%s) ", context);
	}

	/* message */
	len += vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
	va_end(args);
	len += snprintf(buffer + len, sizeof(buffer), "\n");

	int rc = write(log->fd, buffer, len);
	od_syslog(log->syslog, prio, buffer);
	return rc > 0;
}
