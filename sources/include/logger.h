#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/ds/intrusive/lf-stack.h>
#include <machinarium/ds/intrusive/mpsc_queue.h>
#include <machinarium/wait_list.h>

#include <types.h>
#include <list.h>
#include <pid.h>

#define OD_LOGLINE_MAXLEN 1024

#define OD_LOGGER_FORMAT_MAX_TOKENS 128

#define OD_LOGGER_GLOBAL NULL

typedef struct od_logger od_logger_t;

typedef enum { OD_LOG, OD_ERROR, OD_DEBUG, OD_FATAL } od_logger_level_t;

typedef enum {
	OD_LOGGER_ONLINE,
	OD_LOGGER_REOPENING,
	OD_LOGGER_OFFLINE
} od_logger_state_t;

typedef enum {
	OD_LOGGER_FORMAT_TEXT,
	OD_LOGGER_FORMAT_JSON
} od_logger_format_type_t;

typedef enum {
	OD_FMT_LITERAL, /* literal text run */
	OD_FMT_PID, /* %p  */
	OD_FMT_TID, /* %T  */
	OD_FMT_TIMESTAMP, /* %t  */
	OD_FMT_MILLIS, /* %e  */
	OD_FMT_UNIXTIME, /* %n  */
	OD_FMT_LEVEL, /* %l  */
	OD_FMT_CONTEXT, /* %c  */
	OD_FMT_MESSAGE, /* %m  */
	OD_FMT_MESSAGE_ESC, /* %M  */
	OD_FMT_CLIENT_ID, /* %i  */
	OD_FMT_SERVER_ID, /* %s  */
	OD_FMT_USER, /* %u  */
	OD_FMT_DATABASE, /* %d  */
	OD_FMT_APPLICATION_NAME, /* %a  */
	OD_FMT_EXTERNAL_ID, /* %x  */
	OD_FMT_CLIENT_HOST, /* %h  */
	OD_FMT_CLIENT_PORT, /* %r  */
	OD_FMT_SERVER_HOST /* %H  */
} od_fmt_token_type_t;

typedef struct {
	od_fmt_token_type_t type;
	/* owner is original format string */
	const char *literal;
	int literal_len;
} od_fmt_token_t;

typedef struct {
	od_logger_level_t level;
	mm_lf_stack_entry_t link;
	mm_mpsc_node_t node;
	size_t len;
	char text[OD_LOGLINE_MAXLEN];
} od_logger_slot_t;

struct od_logger {
	od_pid_t *pid;
	int log_debug;
	int log_debug_sampling;
	int log_stdout;
	int log_syslog;
	int async;
	int queue_depth;
	char *format;
	int format_len;
	od_logger_format_type_t format_type;

	/* precompiled format-tokens */
	od_fmt_token_t tokens[OD_LOGGER_FORMAT_MAX_TOKENS];
	int tokens_count;

	atomic_int fd;
	atomic_int batching;

	atomic_uint_fast64_t state;
	int64_t machine;

	od_logger_slot_t *slots;
	mm_mpsc_queue_t tasks;

	/*
	 * The last node popped from tasks.  It is now the queue's head
	 * sentinel, so its `next` may still be written by a producer.
	 * We cannot return it to free_slots until the next pop() advances
	 * head past it. Recycled in the next process_log_queue() call.
	 */
	od_logger_slot_t *pending_slot;

	mm_lf_stack_t free_slots;

	mm_wait_list_t notifier;

	atomic_uint_fast64_t dropped_lines;
};

extern od_retcode_t od_logger_init(od_logger_t *, od_pid_t *);
extern od_retcode_t od_logger_load(od_logger_t *logger);

void od_logger_stat(od_logger_t *logger);

static inline void od_logger_set_debug(od_logger_t *logger, int enable,
				       int sampling)
{
	logger->log_debug = enable;
	logger->log_debug_sampling = sampling;
}

static inline void od_logger_set_stdout(od_logger_t *logger, int enable)
{
	logger->log_stdout = enable;
}

void od_logger_set_format(od_logger_t *logger, char *format);

static inline void od_logger_set_async(od_logger_t *logger, int async)
{
	logger->async = async;
}

static inline void od_logger_set_queue_depth(od_logger_t *logger,
					     int queue_depth)
{
	if (queue_depth <= 0) {
		queue_depth = 30000;
	}

	logger->queue_depth = queue_depth;
}

extern int od_logger_open(od_logger_t *, const char *);
extern int od_logger_reopen(od_logger_t *);
extern int od_logger_open_syslog(od_logger_t *, char *, char *);
extern void od_logger_shutdown(od_logger_t *);
extern void od_logger_close(od_logger_t *);
extern void od_logger_write(od_logger_t *, od_logger_level_t, od_client_t *,
			    od_server_t *, const char *, int, const char *, ...)
	__attribute__((format(printf, 7, 8)));

/*
 * Reports whether a debug message from the given client/server context
 * would pass the debug gate in od_logger_write (global or rule-level
 * debug enabled, sampling not taken into account).
 */
extern int od_logger_debug_enabled(od_logger_t *, od_client_t *, od_server_t *);

void od_logger_wait_finish(od_logger_t *);
void od_logger_flush(od_logger_t *);

#define od_log(logger, client, server, ...)                                 \
	od_logger_write(logger, OD_LOG, client, server, __FILE__, __LINE__, \
			__VA_ARGS__)

#define od_glog(client, server, ...)                                        \
	od_logger_write(OD_LOGGER_GLOBAL, OD_LOG, client, server, __FILE__, \
			__LINE__, __VA_ARGS__)

#define od_debug(logger, client, server, ...)                                 \
	od_logger_write(logger, OD_DEBUG, client, server, __FILE__, __LINE__, \
			__VA_ARGS__)

#define od_gdebug(client, server, ...)                                        \
	od_logger_write(OD_LOGGER_GLOBAL, OD_DEBUG, client, server, __FILE__, \
			__LINE__, __VA_ARGS__)

#define od_error(logger, client, server, ...)                                 \
	od_logger_write(logger, OD_ERROR, client, server, __FILE__, __LINE__, \
			__VA_ARGS__)

#define od_gerror(client, server, ...)                                        \
	od_logger_write(OD_LOGGER_GLOBAL, OD_ERROR, client, server, __FILE__, \
			__LINE__, __VA_ARGS__)

#define od_fatal(logger, client, server, ...)                               \
	do {                                                                \
		od_logger_write(logger, OD_FATAL, client, server, __FILE__, \
				__LINE__, __VA_ARGS__);                     \
		exit(1);                                                    \
	} while (0)

#define od_gfatal(client, server, ...)                                      \
	do {                                                                \
		od_logger_write(OD_LOGGER_GLOBAL, OD_FATAL, client, server, \
				__FILE__, __LINE__, __VA_ARGS__);           \
		exit(1);                                                    \
	} while (0)
