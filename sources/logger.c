
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
#include <syslog.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/error.h"
#include "sources/atomic.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"

typedef struct
{
	char *name;
	int   id;
} od_log_syslog_facility_t;

od_log_syslog_facility_t od_log_syslog_facilities[] =
{
	{ "daemon", LOG_DAEMON },
	{ "user",   LOG_USER   },
	{ "local0", LOG_LOCAL0 },
	{ "local1", LOG_LOCAL1 },
	{ "local2", LOG_LOCAL2 },
	{ "local3", LOG_LOCAL3 },
	{ "local4", LOG_LOCAL4 },
	{ "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 },
	{ "local7", LOG_LOCAL7 },
	{  NULL,    0 }
};

static int od_log_syslog_level[] = {
	LOG_INFO, LOG_ERR, LOG_DEBUG, LOG_CRIT
};

static char *od_log_level[] = {
	"info", "error", "debug", "fatal"
};

void od_logger_init(od_logger_t *logger, od_pid_t *pid)
{
	logger->pid = pid;
	logger->log_debug = 0;
	logger->log_stdout = 1;
	logger->log_syslog = 0;
	logger->format = NULL;
	logger->format_len = 0;
	logger->fd = -1;
}

int od_logger_open(od_logger_t *logger, char *path)
{
	logger->fd = open(path, O_RDWR|O_CREAT|O_APPEND, 0644);
	if (logger->fd == -1)
		return -1;
	return 0;
}

int od_logger_open_syslog(od_logger_t *logger, char *ident, char *facility)
{
	int facility_id = LOG_DAEMON;
	if (facility) {
		int i = 0;
		od_log_syslog_facility_t *facility_ptr;
		for (;;) {
			facility_ptr = &od_log_syslog_facilities[i];
			if (facility_ptr->name == NULL)
				break;
			if (strcasecmp(facility_ptr->name, facility) == 0) {
				facility_id = facility_ptr->id;
				break;
			}
			i++;
		}
	}
	logger->log_syslog = 1;
	if (ident == NULL)
		ident = "odissey";
	openlog(ident, 0, facility_id);
	return 0;
}

void od_logger_close(od_logger_t *logger)
{
	if (logger->fd != -1)
		close(logger->fd);
	logger->fd = -1;
}

static char od_logger_escape_tab[256] =
{
	['\0'] = '0',
	['\t'] = 't',
	['\n'] = 'n',
	['\r'] = 'r',
	['\\'] = '\\',
	['=']  = '='
};

__attribute__((hot)) static inline int
od_logger_escape(char *dest, int size, char *fmt, va_list args)
{
	char prefmt[512];
	int  prefmt_len;
	prefmt_len = vsnprintf(prefmt, sizeof(prefmt), fmt, args);

	char *dst_pos = dest;
	char *dst_end = dest + size;
	char *msg_pos = prefmt;
	char *msg_end = prefmt + prefmt_len;

	while (msg_pos < msg_end) {
		char escaped_char;
		escaped_char = od_logger_escape_tab[(int)*msg_pos];
		if (od_unlikely(escaped_char)) {
			if (od_unlikely((dst_end - dst_pos) < 2))
				break;
			dst_pos[0]  = '\\';
			dst_pos[1]  = escaped_char;
			dst_pos    += 2;
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1))
				break;
			dst_pos[0]  = *msg_pos;
			dst_pos    += 1;
		}
		msg_pos++;
	}
	return dst_pos - dest;
}

__attribute__((hot)) static inline int
od_logger_format(od_logger_t *logger, od_logger_level_t level,
                 char *context,
                 od_client_t *client,
                 od_server_t *server,
                 char *fmt, va_list args,
                 char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	char *format_pos = logger->format;
	char *format_end = logger->format + logger->format_len;

	int len;
	while (format_pos < format_end)
	{
		if (*format_pos == '\\') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			if (od_unlikely((dst_end - dst_pos) < 1))
				break;
			switch (*format_pos) {
			case '\\':
				dst_pos[0] = '\\';
				dst_pos   += 1;
				break;
			case 'n':
				dst_pos[0] = '\n';
				dst_pos   += 1;
				break;
			case 't':
				dst_pos[0] = '\t';
				dst_pos   += 1;
				break;
			case 'r':
				dst_pos[0] = '\r';
				dst_pos   += 1;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2))
					break;
				dst_pos[0] = '\\';
				dst_pos[1] = *format_pos;
				dst_pos   += 2;
				break;
			}
		} else
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end))
				break;
			switch (*format_pos) {
			/* timestamp (ms unix epoch) */
			case 'n':
				break;
			/* timestamp (without ms) */
			case 't':
				break;
			/* pid */
			case 'p':
				len = snprintf(dst_pos, dst_end - dst_pos, "%s", logger->pid->pid_sz);
				dst_pos += len;
				break;
			/* client id */
			case 'i':
				if (client) {
					len = snprintf(dst_pos, dst_end - dst_pos, "%s%.*s",
					               client->id.id_prefix,
					               (signed)sizeof(client->id.id), client->id.id);
					dst_pos += len;
					break;
				}
				len = snprintf(dst_pos, dst_end - dst_pos, "none");
				dst_pos += len;
				break;
			/* server id */
			case 's':
				if (server) {
					len = snprintf(dst_pos, dst_end - dst_pos, "%s%.*s",
					               server->id.id_prefix,
					               (signed)sizeof(server->id.id), server->id.id);
					dst_pos += len;
					break;
				}
				len = snprintf(dst_pos, dst_end - dst_pos, "none");
				dst_pos += len;
				break;
			/* user name */
			case 'u':
				break;
			/* database name */
			case 'd':
				break;
			/* context */
			case 'c':
				len = snprintf(dst_pos, dst_end - dst_pos, "%s", context);
				dst_pos += len;
				break;
			/* level */
			case 'l':
				len = snprintf(dst_pos, dst_end - dst_pos, "%s", od_log_level[level]);
				dst_pos += len;
				break;
			/* message */
			case 'm':
				len = vsnprintf(dst_pos, dst_end - dst_pos, fmt, args);
				dst_pos += len;
				break;
			/* message (escaped) */
			case 'M':
				len = od_logger_escape(dst_pos, dst_end - dst_pos, fmt, args);
				dst_pos += len;
				break;
			/* remote host */
			case 'h':
				break;
			/* remote port */
			case 'r':
				break;
			case '%':
				if (od_unlikely((dst_end - dst_pos) < 1))
					break;
				dst_pos[0] = '%';
				dst_pos   += 1;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2))
					break;
				dst_pos[0] = '%';
				dst_pos[1] = *format_pos;
				dst_pos   += 2;
				break;
			}
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1))
				break;
			dst_pos[0] = *format_pos;
			dst_pos   += 1;
		}
		format_pos++;
	}
	return dst_pos - output;
}

void od_logger_write(od_logger_t *logger, od_logger_level_t level,
                     char *context,
                     void *client, void *server,
                     char *fmt, va_list args)
{
	char output[512];
	int  len;
	len = od_logger_format(logger, level, context, client, server,
	                       fmt, args, output, sizeof(output));
	if (logger->fd != -1) {
		(void)write(logger->fd, output, len);
	}
	if (logger->log_stdout) {
		(void)write(0, output, len);
	}
	if (logger->log_syslog) {
		syslog(od_log_syslog_level[level], "%.*s", len, output);
	}
}
