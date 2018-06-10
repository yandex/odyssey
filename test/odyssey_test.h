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
		fprintf(stdout, "%s: ", #function); \
		fflush(stdout); \
		(function)(); \
		fprintf(stdout, "ok\n"); \
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
