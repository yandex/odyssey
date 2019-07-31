#ifndef ODYSSEY_TEST_H
#define ODYSSEY_TEST_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <errno.h>
#include <string.h>

#define odyssey_test(function) \
	do { \
		struct timeval tv_start, tv_stop; \
		fprintf(stdout, "%s: ", #function); \
		fflush(stdout); \
		gettimeofday(&tv_start, NULL); \
		(function)(); \
		gettimeofday(&tv_stop, NULL); \
		fprintf(stdout, "ok %ld.%06ld\n", tv_stop.tv_sec - tv_start.tv_sec, tv_stop.tv_usec - tv_start.tv_usec); \
	} while (0);

#define test(expression) \
	do { \
		if (! (expression)) { \
			fprintf(stdout, "fail (%s:%d) with error \"%s\" (%d) %s\n", \
			        __FILE__, __LINE__, strerror(errno), errno, #expression); \
			fflush(stdout); \
			abort(); \
		} \
	} while (0);

#endif /* ODYSSEY_TEST_H */
