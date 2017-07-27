#ifndef OD_LOGGER_H
#define OD_LOGGER_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_logger od_logger_t;

typedef enum
{
	OD_LOG,
	OD_LOG_ERROR,
	OD_LOG_CLIENT,
	OD_LOG_CLIENT_ERROR,
	OD_LOG_CLIENT_DEBUG,
	OD_LOG_SERVER,
	OD_LOG_SERVER_ERROR,
	OD_LOG_SERVER_DEBUG
} od_logger_event_t;

struct od_logger
{
	od_pid_t      *pid;
	int            log_debug;
	int            log_stdout;
	int            log_tskv;
	od_logsystem_t log_system;
	od_logfile_t   log;
};

void od_logger_init(od_logger_t*, od_pid_t*);
void od_logger_set_debug(od_logger_t*, int);
void od_logger_set_stdout(od_logger_t*, int);
void od_logger_set_tskv(od_logger_t*, int);
int  od_logger_open(od_logger_t*, char*);
int  od_logger_open_syslog(od_logger_t*, char*, char*);
void od_logger_close(od_logger_t*);
void od_loggerv(od_logger_t*, od_logger_event_t, od_id_t*, char*, char*, va_list);

static inline void
od_log(od_logger_t *logger, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG, NULL, NULL, fmt, args);
	va_end(args);
}

static inline void
od_error(od_logger_t *logger, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_ERROR, NULL, context, fmt, args);
	va_end(args);
}

static inline void
od_log_client(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_CLIENT, id, context, fmt, args);
	va_end(args);
}

static inline void
od_debug_client(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	if (! logger->log_debug)
		return;
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_CLIENT_DEBUG, id, context, fmt, args);
	va_end(args);
}

static inline void
od_error_client(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_CLIENT_ERROR, id, context, fmt, args);
	va_end(args);
}

static inline void
od_log_server(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_SERVER, id, context, fmt, args);
	va_end(args);
}

static inline void
od_debug_server(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	if (! logger->log_debug)
		return;
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_SERVER_DEBUG, id, context, fmt, args);
	va_end(args);
}

static inline void
od_error_server(od_logger_t *logger, od_id_t *id, char *context, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_loggerv(logger, OD_LOG_SERVER_ERROR, id, context, fmt, args);
	va_end(args);
}

#endif
