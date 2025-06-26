#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
/* for hba tests */
#include <arpa/inet.h>
#include <pthread.h>

extern char test_prefix[1024];

/* This type of test will run in two cases:
1. No test prefix is provided to the odyssey_test binary.
2. A test prefix is provided, and the function's name 
starts with this prefix. */
#define odyssey_test(function)                                                \
	do {                                                                  \
		if (strlen(test_prefix) != 0 &&                               \
		    strcasestr(#function, test_prefix) == NULL) {             \
			break;                                                \
		}                                                             \
		struct timeval tv_start, tv_stop;                             \
		fprintf(stdout, "%s: ", #function);                           \
		fflush(stdout);                                               \
		gettimeofday(&tv_start, NULL);                                \
		(function)();                                                 \
		gettimeofday(&tv_stop, NULL);                                 \
		fprintf(stdout, "ok (%ld ms)\n",                              \
			(tv_stop.tv_sec - tv_start.tv_sec) * 1000 +           \
				(tv_stop.tv_usec - tv_start.tv_usec) / 1000); \
	} while (0);

/* This test type is analogous to odyssey_test, 
but will not run if a test prefix isn't provided.
It is not intended to run in CI, but rather serves as 
a playground for manual checking and experimenting. */
#define odyssey_playground_test(function)       \
	do {                                    \
		if (strlen(test_prefix) == 0) { \
			break;                  \
		}                               \
		odyssey_test(function);         \
	} while (0);

#define test(expression)                                                    \
	do {                                                                \
		if (!(expression)) {                                        \
			fprintf(stdout,                                     \
				"fail (%s:%d) with error \"%s\" (%d) %s\n", \
				__FILE__, __LINE__, strerror(errno), errno, \
				#expression);                               \
			fflush(stdout);                                     \
			abort();                                            \
		}                                                           \
	} while (0);

static inline void odyssey_test_set_test_prefix(const char *prefix)
{
	test(strlen(prefix) < (int)sizeof(test_prefix) - 1);
	strcpy(test_prefix, prefix);
}
