
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include <machinarium/machinarium.h>
#include <machinarium/channel_limit.h>

#include <logger.h>
#include <client.h>
#include <dns.h>
#include <server.h>
#include <route.h>
#include <rules.h>
#include <msg.h>
#include <global.h>
#include <util.h>

typedef struct {
	char *name;
	int id;
} od_log_syslog_facility_t;

static od_log_syslog_facility_t od_log_syslog_facilities[] = {
	{ "daemon", LOG_DAEMON },
	{ "user", LOG_USER },
	{ "local0", LOG_LOCAL0 },
	{ "local1", LOG_LOCAL1 },
	{ "local2", LOG_LOCAL2 },
	{ "local3", LOG_LOCAL3 },
	{ "local4", LOG_LOCAL4 },
	{ "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 },
	{ "local7", LOG_LOCAL7 },
	{ NULL, 0 }
};

static int od_log_syslog_level[] = { LOG_INFO, LOG_ERR, LOG_DEBUG, LOG_CRIT };

static char *od_log_level[] = { "info", "error", "debug", "fatal" };

od_retcode_t od_logger_init(od_logger_t *logger, od_pid_t *pid)
{
	logger->pid = pid;
	logger->log_debug = 0;
	logger->log_stdout = 1;
	logger->log_syslog = 0;
	logger->format = NULL;
	logger->format_len = 0;
	logger->format_type = OD_LOGGER_FORMAT_TEXT;
	logger->fd = -1;
	atomic_init(&logger->loaded, 0);

	memset(&logger->tasks, 0, sizeof(mm_queue_t));
	logger->slots = NULL;

	atomic_init(&logger->dropped_lines, 0);

	od_list_init(&logger->free_slots);
	pthread_spin_init(&logger->free_slots_lock, PTHREAD_PROCESS_PRIVATE);

	mm_wait_list_init(&logger->notifier, NULL);

	/* set temporary format */
	od_logger_set_format(logger, "%p %t %l (%c) %h %m\n");

	return OK_RESPONSE;
}

static inline void od_logger(void *arg);

od_retcode_t od_logger_load(od_logger_t *logger)
{
	if (!logger->async) {
		return OK_RESPONSE;
	}

	if (atomic_load(&logger->loaded) == 1) {
		return NOT_OK_RESPONSE;
	}

	int rc = mm_queue_init(&logger->tasks, (size_t)logger->queue_depth,
			       sizeof(od_logger_slot_t *), NULL);
	if (rc != 0) {
		return NOT_OK_RESPONSE;
	}

	logger->slots =
		od_malloc(logger->queue_depth * sizeof(od_logger_slot_t));
	if (logger->slots == NULL) {
		mm_queue_destroy(&logger->tasks);
		return NOT_OK_RESPONSE;
	}

	size_t n = (size_t)logger->queue_depth;
	for (size_t i = 0; i < n; ++i) {
		od_logger_slot_t *slot = &logger->slots[i];
		memset(slot, 0, sizeof(od_logger_slot_t));
		od_list_init(&slot->link);

		od_list_append(&logger->free_slots, &slot->link);
	}
	logger->free_slots_count = n;

	char name[32];
	od_snprintf(name, sizeof(name), "logger");
	logger->machine = machine_create(name, od_logger, logger);

	if (logger->machine == -1) {
		od_free(logger->slots);
		mm_queue_destroy(&logger->tasks);
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

int od_logger_open(od_logger_t *logger, char *path)
{
	logger->fd = open(path, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (logger->fd == -1) {
		return -1;
	}
	return 0;
}

int od_logger_reopen(od_logger_t *logger, char *path)
{
	int old_fd = logger->fd;
	int rc = od_logger_open(logger, path);
	if (rc == -1) {
		logger->fd = old_fd;
	} else if (old_fd != -1) {
		close(old_fd);
	}
	return rc;
}

int od_logger_open_syslog(od_logger_t *logger, char *ident, char *facility)
{
	int facility_id = LOG_DAEMON;
	if (facility) {
		int i = 0;
		od_log_syslog_facility_t *facility_ptr;
		for (;;) {
			facility_ptr = &od_log_syslog_facilities[i];
			if (facility_ptr->name == NULL) {
				break;
			}
			if (strcasecmp(facility_ptr->name, facility) == 0) {
				facility_id = facility_ptr->id;
				break;
			}
			i++;
		}
	}
	logger->log_syslog = 1;
	if (ident == NULL) {
		ident = "odyssey";
	}
	openlog(ident, 0, facility_id);
	return 0;
}

void od_logger_close(od_logger_t *logger)
{
	if (logger->fd != -1) {
		close(logger->fd);
	}
	logger->fd = -1;
}

static char od_logger_escape_tab[256] = {
	['\0'] = '0', ['\t'] = 't',  ['\n'] = 'n',
	['\r'] = 'r', ['\\'] = '\\', ['='] = '='
};

__attribute__((hot)) static inline int od_logger_escape(char *dest, int size,
							char *fmt, va_list args)
{
	char prefmt[512];
	int prefmt_len;
	prefmt_len = od_vsnprintf(prefmt, sizeof(prefmt), fmt, args);

	char *dst_pos = dest;
	char *dst_end = dest + size;
	char *msg_pos = prefmt;
	char *msg_end = prefmt + prefmt_len;

	while (msg_pos < msg_end) {
		char escaped_char;
		escaped_char = od_logger_escape_tab[(unsigned char)*msg_pos];
		if (od_unlikely(escaped_char)) {
			if (od_unlikely((dst_end - dst_pos) < 2)) {
				break;
			}
			dst_pos[0] = '\\';
			dst_pos[1] = escaped_char;
			dst_pos += 2;
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1)) {
				break;
			}
			dst_pos[0] = *msg_pos;
			dst_pos += 1;
		}
		msg_pos++;
	}
	return dst_pos - dest;
}

__attribute__((hot)) static inline int
od_logger_format(od_logger_t *logger, od_logger_level_t level, char *context,
		 od_client_t *client, od_server_t *server, char *fmt,
		 va_list args, char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	char *format_pos = logger->format;
	char *format_end = logger->format + logger->format_len;
	char peer[128];

	int len;
	while (format_pos < format_end) {
		if (*format_pos == '\\') {
			format_pos++;
			if (od_unlikely(format_pos == format_end)) {
				break;
			}
			if (od_unlikely((dst_end - dst_pos) < 1)) {
				break;
			}
			switch (*format_pos) {
			case '\\':
				dst_pos[0] = '\\';
				dst_pos += 1;
				break;
			case 'n':
				dst_pos[0] = '\n';
				dst_pos += 1;
				break;
			case 't':
				dst_pos[0] = '\t';
				dst_pos += 1;
				break;
			case 'r':
				dst_pos[0] = '\r';
				dst_pos += 1;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2)) {
					break;
				}
				dst_pos[0] = '\\';
				dst_pos[1] = *format_pos;
				dst_pos += 2;
				break;
			}
		} else if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end)) {
				break;
			}
			switch (*format_pos) {
			/* external_id */
			case 'x': {
				if (client && client->external_id != NULL) {
					len = od_snprintf(dst_pos,
							  dst_end - dst_pos,
							  "%s",
							  client->external_id);
					dst_pos += len;
					break;
				}

				/* fall through fix (if client is not defined will write 'none' to log file) */
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			}
			/* unixtime */
			case 'n': {
				time_t tm = time(NULL);
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%lu", tm);
				dst_pos += len;
				break;
			}
			/* timestamp */
			case 't': {
				struct timeval tv;
				gettimeofday(&tv, NULL);
				struct tm tm;
				len = strftime(dst_pos, dst_end - dst_pos,
					       "%FT%TZ",
					       gmtime_r(&tv.tv_sec, &tm));
				dst_pos += len;

				break;
			}
			/* millis */
			case 'e': {
				struct timeval tv;
				gettimeofday(&tv, NULL);
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%03d",
						  (signed)tv.tv_usec / 1000);
				dst_pos += len;
				break;
			}
			/* pid */
			case 'p':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", logger->pid->pid_sz);
				dst_pos += len;
				break;
			/* thread id */
			case 'T':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "0x%llx", pthread_self());
				dst_pos += len;
				break;
			/* client id */
			case 'i':
				if (client && client->id.id_prefix != NULL) {
					len = od_snprintf(
						dst_pos, dst_end - dst_pos,
						"%s%.*s", client->id.id_prefix,
						(signed)sizeof(client->id.id),
						client->id.id);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* server id */
			case 's':
				if (server && server->id.id_prefix != NULL) {
					len = od_snprintf(
						dst_pos, dst_end - dst_pos,
						"%s%.*s", server->id.id_prefix,
						(signed)sizeof(server->id.id),
						server->id.id);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* user name */
			case 'u':
				if (client && client->startup.user.value_len) {
					len = od_snprintf(
						dst_pos, dst_end - dst_pos,
						"%s",
						client->startup.user.value);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* database name */
			case 'd':
				if (client &&
				    client->startup.database.value_len) {
					len = od_snprintf(
						dst_pos, dst_end - dst_pos,
						"%s",
						client->startup.database.value);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* context */
			case 'c':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", context);
				dst_pos += len;
				break;
			/* level */
			case 'l':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", od_log_level[level]);
				dst_pos += len;
				break;
			/* message */
			case 'm':
				len = od_vsnprintf(dst_pos, dst_end - dst_pos,
						   fmt, args);
				dst_pos += len;
				break;
			/* message (escaped) */
			case 'M':
				len = od_logger_escape(
					dst_pos, dst_end - dst_pos, fmt, args);
				dst_pos += len;
				break;
			/* server host */
			case 'H':
				if (client && client->route) {
					od_route_t *route_ref = client->route;
					len = od_snprintf(
						dst_pos, dst_end - dst_pos,
						"%s",
						route_ref->rule->storage->host);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* client host */
			case 'h':
				if (client && client->io.io) {
					od_getpeername(client->io.io, peer,
						       sizeof(peer), 1, 0);
					len = od_snprintf(dst_pos,
							  dst_end - dst_pos,
							  "%s", peer);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			/* client port */
			case 'r':
				if (client && client->io.io) {
					od_getpeername(client->io.io, peer,
						       sizeof(peer), 0, 1);
					len = od_snprintf(dst_pos,
							  dst_end - dst_pos,
							  "%s", peer);
					dst_pos += len;
					break;
				}
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "none");
				dst_pos += len;
				break;
			case '%':
				if (od_unlikely((dst_end - dst_pos) < 1)) {
					break;
				}
				dst_pos[0] = '%';
				dst_pos += 1;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2)) {
					break;
				}
				dst_pos[0] = '%';
				dst_pos[1] = *format_pos;
				dst_pos += 2;
				break;
			}
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1)) {
				break;
			}
			dst_pos[0] = *format_pos;
			dst_pos += 1;
		}
		format_pos++;
	}

	/* append new line, if format string doesn't have it */
	if (dst_pos < dst_end && dst_pos > output && *(dst_pos - 1) != '\n') {
		*dst_pos = '\n';
		++dst_pos;
	}

	return dst_pos - output;
}

static inline void _od_logger_write_batch(od_logger_t *l,
					  od_logger_slot_t *slots[],
					  struct iovec *iovecs, size_t n)
{
	int rc;
	if (l->fd != -1) {
		rc = writev(l->fd, iovecs, n);
	}
	if (l->log_stdout) {
		rc = writev(STDOUT_FILENO, iovecs, n);
	}
	if (l->log_syslog) {
		for (size_t i = 0; i < n; ++i) {
			syslog(od_log_syslog_level[slots[i]->level], "%.*s",
			       (int)iovecs[i].iov_len,
			       (char *)iovecs[i].iov_base);
		}
	}
	(void)rc;
}

static inline void _od_logger_write(od_logger_t *l, char *data, int len,
				    od_logger_level_t lvl)
{
	int rc;
	if (l->fd != -1) {
		rc = write(l->fd, data, len);
	}
	if (l->log_stdout) {
		rc = write(STDOUT_FILENO, data, len);
	}
	if (l->log_syslog) {
		syslog(od_log_syslog_level[lvl], "%.*s", len, data);
	}
	(void)rc;
}

static inline void log_machine_stats(od_logger_t *logger)
{
	uint64_t count_coroutine = 0;
	uint64_t count_coroutine_cache = 0;
	uint64_t msg_allocated = 0;
	uint64_t msg_cache_count = 0;
	uint64_t msg_cache_gc_count = 0;
	uint64_t msg_cache_size = 0;
	machine_stat(&count_coroutine, &count_coroutine_cache, &msg_allocated,
		     &msg_cache_count, &msg_cache_gc_count, &msg_cache_size);

	od_log(logger, "stats", NULL, NULL,
	       "logger: msg (%" PRIu64 " allocated, %" PRIu64
	       " cached, %" PRIu64 " freed, %" PRIu64 " cache_size), "
	       "coroutines (%" PRIu64 " active, %" PRIu64 " cached), "
	       "dropped lines %" PRIu64 ", queue size %" PRIu64,
	       msg_allocated, msg_cache_count, msg_cache_gc_count,
	       msg_cache_size, count_coroutine, count_coroutine_cache,
	       atomic_load(&logger->dropped_lines),
	       mm_queue_size(&logger->tasks));
}

void od_logger_stat(od_logger_t *logger)
{
	log_machine_stats(logger);
}

static inline void od_logger(void *arg)
{
	od_logger_t *logger = arg;

	atomic_store(&logger->loaded, 1);

	while (atomic_load(&logger->loaded) == 1) {
		static od_logger_slot_t *slot_buf[IOV_MAX];
		static struct iovec iovecs[IOV_MAX];
		size_t nmsg =
			mm_queue_pop_batch(&logger->tasks, slot_buf, IOV_MAX);

		for (size_t i = 0; i < nmsg; ++i) {
			iovecs[i].iov_base = slot_buf[i]->text;
			iovecs[i].iov_len = slot_buf[i]->len;
		}

		_od_logger_write_batch(logger, slot_buf, iovecs, nmsg);

		pthread_spin_lock(&logger->free_slots_lock);
		for (size_t i = 0; i < nmsg; ++i) {
			od_list_append(&logger->free_slots,
				       &(slot_buf[i]->link));
		}
		logger->free_slots_count += nmsg;
		pthread_spin_unlock(&logger->free_slots_lock);

		if (mm_queue_size(&logger->tasks) == 0) {
			mm_wait_list_wait(&logger->notifier, NULL, 500);
		}
	}
}

void od_logger_shutdown(od_logger_t *logger)
{
	atomic_store(&logger->loaded, 0);
}

void od_logger_wait_finish(od_logger_t *logger)
{
	if (!logger->async) {
		return;
	}

	if (machine_wait(logger->machine)) {
		abort();
	}
	mm_wait_list_destroy(&logger->notifier);
	mm_queue_destroy(&logger->tasks);
	od_free(logger->slots);
}

static char od_logger_json_escape_tab[256] = {
	['"'] = '"',  ['\\'] = '\\', ['/'] = '/',  ['\b'] = 'b',
	['\f'] = 'f', ['\n'] = 'n',  ['\r'] = 'r', ['\t'] = 't'
};

__attribute__((hot)) static inline char *
od_logger_json_append_escaped(char *dst, char *dst_end, const char *src)
{
	if (!src) {
		return dst;
	}

	while (*src && dst < dst_end) {
		char escaped = od_logger_json_escape_tab[(unsigned char)*src];
		if (escaped) {
			if (dst + 2 >= dst_end) {
				break;
			}
			*dst++ = '\\';
			*dst++ = escaped;
		} else if ((unsigned char)*src < 0x20) {
			if (dst + 6 >= dst_end) {
				break;
			}
			dst += snprintf(dst, 7, "\\u%04x", (unsigned char)*src);
		} else {
			*dst++ = *src;
		}
		src++;
	}
	return dst;
}

__attribute__((hot)) static inline char *
od_logger_json_add_string(char *dst, char *dst_end, const char *key,
			  const char *value, int add_comma)
{
	if (!value || dst >= dst_end) {
		return dst;
	}

	if (add_comma && dst < dst_end) {
		*dst++ = ',';
	}

	if (dst < dst_end) {
		*dst++ = '"';
	}
	dst = od_logger_json_append_escaped(dst, dst_end, key);
	if (dst < dst_end) {
		*dst++ = '"';
	}
	if (dst < dst_end) {
		*dst++ = ':';
	}

	if (dst < dst_end) {
		*dst++ = '"';
	}
	dst = od_logger_json_append_escaped(dst, dst_end, value);
	if (dst < dst_end) {
		*dst++ = '"';
	}

	return dst;
}

__attribute__((hot)) static inline int
od_logger_format_json(od_logger_t *logger, od_logger_level_t level,
		      char *context, void *client_ptr, void *server_ptr,
		      char *fmt, va_list args, char *output, int output_len)
{
	od_client_t *client = client_ptr;
	od_server_t *server = server_ptr;

	char *dst = output;
	char *dst_end = output + output_len - 2;
	int add_comma = 0;

	if (dst < dst_end) {
		*dst++ = '{';
	}

	/* timestamp */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm tm;
	char tmp_buf[64];
	strftime(tmp_buf, sizeof(tmp_buf), "%FT%TZ", gmtime_r(&tv.tv_sec, &tm));
	dst = od_logger_json_add_string(dst, dst_end, "timestamp", tmp_buf,
					add_comma);
	add_comma = 1;

	dst = od_logger_json_add_string(dst, dst_end, "pid",
					logger->pid->pid_sz, add_comma);

	memset(tmp_buf, 0, sizeof(tmp_buf));
	od_snprintf(tmp_buf, sizeof(tmp_buf), "0x%llx", pthread_self());
	dst = od_logger_json_add_string(dst, dst_end, "tid", tmp_buf,
					add_comma);

	dst = od_logger_json_add_string(dst, dst_end, "level",
					od_log_level[level], add_comma);

	dst = od_logger_json_add_string(dst, dst_end, "context", context,
					add_comma);

	if (dst < dst_end) {
		*dst++ = ',';
	}
	if (dst < dst_end) {
		*dst++ = '"';
	}
	dst = od_logger_json_append_escaped(dst, dst_end, "message");
	if (dst < dst_end) {
		*dst++ = '"';
	}
	if (dst < dst_end) {
		*dst++ = ':';
	}
	if (dst < dst_end) {
		*dst++ = '"';
	}

	char message[1024];
	int msg_len = vsnprintf(message, sizeof(message), fmt, args);
	if (msg_len >= (int)sizeof(message)) {
		msg_len = sizeof(message) - 1;
	}

	dst = od_logger_json_append_escaped(dst, dst_end, message);
	if (dst < dst_end) {
		*dst++ = '"';
	}

	/* client fields */
	if (client) {
		if (dst < dst_end) {
			*dst++ = ',';
		}
		if (dst < dst_end) {
			*dst++ = '"';
		}
		dst = od_logger_json_append_escaped(dst, dst_end, "client");
		if (dst < dst_end) {
			*dst++ = '"';
		}
		if (dst < dst_end) {
			*dst++ = ':';
		}
		if (dst < dst_end) {
			*dst++ = '{';
		}

		int client_comma = 0;

		if (client->id.id_prefix) {
			char client_id[64];
			snprintf(client_id, sizeof(client_id), "%s%.*s",
				 client->id.id_prefix,
				 (int)sizeof(client->id.id), client->id.id);
			dst = od_logger_json_add_string(
				dst, dst_end, "id", client_id, client_comma);
			client_comma = 1;
		}

		if (client->io.io) {
			char peer[64];
			od_getpeername(client->io.io, peer, sizeof(peer), 1, 0);
			dst = od_logger_json_add_string(dst, dst_end, "ip",
							peer, client_comma);
			client_comma = 1;

			od_getpeername(client->io.io, peer, sizeof(peer), 0, 1);
			dst = od_logger_json_add_string(dst, dst_end, "port",
							peer, client_comma);
		}

		if (client->startup.user.value_len) {
			dst = od_logger_json_add_string(
				dst, dst_end, "user",
				client->startup.user.value, client_comma);
			client_comma = 1;
		}

		if (client->startup.database.value_len) {
			dst = od_logger_json_add_string(
				dst, dst_end, "database",
				client->startup.database.value, client_comma);
			client_comma = 1;
		}

		if (client->external_id) {
			dst = od_logger_json_add_string(dst, dst_end,
							"external_id",
							client->external_id,
							client_comma);
			client_comma = 1;
		}

		if (client->route && client->route->rule &&
		    client->route->rule->storage) {
			dst = od_logger_json_add_string(
				dst, dst_end, "server_host",
				client->route->rule->storage->host,
				client_comma);
		}

		/* Close client object */
		if (dst < dst_end) {
			*dst++ = '}';
		}
	}

	/* server fields */
	if (server) {
		if (dst < dst_end) {
			*dst++ = ',';
		}
		if (dst < dst_end) {
			*dst++ = '"';
		}
		dst = od_logger_json_append_escaped(dst, dst_end, "server");
		if (dst < dst_end) {
			*dst++ = '"';
		}
		if (dst < dst_end) {
			*dst++ = ':';
		}
		if (dst < dst_end) {
			*dst++ = '{';
		}

		int server_comma = 0;

		if (server->id.id_prefix) {
			char server_id[64];
			snprintf(server_id, sizeof(server_id), "%s%.*s",
				 server->id.id_prefix,
				 (int)sizeof(server->id.id), server->id.id);
			dst = od_logger_json_add_string(
				dst, dst_end, "id", server_id, server_comma);
		}

		if (dst < dst_end) {
			*dst++ = '}';
		}
	}

	if (dst < dst_end) {
		*dst++ = '}';
	}
	if (dst < dst_end) {
		*dst++ = '\n';
	}

	return dst - output;
}

void od_logger_write(od_logger_t *logger, od_logger_level_t level,
		     char *context, void *client, void *server, char *fmt,
		     va_list args)
{
	if (logger == OD_LOGGER_GLOBAL) {
		logger = od_global_get_logger();
	}

	if (logger->fd == -1 && !logger->log_stdout && !logger->log_syslog) {
		return;
	}

	if (logger->format_type == OD_LOGGER_FORMAT_JSON && fmt &&
	    fmt[0] == '\0') {
		return;
	}

	if (level == OD_DEBUG) {
		int is_debug = logger->log_debug;
		if (!is_debug) {
			od_client_t *client_ref = client;
			od_server_t *server_ref = server;
			if (client_ref && client_ref->rule) {
				is_debug = client_ref->rule->log_debug;
			} else if (server_ref && server_ref->route) {
				od_route_t *route = server_ref->route;
				is_debug = route->rule->log_debug;
			}
		}
		if (!is_debug) {
			return;
		}
	}

	int len;
	char *output;
	size_t output_max;
	od_logger_slot_t *async_slot = NULL;

	uint64_t async = atomic_load(&logger->loaded);

	if (async) {
		pthread_spin_lock(&logger->free_slots_lock);
		if (logger->free_slots_count > 0) {
			od_list_t *i = od_list_pop(&logger->free_slots);
			async_slot = od_container_of(i, od_logger_slot_t, link);
			logger->free_slots_count--;
		}
		pthread_spin_unlock(&logger->free_slots_lock);

		if (async_slot == NULL) {
			/* silently drop lines for overloaded logger */
			atomic_fetch_add(&logger->dropped_lines, 1);
			return;
		}

		output = async_slot->text;
		output_max = sizeof(async_slot->text);
	} else {
		static OD_THREAD_LOCAL char localoutput[OD_LOGLINE_MAXLEN];
		output = localoutput;
		output_max = sizeof(localoutput);
	}

	/* Choose formatter based on format type */
	if (logger->format_type == OD_LOGGER_FORMAT_JSON) {
		len = od_logger_format_json(logger, level, context, client,
					    server, fmt, args, output,
					    output_max);
	} else {
		len = od_logger_format(logger, level, context, client, server,
				       fmt, args, output, output_max);
	}

	if (async_slot) {
		async_slot->len = (size_t)len;
		async_slot->text[len] = '\0';
		async_slot->level = level;

		int new_size =
			mm_queue_push_extended(&logger->tasks, &async_slot);
		if (new_size >= 30) {
			mm_wait_list_notify(&logger->notifier);
		}
	} else {
		_od_logger_write(logger, output, len, level);
	}
}
