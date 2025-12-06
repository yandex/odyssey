#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define OD_LOGLINE_MAXLEN 1024

#define OD_LOGGER_GLOBAL NULL

typedef struct od_logger od_logger_t;

typedef enum { OD_LOG, OD_ERROR, OD_DEBUG, OD_FATAL } od_logger_level_t;

struct od_logger {
	od_pid_t *pid;
	int log_debug;
	int log_stdout;
	int log_syslog;
	char *format;
	int format_len;

	int fd;

	int loaded;
	int64_t machine;
	/* makes sense only with use_asynclog option on */
	machine_channel_t *task_channel;
};

extern od_retcode_t od_logger_init(od_logger_t *, od_pid_t *);
extern od_retcode_t od_logger_load(od_logger_t *logger);

static inline void od_logger_set_debug(od_logger_t *logger, int enable)
{
	logger->log_debug = enable;
}

static inline void od_logger_set_stdout(od_logger_t *logger, int enable)
{
	logger->log_stdout = enable;
}

static inline void od_logger_set_format(od_logger_t *logger, char *format)
{
	logger->format = format;
	logger->format_len = strlen(format);
}

extern int od_logger_open(od_logger_t *, char *);
extern int od_logger_reopen(od_logger_t *, char *);
extern int od_logger_open_syslog(od_logger_t *, char *, char *);
extern void od_logger_shutdown(od_logger_t *);
extern void od_logger_close(od_logger_t *);
extern void od_logger_write(od_logger_t *, od_logger_level_t, char *, void *,
			    void *, char *, va_list);
extern void od_logger_write_plain(od_logger_t *, od_logger_level_t, char *,
				  void *, void *, char *);

void od_logger_wait_finish(od_logger_t *);

static inline void od_log(od_logger_t *logger, char *context, void *client,
			  void *server, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(logger, OD_LOG, context, client, server, fmt, args);
	va_end(args);
}

static inline void od_glog(char *context, void *client, void *server, char *fmt,
			   ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(OD_LOGGER_GLOBAL, OD_LOG, context, client, server, fmt,
			args);
	va_end(args);
}

static inline void od_debug(od_logger_t *logger, char *context, void *client,
			    void *server, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(logger, OD_DEBUG, context, client, server, fmt, args);
	va_end(args);
}

static inline void od_gdebug(char *context, void *client, void *server,
			     char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(OD_LOGGER_GLOBAL, OD_DEBUG, context, client, server,
			fmt, args);
	va_end(args);
}

static inline void od_error(od_logger_t *logger, char *context, void *client,
			    void *server, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(logger, OD_ERROR, context, client, server, fmt, args);
	va_end(args);
}

static inline void od_gerror(char *context, void *client, void *server,
			     char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(OD_LOGGER_GLOBAL, OD_ERROR, context, client, server,
			fmt, args);
	va_end(args);
}

static inline void od_fatal(od_logger_t *logger, char *context, void *client,
			    void *server, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(logger, OD_FATAL, context, client, server, fmt, args);
	va_end(args);
	exit(1);
}

static inline void od_gfatal(char *context, void *client, void *server,
			     char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_logger_write(OD_LOGGER_GLOBAL, OD_FATAL, context, client, server,
			fmt, args);
	va_end(args);
	exit(1);
}

/* log functions to create module-owned loggers with predefined values */
static inline void od_vglog(char *context, void *client, void *server,
			    char *fmt, va_list args)
{
	od_logger_write(OD_LOGGER_GLOBAL, OD_LOG, context, client, server, fmt,
			args);
}

static inline void od_vgdebug(char *context, void *client, void *server,
			      char *fmt, va_list args)
{
	od_logger_write(OD_LOGGER_GLOBAL, OD_DEBUG, context, client, server,
			fmt, args);
}

static inline void od_vgerror(char *context, void *client, void *server,
			      char *fmt, va_list args)
{
	od_logger_write(OD_LOGGER_GLOBAL, OD_ERROR, context, client, server,
			fmt, args);
}

static inline void od_vgfatal(char *context, void *client, void *server,
			      char *fmt, va_list args)
{
	od_logger_write(OD_LOGGER_GLOBAL, OD_FATAL, context, client, server,
			fmt, args);
	exit(1);
}

#define DEFINE_SIMPLE_MODULE_LOGGER(mod, ctx)                                  \
	__attribute__((unused)) static inline void mod##_log(char *fmt, ...)   \
	{                                                                      \
		va_list args;                                                  \
		va_start(args, fmt);                                           \
		od_vglog(ctx, NULL, NULL, fmt, args);                          \
		va_end(args);                                                  \
	}                                                                      \
	__attribute__((unused)) static inline void mod##_debug(char *fmt, ...) \
	{                                                                      \
		va_list args;                                                  \
		va_start(args, fmt);                                           \
		od_vgdebug(ctx, NULL, NULL, fmt, args);                        \
		va_end(args);                                                  \
	}                                                                      \
	__attribute__((unused)) static inline void mod##_error(char *fmt, ...) \
	{                                                                      \
		va_list args;                                                  \
		va_start(args, fmt);                                           \
		od_vgerror(ctx, NULL, NULL, fmt, args);                        \
		va_end(args);                                                  \
	}                                                                      \
	__attribute__((unused)) static inline void mod##_fatal(char *fmt, ...) \
	{                                                                      \
		va_list args;                                                  \
		va_start(args, fmt);                                           \
		od_vgfatal(ctx, NULL, NULL, fmt, args);                        \
		va_end(args);                                                  \
	}
