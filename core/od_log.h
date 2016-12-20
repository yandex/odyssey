#ifndef OD_LOG_H_
#define OD_LOG_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_log_t od_log_t;

struct od_log_t {
	od_pid_t *pid;
	od_syslog_t *syslog;
	int fd;
};

int od_loginit(od_log_t*, od_pid_t*, od_syslog_t*);
int od_logopen(od_log_t*, char*);
int od_logclose(od_log_t*);
int od_logv(od_log_t*, od_syslogprio_t, mm_io_t, char*, char*, va_list);

static inline int
od_log(od_log_t *l, mm_io_t peer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(l, OD_SYSLOG_INFO, peer, NULL, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_debug(od_log_t *l, mm_io_t peer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(l, OD_SYSLOG_DEBUG, peer, "debug: ", fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_error(od_log_t *l, mm_io_t peer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = od_logv(l, OD_SYSLOG_ERROR, peer, "error: ", fmt, args);
	va_end(args);
	return rc;
}

#endif
