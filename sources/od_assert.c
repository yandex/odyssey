/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/context.h>

#if defined(__GLIBC__) || defined(__APPLE__)
#include <execinfo.h>
#define HAVE_BACKTRACE 1
#endif

#include <logger.h>
#include <global.h>
#include <instance.h>
#include <od_assert.h>

#define OD_ASSERT_MAX_FRAMES 64

void od_assert_fail(const char *expr, const char *file, int line)
{
	od_global_t *global = od_global_get();
	od_instance_t *instance = global == NULL ? NULL : global->instance;

	static OD_THREAD_LOCAL char log_buf[4096];
	memset(log_buf, 0, sizeof(log_buf));

#ifdef HAVE_BACKTRACE
	void *frames[OD_ASSERT_MAX_FRAMES];
	int nframes = backtrace(frames, OD_ASSERT_MAX_FRAMES);

	/*
	 * trim frames past coroutine entry point (mm_context_runner)
	 * use a union to convert function pointer to void* without
	 * triggering
	 */
	union {
		void (*fn)(void);
		void *ptr;
	} runner_addr = { .fn = mm_context_runner };

	for (int i = 0; i < nframes; i++) {
		if (frames[i] == runner_addr.ptr) {
			nframes = i;
			break;
		}
	}

	char **symbols = backtrace_symbols(frames, nframes);

	char *log_buf_ptr = log_buf;
	char *log_buf_end = log_buf + sizeof(log_buf);

	for (int i = 0; i < nframes && log_buf_ptr < log_buf_end; i++) {
		log_buf_ptr +=
			snprintf(log_buf_ptr, log_buf_end - log_buf_ptr,
				 "#%d %s\n", i, symbols ? symbols[i] : "??");
	}

	free(symbols);
#else
	snprintf(log_buf, sizeof(log_buf), "(backtrace unavailable)\n");
#endif

	fprintf(stderr, "assertion failed: %s (%s:%d)\n%s\n", expr, file, line,
		log_buf);

	if (global != NULL && instance != NULL) {
		od_error(&instance->logger, "assert", NULL, NULL,
			 "assertion failed: %s (%s:%d)\n%s\n", expr, file, line,
			 log_buf);
		od_logger_flush(&instance->logger);
	}

	fflush(stderr);

	abort();
}
