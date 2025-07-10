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
#include <time.h>
/* for hba tests */
#include <arpa/inet.h>
#include <pthread.h>

extern char test_substring[1024];

/* This type of test will run in two cases:
1. No test substring is provided to the odyssey_test binary.
2. A test substring is provided, and the function's name 
contains this substring. */
#define odyssey_test(function)                                                \
	do {                                                                  \
		if (strlen(test_substring) != 0 &&                            \
		    strcasestr(#function, test_substring) == NULL) {          \
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
but will not run if a test substring isn't provided.
It is not intended to run in CI, but rather serves as 
a playground for manual checking and experimenting. */
#define odyssey_playground_test(function)          \
	do {                                       \
		if (strlen(test_substring) == 0) { \
			break;                     \
		}                                  \
		odyssey_test(function);            \
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

static inline void odyssey_test_set_test_substring(const char *substring)
{
	test(strlen(substring) < (int)sizeof(test_substring) - 1);
	strcpy(test_substring, substring);
}
