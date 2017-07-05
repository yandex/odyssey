#ifndef OD_LOG_H
#define OD_LOG_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_log od_log_t;

struct od_log
{
	int          fd;
	int          debug;
	od_pid_t    *pid;
	od_syslog_t *syslog;
};

int od_log_init(od_log_t*, od_pid_t*, od_syslog_t*);
int od_log_open(od_log_t*, char*);
int od_log_close(od_log_t*);
int od_logv(od_log_t*, od_syslogprio_t, char*,
            char*, od_id_t*, char*, char*, va_list);

static inline void
od_logset_debug(od_log_t *log, int enable)
{
	log->debug = enable;
}

static inline int
od_log(od_log_t *log, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_INFO, NULL, NULL, 0, NULL, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_error(od_log_t *log, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_ERROR, "error:", NULL, 0, context, fmt, args);
	va_end(args);
	return rc;
}

/* client */

static inline int
od_log_client(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_INFO, NULL, "c", id, context, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_debug_client(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	if (! log->debug)
		return 0;
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_INFO, "debug:", "c", id, context, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_error_client(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_ERROR, "error:", "c", id, context, fmt, args);
	va_end(args);
	return rc;
}

/* server */

static inline int
od_log_server(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_INFO, NULL, "s", id, context, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_debug_server(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	if (! log->debug)
		return 0;
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_INFO, "debug:", "s", id, context, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_error_server(od_log_t *log, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(log, OD_SYSLOG_ERROR, "error:", "s", id, context, fmt, args);
	va_end(args);
	return rc;
}

#endif /* OD_LOG_H */
